#ifndef SOURCE_H
#define SOURCE_H

#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <list>

#include "SourceList.h"
#include "FrameBuffer.h"
#include "View.h"

#define DEFAULT_MIXING_TRANSLATION -1.f, 1.f

#define ICON_SOURCE_VIDEO 18, 13
#define ICON_SOURCE_IMAGE 4, 9
#define ICON_SOURCE_DEVICE_SCREEN 0, 2
#define ICON_SOURCE_DEVICE 2, 14
#define ICON_SOURCE_SEQUENCE 3, 9
#define ICON_SOURCE_NETWORK 18, 11
#define ICON_SOURCE_PATTERN 5, 3
#define ICON_SOURCE_SESSION 19, 6
#define ICON_SOURCE_GROUP 0, 7
#define ICON_SOURCE_RENDER 19, 1
#define ICON_SOURCE_CLONE 9, 2
#define ICON_SOURCE_GSTREAMER 16, 16
#define ICON_SOURCE_SRT 14, 5
#define ICON_SOURCE_TEXT 0, 13
#define ICON_SOURCE_SHADER 16, 14
#define ICON_SOURCE 13, 11
#define ICON_WORKSPACE_BACKGROUND 10, 16
#define ICON_WORKSPACE_CENTRAL 11, 16
#define ICON_WORKSPACE_FOREGROUND 12, 16
#define ICON_WORKSPACE 13, 16

class Visitor;
class SourceCallback;
class ImageShader;
class MaskShader;
class ImageProcessingShader;
class FrameBuffer;
class FrameBufferSurface;
class FrameBufferMeshSurface;
class Frame;
class Handles;
class Symbol;
class Character;
class CloneSource;
class MixingGroup;

typedef std::list<CloneSource *> CloneList;

class SourceCore
{
public:
    SourceCore();
    SourceCore(SourceCore const&);
    SourceCore& operator= (SourceCore const& other);
    virtual ~SourceCore();

    // get handle on the nodes used to manipulate the source in a view
    inline Group *group (View::Mode m) const { return groups_.at(m); }
    inline Node  *groupNode (View::Mode m) const { return static_cast<Node*>(groups_.at(m)); }
    void store (View::Mode m);

    // a Source has a shader used to render in fbo
    inline Shader *renderingShader () const { return renderingshader_; }
    // a Source always has an image processing shader
    inline ImageProcessingShader *processingShader () const { return processingshader_; }

    void copy(SourceCore const& other);

    // alpha transfer function
    static float alphaFromCordinates(float x, float y);

protected:
    // nodes
    std::map<View::Mode, Group*> groups_;
    // temporary copy for interaction
    Group *stored_status_;
    // image processing shaders
    ImageProcessingShader *processingshader_;
    // pointer to the currently attached shader
    // (will be processingshader_ if image processing is enabled)
    Shader *renderingshader_;
};

class Source : public SourceCore
{
    friend class SourceLink;
    friend class CloneSource;
    friend class View;
    friend class MixingView;
    friend class MixingGroup;
    friend class GeometryView;
    friend class LayerView;
    friend class TextureView;
    friend class TransitionView;

public:
    // create a source and add it to the list
    // only subclasses of sources can actually be instanciated
    Source (uint64_t id = 0);
    // non assignable class
    Source(Source const&) = delete;
    Source& operator=(Source const&) = delete;
    virtual ~Source ();

    // Get unique id
    inline uint64_t id () const { return id_; }

    // manipulate name of source
    void setName (const std::string &name);
    inline std::string name () const { return name_; }
    inline const char *initials () const { return initials_; }

    // cloning mechanism
    virtual CloneSource *clone (uint64_t id = 0);
    inline bool cloned() const { return !clones_.empty(); }
    SourceList clones() const;

    // Display mode
    typedef enum {
        UNINITIALIZED = 0,
        VISIBLE  = 1,
        SELECTED = 2,
        CURRENT  = 3
    } Mode;
    Mode mode () const;
    void setMode (Mode m);

    // tests if a given node is part of the source
    bool contains (Node *node) const;

