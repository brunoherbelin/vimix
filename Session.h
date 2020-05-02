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

    // update all sources
    void update(float dt);

    // result of render
    inline FrameBuffer *frame() const { return render_.frameBuffer(); }

protected:
    RenderView render_;
    SourceList sources_;
};

#endif // SESSION_H
