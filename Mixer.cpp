#include <algorithm>

#include "defines.h"
#include "View.h"
#include "Primitives.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Mixer.h"

Mixer::Mixer()
{
    current_view_ = &mixing_;
    current_source_ = Source::end();
    update_time_ = GST_CLOCK_TIME_NONE;
}


void Mixer::update()
{
    // compute dt
    if (update_time_ == GST_CLOCK_TIME_NONE)
        update_time_ = gst_util_get_timestamp ();
    gint64 current_time = gst_util_get_timestamp ();
    float dt = static_cast<float>( GST_TIME_AS_MSECONDS(current_time - update_time_) ) * 0.001f;
    update_time_ = current_time;

    // render of all sources
    for( SourceList::iterator it = Source::begin(); it != Source::end(); it++){
        (*it)->render();
    }

    // recursive update of ALL views
    render_.update(dt);
    mixing_.update(dt);
    // TODO other views


}

// draw render view and current view
void Mixer::draw()
{
    // always draw render view
    render_.draw();

    // draw the current view in the window
    current_view_->draw();
}

// manangement of sources
void Mixer::createSourceMedia(std::string uri)
{
    // create source
    MediaSource *m = new MediaSource("s", uri);

    // add Nodes to ALL views
    //
    // Render Node
    render_.scene.root()->addChild(m->group(View::RENDERING));
    // Mixing Node
    mixing_.scene.root()->addChild(m->group(View::MIXING));

    // set source as current
    current_source_ = Source::begin();
}

void Mixer::setCurrentSource(std::string namesource)
{
    current_source_ = std::find_if(Source::begin(), Source::end(), hasName(namesource)) ;
}

void Mixer::setcurrentSource(Source *s)
{
//    current_source_ = Source::
}

Source *Mixer::currentSource()
{
    if ( current_source_ == Source::end() )
        return nullptr;
    else {
        return *(current_source_);
    }
}

// management of view
void Mixer::setCurrentView(View::Mode m)
{
    switch (m) {
    case View::MIXING:
        current_view_ = &mixing_;
        break;
    default:
        current_view_ = &mixing_;
        break;
    }
}

View *Mixer::getView(View::Mode m)
{
    switch (m) {
    case View::RENDERING:
        return &render_;
    case View::MIXING:
        return &mixing_;
    default:
        return nullptr;
    }
}

View *Mixer::currentView()
{
    return current_view_;
}
