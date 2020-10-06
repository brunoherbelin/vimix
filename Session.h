#ifndef SESSION_H
#define SESSION_H

#include <mutex>

#include "View.h"
#include "Source.h"

class Recorder;

class Session
{
public:
    Session();
    ~Session();

    static Session *load(const std::string& filename);

    // add given source into the session
    SourceList::iterator addSource (Source *s);

    // delete the source s from the session
    SourceList::iterator deleteSource (Source *s);

    // remove this source from the session
    // Does not delete the source
    void removeSource(Source *s);

    // get ptr to front most source and remove it from the session
    // Does not delete the source
    Source *popSource();

    // management of list of sources
    bool empty() const;
    uint numSource() const;
    SourceList::iterator begin ();
    SourceList::iterator end ();
    SourceList::iterator find (Source *s);
    SourceList::iterator find (std::string name);
    SourceList::iterator find (Node *node);

    SourceList::iterator find (uint64_t id);
    std::list<uint64_t> getIdList() const;

    SourceList::iterator at (int index);
    int index (SourceList::iterator it) const;

    // update all sources and mark sources which failed
    void update (float dt);

    // update mode (active or not)
    void setActive (bool on);
    inline bool active () { return active_; }

    // return the last source which failed
    Source *failedSource() { return failedSource_; }

    // get frame result of render
    inline FrameBuffer *frame () const { return render_.frame(); }

    // Recorders
    void addRecorder(Recorder *rec);
    Recorder *frontRecorder();
    void stopRecorders();
    void clearRecorders();
    void transferRecorders(Session *dest);

    // configure rendering resolution
    void setResolution(glm::vec3 resolution);

    // manipulate fading of output
    void setFading(float f, bool forcenow = false);
    inline float fading() const { return fading_target_; }

    // configuration for group nodes of views
    inline Group *config (View::Mode m) const { return config_.at(m); }

    // name of file containing this session (for transfer)
    void setFilename(const std::string &filename) { filename_ = filename; }
    std::string filename() const { return filename_; }

    // lock and unlock access (e.g. while saving)
    void lock();
    void unlock();

protected:
    RenderView render_;
    std::string filename_;
    Source *failedSource_;
    SourceList sources_;
    std::map<View::Mode, Group*> config_;
    bool active_;
    std::list<Recorder *> recorders_;
    float fading_target_;
    std::mutex access_;
};


#endif // SESSION_H
