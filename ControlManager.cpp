/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <thread>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <iomanip>

#include "osc/OscOutboundPacketStream.h"

#include "defines.h"
#include "Log.h"
#include "Settings.h"
#include "BaseToolkit.h"
#include "Mixer.h"
#include "Source.h"
#include "SourceCallback.h"
#include "ActionManager.h"
#include "SystemToolkit.h"
#include "tinyxml2Toolkit.h"

#include "UserInterfaceManager.h"
#include "RenderingManager.h"
#include <GLFW/glfw3.h>

#include "ControlManager.h"

#ifndef NDEBUG
#define CONTROL_DEBUG
#endif

#define CONTROL_OSC_MSG "OSC: "

//bool  Control::input_active[INPUT_MAX]{};
//float Control::input_values[INPUT_MAX]{};
//std::mutex Control::input_access_;


void Control::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{
#ifdef CONTROL_DEBUG
        Log::Info(CONTROL_OSC_MSG "received '%s' from %s", FullMessage(m).c_str(), sender);
#endif
        // Preprocessing with Translator
        std::string address_pattern = Control::manager().translate(m.AddressPattern());

        // structured OSC address
        std::list<std::string> address = BaseToolkit::splitted(address_pattern, OSC_SEPARATOR);
        //
        // A wellformed OSC address is in the form '/vimix/target/attribute {arguments}'
        // First test: should have 3 elements and start with APP_NAME ('vimix')
        //
        if (address.size() > 2 && address.front().compare(OSC_PREFIX) == 0 ){
            // done with the first part of the OSC address
            address.pop_front();
            // next part of the OSC message is the target
            std::string target = address.front();
            // next part of the OSC message is the attribute
            address.pop_front();
            std::string attribute = address.front();
            // Log target: just print text in log window
            if ( target.compare(OSC_INFO) == 0 )
            {
                if ( attribute.compare(OSC_INFO_NOTIFY) == 0) {
                    Log::Notify(CONTROL_OSC_MSG "Received '%s' from %s", FullMessage(m).c_str(), sender);
                }
                else if ( attribute.compare(OSC_INFO_LOG) == 0) {
                    Log::Info(CONTROL_OSC_MSG "Received '%s' from %s", FullMessage(m).c_str(), sender);
                }
            }
            // Output target: concerns attributes of the rendering output
            else if ( target.compare(OSC_OUTPUT) == 0 )
            {
                if ( Control::manager().receiveOutputAttribute(attribute, m.ArgumentStream())) {
                    // send the global status
                    Control::manager().sendOutputStatus(remoteEndpoint);
                }
            }
            // Multitouch target: user input on 'Multitouch' tab
            else if ( target.compare(OSC_MULTITOUCH) == 0 )
            {
                Control::manager().receiveMultitouchAttribute(attribute, m.ArgumentStream());
            }
            // Session target: concerns attributes of the session
            else if ( target.compare(OSC_SESSION) == 0 )
            {
                if ( Control::manager().receiveSessionAttribute(attribute, m.ArgumentStream()) ) {
                    // send the global status
                    Control::manager().sendOutputStatus(remoteEndpoint);
                    // send the status of all sources
                    Control::manager().sendSourcesStatus(remoteEndpoint, m.ArgumentStream());
                }
            }
            // ALL sources target: apply attribute to all sources of the session
            else if ( target.compare(OSC_ALL) == 0 )
            {
                // Loop over selected sources
                for (SourceList::iterator it = Mixer::manager().session()->begin(); it != Mixer::manager().session()->end(); ++it) {
                    // apply attributes
                    if ( Control::manager().receiveSourceAttribute( *it, attribute, m.ArgumentStream()) && Mixer::manager().currentSource() == *it)
                        // and send back feedback if needed
                        Control::manager().sendSourceAttibutes(remoteEndpoint, OSC_CURRENT);
                }
            }
            // Selected sources target: apply attribute to all sources of the selection
            else if ( target.compare(OSC_SELECTED) == 0 )
            {
                // Loop over selected sources
                for (SourceList::iterator it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                    // apply attributes
                    if ( Control::manager().receiveSourceAttribute( *it, attribute, m.ArgumentStream()) && Mixer::manager().currentSource() == *it)
                        // and send back feedback if needed
                        Control::manager().sendSourceAttibutes(remoteEndpoint, OSC_CURRENT);
                }
            }
            // Current source target: apply attribute to the current sources
            else if ( target.compare(OSC_CURRENT) == 0 )
            {
                int sourceid = -1;
                if ( attribute.compare(OSC_SYNC) == 0) {
                    // send the status of all sources
                    Control::manager().sendSourcesStatus(remoteEndpoint, m.ArgumentStream());
                }
                else if ( attribute.compare(OSC_NEXT) == 0) {
                    // set current to NEXT
                    Mixer::manager().setCurrentNext();
                    // send the status of all sources
                    Control::manager().sendSourcesStatus(remoteEndpoint, m.ArgumentStream());
                }
                else if ( attribute.compare(OSC_PREVIOUS) == 0) {
                    // set current to PREVIOUS
                    Mixer::manager().setCurrentPrevious();
                    // send the status of all sources
                    Control::manager().sendSourcesStatus(remoteEndpoint, m.ArgumentStream());
                }
                else if ( BaseToolkit::is_a_number( attribute.substr(1), &sourceid) ){
                    // set current to given INDEX
                    Mixer::manager().setCurrentIndex(sourceid);
                    // send the status of all sources
                    Control::manager().sendSourcesStatus(remoteEndpoint, m.ArgumentStream());
                }
                // all other attributes operate on current source
                else {
                    // apply attributes to current source
                    if ( Control::manager().receiveSourceAttribute( Mixer::manager().currentSource(), attribute, m.ArgumentStream()) )
                        // and send back feedback if needed
                        Control::manager().sendSourceAttibutes(remoteEndpoint, OSC_CURRENT);
                }
            }
            // General case: try to identify the target
            else {
                // try to find source by index
                Source *s = nullptr;
                int sourceid = -1;
                if ( BaseToolkit::is_a_number(target.substr(1), &sourceid) )
                    s = Mixer::manager().sourceAtIndex(sourceid);

                // if failed, try to find source by name
                if (s == nullptr)
                    s = Mixer::manager().findSource(target.substr(1));

                // if a source with the given target name or index was found
                if (s) {
                    // apply attributes to source
                    if ( Control::manager().receiveSourceAttribute(s, attribute, m.ArgumentStream()) )
                        // and send back feedback if needed
                        Control::manager().sendSourceAttibutes(remoteEndpoint, target, s);
                }
                else
                    Log::Info(CONTROL_OSC_MSG "Unknown target '%s' requested by %s.", target.c_str(), sender);
            }
        }
        else {
            Log::Info(CONTROL_OSC_MSG "Unknown osc message '%s' sent by %s.", m.AddressPattern(), sender);
        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info(CONTROL_OSC_MSG "Ignoring error in message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}


std::string Control::RequestListener::FullMessage( const osc::ReceivedMessage& m )
{
    // build a string with the address pattern of the message
    std::ostringstream message;
    message << m.AddressPattern() << " ";

    // try to fill the string with the arguments
    std::ostringstream arguments;
    try{
        // loop over all arguments
        osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
        while (arg != m.ArgumentsEnd()) {
            if( arg->IsBool() ){
                bool a = (arg++)->AsBoolUnchecked();
                message << (a ? "T" : "F");
            }
            else if( arg->IsInt32() ){
                int a = (arg++)->AsInt32Unchecked();
                message << "i";
                arguments << " " << a;
            }
            else if( arg->IsFloat() ){
                float a = (arg++)->AsFloatUnchecked();
                message << "f";
                arguments << " " << std::fixed << std::setprecision(2) << a;
            }
            else if( arg->IsString() ){
                const char *a = (arg++)->AsStringUnchecked();
                message << "s";
                arguments << " " << a;
            }
        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info(CONTROL_OSC_MSG "Ignoring error in message '%s': %s", m.AddressPattern(), e.what());
    }

    // append list of arguments to the message string
    message << arguments.str();

    // returns the full message
    return message.str();
}


Control::Control() : receiver_(nullptr)
{
    for (size_t i = 0; i < INPUT_MULTITOUCH_COUNT; ++i) {
        multitouch_active[i] = false;
        multitouch_values[i] = glm::vec2(0.f);
    }

    for (size_t i = 0; i < INPUT_MAX; ++i) {
        input_active[i] = false;
        input_values[i] = 0.f;
    }
}

Control::~Control()
{
    terminate();
}

std::string Control::translate (std::string addresspattern)
{
    auto it_translation  = translation_.find(addresspattern);
    if ( it_translation != translation_.end() )
        return it_translation->second;
    else
        return addresspattern;
}

void Control::loadOscConfig()
{
    // reset translations
    translation_.clear();

    // load osc config file
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLError eResult = xmlDoc.LoadFile(Settings::application.control.osc_filename.c_str());

    // the only reason to return false is if the file does not exist or is empty
    if (eResult == tinyxml2::XML_ERROR_FILE_NOT_FOUND
      | eResult == tinyxml2::XML_ERROR_FILE_COULD_NOT_BE_OPENED
      | eResult == tinyxml2::XML_ERROR_FILE_READ_ERROR
      | eResult == tinyxml2::XML_ERROR_EMPTY_DOCUMENT )
        resetOscConfig();

    // found the file, could open and read it
    else if (eResult != tinyxml2::XML_SUCCESS)
        Log::Warning(CONTROL_OSC_MSG "Error while parsing Translator: %s", xmlDoc.ErrorIDToName(eResult));

    // no XML parsing error
    else {
        // parse all entries 'osc'
        tinyxml2::XMLElement* osc = xmlDoc.FirstChildElement("osc");
        for( ; osc ; osc=osc->NextSiblingElement())  {
            // get the 'from' entry
            tinyxml2::XMLElement* from = osc->FirstChildElement("from");
            if (from) {
                const char *str_from = from->GetText();
                if (str_from) {
                    // get the 'to' entry
                    tinyxml2::XMLElement* to = osc->FirstChildElement("to");
                    if (to) {
                        const char *str_to = to->GetText();
                        // if could get both; add to translator
                        if (str_to)
                            translation_[str_from] = str_to;
                    }
                }
            }
        }
    }

    Log::Info(CONTROL_OSC_MSG "Loaded %d translation%s.", translation_.size(), translation_.size()>1?"s":"");
}

void Control::resetOscConfig()
{
    // generate a template xml translation dictionnary
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);
    tinyxml2::XMLComment *pComment = xmlDoc.NewComment("The OSC translator converts OSC address patterns into other ones.\n"
                                                       "Complete the dictionnary by adding as many <osc> translations as you want.\n"
                                                       "Each <osc> should contain a <from> pattern to translate into a <to> pattern.\n"
                                                       "More at https://github.com/brunoherbelin/vimix/wiki/Open-Sound-Control-API.");
    xmlDoc.InsertEndChild(pComment);
    tinyxml2::XMLElement *from = xmlDoc.NewElement( "from" );
    from->InsertFirstChild( xmlDoc.NewText("/example/osc/message") );
    tinyxml2::XMLElement *to = xmlDoc.NewElement( "to" );
    to->InsertFirstChild( xmlDoc.NewText("/vimix/info/log") );
    tinyxml2::XMLElement *osc = xmlDoc.NewElement("osc");
    osc->InsertEndChild(from);
    osc->InsertEndChild(to);
    xmlDoc.InsertEndChild(osc);

    // save xml in osc config file
    xmlDoc.SaveFile(Settings::application.control.osc_filename.c_str());

    // reset and fill translation with default example
    translation_.clear();
    translation_["/example/osc/message"] = "/vimix/info/log";
}

bool Control::init()
{
    //
    // terminate before init (allows calling init() multiple times)
    //
    terminate();

    //
    // set keyboard callback
    //
    GLFWwindow *main = Rendering::manager().mainWindow().window();
    GLFWwindow *output = Rendering::manager().outputWindow().window();
    glfwSetKeyCallback( main, Control::keyboardCalback);
    glfwSetKeyCallback( output, Control::keyboardCalback);

    //
    // load OSC Translator
    //
    loadOscConfig();

    //
    // launch OSC listener
    //
    try {
        // try to create listenning socket
        // through exception runtime if fails
        receiver_ = new UdpListeningReceiveSocket( IpEndpointName( IpEndpointName::ANY_ADDRESS,
                                                                   Settings::application.control.osc_port_receive ), &listener_ );
        // listen for answers in a separate thread
        std::thread(listen).detach();

        // inform user
        IpEndpointName ip = receiver_->LocalEndpointFor( IpEndpointName( NetworkToolkit::hostname().c_str(),
                                                                         Settings::application.control.osc_port_receive ));
        static char *addresseip = (char *)malloc(IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH);
        ip.AddressAndPortAsString(addresseip);
        Log::Info(CONTROL_OSC_MSG "Listening to UDP messages sent to %s", addresseip);
    }
    catch (const std::runtime_error &e) {
        // arg, the receiver could not be initialized
        // (often because the port was not available)
        receiver_ = nullptr;
        Log::Warning(CONTROL_OSC_MSG "The port %d is already used by another program; %s", Settings::application.control.osc_port_receive, e.what());
    }

    return receiver_ != nullptr;
}

void Control::update()
{

    // read joystick buttons
    int num_buttons = 0;
    const unsigned char *state_buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &num_buttons );
    // map to Control input array
    for (int b = 0; b < num_buttons; ++b) {
        input_access_.lock();
        input_active[INPUT_JOYSTICK_FIRST_BUTTON + b] = state_buttons[b] == GLFW_PRESS;
        input_values[INPUT_JOYSTICK_FIRST_BUTTON + b] = state_buttons[b] == GLFW_PRESS ? 1.f : 0.f;
        input_access_.unlock();
    }

    // read joystick axis
    int num_axis = 0;
    const float *state_axis = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &num_axis );
    for (int a = 0; a < num_axis; ++a) {
        input_access_.lock();
        input_active[INPUT_JOYSTICK_FIRST_AXIS + a] = ABS(state_axis[a]) > 0.02 ? true : false;
        input_values[INPUT_JOYSTICK_FIRST_AXIS + a] = state_axis[a];
        input_access_.unlock();
    }

    // multitouch input needs to be cleared when no more OSC input comes in
    for (int m = 0; m < INPUT_MULTITOUCH_COUNT; ++m) {
        if ( multitouch_active[m] > 0 )
            multitouch_active[m] -= 1;
        else {
            input_access_.lock();
            input_active[INPUT_MULTITOUCH_FIRST + m] = false;
            input_values[INPUT_MULTITOUCH_FIRST + m] = 0.f;
            multitouch_values[m] = glm::vec2(0.f);
            input_access_.unlock();
        }
    }

}

void Control::listen()
{
    if (Control::manager().receiver_)
        Control::manager().receiver_->Run();

    Control::manager().receiver_end_.notify_all();
}

void Control::terminate()
{
    if ( receiver_ != nullptr ) {

        // request termination of receiver
        receiver_->AsynchronousBreak();

        // wait for the receiver_end_ notification
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        // if waited more than 2 seconds, its dead :(
        if ( receiver_end_.wait_for(lck,std::chrono::seconds(2)) == std::cv_status::timeout)
            Log::Warning(CONTROL_OSC_MSG "Failed to terminate; try again.");

        // delete receiver and ready to initialize
        delete receiver_;
        receiver_ = nullptr;
    }

}


bool Control::receiveOutputAttribute(const std::string &attribute,
                       osc::ReceivedMessageArgumentStream arguments)
{
    bool need_feedback = false;

    try {
        if ( attribute.compare(OSC_SYNC) == 0) {
            need_feedback = true;
        }
        /// e.g. '/vimix/output/enable' or '/vimix/output/enable 1.0' or '/vimix/output/enable 0.0'
        else if ( attribute.compare(OSC_OUTPUT_ENABLE) == 0) {
            float on = 1.f;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            Settings::application.render.disabled = on < 0.5f;
        }
        /// e.g. '/vimix/output/disable' or '/vimix/output/disable 1.0' or '/vimix/output/disable 0.0'
        else if ( attribute.compare(OSC_OUTPUT_DISABLE) == 0) {
            float on = 1.f;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            Settings::application.render.disabled = on > 0.5f;
        }
        /// e.g. '/vimix/output/fading f 0.2' or '/vimix/output/fading ff 1.0 300.f'
        else if ( attribute.compare(OSC_OUTPUT_FADING) == 0) {
            float f = 0.f, d = 0.f;
            // first argument is fading value
            arguments >> f;
            if (arguments.Eos())
                arguments >> osc::EndMessage;
            // if a second argument is given, it is a duration
            else
                arguments >> d >> osc::EndMessage;
            Mixer::manager().session()->setFadingTarget(f, d);
        }
        /// e.g. '/vimix/output/fadein' or '/vimix/output/fadein f 300.f'
        else if ( attribute.compare(OSC_OUTPUT_FADE_IN) == 0) {
            float f = 0.f;
            // if argument is given, it is a duration
            if (!arguments.Eos())
                arguments >> f >> osc::EndMessage;
            Mixer::manager().session()->setFadingTarget( Mixer::manager().session()->fading() - f * 0.01);
            need_feedback = true;
        }
        else if ( attribute.compare(OSC_OUTPUT_FADE_OUT) == 0) {
            float f = 0.f;
            // if argument is given, it is a duration
            if (!arguments.Eos())
                arguments >> f >> osc::EndMessage;
            Mixer::manager().session()->setFadingTarget( Mixer::manager().session()->fading() + f * 0.01);
            need_feedback = true;
        }
#ifdef CONTROL_DEBUG
        else {
            Log::Info(CONTROL_OSC_MSG "Ignoring attribute '%s' for target 'output'", attribute.c_str());
        }
#endif

    }
    catch (osc::MissingArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Missing argument for attribute '%s' for target 'output'", attribute.c_str());
    }
    catch (osc::ExcessArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Too many arguments for attribute '%s' for target 'output'", attribute.c_str());
    }
    catch (osc::WrongArgumentTypeException &e) {
        Log::Info(CONTROL_OSC_MSG "Invalid argument for attribute '%s' for target 'output'", attribute.c_str());
    }

    return need_feedback;
}

bool Control::receiveSourceAttribute(Source *target, const std::string &attribute,
                       osc::ReceivedMessageArgumentStream arguments)
{
    bool send_feedback = false;

    if (target == nullptr)
        return send_feedback;

    try {
        /// e.g. '/vimix/current/play' or '/vimix/current/play T' or '/vimix/current/play F'
        if ( attribute.compare(OSC_SOURCE_PLAY) == 0) {
            float on = 1.f;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            target->call( new Play(on > 0.5f) );
        }
        /// e.g. '/vimix/current/pause' or '/vimix/current/pause T' or '/vimix/current/pause F'
        else if ( attribute.compare(OSC_SOURCE_PAUSE) == 0) {
            float on = 1.f;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            target->call( new Play(on < 0.5f) );
        }
        /// e.g. '/vimix/current/replay'
        else if ( attribute.compare(OSC_SOURCE_REPLAY) == 0) {
            target->call( new RePlay() );
        }
        /// e.g. '/vimix/current/alpha f 0.3'
        else if ( attribute.compare(OSC_SOURCE_LOCK) == 0) {
            float x = 1.f;
            arguments >> x >> osc::EndMessage;
            target->call( new Lock(x > 0.5f ? true : false) );
        }
        /// e.g. '/vimix/current/alpha f 0.3'
        else if ( attribute.compare(OSC_SOURCE_ALPHA) == 0) {
            float x = 1.f;
            arguments >> x >> osc::EndMessage;
            target->call( new SetAlpha(x), true );
        }
        /// e.g. '/vimix/current/alpha f 0.3'
        else if ( attribute.compare(OSC_SOURCE_LOOM) == 0) {
            float x = 1.f;
            arguments >> x >> osc::EndMessage;
            target->call( new Loom(x), true );
            // this will require to send feedback status about source
            send_feedback = true;
        }
        /// e.g. '/vimix/current/transparency f 0.7'
        else if ( attribute.compare(OSC_SOURCE_TRANSPARENCY) == 0) {
            float x = 0.f;
            arguments >> x >> osc::EndMessage;
            target->call( new SetAlpha(1.f - x), true );
        }
        /// e.g. '/vimix/current/depth f 5.0'
        else if ( attribute.compare(OSC_SOURCE_DEPTH) == 0) {
            float x = 0.f;
            arguments >> x >> osc::EndMessage;
            target->call( new SetDepth(x), true );
        }
        /// e.g. '/vimix/current/translation ff 10.0 2.2'
        else if ( attribute.compare(OSC_SOURCE_GRAB) == 0) {
            float x = 0.f, y = 0.f;
            arguments >> x >> y >> osc::EndMessage;
            target->call( new Grab( x, y), true );
        }
        /// e.g. '/vimix/current/scale ff 10.0 2.2'
        else if ( attribute.compare(OSC_SOURCE_RESIZE) == 0) {
            float x = 0.f, y = 0.f;
            arguments >> x >> y >> osc::EndMessage;
            target->call( new Resize( x, y), true );
        }
        /// e.g. '/vimix/current/turn f 1.0'
        else if ( attribute.compare(OSC_SOURCE_TURN) == 0) {
            float x = 0.f, y = 0.f;
            arguments >> x;
            if (arguments.Eos())
                arguments >> osc::EndMessage;
            else // ignore second argument
                arguments >> y >> osc::EndMessage;
            target->call( new Turn( x ), true );
        }
        /// e.g. '/vimix/current/reset'
        else if ( attribute.compare(OSC_SOURCE_RESET) == 0) {
            target->call( new ResetGeometry(), true );
        }
#ifdef CONTROL_DEBUG
        else {
            Log::Info(CONTROL_OSC_MSG "Ignoring attribute '%s' for target %s.", attribute.c_str(), target->name().c_str());
        }
#endif

        // overwrite value if source locked
        if (target->locked())
            send_feedback = true;

    }
    catch (osc::MissingArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Missing argument for attribute '%s' for target %s.", attribute.c_str(), target->name().c_str());
    }
    catch (osc::ExcessArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Too many arguments for attribute '%s' for target %s.", attribute.c_str(), target->name().c_str());
    }
    catch (osc::WrongArgumentTypeException &e) {
        Log::Info(CONTROL_OSC_MSG "Invalid argument for attribute '%s' for target %s.", attribute.c_str(), target->name().c_str());
    }

    return send_feedback;
}

bool Control::receiveSessionAttribute(const std::string &attribute,
                       osc::ReceivedMessageArgumentStream arguments)
{
    bool send_feedback = false;

    try {
        if ( attribute.compare(OSC_SYNC) == 0) {
            send_feedback = true;
        }
        else if ( attribute.compare(OSC_SESSION_VERSION) == 0) {
            float v = 0.f;
            arguments >> v >> osc::EndMessage;
            size_t id = (int) ceil(v);
            std::list<uint64_t> snapshots = Action::manager().snapshots();
            if ( id <  snapshots.size() ) {
                for (size_t i = 0; i < id; ++i)
                    snapshots.pop_back();
                uint64_t snap = snapshots.back();
                Action::manager().restore(snap);
            }
            send_feedback = true;
        }
#ifdef CONTROL_DEBUG
        else {
            Log::Info(CONTROL_OSC_MSG "Ignoring attribute '%s' for target 'session'", attribute.c_str());
        }
#endif

    }
    catch (osc::MissingArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Missing argument for attribute '%s' for target 'session'", attribute.c_str());
    }
    catch (osc::ExcessArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Too many arguments for attribute '%s' for target 'session'", attribute.c_str());
    }
    catch (osc::WrongArgumentTypeException &e) {
        Log::Info(CONTROL_OSC_MSG "Invalid argument for attribute '%s' for target 'session'", attribute.c_str());
    }

    return send_feedback;
}

void Control::receiveMultitouchAttribute(const std::string &attribute,
                        osc::ReceivedMessageArgumentStream arguments)
{
    try {
        // address should be in the form /vimix/multitouch/i
        int t = -1;
        if ( BaseToolkit::is_a_number(attribute.substr(1), &t) && t >= 0 && t < INPUT_MULTITOUCH_COUNT )
        {
            // get value inputs
            float x = 0.f, y = 0.f;
            if ( !arguments.Eos())
                arguments >> x >> y >> osc::EndMessage;

            input_access_.lock();

            // if the touch was already pressed
            if ( multitouch_active[t] > 0 ) {
                // active value decreases with the distance from original press position
                input_values[INPUT_MULTITOUCH_FIRST + t]  = 1.f - glm::distance(Control::multitouch_values[t], glm::vec2(x, y)) / M_SQRT2;
            }
            // first time touch is pressed
            else {
                // store original press position
                multitouch_values[t] = glm::vec2(x, y);
                // active value is 1.f at first press (full)
                input_values[INPUT_MULTITOUCH_FIRST + t] = 1.f;
            }
            // keep track of button press
            multitouch_active[t] = 3;
            // set array of active input
            input_active[INPUT_MULTITOUCH_FIRST + t] = true;

            input_access_.unlock();
        }
    }
    catch (osc::MissingArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Missing argument for attribute '%s' for target %s.", attribute.c_str(), OSC_MULTITOUCH);
    }
    catch (osc::ExcessArgumentException &e) {
        Log::Info(CONTROL_OSC_MSG "Too many arguments for attribute '%s' for target %s.", attribute.c_str(), OSC_MULTITOUCH);
    }
    catch (osc::WrongArgumentTypeException &e) {
        Log::Info(CONTROL_OSC_MSG "Invalid argument for attribute '%s' for target %s.", attribute.c_str(), OSC_MULTITOUCH);
    }

}

void Control::sendSourceAttibutes(const IpEndpointName &remoteEndpoint, std::string target, Source *s)
{
    // default values
    char name[21] = {"\0"};
    float lock = 0.f;
    float play = 0.f;
    float depth = 0.f;
    float alpha = 0.f;

    // get source or current source
    Source *_s = s;
    if ( target.compare(OSC_CURRENT) == 0 )
        _s = Mixer::manager().currentSource();

    // fill values if the source is valid
    if (_s!=nullptr) {
        strncpy(name, _s->name().c_str(), 20);
        lock  = _s->locked() ? 1.f : 0.f;
        play  = _s->playing() ? 1.f : 0.f;
        depth = _s->depth();
        alpha = _s->alpha();
    }

    // build socket to send message to indicated endpoint
    UdpTransmitSocket socket( IpEndpointName( remoteEndpoint.address, Settings::application.control.osc_port_send ) );

    // build messages packet
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

    // create bundle
    p.Clear();
    p << osc::BeginBundle();

    /// name
    std::string address = std::string(OSC_PREFIX) + target + OSC_SOURCE_NAME;
    p << osc::BeginMessage( address.c_str() ) << name << osc::EndMessage;
    /// Play status
    address = std::string(OSC_PREFIX) + target + OSC_SOURCE_LOCK;
    p << osc::BeginMessage( address.c_str() ) << lock << osc::EndMessage;
    /// Play status
    address = std::string(OSC_PREFIX) + target + OSC_SOURCE_PLAY;
    p << osc::BeginMessage( address.c_str() ) << play << osc::EndMessage;
    /// Depth
    address = std::string(OSC_PREFIX) + target + OSC_SOURCE_DEPTH;
    p << osc::BeginMessage( address.c_str() ) << depth << osc::EndMessage;
    /// Alpha
    address = std::string(OSC_PREFIX) + target + OSC_SOURCE_ALPHA;
    p << osc::BeginMessage( address.c_str() ) << alpha << osc::EndMessage;

    // send bundle
    p << osc::EndBundle;
    socket.Send( p.Data(), p.Size() );
}


void Control::sendSourcesStatus(const IpEndpointName &remoteEndpoint, osc::ReceivedMessageArgumentStream arguments)
{
    //  (if an argument is given, it indicates the number of sources to update)
    float N = 0.f;
    if ( !arguments.Eos())
        arguments >> N >> osc::EndMessage;

    // build socket to send message to indicated endpoint
    UdpTransmitSocket socket( IpEndpointName( remoteEndpoint.address, Settings::application.control.osc_port_send ) );

    // build messages packet
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

    p.Clear();
    p << osc::BeginBundle();

    int i = 0;
    char oscaddr[128];
    int index_current = Mixer::manager().indexCurrentSource();
    for (; i < Mixer::manager().count(); ++i) {
        // send status of currently selected
        sprintf(oscaddr, OSC_PREFIX OSC_CURRENT "/%d", i);
        p << osc::BeginMessage( oscaddr ) << (index_current == i ? 1.f : 0.f) << osc::EndMessage;

        // send status of alpha
        sprintf(oscaddr, OSC_PREFIX "/%d" OSC_SOURCE_ALPHA, i);
        p << osc::BeginMessage( oscaddr ) << Mixer::manager().sourceAtIndex(i)->alpha() << osc::EndMessage;
    }

    for (; i < (int) N ; ++i) {
        // reset status of currently selected
        sprintf(oscaddr, OSC_PREFIX OSC_CURRENT "/%d", i);
        p << osc::BeginMessage( oscaddr ) <<  0.f << osc::EndMessage;

        // reset status of alpha
        sprintf(oscaddr, OSC_PREFIX "/%d" OSC_SOURCE_ALPHA, i);
        p << osc::BeginMessage( oscaddr ) <<  0.f << osc::EndMessage;
    }

    p << osc::EndBundle;
    socket.Send( p.Data(), p.Size() );

    // send status of current source
    sendSourceAttibutes(remoteEndpoint, OSC_CURRENT);
}


void Control::sendOutputStatus(const IpEndpointName &remoteEndpoint)
{
    // build socket to send message to indicated endpoint
    UdpTransmitSocket socket( IpEndpointName( remoteEndpoint.address, Settings::application.control.osc_port_send ) );

    // build messages packet
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

    p.Clear();
    p << osc::BeginBundle();

    /// output attributes
    p << osc::BeginMessage( OSC_PREFIX OSC_OUTPUT OSC_OUTPUT_ENABLE );
    p << (Settings::application.render.disabled ? 0.f : 1.f);
    p << osc::EndMessage;
    p << osc::BeginMessage( OSC_PREFIX OSC_OUTPUT OSC_OUTPUT_FADING );
    p << Mixer::manager().session()->fading();
    p << osc::EndMessage;

    p << osc::EndBundle;
    socket.Send( p.Data(), p.Size() );
}

void Control::keyboardCalback(GLFWwindow* window, int key, int, int action, int mods)
{
    if (UserInterface::manager().keyboardAvailable() && !mods )
    {
        Control::manager().input_access_.lock();
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            Control::manager().input_active[INPUT_KEYBOARD_FIRST + key - GLFW_KEY_A] = action > GLFW_RELEASE;
            Control::manager().input_values[INPUT_KEYBOARD_FIRST + key - GLFW_KEY_A] = action > GLFW_RELEASE ? 1.f : 0.f;
        }
        else if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL) {
            Control::manager().input_active[INPUT_NUMPAD_FIRST + key - GLFW_KEY_KP_0] = action > GLFW_RELEASE;
            Control::manager().input_values[INPUT_NUMPAD_FIRST + key - GLFW_KEY_KP_0] = action > GLFW_RELEASE ? 1.f : 0.f;
        }
        else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
        {
            static GLFWwindow *output = Rendering::manager().outputWindow().window();
            if (window==output)
                Rendering::manager().outputWindow().exitFullscreen();
        }
        Control::manager().input_access_.unlock();
    }
}

