#include "UpdateCallback.h"

#include "Scene.h"

#include <glm/gtc/type_ptr.hpp>

UpdateCallback::UpdateCallback() : enabled_(true)
{

}

void MoveToCenterCallback::update(Node *n, float dt)
{
    // set start position on first run or upon call of reset()
    if (!initialized_){
        initial_position_ = n->translation_;
        initialized_ = true;
    }

    // calculate amplitude of movement
    float percent = dt / duration_;

    // perform movement : translation to the center (0, 0)
    n->translation_ = initial_position_ - percent * initial_position_;

    // detect end of movement
    if ( glm::distance(initial_position_, n->translation_) < 0.1f) {
        disable();
    }
}
