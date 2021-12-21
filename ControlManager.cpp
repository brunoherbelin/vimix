/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "osc/OscOutboundPacketStream.h"

#include "Log.h"
#include "Settings.h"
#include "BaseToolkit.h"
#include "Mixer.h"
#include "Source.h"

#include "ControlManager.h"

#ifndef NDEBUG
#define CONTROL_DEBUG
#endif

#define CONTROL_OSC_MSG "OSC: "

void Control::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{
#ifdef CONTROL_DEBUG
        Log::Info(CONTROL_OSC_MSG "received '%s' from %s", m.AddressPattern(), sender);
#endif
        // TODO Preprocessing with Translator

        // structured OSC address
        std::list<std::string> address = BaseToolkit::splitted(m.AddressPattern(), OSC_SEPARATOR);
        //
        // A wellformed OSC address is in the form '/vimix/target/attribute {arguments}'
        // First test: should have 3 elements and start with APP_NAME ('vimix')
        //
        if (address.size() == 3 && address.front().compare(OSC_PREFIX) == 0 ){
            // done with the first part of the OSC address
            address.pop_front();
            //
            // Execute next part of the OSC message according to target
            //
            std::string target = address.front();
            std::string attribute = address.back();
            // Log target: just print text in log window
            if ( target.compare(OSC_INFO) == 0 )
            {
                if ( attribute.compare(OSC_INFO_TEST) == 0)
                    Control::manager().sendStatus(remoteEndpoint);
                else if ( attribute.compare(OSC_INFO_LOG) == 0)
                    Log::Info(CONTROL_OSC_MSG "received '%s' from %s", m.AddressPattern(), sender);
            }
            // Output target: concerns attributes of the rendering output
            else if ( target.compare(OSC_OUTPUT) == 0 )
            {
                Control::manager().receiveOutputAttribute(attribute, m.ArgumentStream());
            }
            // ALL sources target: apply attribute to all sources of the session
            else if ( target.compare(OSC_ALL) == 0 )
            {
                // Loop over selected sources
                for (SourceList::iterator it = Mixer::manager().session()->begin(); it != Mixer::manager().session()->end(); ++it) {
                    // attributes operate on current source
                    Control::manager().receiveSourceAttribute( *it, attribute, m.ArgumentStream());
                }
            }
            // Selected sources target: apply attribute to all sources of the selection
            else if ( target.compare(OSC_SELECTED) == 0 )
            {
                // Loop over selected sources
                for (SourceList::iterator it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                    // attributes operate on current source
                    Control::manager().receiveSourceAttribute( *it, attribute, m.ArgumentStream());
                }
            }
            // Current source target: apply attribute to the current sources
            else if ( target.compare(OSC_CURRENT) == 0 )
            {
                // attributes to change current
                if ( attribute.compare(OSC_SET) == 0) {
                    int index = 0;
                    m.ArgumentStream() >> index >> osc::EndMessage;
                    Mixer::manager().setCurrentIndex(index);
                    // confirm by sending back the current source attributes
                    Control::manager().sendCurrentSourceAttibutes(remoteEndpoint);
                }
                else if ( attribute.compare(OSC_NEXT) == 0) {
                    Mixer::manager().setCurrentNext();
                    // confirm by sending back the current source attributes
                    Control::manager().sendCurrentSourceAttibutes(remoteEndpoint);
                }
                else if ( attribute.compare(OSC_PREVIOUS) == 0) {
                    Mixer::manager().setCurrentPrevious();
                    // confirm by sending back the current source attributes
                    Control::manager().sendCurrentSourceAttibutes(remoteEndpoint);
                }
                // all other attributes operate on current source
                else
                    Control::manager().receiveSourceAttribute( Mixer::manager().currentSource(), attribute, m.ArgumentStream());
            }
            // General case: try to identify the target
            else {
                // remove osc separator
                target.erase(0,1);
                // try to find source by name
                Source *s = Mixer::manager().findSource(target);
                // if failed, try to find source by index
                if (s == nullptr) {
                    int N = -1;
                    try {
                        N = std::stoi(target);
                    }  catch (const std::invalid_argument&) {
                        N = -1;
                    }
                    if (N>=0)
                        s = Mixer::manager().sourceAtIndex(N);
                }
                if (s)
                    Control::manager().receiveSourceAttribute(s, attribute, m.ArgumentStream());
                else
                    Log::Info(CONTROL_OSC_MSG "Unknown target '%s' requested by %s.", target.c_str(), sender);
            }
        }
#ifdef CONTROL_DEBUG
        else {
            Log::Info(CONTROL_OSC_MSG "Ignoring malformed message '%s' from %s.", m.AddressPattern(), sender);
        }
#endif
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info(CONTROL_OSC_MSG "Ignoring error in message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}


void Control::listen()
{
#ifdef CONTROL_DEBUG
    Log::Info(CONTROL_OSC_MSG "Accepting messages on port %d", Settings::application.control.osc_port_receive);
#endif
    if (Control::manager().receiver_)
        Control::manager().receiver_->Run();
}


Control::Control() : receiver_(nullptr)
{
}

Control::~Control()
{
    if (receiver_!=nullptr) {
        receiver_->Break();
        delete receiver_;
    }

}

bool Control::init()
{
    try {
        // try to create listenning socket
        // through exception runtime if fails
        receiver_ = new UdpListeningReceiveSocket( IpEndpointName( IpEndpointName::ANY_ADDRESS,
                                                                   Settings::application.control.osc_port_receive ), &listener_ );
    }
    catch (const std::runtime_error&) {
        // arg, the receiver could not be initialized
        // because the port was not available
        receiver_ = nullptr;
    }

    // listen for answers
    std::thread(listen).detach();

    return receiver_ != nullptr;
}

void Control::terminate()
{
    if (receiver_!=nullptr)
        receiver_->AsynchronousBreak();
}


void Control::receiveOutputAttribute(const std::string &attribute,
                       osc::ReceivedMessageArgumentStream arguments)
{
    try {
        /// e.g. '/vimix/output/enable' or '/vimix/output/enable T' or '/vimix/output/enable F'
        if ( attribute.compare(OSC_OUTPUT_ENABLE) == 0) {
            bool on = true;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            Settings::application.render.disabled = !on;
        }
        /// e.g. '/vimix/output/disable' or '/vimix/output/disable T' or '/vimix/output/disable F'
        else if ( attribute.compare(OSC_OUTPUT_DISABLE) == 0) {
            bool on = true;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            Settings::application.render.disabled = on;
        }
        /// e.g. '/vimix/output/fading f 0.2'
        else if ( attribute.compare(OSC_OUTPUT_FADING) == 0) {
            float fading = 0.f;
            arguments >> fading >> osc::EndMessage;
            Mixer::manager().session()->setFading(fading); // TODO move cursor when in Mixing view
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
}

void Control::receiveSourceAttribute(Source *target, const std::string &attribute,
                       osc::ReceivedMessageArgumentStream arguments)
{
    if (target == nullptr)
        return;

    try {
        /// e.g. '/vimix/current/play' or '/vimix/current/play T' or '/vimix/current/play F'
        if ( attribute.compare(OSC_SOURCE_PLAY) == 0) {
            bool on = true;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            target->call( new SetPlay(on) );
        }
        /// e.g. '/vimix/current/pause' or '/vimix/current/pause T' or '/vimix/current/pause F'
        else if ( attribute.compare(OSC_SOURCE_PAUSE) == 0) {
            bool on = true;
            if ( !arguments.Eos()) {
                arguments >> on >> osc::EndMessage;
            }
            target->call( new SetPlay(!on) );
        }
        /// e.g. '/vimix/current/replay'
        else if ( attribute.compare(OSC_SOURCE_REPLAY) == 0) {
            target->call( new RePlay() );
        }
        /// e.g. '/vimix/current/alpha f 0.3'
        else if ( attribute.compare(OSC_SOURCE_ALPHA) == 0) {
            float x = 1.f;
            arguments >> x >> osc::EndMessage;
            target->call( new SetAlpha(x), true );
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
            target->call( new SetDepth(x) );
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
#ifdef CONTROL_DEBUG
        else {
            Log::Info(CONTROL_OSC_MSG "Ignoring attribute '%s' for target %s.", attribute.c_str(), target->name().c_str());
        }
#endif

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
}


void Control::sendCurrentSourceAttibutes(const IpEndpointName &remoteEndpoint)
{
    Source *s = Mixer::manager().currentSource();
    if (s!=nullptr) {

        // build socket to send message to indicated endpoint
        UdpTransmitSocket socket( IpEndpointName( remoteEndpoint.address, Settings::application.control.osc_port_send ) );

        // build messages packet
        char buffer[IP_MTU_SIZE];
        osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

        // create bundle
        p.Clear();
        p << osc::BeginBundle();

        ///
        /// messages
        ///
        /// Play status
        p << osc::BeginMessage( OSC_PREFIX OSC_CURRENT OSC_SOURCE_PLAY );
        p << s->playing();
        p << osc::EndMessage;
        /// Depth
        p << osc::BeginMessage( OSC_PREFIX OSC_CURRENT OSC_SOURCE_DEPTH );
        p << s->depth();
        p << osc::EndMessage;
        /// Alpha
        p << osc::BeginMessage( OSC_PREFIX OSC_CURRENT OSC_SOURCE_ALPHA );
        p << s->alpha();
        p << osc::EndMessage;
        /// indexed alpha
        std::string addresspattern = std::string(OSC_PREFIX) + OSC_SEPARATOR;
        addresspattern += std::to_string(Mixer::manager().indexCurrentSource()) + OSC_SOURCE_ALPHA;
        p << osc::BeginMessage(addresspattern.c_str());
        p << s->alpha();
        p << osc::EndMessage;
        socket.Send( p.Data(), p.Size() );

        // send bundle
        p << osc::EndBundle;
        socket.Send( p.Data(), p.Size() );
    }
}

void Control::sendStatus(const IpEndpointName &remoteEndpoint)
{
    // build socket to send message to indicated endpoint
    UdpTransmitSocket socket( IpEndpointName( remoteEndpoint.address, Settings::application.control.osc_port_send ) );

    // build messages packet
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

    p.Clear();
    p << osc::BeginBundle();

    /// Agree to test
    p << osc::BeginMessage( OSC_PREFIX OSC_INFO OSC_INFO_TEST );
    p << true;
    p << osc::EndMessage;

    /// send output attributes
    p << osc::BeginMessage( OSC_PREFIX OSC_OUTPUT OSC_OUTPUT_ENABLE );
    p << !Settings::application.render.disabled;
    p << osc::EndMessage;
    p << osc::BeginMessage( OSC_PREFIX OSC_OUTPUT OSC_OUTPUT_FADING );
    p << Mixer::manager().session()->fading();
    p << osc::EndMessage;

    /// send all sources alpha
    for (int i = 0; i < Mixer::manager().count(); ++i) {
        std::string addresspattern = std::string(OSC_PREFIX) + OSC_SEPARATOR;
        addresspattern += std::to_string(i) + OSC_SOURCE_ALPHA;
        p << osc::BeginMessage(addresspattern.c_str());
        p << Mixer::manager().sourceAtIndex(i)->alpha();
        p << osc::EndMessage;
    }

    p << osc::EndBundle;
    socket.Send( p.Data(), p.Size() );
}
