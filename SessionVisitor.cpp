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


SessionVisitor::SessionVisitor()
{    
    xmlDoc_ = new XMLDocument;

    xmlCurrent_ = nullptr;
    xmlRoot_ = nullptr;
}

void SessionVisitor::visit(Node &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("Node");
    newelement->SetAttribute("visible", n.visible_);

    XMLElement *scale = XMLElementFromGLM(xmlDoc_, n.scale_);
    newelement->InsertEndChild(scale);
    XMLElement *translation = XMLElementFromGLM(xmlDoc_, n.translation_);
    newelement->InsertEndChild(translation);
    XMLElement *rotation = XMLElementFromGLM(xmlDoc_, n.rotation_);
    newelement->InsertEndChild(rotation);

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
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        (*node)->accept(*this);
        // revert to group as current
        xmlCurrent_ = group;
    }
    n.visible_;
}

void SessionVisitor::visit(Switch &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "Switch");
    xmlCurrent_->SetAttribute("active", n.getIndexActiveChild());
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


void SessionVisitor::visit(ImageSurface &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "ImageSurface");

    XMLText *filename = xmlDoc_->NewText( n.getFilename().c_str() );
    XMLElement *image = xmlDoc_->NewElement("filename");
    image->InsertEndChild(filename);
    xmlCurrent_->InsertEndChild(image);
}

void SessionVisitor::visit(MediaSurface &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "MediaSurface");

    n.getMediaPlayer()->accept(*this);
}

void SessionVisitor::visit(MediaPlayer &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("MediaPlayer");
    newelement->SetAttribute("play", n.isPlaying());
    newelement->SetAttribute("loop", (int) n.loop());
    newelement->SetAttribute("speed", n.playSpeed());
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

void SessionVisitor::visit(LineSquare &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineSquare");

}

void SessionVisitor::visit(LineCircle &n)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineCircle");

    XMLElement *color = XMLElementFromGLM(xmlDoc_, n.getColor());
    color->SetAttribute("type", "RGBA");
    xmlCurrent_->InsertEndChild(color);
}


void SessionVisitor::visit(Scene &n)
{
    std::string s = "Capture time " + GstToolkit::date_time_string();
    XMLComment *pComment = xmlDoc_->NewComment(s.c_str());
    xmlDoc_->InsertEndChild(pComment);

    xmlRoot_ = xmlDoc_->NewElement("Scene");
    xmlDoc_->InsertEndChild(xmlRoot_);

    // start recursive traverse from root node
    xmlCurrent_ = xmlRoot_;
    n.getRoot()->accept(*this);
}

void SessionVisitor::save(std::string filename)
{
    XMLDeclaration *pDec = xmlDoc_->NewDeclaration();
    xmlDoc_->InsertFirstChild(pDec);

    std::string s = "Save time " + GstToolkit::date_time_string();
    XMLComment *pComment = xmlDoc_->NewComment(s.c_str());
    xmlDoc_->InsertEndChild(pComment);

    // save session
    XMLError eResult = xmlDoc_->SaveFile(filename.c_str());
    XMLCheckResult(eResult);
}
