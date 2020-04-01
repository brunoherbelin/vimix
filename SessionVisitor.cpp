#include "SessionVisitor.h"

#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "MediaPlayer.h"
#include "GstToolkit.h"

#include <iostream>

#include <tinyxml2.h>
using namespace tinyxml2;


SessionVisitor::SessionVisitor(std::string filename) : filename_(filename)
{    
    xmlDoc_ = new XMLDocument;
    xmlCurrent_ = nullptr;
}

void SessionVisitor::visit(Node &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("Node");
//    xmlCurrent_->SetAttribute("type", "Undefined");

    XMLElement *transform = XMLElementFromGLM(xmlDoc_, n.transform_);
    newelement->InsertEndChild(transform);

    // insert into hierarchy
    xmlCurrent_->InsertEndChild(newelement);
    xmlCurrent_ = newelement;  // parent for next visits
}

void SessionVisitor::visit(Group &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Group");

    // loop over members of a group
    XMLElement *group = xmlCurrent_;
    for (int i = 0; i < n.numChildren(); ++i)
    {
        // recursive
        n.getChild(i)->accept(*this);
        xmlCurrent_->SetAttribute("index", i);
        // revert to group as current
        xmlCurrent_ = group;
    }
}

void SessionVisitor::visit(Switch &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Switch");
    xmlCurrent_->SetAttribute("active", n.activeIndex());
}

void SessionVisitor::visit(Primitive &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Primitive");

    // go over members of a primitive
    XMLElement *Primitive = xmlCurrent_;

    xmlCurrent_ = xmlDoc_->NewElement("Shader");
    n.getShader()->accept(*this);
    Primitive->InsertEndChild(xmlCurrent_);

    // revert to primitive as current
    xmlCurrent_ = Primitive;
}


void SessionVisitor::visit(TexturedRectangle &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "TexturedRectangle");

    XMLText *filename = xmlDoc_->NewText( n.getResourcePath().c_str() );
    XMLElement *image = xmlDoc_->NewElement("filename");
    image->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(image);
}

void SessionVisitor::visit(MediaRectangle &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "MediaRectangle");

    XMLText *filename = xmlDoc_->NewText( n.getMediaPath().c_str() );
    XMLElement *media = xmlDoc_->NewElement("filename");
    media->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(media);

    n.getMediaPlayer()->accept(*this);

}

void SessionVisitor::visit(MediaPlayer &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("MediaPlayer");
    newelement->SetAttribute("play", n.isPlaying());
    newelement->SetAttribute("loop", (int) n.Loop());
    newelement->SetAttribute("speed", n.PlaySpeed());
    xmlCurrent_->InsertEndChild(newelement);
}

void SessionVisitor::visit(Shader &n)
{
    // Shader of a simple type
    xmlCurrent_->SetAttribute("type", "DefaultShader");

    XMLElement *color = XMLElementFromGLM(xmlDoc_, n.color);
    color->SetAttribute("type", "RGBA");
    xmlCurrent_->InsertEndChild(color);

    XMLElement *blend = xmlDoc_->NewElement("blending");
    blend->SetAttribute("mode", (int) n.blending);
    xmlCurrent_->InsertEndChild(blend);

}

void SessionVisitor::visit(ImageShader &n)
{
    // Shader of a textured type
    xmlCurrent_->SetAttribute("type", "ImageShader");

    XMLElement *filter = xmlDoc_->NewElement("filter");
    filter->SetAttribute("brightness", n.brightness);
    filter->SetAttribute("contrast", n.contrast);
    xmlCurrent_->InsertEndChild(filter);

}

void SessionVisitor::visit(LineStrip &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineStrip");

    XMLElement *color = XMLElementFromGLM(xmlDoc_, n.getColor());
    color->SetAttribute("type", "RGBA");
    xmlCurrent_->InsertEndChild(color);

    std::vector<glm::vec3> points = n.getPoints();
    for(size_t i = 0; i < points.size(); ++i)
    {
        XMLElement *p = XMLElementFromGLM(xmlDoc_, points[i]);
        p->SetAttribute("index", (int) i);
        xmlCurrent_->InsertEndChild(p);
    }
}

void SessionVisitor::visit(Scene &n)
{
    XMLDeclaration *pDec = xmlDoc_->NewDeclaration();
    xmlDoc_->InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc_->NewElement("Scene");
    xmlDoc_->InsertEndChild(pRoot);

    std::string s = "Saved on " + GstToolkit::date_time_string();
    XMLComment *pComment = xmlDoc_->NewComment(s.c_str());
    pRoot->InsertEndChild(pComment);

    // start recursive traverse from root node
    xmlCurrent_ = pRoot;
    n.getRoot()->accept(*this);

    // save scene
    XMLError eResult = xmlDoc_->SaveFile(filename_.c_str());
    XMLCheckResult(eResult);
}