    // the image processing shader can be enabled or disabled
    // (NB: when disabled, a simple ImageShader is applied)
    void setImageProcessingEnabled (bool on);
    bool imageProcessingEnabled () const;

    // a Source has a shader to control mixing effects
    inline ImageShader *blendingShader () const { return blendingshader_; }

    // every Source has a frame buffer from the renderbuffer
    virtual FrameBuffer *frame () const;

    // touch to request update
    enum UpdateFlags_ {
        SourceUpdate_None      = 0,
        SourceUpdate_Render    = 1 << 1,
        SourceUpdate_Mask      = 1 << 2,
        SourceUpdate_Mask_fill = 1 << 3,
        SourceUpdate_Audio     = 1 << 4
    };
    typedef int UpdateFlags;
    inline void touch (UpdateFlags f = SourceUpdate_Render) { need_update_ |= f; }

    // informs if its ready (i.e. initialized)
    inline bool ready () const  { return ready_; }

    // a Source shall be updated before displayed (Mixing, Geometry and Layer)
    virtual void update (float dt);

    // add callback to each update
    void call(SourceCallback *callback, bool override = false);
    void finish(SourceCallback *callback);

    // update mode
    inline  bool active () const { return active_; }
    virtual void setActive (bool on);
    void setActive (float threshold);

    // lock mode
    inline  bool locked () const { return locked_; }
    virtual void setLocked (bool on);

    // Workspace
    typedef enum {
        WORKSPACE_BACKGROUND = 0,
        WORKSPACE_CENTRAL    = 1,
        WORKSPACE_FOREGROUND = 2,
        WORKSPACE_ANY        = 3
    } Workspace;
    inline Workspace workspace () const { return workspace_; }

    // a Source shall define a way to play
    virtual bool playable () const  = 0;
    virtual bool playing () const = 0;
    virtual void play (bool on) = 0;
    virtual void replay () {}
    virtual void reload () {}
    virtual guint64 playtime () const { return 0; }

    // a Source shall informs if the source failed (i.e. shall be deleted)
    typedef enum {
        FAIL_NONE = 0,
        FAIL_RETRY= 1,
        FAIL_CRITICAL = 2,
        FAIL_FATAL  = 3
    } Failure;
    virtual Failure failed () const = 0;

    // a Source shall define a way to get a texture
    virtual uint texture () const = 0;
    void setTextureMirrored (bool on);
    bool textureMirrored ();

    // a Source shall define how to render into the frame buffer
    virtual void render ();

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    // operations on mask
    // get shader used to render mask
    inline MaskShader *maskShader () const { return maskshader_; }
    // set/get framebuffer used for mask painting
    inline FrameBufferImage *getMask () const { return maskimage_; }
    void setMask (FrameBufferImage *img);
    void storeMask (FrameBufferImage *img = nullptr);
    // link to source used as mask
    inline  SourceLink *maskSource() const { return masksource_; }

    // get properties
    float depth () const;
    float alpha () const;
    bool visible() const;
    virtual bool texturePostProcessed () const;

    // groups for mixing
    MixingGroup *mixingGroup() const { return mixinggroup_; }
    void clearMixingGroup();

    // audio
    enum AudioFlags_ { Audio_none = 0, Audio_available = 1 << 1, Audio_enabled = 1 << 2 };
    typedef int AudioFlags;
    inline AudioFlags audioFlags() const { return audio_flags_; }
    void setAudioEnabled(bool on);
    typedef enum {
        VOLUME_BASE    = 0,
        VOLUME_ALPHA   = 1,
        VOLUME_OPACITY = 2,
        VOLUME_PARENT  = 3,
        VOLUME_SESSION = 4
    } AudioVolumeFactor;
    void setAudioVolumeFactor(AudioVolumeFactor index, float value);
    inline float audioVolumeFactor(AudioVolumeFactor index) const { return audio_volume_[index]; }
    enum AudioVolumeMixingFlags_ {
        Volume_mult_none    = 0,
        Volume_mult_alpha   = 1 << 1,
        Volume_mult_opacity = 1 << 2,
        Volume_mult_parent  = 1 << 3,
        Volume_mult_session = 1 << 4
    };
    typedef int AudioVolumeMixing;
    void setAudioVolumeMix(AudioVolumeMixing f);
    inline AudioVolumeMixing audioVolumeMix() const { return audio_volume_mix_; }
    // default implementation of audio support is empty
    virtual void updateAudio() {}

