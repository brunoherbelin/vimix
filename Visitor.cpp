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
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/virtrev/xstream.hpp>

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
    std::cerr << "Group" << std::endl;

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

void SessionVisitor::visit(Node &n)
{
    XMLElement *transform = xmlDoc_->NewElement("Transform");
    XMLText *matrix = xmlDoc_->NewText( glm::to_string(n.transform_).c_str() );
    transform->InsertEndChild(matrix);
    xmlCurrent_->InsertEndChild(transform);
}

void SessionVisitor::visit(Primitive &n)
{
    std::cerr << "Primitive" << std::endl;
    visit( (Node&) n);
}

void SessionVisitor::visit(TexturedRectangle &n)
{
    std::cerr << "TexturedRectangle" << std::endl;

}

void SessionVisitor::visit(MediaRectangle &n)
{
    std::cerr << "MediaRectangle" << std::endl;

    // type specific
    XMLElement *xmlParent = xmlCurrent_;
    xmlCurrent_ = xmlDoc_->NewElement("MediaRectangle");
    xmlCurrent_->SetAttribute("Filename", n.getMediaPath().c_str() );

    // inherited from Primitive
    visit( (Primitive&) n);

    // recursive
    xmlParent->InsertEndChild(xmlCurrent_);
    xmlCurrent_ = xmlParent;
}

void SessionVisitor::visit(LineStrip &n)
{
    std::cerr << "LineStrip" << std::endl;

}

void SessionVisitor::visit(Scene &n)
{
    std::cerr << "root" << std::endl;

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
