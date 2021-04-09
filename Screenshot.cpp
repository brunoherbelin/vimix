#include "Screenshot.h"

#include <memory.h>
#include <assert.h>
#include <thread>
#include <atomic>

#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>



Screenshot::Screenshot()
{
    Width = Height = 0;
    Data = nullptr;
    Pbo = 0;
    Pbo_size = 0;
    Pbo_full = false;
}

Screenshot::~Screenshot()
{
    if (Pbo > 0)
        glDeleteBuffers(1, &Pbo);
    if (Data)
        free(Data);
}

bool Screenshot::isFull()
{
    return Pbo_full;
}

void Screenshot::captureGL(int x, int y, int w, int h)
{
    Width = w - x;
    Height = h - y;
    unsigned int size = Width * Height * 3;

    // create BPO
    if (Pbo == 0)
        glGenBuffers(1, &Pbo);

    // bind
    glBindBuffer(GL_PIXEL_PACK_BUFFER, Pbo);

    // init
    if (Pbo_size != size) {
        Pbo_size = size;
        if (Data)  free(Data);
        Data = (unsigned char*) malloc(Pbo_size);
        glBufferData(GL_PIXEL_PACK_BUFFER, Pbo_size, NULL, GL_STREAM_READ);
    }

    // screenshot to PBO (fast)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, 0);
    Pbo_full = true;

    // done
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void Screenshot::save(std::string filename)
{
    // is there something to save?
    if (Pbo && Pbo_size > 0 && Pbo_full) {

        // bind buffer
        glBindBuffer(GL_PIXEL_PACK_BUFFER, Pbo);

        // get pixels (quite fast)
        unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (NULL != ptr) {
            memmove(Data, ptr, Pbo_size);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }

        // initiate saving in thread (slow)
        std::thread(storeToFile, this, filename).detach();

        // ready for next
        Pbo_full = false;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

}

void Screenshot::RemoveAlpha()
{
    unsigned int* p = (unsigned int*)Data;
    int n = Width * Height;
    while (n-- > 0)
    {
        *p |= 0xFF000000;
        p++;
    }
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

// Thread to perform slow operation of saving to file
void Screenshot::storeToFile(Screenshot *s, std::string filename)
{
    static std::atomic<bool> ScreenshotSavePending_ = false;
    // only one save at a time
    if (ScreenshotSavePending_)
        return;
    ScreenshotSavePending_ = true;
    // got data to save ?
    if (s && s->Data) {
        // save file
        stbi_flip_vertically_on_write(true);
        stbi_write_png(filename.c_str(), s->Width, s->Height, 3, s->Data, s->Width * 3);
    }
    ScreenshotSavePending_ = false;
}
