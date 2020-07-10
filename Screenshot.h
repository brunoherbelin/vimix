#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <string>

class Screenshot
{
    int             Width, Height;
    unsigned char * Data;
    unsigned int    Pbo;
    unsigned int    Pbo_size;
    bool            Pbo_full;

    void RemoveAlpha();
    void FlipVertical();
    static void storeToFile(Screenshot *s, std::string filename);

public:
    Screenshot();
    ~Screenshot();

    // Quick usage :
    // 1) Capture screenshot
    void captureGL(int x, int y, int w, int h);
    // 2) if it is full after capture
    bool isFull();
    // 3) then you can save to file
    void save(std::string filename);
};

#endif // SCREENSHOT_H