    // Source propertis querry
    struct hasNode
    {
        bool operator()(const Source* elem) const;
        hasNode(Node *n) : _n(n) { }
    private:
        Node *_n;
    };

    struct hasName
    {
        inline bool operator()(const Source* elem) const {
           return (elem && elem->name() == _n);
        }
        hasName(std::string n) : _n(n) { }
    private:
        std::string _n;
    };

    struct hasId
    {
        inline bool operator()(const Source* elem) const {
           return (elem && elem->id() == _id);
        }
        hasId(uint64_t id) : _id(id) { }
    private:
        uint64_t _id;
    };

    struct hasDepth
    {
        inline bool operator()(const Source* elem) const {
           return (elem && elem->depth()>_from && elem->depth()<_to );
        }
        hasDepth(float d1, float d2) {
            _from = MIN(d1, d2);
            _to   = MAX(d1, d2);
        }
    private:
        float _from;
        float _to;
    };

    static bool isCurrent (const Source* elem)  {
        return (elem && elem->mode_ == Source::CURRENT);
    }

    static bool isInitialized (const Source* elem)  {
        return (elem && ( elem->mode_ > Source::UNINITIALIZED || elem->failed() ) );
    }

    // class-dependent icon
    virtual glm::ivec2 icon () const { return glm::ivec2(ICON_SOURCE); }

    // class-dependent notification
    virtual std::string info () const { return "Undefined"; }

    // DRAFT & NOT USED
    SourceLink processingshader_link_;
    glm::vec2 attractor(size_t i) const;
    void setAttractor(size_t i, glm::vec2 a);

protected:
    // name
    std::string name_;
    char initials_[3];
    uint64_t id_;

    // every Source shall be initialized to be ready after first draw
    virtual void init() = 0;
    bool ready_;

    // render() fills in the renderbuffer at every frame
    // NB: rendershader_ is applied at render()
    FrameBuffer *renderbuffer_;
    void attach(FrameBuffer *renderbuffer);

    // the rendersurface draws the renderbuffer in the scene
    // It is associated to the rendershader for mixing effects
    FrameBufferMeshSurface *rendersurface_;

    // for the mixer, we have a surface with stippling to show
    // the rendering, and a preview of the original texture
    FrameBufferSurface *mixingsurface_;
    Surface *activesurface_;

    // blendingshader provides mixing controls
    ImageShader *blendingshader_;
    ImageShader *mixingshader_;

    // shader and buffer to draw mask
    MaskShader *maskshader_;
    FrameBuffer *maskbuffer_;
    Surface *masksurface_;
    FrameBufferImage *maskimage_;
    SourceLink *masksource_;

    // surface to draw on
    Surface *texturesurface_;

    // mode for display
    Mode mode_;

    // overlays and frames to be displayed on top of source
    std::map<View::Mode, Group*> overlays_;
    std::map<View::Mode, Switch*> frames_;
    std::map<View::Mode, Handles*[15]> handles_;
    Handles *lock_, *unlock_;
    Switch *locker_, *manipulator_, *blendmode_;
    Symbol *symbol_;
    Character  *initial_0_, *initial_1_;

    // update
    bool  active_;
    bool  locked_;
    UpdateFlags   need_update_;
    float dt_;
    Workspace  workspace_;

    // callbacks
    std::list<SourceCallback *> update_callbacks_;
    std::mutex access_callbacks_;
    void updateCallbacks(float dt);

    // clones
    CloneList clones_;

    // links
    SourceLinkList links_;

    // Mixing
    MixingGroup *mixinggroup_;
    Switch *overlay_mixinggroup_;
    Symbol *rotation_mixingroup_;

    // audio
    AudioFlags audio_flags_;
    float audio_volume_[5];
    AudioVolumeMixing audio_volume_mix_;
};




#endif // SOURCE_H
