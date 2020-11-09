#ifndef LOOPBACK_H
#define LOOPBACK_H

#include <vector>

#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "FrameGrabber.h"


class Loopback : public FrameGrabber
{
    static std::string  system_loopback_pipeline;
    static std::string  system_loopback_name;
    static bool system_loopback_initialized;

    void init(GstCaps *caps) override;
    void terminate() override;

public:

    Loopback();

    static bool systemLoopbackInitialized();
    static bool initializeSystemLoopback();

};


#endif // LOOPBACK_H
