/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <glad/glad.h>

#include "NavigatorInternal.h"

#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Mixer.h"
#include "FrameBuffer.h"
#include "Source/MediaSource.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Visitor/InfoVisitor.h"

#include "NavigatorWidgets.h"

///
/// SOURCE PREVIEW
///

SourcePreview::SourcePreview() : source_(nullptr), label_(""), reset_(0)
{

}

void SourcePreview::setSource(Source *s, const std::string &label)
{
    if(source_)
        delete source_;

    source_ = s;
    label_ = label;
    reset_ = true;
}

Source * SourcePreview::getSource()
{
    Source *s = source_;
    source_ = nullptr;
    return s;
}

void SourcePreview::Render(float width)
{
    if(source_) {
        // cancel if failed
        if (source_->failed()) {
            // remove from list of recent import files if relevant
            MediaSource *failedFile = dynamic_cast<MediaSource *>(source_);
            if (failedFile != nullptr) {
                Settings::application.recentImport.remove( failedFile->path() );
            }
            setSource();
        }
        else
        {
            // render framebuffer
            if ( reset_  && source_->ready() ) {
                // trick to ensure a minimum of 2 frames are rendered actively
                source_->setActive(true);
                source_->update( Mixer::manager().dt() );
                source_->render();
                source_->setActive(false);
                reset_ = false;
            }
            else {
                // update source
                source_->update( Mixer::manager().dt() );
                source_->render();
            }

            // draw preview
            FrameBuffer *frame = source_->frame();
            ImVec2 preview_size(width, width / frame->aspectRatio());
            ImGui::Image((void*)(uintptr_t) frame->texture(), preview_size);
            bool mouseover = ImGui::IsItemHovered();
            if (mouseover) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(label_.c_str());
                ImGui::EndTooltip();
            }
            // if the source is playable and once its ready
            if (source_->playable() && source_->ready()) {
                // activate the source on mouse over
                if (source_->active() != mouseover)
                    source_->setActive(mouseover);
                // show icon '>' to indicate if we can activate it
                if (!mouseover) {
                    ImVec2 pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(pos + preview_size * ImVec2(0.5f, -0.6f));
                    ImGuiToolkit::Icon(ICON_VI_PLAY);
                    ImGui::SetCursorPos(pos);
                }
            }
            // information text
            ImGuiToolkit::Icon(source_->icon().x, source_->icon().y);
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::Text("%s", source_->info().c_str());
            if (source_->ready()) {
                static InfoVisitor _info;
                source_->accept(_info);
                ImGui::Text("%s", _info.str().c_str());
            }
            else
                ImGui::Text("loading...");
        }
    }
}

bool SourcePreview::ready() const
{
    return source_ != nullptr && source_->ready();
}

///
/// THUMBNAIL
///

Thumbnail::Thumbnail() : aspect_ratio_(-1.f), texture_(0)
{
}

Thumbnail::~Thumbnail()
{
    if (texture_)
        glDeleteTextures(1, &texture_);
}

bool Thumbnail::filled()
{
    return aspect_ratio_ > 0.f;
}

void Thumbnail::reset()
{
    aspect_ratio_ = -1.f;
}

void Thumbnail::fill(const FrameBufferImage *image)
{
    if (!texture_) {
        glGenTextures(1, &texture_);
        glBindTexture( GL_TEXTURE_2D, texture_);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, SESSION_THUMBNAIL_HEIGHT * 3, SESSION_THUMBNAIL_HEIGHT);
    }

    aspect_ratio_ = static_cast<float>(image->width) / static_cast<float>(image->height);
    glBindTexture( GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->rgb);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Thumbnail::Render(float width)
{
    if (filled())
        ImGui::Image((void*)(intptr_t)texture_, ImVec2(width, width/aspect_ratio_), ImVec2(0,0), ImVec2(0.333f*aspect_ratio_, 1.f));
}

