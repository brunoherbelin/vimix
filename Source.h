#ifndef SOURCE_H
#define SOURCE_H

#include <string>
#include <map>
#include <list>

#include "View.h"

class ImageShader;
class Surface;
class FrameBuffer;
class MediaPlayer;
class FrameBufferSurface;
class Frame;

class Source;
// TODO : source set sorted by shader
// so that the render loop avoid switching
typedef std::list<Source *> SourceList;

class Source
{
public:
    // create a source and add it to the list
    // only subclasses of sources can actually be instanciated
    Source(std::string name = "");
    virtual ~Source();

    // manipulate name of source
    inline std::string name () const { return name_; }
    std::string rename (std::string newname);

    // get handle on the node used to manipulate the source in a view
    inline Group *group(View::Mode m) const { return groups_.at(m); }

    // every Source have a shader to control visual effects
    virtual ImageShader *shader() const = 0;

    // every Source shall be rendered before draw
    virtual void render(bool current) = 0;

    // global management of list of sources
    static SourceList::iterator begin();
    static SourceList::iterator end();
    static SourceList::iterator find(Source *s);
    static SourceList::iterator find(std::string name);
    static SourceList::iterator find(Node *node);
    static uint numSource();

protected:
    // name
    std::string name_;

    // every Source shall be initialized on first draw
    bool initialized_;
    virtual void init() = 0;

    // nodes
    std::map<View::Mode, Group*> groups_;

    // render() fills in the renderbuffer at every frame
    // NB: additional shader (custom) are applied inside render()
    FrameBuffer *renderbuffer_;

    // the rendersurface draws the renderbuffer in the scene
    // It is associated to the sourceshader for mixing effects
    // (aka visual effect applied in scene, not in render() )
    FrameBufferSurface *rendersurface_;

    // static global list of sources
    static SourceList sources_;
};


struct hasName: public std::unary_function<Source*, bool>
{
    inline bool operator()(const Source* elem) const {
       return (elem && elem->name() == _n);
    }
    hasName(std::string n) : _n(n) { }

private:
    std::string _n;
};

struct hasNode: public std::unary_function<Source*, bool>
{
    bool operator()(const Source* elem) const;
    hasNode(Node *n) : _n(n) { }

private:
    Node *_n;
};

class MediaSource : public Source
{
public:
    MediaSource(std::string name, std::string uri);
    ~MediaSource();

    void render(bool current);

    // Source interface
    ImageShader *shader() const;

    // Media specific interface
    std::string uri() const;
    MediaPlayer *mediaplayer() const;

protected:

    virtual void init();

    Frame *mixingoverlay_;
    Surface *mediasurface_;
    std::string uri_;
    MediaPlayer *mediaplayer_;
};


#endif // SOURCE_H
