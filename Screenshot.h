#ifndef SCREENSHOT_H
#define SCREENSHOT_H

class Screenshot
{
    int             Width, Height;
    unsigned int *  Data;

public:
    Screenshot();
    ~Screenshot();

    bool IsFull();
    void Clear();

    void CreateEmpty(int w, int h);
    void CreateFromCaptureGL(int x, int y, int w, int h);

    void RemoveAlpha();
    void FlipVertical();

    void SaveFile(const char* filename);
    void BlitTo(Screenshot* dst, int src_x, int src_y, int dst_x, int dst_y, int w, int h) const;
};

#endif // SCREENSHOT_H
