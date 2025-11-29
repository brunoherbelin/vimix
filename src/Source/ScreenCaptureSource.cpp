/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#include "Log.h"
#include "Scene/Decorations.h"
#include "Stream.h"
#include "Visitor/Visitor.h"

#include "ScreenCaptureSource.h"

#ifndef NDEBUG
#define SCREENCAPTURE_DEBUG
#endif



#if defined(APPLE)
std::string gst_plugin_vidcap = "avfvideosrc capture-screen=true";
#else
std::string gst_plugin_vidcap = "ximagesrc show-pointer=false";

#include <xcb/xcb.h>
#include <X11/Xlib.h>
#include <xcb/xproto.h>
int X11_error_handler(Display *d, XErrorEvent *e);
std::map<unsigned long, std::string> getListX11Windows();

#endif


struct hasScreenCaptureId
{
    inline bool operator()(const ScreenCaptureHandle &elem) const {
       return (elem.id == _id);
    }
    explicit hasScreenCaptureId(unsigned long id) : _id(id) { }
private:
    unsigned long _id;
};

struct hasScreenCaptureName
{
    inline bool operator()(const ScreenCaptureHandle &elem) const {
       return (elem.name.compare(_name) == 0);
    }
    explicit hasScreenCaptureName(const std::string &name) : _name(name) { }
private:
    std::string _name;
};

struct hasAssociatedSource
{
    inline bool operator()(const ScreenCaptureHandle &elem) const {
        auto sit = std::find(elem.associated_sources.begin(), elem.associated_sources.end(), s_);
        return sit != elem.associated_sources.end();
    }
    explicit hasAssociatedSource(Source *s) : s_(s) { }
private:
    Source *s_;
};

void ScreenCaptureHandle::update(const std::string &newname)
{
    name = newname;
}

void ScreenCapture::add(const std::string &windowname, const std::string &pipeline, unsigned long id)
{
    std::string pln = pipeline;

#if defined(LINUX)
    if (id > 0)
        pln += " xid=" + std::to_string(id);
#endif

    GstToolkit::PipelineConfigSet confs = GstToolkit::getPipelineConfigs(pln);

    if (!confs.empty()) {
        GstToolkit::PipelineConfig best = *confs.rbegin();
        GstToolkit::PipelineConfigSet confscreen;
        // limit framerate to 30fps
        best.fps_numerator = MIN( best.fps_numerator, 30);
        best.fps_denominator = 1;
        confscreen.insert(best);

        // remove previous occurence if already exists
        remove(windowname, id);

        // add to config list
        access_.lock();
        ScreenCaptureHandle handle;
        handle.name = windowname;
        handle.pipeline = pln;
        handle.id = id;
        handle.configs = confscreen;

        handles_.push_back(handle);
        access_.unlock();
    }

}


void ScreenCapture::remove(const std::string &windowname, unsigned long id)
{
    // lock before change
    access_.lock();

    std::vector< ScreenCaptureHandle >::const_iterator handle = handles_.cend();

    // if id is provided, find a device with the id
    if (id>0)
        handle = std::find_if(handles_.cbegin(), handles_.cend(), hasScreenCaptureId(id) );

    // nofound : find device with the window name
    if ( handle == handles_.cend() )
        handle = std::find_if(handles_.cbegin(), handles_.cend(), hasScreenCaptureName(windowname) );

    // found handle either by id or by name
    if ( handle != handles_.cend() ) {

        // just inform if there is no source connected
        if (handle->associated_sources.empty()) {
            Log::Info("Window %s available.", windowname.c_str());
        }
        else {
            // otherwise unplug all sources and close their streams
            for (auto sit = handle->associated_sources.begin(); sit != handle->associated_sources.end(); ++sit) {
                //  inform user
                Log::Warning("Window %s closed: source %s deleted.", windowname.c_str(), (*sit)->name().c_str());
                (*sit)->trash();
            }
        }

        // remove the handle
        handles_.erase(handle);
    }

    // unlock access
    access_.unlock();
}


//#ifdef LINUX
//Display *x11_display = NULL;
//#endif

