#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <string>

#include "Toolkit/GstToolkit.h"

class Screenshot
{
    int             Width, Height, bpp;
    bool            VFlip;
    unsigned int    Pbo;
    unsigned int    Pbo_size;
    bool            Pbo_full;

    static void storeToFile(GstBuffer *buffer, GstCaps *caps, GstToolkit::Image format, std::string filename);
    void capture();

public:
    Screenshot();
    ~Screenshot();

    // Quick usage :
    // 1) Capture screenshot
    void captureGL(int w, int h);
    void captureFramebuffer(class FrameBuffer *fb);
    // 2) if it is full after capture
    bool isFull();
    // 3) then you can save to file (encoded in given image format)
    //    returns false if the format cannot be used
    bool save(std::string filename, GstToolkit::Image format = GstToolkit::IMAGE_PNG);
};

#endif // SCREENSHOT_H
