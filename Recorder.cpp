#include <thread>

#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

#include "SystemToolkit.h"
#include "FrameBuffer.h"
#include "Log.h"

#include "Recorder.h"


Recorder::Recorder() : enabled_(true), finished_(false)
{

}


FrameRecorder::FrameRecorder() : Recorder()
{
    filename_ = SystemToolkit::home_path() + SystemToolkit::date_time_string() + "_vimix.png";
}


// Thread to perform slow operation of saving to file
void save_png(std::string filename, unsigned char *data, uint w, uint h, uint c)
{
    // got data to save ?
    if (data) {
        // save file
        stbi_write_png(filename.c_str(), w, h, c, data, w * c);
        // notify
        Log::Notify("Capture %s saved.", filename.c_str());
    }
}

void FrameRecorder::addFrame(const FrameBuffer *frame_buffer, float dt)
{
    if (enabled_)
    {
        uint w = frame_buffer->width();
        uint h = frame_buffer->height();
        uint c = frame_buffer->use_alpha() ? 4 : 3;
        GLenum format = frame_buffer->use_alpha() ? GL_RGBA : GL_RGB;
        uint size = w * h * c;
        unsigned char * data = (unsigned char*) malloc(size);

        glGetTextureSubImage( frame_buffer->texture(), 0, 0, 0, 0, w, h, 1, format, GL_UNSIGNED_BYTE, size, data);

        // save in separate thread
        std::thread(save_png, filename_, data, w, h, c).detach();
    }

    // record one frame only
    finished_ = true;
}
