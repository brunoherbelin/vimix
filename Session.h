#ifndef SESSION_H
#define SESSION_H


#include "View.h"
#include "Source.h"

class Session
{
public:
    Session();
    ~Session();

    // add or remove sources
    SourceList::iterator addSource(Source *s);
    SourceList::iterator deleteSource(Source *s);

    // management of list of sources
    SourceList::iterator begin();
    SourceList::iterator end();
    SourceList::iterator find(int index);
    SourceList::iterator find(Source *s);
    SourceList::iterator find(std::string name);
    SourceList::iterator find(Node *node);

    uint numSource() const;
    int index(SourceList::iterator it) const;

    // update all sources
    void update(float dt);

    // result of render
    inline FrameBuffer *frame() const { return render_.frameBuffer(); }

    // configuration for group nodes of views
    inline Group *config(View::Mode m) const { return config_.at(m); }

protected:
    RenderView render_;
    SourceList sources_;

    std::map<View::Mode, Group*> config_;
};

#endif // SESSION_H
