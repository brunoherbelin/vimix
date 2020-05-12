#include "SessionVisitor.h"

#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "Source.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"

#include <iostream>

#include <tinyxml2.h>
using namespace tinyxml2;


SessionVisitor::SessionVisitor(tinyxml2::XMLDocument *doc,
                               tinyxml2::XMLElement *root,
                               bool recursive) : Visitor(), xmlCurrent_(root), recursive_(recursive)
{    
    if (doc == nullptr)
        xmlDoc_ = new XMLDocument;
    else
        xmlDoc_ = doc;
}

tinyxml2::XMLElement *SessionVisitor::NodeToXML(Node &n, tinyxml2::XMLDocument *doc)
{
    XMLElement *newelement = doc->NewElement("Node");
    newelement->SetAttribute("visible", n.visible_);

    XMLElement *scale = doc->NewElement("scale");
    scale->InsertEndChild( XMLElementFromGLM(doc, n.scale_) );
    newelement->InsertEndChild(scale);

    XMLElement *translation = doc->NewElement("translation");
    translation->InsertEndChild( XMLElementFromGLM(doc, n.translation_) );
    newelement->InsertEndChild(translation);

    XMLElement *rotation = doc->NewElement("rotation");
    rotation->InsertEndChild( XMLElementFromGLM(doc, n.rotation_) );
    newelement->InsertEndChild(rotation);

    return newelement;
}

void SessionVisitor::visit(Node &n)
{
    XMLElement *newelement = NodeToXML(n, xmlDoc_);

    // insert into hierarchy
    xmlCurrent_->InsertEndChild(newelement);

    // parent for next visits
    xmlCurrent_ = newelement;
}

void SessionVisitor::visit(Group &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Group");

    if (recursive_) {
        // loop over members of a group
        XMLElement *group = xmlCurrent_;
        for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
            (*node)->accept(*this);
            // revert to group as current
            xmlCurrent_ = group;
        }
    }
}

void SessionVisitor::visit(Switch &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Switch");
    xmlCurrent_->SetAttribute("active", n.getIndexActiveChild());

    if (recursive_) {
        // loop over members of the group
        XMLElement *group = xmlCurrent_;
        for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
            (*node)->accept(*this);
            // revert to group as current
            xmlCurrent_ = group;
        }
    }
}

void SessionVisitor::visit(Animation &n)
{
    // Group of a different type
    xmlCurrent_->SetAttribute("type", "Animation");

    XMLElement *anim = xmlDoc_->NewElement("Movement");
    anim->SetAttribute("speed", n.speed_);
    anim->SetAttribute("radius", n.radius_);
    XMLElement *axis = XMLElementFromGLM(xmlDoc_, n.axis_);
    anim->InsertEndChild(axis);
    xmlCurrent_->InsertEndChild(anim);
}

void SessionVisitor::visit(Primitive &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Primitive");

    if (recursive_) {
        // go over members of a primitive
        XMLElement *Primitive = xmlCurrent_;

        xmlCurrent_ = xmlDoc_->NewElement("Shader");
        n.shader()->accept(*this);
        Primitive->InsertEndChild(xmlCurrent_);

        // revert to primitive as current
        xmlCurrent_ = Primitive;
    }
}


void SessionVisitor::visit(Surface &n)
{

}

void SessionVisitor::visit(ImageSurface &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "ImageSurface");

    XMLText *filename = xmlDoc_->NewText( n.resource().c_str() );
    XMLElement *image = xmlDoc_->NewElement("resource");
    image->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(image);
}

void SessionVisitor::visit(FrameBufferSurface &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "FrameBufferSurface");
}

void SessionVisitor::visit(MediaSurface &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "MediaSurface");

    n.mediaPlayer()->accept(*this);
}

void SessionVisitor::visit(MediaPlayer &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("MediaPlayer");
    newelement->SetAttribute("play", n.isPlaying());
    newelement->SetAttribute("loop", (int) n.loop());
    newelement->SetAttribute("speed", n.playSpeed());

 // TODO Segments

    xmlCurrent_->InsertEndChild(newelement);
}

void SessionVisitor::visit(Shader &n)
{
    // Shader of a simple type
    xmlCurrent_->SetAttribute("type", "Shader");

    XMLElement *color = xmlDoc_->NewElement("color");
    color->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.color) );
    xmlCurrent_->InsertEndChild(color);

    XMLElement *blend = xmlDoc_->NewElement("blending");
    blend->SetAttribute("mode", (int) n.blending);
    xmlCurrent_->InsertEndChild(blend);

}