ScreenCapture::ScreenCapture(): monitor_initialized_(false)
{
//#ifdef LINUX
//    // Capture X11 errors to prevent crashing when capturing a window that is closed
//    x11_display = XOpenDisplay(NULL);
//    XSetErrorHandler(X11_error_handler);
//#endif

    // launch monitor
    std::thread(launchMonitoring, this).detach();
}


ScreenCapture::~ScreenCapture()
{
//#ifdef LINUX
//    XCloseDisplay(x11_display);
//#endif
}

void ScreenCapture::launchMonitoring(ScreenCapture *sc)
{
#if defined(LINUX)

    // Get the list of windows in X11
    std::map<unsigned long,std::string> windowlist = getListX11Windows();
    // Add the window id=0 for screen capture entire screen
    windowlist[0] = SCREEN_CAPTURE_NAME;

    // Go through the current list of window handles
    sc->access_.lock();
    for (auto hit = sc->handles_.begin(); hit != sc->handles_.end();) {
        // if this window handle is still in X11 list
        if (windowlist.count(hit->id) > 0) {
            // update name of window (could have changed)
            hit->update(windowlist.at(hit->id));
            // remove from list of X11 windows
            windowlist.erase(hit->id);
            // keep in the window handles
            ++hit;
        }
        else {
            // remove from window handles
            hit = sc->handles_.erase(hit);
        }
    }
    sc->access_.unlock();

    // Add in window handles list the remaining X11 windows
    for (auto wit = windowlist.cbegin(); wit != windowlist.cend(); ++wit) {
        // Add capture screen of this window
        sc->add( wit->second, gst_plugin_vidcap, wit->first);
    }

    // monitor is initialized
    sc->monitor_initialized_ = true;
    sc->monitor_initialization_.notify_all();

    // give time before continuing
    std::this_thread::sleep_for ( std::chrono::seconds(2) );

    // de-initialize so next call will update
    sc->monitor_initialized_ = false;

#else
    // only one screen capture possibility on OSX
    sc->add(SCREEN_CAPTURE_NAME, gst_plugin_vidcap);

    // monitor is initialized
    sc->monitor_initialized_ = true;
    sc->monitor_initialization_.notify_all();
#endif
}

bool ScreenCapture::initialized()
{
    return ScreenCapture::manager().monitor_initialized_;
}

void ScreenCapture::reload()
{
    if ( !ScreenCapture::initialized() ) {
        // relaunch monitor
        std::thread(launchMonitoring, this).detach();

        // wait for monitor initialization (if not already initialized)
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        ScreenCapture::manager().monitor_initialization_.wait(lck, ScreenCapture::initialized);
    }
}

int ScreenCapture::numWindow()
{
    reload();

    access_.lock();
    int ret = handles_.size();
    access_.unlock();

    return ret;
}

bool ScreenCapture::exists(const std::string &window)
{
    reload();

    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasScreenCaptureName(window));
    bool ret = (h != handles_.cend());
    access_.unlock();

    return ret;
}

std::string ScreenCapture::name(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].name;
    access_.unlock();

    return ret;
}

std::string ScreenCapture::description(int index)
{
    std::string ret = "";

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].pipeline;
    access_.unlock();

    return ret;
}

GstToolkit::PipelineConfigSet ScreenCapture::config(int index)
{
    GstToolkit::PipelineConfigSet ret;

    access_.lock();
    if (index > -1 && index < (int) handles_.size())
        ret = handles_[index].configs;
    access_.unlock();

    return ret;
}

int  ScreenCapture::index(const std::string &window)
{
    int i = -1;

    access_.lock();
    auto h = std::find_if(handles_.cbegin(), handles_.cend(), hasScreenCaptureName(window));
    if (h != handles_.cend())
        i = std::distance(handles_.cbegin(), h);
    access_.unlock();

    return i;
}

