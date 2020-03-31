#include "Visitor.h"
#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "MediaPlayer.h"
#include "GstToolkit.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <tinyxml2.h>
using namespace tinyxml2;

#ifndef XMLCheckResult
    #define XMLCheckResult(a_eResult) if (a_eResult != XML_SUCCESS) { Log::Warning("XML error %i\n", a_eResult); return; }
#endif

SessionVisitor::SessionVisitor(std::string filename) : filename_(filename)
{    
    xmlDoc_ = new XMLDocument;
    xmlCurrent_ = nullptr;
}

void SessionVisitor::visit(Group &n)
{
    XMLElement *xmlParent = xmlCurrent_;
    xmlCurrent_ = xmlDoc_->NewElement("Group");
    visit( (Node&) n);

    for (int node = 0; node < n.numChildren(); ++node)
    {
        Node *child = n.getChild(node);

        Group *g = dynamic_cast<Group *>(child);
        if (g != nullptr) {
            g->accept(*this);
            continue;
        }

        TexturedRectangle *tr = dynamic_cast<TexturedRectangle *>(child);
        if (tr != nullptr) {
            tr->accept(*this);
            continue;
        }

        MediaRectangle *mr = dynamic_cast<MediaRectangle *>(child);
        if (mr != nullptr) {
            mr->accept(*this);
            continue;
        }

        LineStrip *ls = dynamic_cast<LineStrip *>(child);
        if (ls != nullptr) {
            ls->accept(*this);
            continue;
        }

        Primitive *p = dynamic_cast<Primitive *>(child);
        if (p != nullptr) {
            p->accept(*this);
            continue;
        }

    }

    // recursive
    xmlParent->InsertEndChild(xmlCurrent_);
    xmlCurrent_ = xmlParent;

}


XMLElement *XMLElementGLM(XMLDocument *doc, glm::vec3 vector)
{
    XMLElement *newelement = doc->NewElement( "vec3" );
    newelement->SetAttribute("x", vector[0]);
    newelement->SetAttribute("y", vector[1]);
    newelement->SetAttribute("z", vector[2]);
    return newelement;
}

XMLElement *XMLElementGLM(XMLDocument *doc, glm::vec4 vector)
{
    XMLElement *newelement = doc->NewElement( "vec4" );
    newelement->SetAttribute("x", vector[0]);
    newelement->SetAttribute("y", vector[1]);
    newelement->SetAttribute("z", vector[2]);
    newelement->SetAttribute("w", vector[3]);
    return newelement;
}

XMLElement *XMLElementGLM(XMLDocument *doc, glm::mat4 matrix)
{
    XMLElement *newelement = doc->NewElement( "mat4" );
    for (int r = 0 ; r < 4 ; r ++)
    {
        glm::vec4 row = glm::row(matrix, r);
        XMLElement *rowxml = XMLElementGLM(doc, row);
        rowxml->SetAttribute("row", r);
        newelement->InsertEndChild(rowxml);
    }
    return newelement;
}

void SessionVisitor::visit(Node &n)
{
    XMLElement *transform = XMLElementGLM(xmlDoc_, n.transform_);
    xmlCurrent_->InsertEndChild(transform);
}

void SessionVisitor::visit(Primitive &n)
{
    visit( (Node&) n);
}

void SessionVisitor::visit(TexturedRectangle &n)
{
    // type specific
    XMLElement *xmlParent = xmlCurrent_;
    xmlCurrent_ = xmlDoc_->NewElement("TexturedRectangle");

    XMLElement *image = xmlDoc_->NewElement("filename");
    xmlCurrent_->InsertEndChild(image);
    XMLText *filename = xmlDoc_->NewText( n.getResourcePath().c_str() );
    image->InsertEndChild(filename);

    // inherited from Primitive
    visit( (Primitive&) n);

    // recursive
    xmlParent->InsertEndChild(xmlCurrent_);
    xmlCurrent_ = xmlParent;
}

void SessionVisitor::visit(MediaRectangle &n)
{
    // type specific
    XMLElement *xmlParent = xmlCurrent_;
    xmlCurrent_ = xmlDoc_->NewElement("MediaRectangle");

    XMLElement *media = xmlDoc_->NewElement("filename");
    xmlCurrent_->InsertEndChild(media);
    XMLText *filename = xmlDoc_->NewText( n.getMediaPath().c_str() );
    media->InsertEndChild(filename);

    // TODO : visit MediaPlayer

    // inherited from Primitive
    visit( (Primitive&) n);

    // recursive
    xmlParent->InsertEndChild(xmlCurrent_);
    xmlCurrent_ = xmlParent;
}

void SessionVisitor::visit(LineStrip &n)
{
    // type specific
    XMLElement *xmlParent = xmlCurrent_;
    xmlCurrent_ = xmlDoc_->NewElement("LineStrip");

    XMLElement *color = XMLElementGLM(xmlDoc_, n.getColor());
    color->SetAttribute("type", "RGBA");
    xmlCurrent_->InsertEndChild(color);

    std::vector<glm::vec3> points = n.getPoints();
    for(size_t i = 0; i < points.size(); ++i)
    {
        XMLElement *p = XMLElementGLM(xmlDoc_, points[i]);
        p->SetAttribute("point", (int) i);
        xmlCurrent_->InsertEndChild(p);
    }

    // inherited from Primitive
    visit( (Primitive&) n);

    // recursive
    xmlParent->InsertEndChild(xmlCurrent_);
    xmlCurrent_ = xmlParent;
}

void SessionVisitor::visit(Scene &n)
{
    XMLDeclaration *pDec = xmlDoc_->NewDeclaration();
    xmlDoc_->InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc_->NewElement("Session");
    xmlDoc_->InsertEndChild(pRoot);

    std::string s = "Saved on " + GstToolkit::date_time_string();
    XMLComment *pComment = xmlDoc_->NewComment(s.c_str());
    pRoot->InsertEndChild(pComment);

    // save scene
    xmlCurrent_ = pRoot;
    n.getRoot()->accept(*this);

    XMLError eResult = xmlDoc_->SaveFile(filename_.c_str());
    XMLCheckResult(eResult);
}
