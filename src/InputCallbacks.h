#ifndef INPUTCALLBACKS_H
#define INPUTCALLBACKS_H

#include <list>
#include <map>
#include <vector>

#include "Source/SourceList.h"
#include "Metronome.h"

class Session;
class SourceCallback;

/**
 * @brief The InputCallbacks class holds the actions associated to the inputs of a Session.
 *
 * An input (a key of the keyboard, a button of a gamepad, a timer, etc.) can be assigned
 * to one or several SourceCallback applied to a Target (a Source, a batch, or the current
 * source). This class owns the callback models: they are deleted when removed from it.
 *
 * NB: non copyable, as it owns the SourceCallback models it holds.
 */
class InputCallbacks
{
public:

    /**
     * @brief The Assignment struct associates one input to one action on one target.
     */
    struct Assignment {
        bool active_;
        SourceCallback *model_;
        std::map<uint64_t, std::pair< SourceCallback *, SourceCallback *> > instances_;
        Target target_;
        Assignment() {
            active_ = false;
            model_   = nullptr;
            target_  = nullptr;
        }
        ~Assignment();
        void clear();
    };

    // all the assignments, indexed by input
    typedef std::multimap<uint, Assignment>  Map;

    // NB: an InputCallbacks always belongs to a Session
    InputCallbacks(Session *parent);
    ~InputCallbacks();

    // non copyable: this class owns the callback models
    InputCallbacks(InputCallbacks const&) = delete;
    InputCallbacks& operator=(InputCallbacks const&) = delete;

    // iterate over all assignments
    inline Map::iterator begin () { return input_callbacks_.begin(); }
    inline Map::iterator end   () { return input_callbacks_.end(); }
    inline bool empty () const { return input_callbacks_.empty(); }

    // assign an action on a target to an input (takes ownership of the callback)
    void assign(uint input, Target target, SourceCallback *callback);
    // list of all the (target, action) assigned to an input
    std::list< std::pair<Target, SourceCallback*> > at(uint input);
    // remove one action
    void remove (SourceCallback *callback);
    // remove all actions on a target
    void removeAll(Target target);
    // remove all actions of an input
    void removeAll(uint input);
    // remove all actions
    void clear ();

    // list of all inputs having at least one action
    std::list<uint> assignedInputs();
    // true if the input has at least one action
    bool assigned(uint input);
    // move all actions from an input to another
    void swap(uint from, uint to);
    // duplicate all actions of an input to another
    void copy(uint from, uint to);

    // list of all inputs acting on a source
    std::list<uint> inputsForSource( uint64_t id );
    // remove all actions targetting a source
    void removeSource( uint64_t id );

    // get a copy of all assignments (with cloned callback models)
    Map copyMap() const;
    // NB: takes ownership of the callback models held in the map, and empties it
    void import(Map &callbacks);

    // synchronization of the actions of an input with the Metronome
    void setSynchrony(uint input, Metronome::Synchronicity sync);
    std::vector<Metronome::Synchronicity> synchrony();
    Metronome::Synchronicity synchrony(uint input);

private:
    // the session owning these callbacks (to reach its sources and batches)
    Session *session_;

    Map input_callbacks_;
    std::vector<Metronome::Synchronicity> input_sync_;
};

#endif // INPUTCALLBACKS_H
