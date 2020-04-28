
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "Source.h"

#include "defines.h"
#include "FrameBuffer.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Mesh.h"
#include "MediaPlayer.h"
#include "SearchVisitor.h"


// gobal static list of all sources
SourceList Source::sources_;

Source::Source(std::string name) : name_(""), initialized_(false)
{
    // set a name
    rename(name);

    // create groups for each view
    // default rendering node
    groups_[View::RENDERING] = new Group;

    // default mixing nodes
    groups_[View::MIXING] = new Group;
    Frame *frame = new Frame(Frame::ROUND_THIN);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( 0.8f, 0.8f, 0.0f, 0.9f);
    groups_[View::MIXING]->addChild(frame);
    groups_[View::MIXING]->scale_ = glm::vec3(0.15f, 0.15f, 1.f);

    // default geometry nodes
    groups_[View::GEOMETRY] = new Group;

    // will be associated to nodes later
    mixingshader_ = new ImageShader();
    rendershader_ = new ImageProcessingShader();
    renderbuffer_ = nullptr;
    rendersurface_ = nullptr;

    // add source to the list
    sources_.push_front(this);
}

Source::~Source()
{
    // delete render objects
    delete rendershader_;
    if (renderbuffer_)
        delete renderbuffer_;

    // delete groups and their children
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
    delete groups_[View::GEOMETRY];
    groups_.clear();

    // remove this source from the list
    sources_.remove(this);
}

bool hasNode::operator()(const Source* elem) const
{
    if (elem)
    {
        SearchVisitor sv(_n);
        elem->group(View::MIXING)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::GEOMETRY)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::RENDERING)->accept(sv);
        return sv.found();
    }

    return false;
}

std::string Source::rename (std::string newname)
{
    // tentative new name
    std::string tentativename = newname;

    // refuse to rename to an empty name
    if ( newname.empty() )
        tentativename = "source";

    // trivial case : same name as current
    if ( tentativename == name_ )
        return name_;

    // search for a source of the name 'tentativename'
    std::string basename = tentativename;
    int count = 1;
    while ( std::find_if(sources_.begin(), sources_.end(), hasName(tentativename)) != sources_.end() ) {
        tentativename = basename + std::to_string(++count);
    }

    // ok to rename
    name_ = tentativename;
    return name_;
}

SourceList::iterator Source::begin()
{
    return sources_.begin();
}

SourceList::iterator Source::end()
{
    return sources_.end();
}

SourceList::iterator Source::find(Source *s)
{
    return std::find(sources_.begin(), sources_.end(), s);
}

SourceList::iterator Source::find(std::string namesource)
{
    return std::find_if(Source::begin(), Source::end(), hasName(namesource));
}

SourceList::iterator Source::find(Node *node)
{
    return std::find_if(Source::begin(), Source::end(), hasNode(node));
}

uint Source::numSource()
{
    return sources_.size();
}

MediaSource::MediaSource(std::string name, std::string uri) : Source(name), uri_(uri)
{
    // create media player
    mediaplayer_ = new MediaPlayer;
    mediaplayer_->open(uri_);
    mediaplayer_->play(true);

    // create media surface:
    // - textured with original texture from media player
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    mediasurface_ = new Surface(rendershader_);

    // extra overlay for mixing view
    mixingoverlay_ = new Frame(Frame::ROUND_LARGE);
    mixingoverlay_->overlay_ = new Mesh("mesh/icon_video.ply");
    mixingoverlay_->translation_.z = 0.1;
    mixingoverlay_->color = glm::vec4( 0.8f, 0.8f, 0.0f, 1.f);
    mixingoverlay_->visible_ = false;
    groups_[View::MIXING]->addChild(mixingoverlay_);

}

MediaSource::~MediaSource()
{
    delete mediasurface_;
    delete mediaplayer_;
    // TODO verify that all surfaces and node is deleted in Source destructor
}

std::string MediaSource::uri() const
{
    return uri_;
}

MediaPlayer *MediaSource::mediaplayer() const
{
    return mediaplayer_;
}

void MediaSource::init()
{
    if ( mediaplayer_->isOpen() ) {

        // update video
        mediaplayer_->update();

        // once the texture of media player is created
        if (mediaplayer_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            mediasurface_->setTextureIndex( mediaplayer_->texture() );

            // create Frame buffer matching size of media player
            renderbuffer_ = new FrameBuffer(mediaplayer()->width(), mediaplayer()->height());

//            // setup shader resolution for texture 0
//            rendershader_->iChannelResolution[0] = glm::vec3(mediaplayer()->width(), mediaplayer()->height(), 0.f);

            // create the surfaces to draw the frame buffer in the views
            // TODO Provide the source specific effect shader
            rendersurface_ = new FrameBufferSurface(renderbuffer_, mixingshader_);
            groups_[View::RENDERING]->addChild(rendersurface_);
            groups_[View::GEOMETRY]->addChild(rendersurface_);
            groups_[View::MIXING]->addChild(rendersurface_);

            // for mixing view, add another surface to overlay (for stippled view in transparency)
            Surface *surfacemix = new FrameBufferSurface(renderbuffer_);
            ImageShader *is = static_cast<ImageShader *>(surfacemix->shader());
            if (is)  is->stipple = 1.0;
            groups_[View::MIXING]->addChild(surfacemix);

            // scale all mixing nodes to match aspect ratio of the media
            for (NodeSet::iterator node = groups_[View::MIXING]->begin();
                 node != groups_[View::MIXING]->end(); node++) {
                (*node)->scale_.x = mediaplayer_->aspectRatio();
            }

//            mixingshader_->mask = ImageShader::mask_presets["Halo"];

            // done init once and for all
            initialized_ = true;
        }
    }

}

void MediaSource::render(bool current)
{
    if (!initialized_)
        init();
    else {
        // update video
        mediaplayer_->update();

        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        mediasurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();

        // read position of the mixing node and interpret this as transparency of render output
        float alpha = 1.0 - CLAMP( SQUARE( glm::length(groups_[View::MIXING]->translation_) ), 0.f, 1.f );
        mixingshader_->color.a = alpha;

        // TODO modify geometry
        groups_[View::RENDERING]->translation_ = groups_[View::GEOMETRY]->translation_;

        // make Overlay visible if it is current source
        mixingoverlay_->visible_ = current;
    }
}

FrameBuffer *MediaSource::frame() const
{
    if (initialized_ && renderbuffer_)
    {
        return renderbuffer_;
    }
    else {
        static FrameBuffer *black = new FrameBuffer(640,480);
        return black;
    }

}