ScreenCaptureSource::ScreenCaptureSource(uint64_t id) : StreamSource(id), failure_(FAIL_NONE)
{
    // set symbol
    symbol_ = new Symbol(Symbol::SCREEN, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

ScreenCaptureSource::~ScreenCaptureSource()
{
    unsetWindow();
}

void ScreenCaptureSource::unsetWindow()
{
    // unregister this device source from a Device handler
    auto h = std::find_if(ScreenCapture::manager().handles_.begin(), ScreenCapture::manager().handles_.end(), hasAssociatedSource(this));
    if (h != ScreenCapture::manager().handles_.end())
    {
        // remove this pointer to the list of connected sources
        h->associated_sources.remove(this);
        // if this is the last source connected to the device handler
        // the stream will be removed by the ~StreamSource destructor
        // and the device handler should not keep reference to it
        if (h->associated_sources.empty())
            // otherwise just cancel the reference to the stream
            h->stream = nullptr;
        // else this means another DeviceSource is using this stream
        // and we should avoid to delete the stream in the ~StreamSource destructor
        else
            stream_ = nullptr;
    }

    window_ = "";
}


void ScreenCaptureSource::reconnect()
{
    // remember name
    std::string d = window_;
    // disconnect
    unsetWindow();
    // connect
    setWindow(d);
}

void ScreenCaptureSource::setWindow(const std::string &windowname)
{
    if (window_.compare(windowname) == 0)
        return;

    // instanciate and wait for monitor initialization if not already initialized
    ScreenCapture::manager().reload();

    // if changing device
    if (!window_.empty())
        unsetWindow();

    // if the stream referenced in this source remains after unsetDevice
    if (stream_) {
        delete stream_;
        stream_ = nullptr;
    }

    // remember device name
    window_ = windowname;

    // check existence of a window handle with that name
    auto h = std::find_if(ScreenCapture::manager().handles_.begin(), ScreenCapture::manager().handles_.end(), hasScreenCaptureName(window_));

    // found a device handle
    if ( h != ScreenCapture::manager().handles_.end()) {

        // find if a DeviceHandle with this device name already has a stream that is open
        if ( h->stream != nullptr ) {
            // just use it !
            stream_ = h->stream;
            // reinit to adapt to new stream
            init();
        }
        else {
            // start filling in the gstreamer pipeline
            std::ostringstream pipeline;
            pipeline << h->pipeline;

            // test the device and get config
            GstToolkit::PipelineConfigSet confs = h->configs;
#ifdef SCREENCAPTURE_DEBUG
            g_printerr("\nScreenCapture '%s' added with configs:\n", h->pipeline.c_str());
            for( GstToolkit::PipelineConfigSet::iterator it = confs.begin(); it != confs.end(); ++it ){
                float fps = static_cast<float>((*it).fps_numerator) / static_cast<float>((*it).fps_denominator);
                g_printerr(" - %s %s %d x %d  %.1f fps\n", (*it).stream.c_str(), (*it).format.c_str(), (*it).width, (*it).height, fps);
            }
#endif
            if (!confs.empty()) {
                GstToolkit::PipelineConfig best = *confs.rbegin();
                float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);
                Log::Info("ScreenCapture %s selected its optimal config: %s %s %dx%d@%.1ffps", window_.c_str(), best.stream.c_str(), best.format.c_str(), best.width, best.height, fps);

                pipeline << " ! " << best.stream;
                if (!best.format.empty())
                    pipeline << ",format=" << best.format;
                pipeline << ",framerate=" << best.fps_numerator << "/" << best.fps_denominator;

                // convert (force alpha to 1)
                pipeline << " ! alpha alpha=1 ! queue ! videoconvert ! videoscale";

                // delete and reset render buffer to enforce re-init of StreamSource
                if (renderbuffer_)
                    delete renderbuffer_;
                renderbuffer_ = nullptr;

                // new stream
                stream_ = h->stream = new Stream;

                // open gstreamer
                h->stream->open( pipeline.str(), best.width, best.height);
                h->stream->play(true);
            }
        }

        // reference this source in the handle
        h->associated_sources.push_back(this);

        // will be ready after init and one frame rendered
        ready_ = false;
    }
    else {
        trash();
        Log::Warning("No window named '%s'", window_.c_str());
    }
}

void ScreenCaptureSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    if (stream_) {
        // change status of stream (only if status changed)
        if (active_ != was_active) {

            // activate a source if any of the handled device source is active
            auto h = std::find_if(ScreenCapture::manager().handles_.begin(), ScreenCapture::manager().handles_.end(), hasAssociatedSource(this));
            if (h != ScreenCapture::manager().handles_.end()) {
                bool streamactive = false;
                for (auto sit = h->associated_sources.begin(); sit != h->associated_sources.end(); ++sit) {
                    if ( (*sit)->active_ )
                        streamactive = true;
                }
                stream_->enable(streamactive);
            }
        }
    }

}

