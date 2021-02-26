// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>


#include "imgui.h"
#include "ImGuiToolkit.h"

// memmove
#include <string.h>
#include <sstream>
#include <regex>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "Session.h"
#include "Resource.h"
#include "Source.h"
#include "SessionSource.h"
#include "PickingVisitor.h"
#include "BoundingBoxVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "UpdateCallback.h"
#include "ActionManager.h"
#include "Log.h"

#include "RenderView.h"


RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr), fading_overlay_(nullptr)
{
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
    if (fading_overlay_)
        delete fading_overlay_;
}

bool RenderView::canSelect(Source *s) {

    return false;
}

void RenderView::setFading(float f)
{
    if (fading_overlay_ == nullptr)
        fading_overlay_ = new Surface;

    fading_overlay_->shader()->color.a = CLAMP( f < EPSILON ? 0.f : f, 0.f, 1.f);
}

float RenderView::fading() const
{
    if (fading_overlay_)
        return fading_overlay_->shader()->color.a;
    else
        return 0.f;
}

void RenderView::setResolution(glm::vec3 resolution, bool useAlpha)
{
    // use default resolution if invalid resolution is given (default behavior)
    if (resolution.x < 2.f || resolution.y < 2.f)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.render.ratio, Settings::application.render.res);

    // do we need to change resolution ?
    if (frame_buffer_ && frame_buffer_->resolution() != resolution)  {

        // new frame buffer
        delete frame_buffer_;
        frame_buffer_ = nullptr;
    }

    if (!frame_buffer_)
        // output frame is an RBG Multisamples FrameBuffer
        frame_buffer_ = new FrameBuffer(resolution, useAlpha, true);

    // reset fading
    setFading();
}

void RenderView::draw()
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    if (frame_buffer_) {
        // draw in frame buffer
        glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / frame_buffer_->aspectRatio(), 1.f, 1.f));

        // render the scene normally (pre-multiplied alpha in RGB)
        frame_buffer_->begin();
        scene.root()->draw(glm::identity<glm::mat4>(), P);
        fading_overlay_->draw(glm::identity<glm::mat4>(), projection);
        frame_buffer_->end();
    }
}
