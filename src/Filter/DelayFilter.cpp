/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2024 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Log.h"
#include "FrameBuffer.h"
#include "Resource.h"
#include "Scene/Primitives.h"
#include "Visitor/Visitor.h"

#include "Filter/DelayFilter.h"

DelayFilter::DelayFilter(): FrameBufferFilter(),
    now_(0.0), delay_(0.5)
{

}

DelayFilter::~DelayFilter()
{
    // delete all frame buffers
    while (!frames_.empty()) {
        if (frames_.front() != nullptr)
            delete frames_.front();
        frames_.pop();
    }

    while (!elapsed_.empty())
        elapsed_.pop();
}

void DelayFilter::reset ()
{
    // delete all frame buffers
    while (!frames_.empty()) {
        if (frames_.front() != nullptr)
            delete frames_.front();
        frames_.pop();
    }

    while (!elapsed_.empty())
        elapsed_.pop();

    now_ = 0.0;
}

double DelayFilter::updateTime ()
{
    if (!elapsed_.empty())
        return elapsed_.front();

    return 0.;
}

void DelayFilter::update (float dt)
{
    if (input_) {

        // What time is it?
        now_ += double(dt) * 0.001;

        // fill the queue until we reach the delay time
        if (now_ - elapsed_.front() < delay_){

            if (Rendering::shouldHaveEnoughMemory(input_->resolution(), input_->flags()) )
            {
                // add new element to queue (back)
                frames_.push( new FrameBuffer( input_->resolution(), input_->flags() ));
                elapsed_.push( now_ );
            }
        }
        // empty the queue if we exceed the delay time
        else if (!frames_.empty()) {

            // temp pointer to loop 
            FrameBuffer *temp_ = nullptr;

            do {
                // after first iteration, delete temp_
                if (temp_ != nullptr) 
                    delete temp_;
                
                // take and remove front element of the queue
                // (does not fail as we check !frames_.empty())
                temp_ = frames_.front();
                frames_.pop();
                elapsed_.pop();

                // Loop with a margin to avoid popping and pushing too often
            } while (now_ - elapsed_.front() > ( delay_ + double(dt) * 0.003 ) && !frames_.empty());

            // reinsert last popped element to queue (back) to avoid re-allocating it
            frames_.push( temp_ );
            elapsed_.push( now_ );

        }
    }
}

uint DelayFilter::texture () const
{
    if (!frames_.empty())
        return frames_.front()->texture();
    else if (input_)
        return input_->texture();
    else
        return Resource::getTextureBlack();
}

glm::vec3 DelayFilter::resolution () const
{
    if (input_)
        return input_->resolution();

    return glm::vec3(1,1,0);
}

void DelayFilter::draw (FrameBuffer *input)
{
    input_ = input;

    if ( enabled() )
    {
        // make sure the queue is not empty
        if ( input_ && !frames_.empty() ) {
            // blit input framebuffer in the newest image in queue (back)
            input_->blit( frames_.back() );
        }
    }
}

void DelayFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}

