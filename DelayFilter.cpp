
#include "Log.h"
#include "FrameBuffer.h"
#include "Resource.h"
#include "Primitives.h"
#include "Visitor.h"

#include "DelayFilter.h"

DelayFilter::DelayFilter(): FrameBufferFilter(),
    temp_frame_(nullptr), now_(0.0), delay_(0.5)
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

void DelayFilter::update (float dt)
{
    if (input_) {

        // What time is it?
        now_ += double(dt) * 0.001;

        // if temporary FBO was pending to be deleted, delete it now
        if (temp_frame_ != nullptr) {
            delete temp_frame_;
            temp_frame_ = nullptr;
        }

        // is the total buffer of images longer than delay ?
        if ( !frames_.empty() && now_ - elapsed_.front() > delay_ )
        {
            // remember FBO to be reused if needed (see below) or deleted later
            temp_frame_ = frames_.front();

            // remove element from queue (front)
            frames_.pop();
            elapsed_.pop();
        }

        // add image to queue to accumulate buffer images until delay reached (with margin)
        if ( frames_.empty() || now_ - elapsed_.front() < delay_ + ( double(dt) * 0.002) )
        {
            // create a FBO if none can be reused (from above) and test for RAM in GPU
            if (temp_frame_ == nullptr && ( frames_.empty() || Rendering::shouldHaveEnoughMemory(input_->resolution(), input_->use_alpha()) ) ){
                temp_frame_ = new FrameBuffer( input_->resolution(), input_->use_alpha() );
            }
            // image available
            if (temp_frame_ != nullptr) {
                // add element to queue (back)
                frames_.push( temp_frame_ );
                elapsed_.push( now_ );
                // temp_frame_ FBO is now used, it should not be deleted
                temp_frame_ = nullptr;
            }
            else {
                // set delay to maximum affordable
                delay_ = now_ - elapsed_.front() - (dt * 0.001);
                Log::Warning("Cannot satisfy delay: not enough RAM in graphics card.");
            }
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

    if ( enabled() ) // TODO TEST DISABLE
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

