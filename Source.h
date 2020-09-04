#ifndef SOURCE_H
#define SOURCE_H

#include <string>
#include <map>
#include <atomic>
#include <list>

#include "View.h"
#include "Decorations.h"

class ImageShader;
class ImageProcessingShader;
class FrameBuffer;
class FrameBufferSurface;
class MediaPlayer;
class Surface;
class Session;
class Frame;
class Source;
class CloneSource;

typedef std::list<Source *> SourceList;
typedef std::list<CloneSource *> CloneList;

class Source
{
    friend class CloneSource;
    friend class View;
    friend class MixingView;
    friend class GeometryView;
    friend class LayerView;
    friend class TransitionView;

public:
    // create a source and add it to the list
    // only subclasses of sources can actually be instanciated
    Source();
    virtual ~Source();

    // manipulate name of source
    void setName (const std::string &name);
    inline std::string name () const { return name_; }
    inline const char *initials () const { return initials_; }

    // cloning mechanism
    virtual CloneSource *clone ();

    // Display mode
    typedef enum {
        UNINITIALIZED   = 0,
        VISIBLE   = 1,
        SELECTED = 2,
        CURRENT  = 3
    } Mode;
    Mode mode () const;
    void setMode (Mode m);

    // get handle on the nodes used to manipulate the source in a view
    inline Group *group (View::Mode m) const { return groups_.at(m); }
    inline Node  *groupNode (View::Mode m) const { return static_cast<Node*>(groups_.at(m)); }

    // tests if a given node is part of the source
    bool contains (Node *node) const;

    // a Source has a shader used to render in fbo
    inline Shader *renderingShader() const { return renderingshader_; }

    // the rendering shader always have an image processing shader
    inline ImageProcessingShader *processingShader () const { return processingshader_; }

    // the image processing shader can be enabled or disabled
    // (NB: when disabled, a simple ImageShader is applied)
    void setImageProcessingEnabled (bool on);
    bool imageProcessingEnabled();

    // a Source has a shader to control mixing effects
    inline ImageShader *blendingShader () const { return blendingshader_; }

    // every Source has a frame buffer from the renderbuffer
    virtual FrameBuffer *frame () const;

    // touch to request update
    inline void touch () { need_update_ = true; }

    // informs if its ready (i.e. initialized)
    inline bool ready() const  { return initialized_; }

    // a Source shall be updated before displayed (Mixing, Geometry and Layer)
    virtual void update (float dt);

    // update mode
    virtual void setActive (bool on);
    inline bool active () { return active_; }

    // a Source shall informs if the source failed (i.e. shall be deleted)
    virtual bool failed() const = 0;

    // a Source shall define a way to get a texture
    virtual uint texture() const = 0;

    // a Source shall define how to render into the frame buffer
    virtual void render() = 0;

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    struct hasNode: public std::unary_function<Source*, bool>
    {
        bool operator()(const Source* elem) const;
        hasNode(Node *n) : _n(n) { }
    private:
        Node *_n;
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


protected:
    // name
    std::string name_;
    char initials_[3];

    // every Source shall be initialized on first draw
    bool initialized_;
    virtual void init() = 0;

    // nodes
    std::map<View::Mode, Group*> groups_;

    // render() fills in the renderbuffer at every frame
    // NB: rendershader_ is applied at render()
    FrameBuffer *renderbuffer_;
    void attach(FrameBuffer *renderbuffer);

    // the rendersurface draws the renderbuffer in the scene
    // It is associated to the rendershader for mixing effects
    FrameBufferSurface *rendersurface_;

    // image processing shaders
    ImageProcessingShader *processingshader_;
    // pointer to the currently attached shader
    // (will be processingshader_ if image processing is enabled)
    Shader *renderingshader_;
    // every sub class will attach the shader to a different node / hierarchy
    virtual void replaceRenderingShader() = 0;

    // blendingshader provides mixing controls
    ImageShader *blendingshader_;

    // mode for display
    Mode mode_;

    // overlays and frames to be displayed on top of source
    std::map<View::Mode, Group*> overlays_;
    std::map<View::Mode, Switch*> frames_;
    Handles *handle_[5];

    // update
    bool  active_;
    bool  need_update_;
    float dt_;
    Group *stored_status_;

    // clones
    CloneList clones_;
};



class CloneSource : public Source
{
    friend class Source;

public:
    ~CloneSource();

    // implementation of source API
    void setActive (bool on) override;
    void render() override;
    uint texture() const override;
    bool failed() const override  { return origin_ == nullptr; }
    void accept (Visitor& v) override;

    CloneSource *clone() override;
    inline void detach() { origin_ = nullptr; }
    inline Source *origin() const { return origin_; }

protected:
    // only Source class can create new CloneSource via clone();
    CloneSource(Source *origin);

    void init() override;
    void replaceRenderingShader() override;
    Surface *clonesurface_;
    Source *origin_;
};


#endif // SOURCE_H