bool Control::inputActive(uint id)
{
    input_access_.lock();
    const bool ret = input_active[MIN(id,INPUT_MAX)];
    input_access_.unlock();

    return ret && !Settings::application.mapping.disabled;
}

float Control::inputValue(uint id)
{
    input_access_.lock();
    const float ret = input_values[MIN(id,INPUT_MAX)];
    input_access_.unlock();

    return ret;
}

std::string Control::inputLabel(uint id)
{
    std::string label;

    if ( id >= INPUT_KEYBOARD_FIRST && id <= INPUT_KEYBOARD_LAST )
    {
        label = std::string( 1, 'A' + (char) (id -INPUT_KEYBOARD_FIRST) );
    }
    else if ( id >= INPUT_NUMPAD_FIRST && id <= INPUT_NUMPAD_LAST )
    {
        int k = GLFW_KEY_KP_0 + id -INPUT_NUMPAD_FIRST;
        label = std::string( glfwGetKeyName( k, glfwGetKeyScancode(k) ) );
    }
    else if ( id >= INPUT_JOYSTICK_FIRST && id <= INPUT_JOYSTICK_LAST )
    {
        std::vector< std::string > joystick_labels = { "Button A", "Button B", "Button X", "Button Y",
                                                       "Left bumper", "Right bumper", "Back", "Start",
                                                       "Guide", "Left thumb", "Right thumb",
                                                       "Up", "Right", "Down", "Left",
                                                       "Left Axis X", "Left Axis Y", "Left Trigger",
                                                       "Right Axis X", "Right Axis Y", "Right Trigger" };
        label = joystick_labels[id - INPUT_JOYSTICK_FIRST];
    }
    else if ( id >= INPUT_MULTITOUCH_FIRST && id <= INPUT_MULTITOUCH_LAST )
    {
        label = std::string( "Multitouch ") + std::to_string(id - INPUT_MULTITOUCH_FIRST);
    }
    else if ( id >= INPUT_CUSTOM_FIRST && id <= INPUT_CUSTOM_LAST )
    {

    }

    return label;
}
