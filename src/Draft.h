#ifndef DRAFT_H
#define DRAFT_H

#include "Interpolator.h"

class Source;

/**
 * @brief The Draft manager
 *
 * In DRAFT mode, the user edits the sources of the current session without
 * changing the output: sources hold the draft state, while their live state
 * is kept aside (see Source::beginDraft) and rendered in the session frame.
 * The draft is rendered in the draft frame of the session.
 *
 * Callbacks of sources (inputs, metronome, etc.) keep operating on the live state.
 *
 * Leaving the DRAFT mode either cancels the draft, or applies it with
 * an animation from the live state to the draft (only on modified properties).
 */
class Draft
{
    // Private Constructor
    Draft();
    Draft(Draft const& copy) = delete;
    Draft& operator=(Draft const& copy) = delete;

public:

    static Draft& manager ()
    {
        // The only instance
        static Draft _instance;
        return _instance;
    }

    typedef enum {
        DRAFT_OFF = 0,
        DRAFT_EDIT,
        DRAFT_ANIMATE
    } State;
    inline State state () const { return state_; }
    // user is editing a draft
    inline bool active () const { return state_ == DRAFT_EDIT; }
    // user is editing a draft or the draft is being applied
    inline bool busy () const { return state_ != DRAFT_OFF; }

    // enter DRAFT mode
    bool enter ();
    // leave DRAFT mode without change
    void cancel ();
    // leave DRAFT mode with an animation of given duration (ms) to the draft
    void apply (float duration);
    // leave DRAFT mode or finish animation immediately
    void terminate ();

    // progress of animation [0 1]
    float progress () const;

    // to be called at each frame (dt in ms)
    void update (float dt);

    // a source is deleted
    void forget (Source *s);

private:
    State state_;
    Interpolator interpolator_;
    float duration_;
    float progress_;

    void finish ();
};

#endif // DRAFT_H
