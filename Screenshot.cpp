#include "Screenshot.h"

#include <memory.h>
#include <assert.h>

#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

Screenshot::Screenshot()
{
    Width = Height = 0;
    Data = nullptr;
}

Screenshot::~Screenshot()
{
    Clear();
}

bool Screenshot::IsFull()
{
    return Data != nullptr;
}

void Screenshot::Clear()
{
    if (IsFull())
        free(Data);
    Data = nullptr;
}

void Screenshot::CreateEmpty(int w, int h)
{
    Clear();
    Width = w;
    Height = h;
    Data = (unsigned int*) malloc(Width * Height * 4 * sizeof(unsigned int));
    memset(Data, 0, Width * Height * 4);
}

void Screenshot::CreateFromCaptureGL(int x, int y, int w, int h)
{
    Clear();
    Width = w;
    Height = h;
    Data = (unsigned int*) malloc(Width * Height * 4);

    // actual capture of frame buffer
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, Data);

    // make it usable
    RemoveAlpha();
    FlipVertical();
}

void Screenshot::SaveFile(const char* filename)
{
    if (Data)
        stbi_write_png(filename, Width, Height, 4, Data, Width * 4);
}

void Screenshot::RemoveAlpha()
{
    unsigned int* p = Data;
    int n = Width * Height;
    while (n-- > 0)
    {
        *p |= 0xFF000000;
        p++;
    }
}

void Screenshot::BlitTo(Screenshot* dst, int src_x, int src_y, int dst_x, int dst_y, int w, int h) const
{
    const Screenshot* src = this;
    assert(dst != src);
    assert(dst != NULL);
    assert(src_x >= 0 && src_y >= 0);
    assert(src_x + w <= src->Width);
    assert(src_y + h <= src->Height);
    assert(dst_x >= 0 && dst_y >= 0);
    assert(dst_x + w <= dst->Width);
    assert(dst_y + h <= dst->Height);
    for (int y = 0; y < h; y++)
        memcpy(dst->Data + dst_x + (dst_y + y) * dst->Width, src->Data + src_x + (src_y + y) * src->Width, w * 4);
}

void Screenshot::FlipVertical()
{
    int comp = 4;
    int stride = Width * comp;
    unsigned char* line_tmp = new unsigned char[stride];
    unsigned char* line_a = (unsigned char*)Data;
    unsigned char* line_b = (unsigned char*)Data + (stride * (Height - 1));
    while (line_a < line_b)
    {
        memcpy(line_tmp, line_a, stride);
        memcpy(line_a, line_b, stride);
        memcpy(line_b, line_tmp, stride);
        line_a += stride;
        line_b -= stride;
    }
    delete[] line_tmp;
}

