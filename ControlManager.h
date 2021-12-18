#ifndef CONTROL_H
#define CONTROL_H

#include "NetworkToolkit.h"

class Control
{
    // Private Constructor
    Control();
    Control(Control const& copy) = delete;
    Control& operator=(Control const& copy) = delete;

public:

    static Control& manager ()
    {
        // The only instance
        static Control _instance;
        return _instance;
    }
    ~Control();

    bool init();
    void terminate();

    // void setOscPort(int P);

protected:

    class RequestListener : public osc::OscPacketListener {
    protected:
        virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                     const IpEndpointName& remoteEndpoint );
    };

private:

    static void listen();
    RequestListener listener_;
    UdpListeningReceiveSocket *receiver_;
};

#endif // CONTROL_H
