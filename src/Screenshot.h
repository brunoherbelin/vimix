#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <string>

class Screenshot
{
    int             Width, Height, bpp;
    bool            VFlip;
    unsigned int    Pbo;
    unsigned int    Pbo_size;
    bool            Pbo_full;

    static void storeToFile(unsigned char *pixels, int w, int h, int bpp, bool vflip, std::string filename);
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
    // 3) then you can save to file
    void save(std::string filename);
};

#endif // SCREENSHOT_H
