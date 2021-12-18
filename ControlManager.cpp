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

#include "osc/OscOutboundPacketStream.h"

#include "Log.h"
#include "Settings.h"
#include "BaseToolkit.h"

#include "ControlManager.h"

#ifndef NDEBUG
#define CONTROL_DEBUG
#endif


void Control::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                           const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{

#ifdef CONTROL_DEBUG
        Log::Info("%s sent an OSC message %s.", sender, m.AddressPattern());
#endif

        std::list<std::string> address = BaseToolkit::splitted(m.AddressPattern(), '/');

        if (address.size() == 3 && address.front().compare(APP_NAME) == 0 ){
            address.pop_front();
            Log::Info("Wellformed vimix message %s.", address.front().c_str());
        }

    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}


void Control::listen()
{
#ifdef CONTROL_DEBUG
    Log::Info("Accepting OSC messages on port %d", Settings::application.control.osc_port);
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
                                                                   Settings::application.control.osc_port ), &listener_ );
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
