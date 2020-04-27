#ifndef SOURCE_H
#define SOURCE_H

#include <string>
#include <map>
#include <list>

#include "View.h"

class ImageShader;
class ImageProcessingShader;
class FrameBuffer;
class FrameBufferSurface;
class MediaPlayer;
class Surface;
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

    // every Source has a shader to control image processing effects
    inline ImageProcessingShader *processingShader() const { return rendershader_; }

    // every Source has a shader to control mixing effects
    inline ImageShader *mixingShader() const { return mixingshader_; }

    // every Source shall have a frame buffer
    virtual FrameBuffer *frame() const = 0;

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
    // NB: rendershader_ is applied at render()
    FrameBuffer *renderbuffer_;

    // the rendersurface draws the renderbuffer in the scene
    // It is associated to the rendershader for mixing effects
    FrameBufferSurface *rendersurface_;

    // rendershader provides image processing controls
    ImageProcessingShader *rendershader_;

    // mixingshader provides mixing controls
    ImageShader *mixingshader_;

    Frame *mixingoverlay_;

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

    FrameBuffer *frame() const;
    ImageShader *mixingShader() const;
    void render(bool current);

    // Media specific interface
    std::string uri() const;
    MediaPlayer *mediaplayer() const;

protected:

    virtual void init();

    Surface *mediasurface_;
    std::string uri_;
    MediaPlayer *mediaplayer_;
};


#endif // SOURCE_H