void SessionVisitor::visit(ImageShader &n)
{
    // Shader of a textured type
    xmlCurrent_->SetAttribute("type", "ImageShader");

    XMLElement *uniforms = xmlDoc_->NewElement("uniforms");
    uniforms->SetAttribute("stipple", n.stipple);
    uniforms->SetAttribute("mask", n.mask);
    xmlCurrent_->InsertEndChild(uniforms);

}

void SessionVisitor::visit(ImageProcessingShader &n)
{
    // Shader of a textured type
    xmlCurrent_->SetAttribute("type", "ImageProcessingShader");

    XMLElement *filter = xmlDoc_->NewElement("uniforms");
    filter->SetAttribute("brightness", n.brightness);
    filter->SetAttribute("contrast", n.contrast);
    filter->SetAttribute("saturation", n.saturation);
    filter->SetAttribute("hueshift", n.hueshift);
    filter->SetAttribute("threshold", n.threshold);
    filter->SetAttribute("lumakey", n.lumakey);
    filter->SetAttribute("nbColors", n.nbColors);
    filter->SetAttribute("invert", n.invert);
    filter->SetAttribute("chromadelta", n.chromadelta);
    filter->SetAttribute("filterid", n.filterid);
    xmlCurrent_->InsertEndChild(filter);

    XMLElement *gamma = xmlDoc_->NewElement("gamma");
    gamma->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.gamma) );
    xmlCurrent_->InsertEndChild(gamma);

    XMLElement *levels = xmlDoc_->NewElement("levels");
    levels->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.levels) );
    xmlCurrent_->InsertEndChild(levels);

    XMLElement *chromakey = xmlDoc_->NewElement("chromakey");
    chromakey->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.chromakey) );
    xmlCurrent_->InsertEndChild(chromakey);

}

void SessionVisitor::visit(LineStrip &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineStrip");

    XMLElement *color = xmlDoc_->NewElement("color");
    color->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.getColor()) );
    xmlCurrent_->InsertEndChild(color);

    std::vector<glm::vec3> points = n.getPoints();
    for(size_t i = 0; i < points.size(); ++i)
    {
        XMLElement *p = XMLElementFromGLM(xmlDoc_, points[i]);
        p->SetAttribute("index", (int) i);
        xmlCurrent_->InsertEndChild(p);
    }
}

void SessionVisitor::visit(LineSquare &)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineSquare");

}

void SessionVisitor::visit(LineCircle &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineCircle");

    XMLElement *color = xmlDoc_->NewElement("color");
    color->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.getColor()) );
    xmlCurrent_->InsertEndChild(color);
}

void SessionVisitor::visit(Mesh &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Mesh");

    XMLText *filename = xmlDoc_->NewText( n.meshPath().c_str() );
    XMLElement *obj = xmlDoc_->NewElement("resource");
    obj->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(obj);

    filename = xmlDoc_->NewText( n.texturePath().c_str() );
    XMLElement *tex = xmlDoc_->NewElement("texture");
    tex->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(tex);
}

void SessionVisitor::visit(Frame &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Frame");

    XMLElement *color = xmlDoc_->NewElement("color");
    color->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.color) );
    xmlCurrent_->InsertEndChild(color);

}

void SessionVisitor::visit(Scene &n)
{
    XMLElement *xmlRoot = xmlDoc_->NewElement("Scene");
    xmlDoc_->InsertEndChild(xmlRoot);

    // start recursive traverse from root node
    recursive_ = true;
    xmlCurrent_ = xmlRoot;
    n.root()->accept(*this);
}

void SessionVisitor::visit (Source& s)
{
    XMLElement *sourceNode = xmlDoc_->NewElement( "Source" );
    sourceNode->SetAttribute("name", s.name().c_str() );

    // insert into hierarchy
    xmlCurrent_->InsertFirstChild(sourceNode);

    xmlCurrent_ = xmlDoc_->NewElement( "Mixing" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.node(View::MIXING)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Geometry" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.node(View::GEOMETRY)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Blending" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.blendingShader()->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "ImageProcessing" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.processingShader()->accept(*this);

    xmlCurrent_ = sourceNode;  // parent for next visits (other subtypes of Source)
}

void SessionVisitor::visit (MediaSource& s)
{
    xmlCurrent_->SetAttribute("type", "MediaSource");

    XMLElement *uri = xmlDoc_->NewElement("uri");
    xmlCurrent_->InsertEndChild(uri);
    XMLText *text = xmlDoc_->NewText( s.path().c_str() );
    uri->InsertEndChild( text );

    s.mediaplayer()->accept(*this);
}
