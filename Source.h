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
class MediaSurface;

class Source;
// TODO : source set sorted by shader
// so that the render loop avoid switching
typedef std::list<Source *> SourceList;

class Source
{
public:
    // create a source and add it to the list
    // only subclasses of sources can actually be instanciated
    Source(std::string name);
    virtual ~Source();

    // manipulate name of source
    inline std::string name () const { return name_; }
    std::string rename (std::string newname);

    // get handle on the node used to manipulate the source in a view
    inline Group *group(View::Mode m) { return groups_[m]; }

    // every Source have a shader to control visual effects
    virtual Shader *shader() const = 0;

    // every Source shall be rendered before draw
    virtual void render() = 0;

    // global management of list of sources
    static SourceList::iterator begin();
    static SourceList::iterator end();
    static uint numSource();

protected:
    // name
    std::string name_;

    // nodes
    std::map<View::Mode, Group*> groups_;

    // static global list of sources
    static SourceList sources_;
};


struct hasName: public std::unary_function<Source*, bool>
{
    inline bool operator()(const Source* elem) const
    {
       return (elem && elem->name() == _n);
    }

    hasName(std::string n) : _n(n) { }

private:
    std::string _n;

};

class MediaSource : public Source
{
public:
    MediaSource(std::string name, std::string uri);
    ~MediaSource();

    void render();

    // Source interface
    Shader *shader() const;

    // Media specific interface
    std::string uri() const;
    MediaPlayer *mediaplayer() const;

protected:

    MediaSurface *surface_;
};


#endif // SOURCE_H