void ScreenCaptureSource::accept(Visitor& v)
{
    StreamSource::accept(v);
    v.visit(*this);
}

Source::Failure ScreenCaptureSource::failed() const
{
    if ( StreamSource::failed() )
        return FAIL_CRITICAL;

    return failure_;
}

glm::ivec2 ScreenCaptureSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_DEVICE_SCREEN);
}

std::string ScreenCaptureSource::info() const
{
    return "Screen capture";
}


#if defined(LINUX)

//int X11_error_handler(Display *d, XErrorEvent *e)
//{
//#ifdef SCREENCAPTURE_DEBUG
//    char msg[80];
//    XGetErrorText(d, e->error_code, msg, sizeof(msg));

//    g_printerr("\nX11 Error %d (%s): request %d.%d on xid %lu\n",
//            e->error_code, msg, e->request_code,
//            e->minor_code, e->resourceid);
//#endif

//    if (e->request_code == X_GetWindowAttributes || e->request_code == X_GetImage)
//        ScreenCapture::manager().remove(SCREEN_CAPTURE_NAME, e->resourceid);

//    return 0;
//}

xcb_atom_t getatom(xcb_connection_t* c, const char *atom_name)
{
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_atom_t atom = 0;
    xcb_intern_atom_reply_t *rep;

    atom_cookie = xcb_intern_atom(c, 0, strlen(atom_name), atom_name);
    rep = xcb_intern_atom_reply(c, atom_cookie, NULL);
    if (NULL != rep)  {
        atom = rep->atom;
        free(rep);
    }
    return atom;
}


std::map<unsigned long,std::string> getListX11Windows()
{
    std::map<unsigned long,std::string> windowlist;

    xcb_generic_error_t *e;
    xcb_connection_t* c = xcb_connect(NULL, NULL);
    static xcb_atom_t net_client_list = getatom(c, "_NET_CLIENT_LIST");
    static xcb_atom_t net_wm_name = getatom(c, "_NET_WM_NAME");
    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    xcb_get_property_cookie_t prop_cookie_list;
    xcb_get_property_reply_t *reply_prop_list;
    prop_cookie_list = xcb_get_property(c, 0, screen->root, net_client_list, XCB_GET_PROPERTY_TYPE_ANY, 0, 100);
    reply_prop_list = xcb_get_property_reply(c, prop_cookie_list, &e);
    if(e) {
        free(e);
    }
    if(reply_prop_list) {
        int value_len = xcb_get_property_value_length(reply_prop_list);
        if(value_len) {
            xcb_window_t* win = (xcb_window_t*) xcb_get_property_value(reply_prop_list);
            for(int i=0; i<value_len/4; i++) {
                xcb_get_property_cookie_t prop_cookie = xcb_get_property(c, 0, win[i], net_wm_name, XCB_GET_PROPERTY_TYPE_ANY, 0, 1000);
                xcb_get_property_reply_t *reply_prop  = xcb_get_property_reply(c, prop_cookie, &e);
                if(e) {
                    free(e);
                }
                if(reply_prop) {
                    int reply_len = xcb_get_property_value_length(reply_prop);
                    if( reply_len > 0) {
                        char windowname[1000];
                        snprintf(windowname, reply_len + 1, "%s", (char *) xcb_get_property_value(reply_prop) );
                        if (isalpha(windowname[0]))
                            windowlist[ win[i] ] = std::string ( windowname );
                    }
                    free(reply_prop);
                }
            }
        }
        free(reply_prop_list);
    }

    return windowlist;
}

#endif






