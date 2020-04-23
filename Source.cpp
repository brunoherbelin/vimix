
#include <algorithm>

#include "Source.h"

#include "FrameBuffer.h"
#include "ImageShader.h"
#include "Primitives.h"
#include "Mesh.h"
#include "MediaPlayer.h"
#include "SearchVisitor.h"


// gobal static list of all sources
SourceList Source::sources_;

Source::Source(std::string name) : name_("")
{
    // set a name
    rename(name);

    // create groups for each view

    // default rendering node
    groups_[View::RENDERING] = new Group;
    groups_[View::RENDERING]->scale_ = glm::vec3(5.f, 5.f, 1.f); // fit height full window

    // default mixing nodes
    groups_[View::MIXING] = new Group;
    Frame *frame = new Frame;
    frame->translation_.z = 0.1;
    groups_[View::MIXING]->addChild(frame);
    groups_[View::MIXING]->scale_ = glm::vec3(0.25f, 0.25f, 1.f);

    // add source to the list
    sources_.push_front(this);
}

Source::~Source()
{
    // delete groups and their children
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
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

MediaSource::MediaSource(std::string name, std::string uri) : Source(name)
{
    surface_ = new MediaSurface(uri);

    // add the surface to draw in the views
    groups_[View::RENDERING]->addChild(surface_);
    groups_[View::MIXING]->addChild(surface_);

}

MediaSource::~MediaSource()
{
    // TODO verify that surface_ node is deleted in Source destructor
}

Shader *MediaSource::shader() const
{
    return surface_->shader();
}

std::string MediaSource::uri() const
{
    return surface_->getUri();
}

MediaPlayer *MediaSource::mediaplayer() const
{
    return surface_->getMediaPlayer();
}

void MediaSource::render()
{
//    surface_->shader()

    // scalle all mixing nodes to match scale of surface
    for (NodeSet::iterator node = groups_[View::MIXING]->begin();
         node != groups_[View::MIXING]->end(); node++) {
        (*node)->scale_ = surface_->scale_;
    }

    // read position of the mixing node and interpret this as transparency change
    float alpha = 1.0 - ABS( groups_[View::MIXING]->translation_.x );
    surface_->shader()->color.a = alpha;

}
