#ifndef SOURCE_H
#define SOURCE_H

#include <string>
#include <list>

#include "FrameBuffer.h"
#include "ImageShader.h"
#include "Primitives.h"

class Source;
// TODO : source set sorted by shader
// so that the render loop avoid switching
typedef std::list<Source *> SourceList;

class Source
{
public:
    // create a source and add it to the list
    Source(std::string name);
    virtual ~Source();

    // manipulate name of source
    inline std::string name () const { return name_; }
    std::string rename (std::string newname);

    // void setCustomShader();
    inline Shader *shader() const { return shader_; }

    // for scene
    inline FrameBufferSurface *surface() const { return surface_; }

    // only subclasses of sources can actually be instanciated
    virtual void render() = 0;

    // global management of list of sources
    static SourceList::iterator begin();
    static SourceList::iterator end();
    static uint numSource();

protected:
    // name
    std::string name_;

    // a source draws in a frame buffer an input using a given shader
    FrameBuffer *buffer_;
    ImageShader *shader_;

    // a surface is used to draw in the scenes
    FrameBufferSurface *surface_;

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

#endif // SOURCE_H
