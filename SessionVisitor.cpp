#include "SessionVisitor.h"

#include "Log.h"
#include "defines.h"
#include "Scene.h"
#include "Decorations.h"
#include "Source.h"
#include "MediaSource.h"
#include "Session.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "MultiFileSource.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "MixingGroup.h"
#include "SystemToolkit.h"
#include "ActionManager.h"

#include <iostream>
#include <locale>

#include <tinyxml2.h>
using namespace tinyxml2;


bool SessionVisitor::saveSession(const std::string& filename, Session *session)
{
    // impose C locale
    setlocale(LC_ALL, "C");

    // creation of XML doc
    XMLDocument xmlDoc;

    XMLElement *rootnode = xmlDoc.NewElement(APP_NAME);
    rootnode->SetAttribute("major", XML_VERSION_MAJOR);
    rootnode->SetAttribute("minor", XML_VERSION_MINOR);
    rootnode->SetAttribute("size", session->numSource());
    rootnode->SetAttribute("date", SystemToolkit::date_time_string().c_str());
    rootnode->SetAttribute("resolution", session->frame()->info().c_str());
    xmlDoc.InsertEndChild(rootnode);

    // 1. list of sources
    XMLElement *sessionNode = xmlDoc.NewElement("Session");
    xmlDoc.InsertEndChild(sessionNode);
    SessionVisitor sv(&xmlDoc, sessionNode);
    sv.sessionFilePath_ = SystemToolkit::path_filename(filename);
    for (auto iter = session->begin(); iter != session->end(); ++iter, sv.setRoot(sessionNode) )
        // source visitor
        (*iter)->accept(sv);

    // get the thumbnail
    FrameBufferImage *thumbnail = session->thumbnail();
    XMLElement *imageelement = SessionVisitor::ImageToXML(thumbnail, &xmlDoc);
    if (imageelement)
        sessionNode->InsertEndChild(imageelement);
    delete thumbnail;

    // 2. config of views
    saveConfig( &xmlDoc, session );

    // 3. snapshots
    saveSnapshots( &xmlDoc, session );

    // 4. optional notes
    saveNotes( &xmlDoc, session );

    // 5. optional playlists
    savePlayGroups( &xmlDoc, session );

    // save file to disk
    return ( XMLSaveDoc(&xmlDoc, filename) );
}

void SessionVisitor::saveConfig(tinyxml2::XMLDocument *doc, Session *session)
{
    if (doc != nullptr && session != nullptr)
    {
        XMLElement *views = doc->NewElement("Views");

        XMLElement *mixing = doc->NewElement( "Mixing" );
        mixing->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::MIXING), doc));
        views->InsertEndChild(mixing);

        XMLElement *geometry = doc->NewElement( "Geometry" );
        geometry->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::GEOMETRY), doc));
        views->InsertEndChild(geometry);

        XMLElement *layer = doc->NewElement( "Layer" );
        layer->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::LAYER), doc));
        views->InsertEndChild(layer);

        XMLElement *appearance = doc->NewElement( "Texture" );
        appearance->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::TEXTURE), doc));
        views->InsertEndChild(appearance);

        XMLElement *render = doc->NewElement( "Rendering" );
        render->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::RENDERING), doc));
        views->InsertEndChild(render);

        doc->InsertEndChild(views);
    }
}


void SessionVisitor::saveSnapshots(tinyxml2::XMLDocument *doc, Session *session)
{
    if (doc != nullptr && session != nullptr)
    {
        XMLElement *snapshots = doc->NewElement("Snapshots");
        const XMLElement* N = session->snapshots()->xmlDoc_->FirstChildElement();
        for( ; N ; N=N->NextSiblingElement())
            snapshots->InsertEndChild( N->DeepClone( doc ));
        doc->InsertEndChild(snapshots);
    }
}

void SessionVisitor::saveNotes(tinyxml2::XMLDocument *doc, Session *session)
{
    if (doc != nullptr && session != nullptr)
    {
        XMLElement *notes = doc->NewElement("Notes");
        for (auto nit = session->beginNotes(); nit != session->endNotes(); ++nit) {
            XMLElement *note = doc->NewElement( "Note" );
            note->SetAttribute("large", (*nit).large );
            note->SetAttribute("stick", (*nit).stick );
            XMLElement *pos = doc->NewElement("pos");
            pos->InsertEndChild( XMLElementFromGLM(doc, (*nit).pos) );
            note->InsertEndChild(pos);
            XMLElement *size = doc->NewElement("size");
            size->InsertEndChild( XMLElementFromGLM(doc, (*nit).size) );
            note->InsertEndChild(size);
            XMLElement *content = doc->NewElement("text");
            XMLText *text = doc->NewText( (*nit).text.c_str() );
            content->InsertEndChild( text );
            note->InsertEndChild(content);

            notes->InsertEndChild(note);
        }
        doc->InsertEndChild(notes);
    }
}

void SessionVisitor::savePlayGroups(tinyxml2::XMLDocument *doc, Session *session)
{
    if (doc != nullptr && session != nullptr)
    {
        XMLElement *playlistNode = doc->NewElement("PlayGroups");
        std::vector<SourceIdList> pl = session->getPlayGroups();
        for (auto plit = pl.begin(); plit != pl.end(); ++plit) {
            XMLElement *list = doc->NewElement("PlayGroup");
            playlistNode->InsertEndChild(list);
            for (auto id = plit->begin(); id != plit->end(); ++id) {
                XMLElement *sour = doc->NewElement("source");
                sour->SetAttribute("id", *id);
                list->InsertEndChild(sour);
            }
        }
        doc->InsertEndChild(playlistNode);
    }
}

SessionVisitor::SessionVisitor(tinyxml2::XMLDocument *doc,
                               tinyxml2::XMLElement *root,
                               bool recursive) : Visitor(), recursive_(recursive), xmlCurrent_(root)
{    
    // impose C locale
    setlocale(LC_ALL, "C");

    if (doc == nullptr)
        xmlDoc_ = new XMLDocument;
    else
        xmlDoc_ = doc;
}

XMLElement *SessionVisitor::NodeToXML(const Node &n, XMLDocument *doc)
{
    XMLElement *newelement = doc->NewElement("Node");
    newelement->SetAttribute("visible", n.visible_);
    newelement->SetAttribute("id", n.id());

    XMLElement *scale = doc->NewElement("scale");
    scale->InsertEndChild( XMLElementFromGLM(doc, n.scale_) );
    newelement->InsertEndChild(scale);

    XMLElement *translation = doc->NewElement("translation");
    translation->InsertEndChild( XMLElementFromGLM(doc, n.translation_) );
    newelement->InsertEndChild(translation);

    XMLElement *rotation = doc->NewElement("rotation");
    rotation->InsertEndChild( XMLElementFromGLM(doc, n.rotation_) );
    newelement->InsertEndChild(rotation);

    XMLElement *crop = doc->NewElement("crop");
    crop->InsertEndChild( XMLElementFromGLM(doc, n.crop_) );
    newelement->InsertEndChild(crop);

    return newelement;
}


XMLElement *SessionVisitor::ImageToXML(const FrameBufferImage *img, XMLDocument *doc)
{
    XMLElement *imageelement = nullptr;
    if (img != nullptr) {
        // get the jpeg encoded buffer
        FrameBufferImage::jpegBuffer jpgimg = img->getJpeg();
        if (jpgimg.buffer != nullptr) {
            // fill the xml array with jpeg buffer
            XMLElement *array = XMLElementEncodeArray(doc, jpgimg.buffer, jpgimg.len);
            // free the buffer
            free(jpgimg.buffer);
            // if we could create the array
            if (array) {
                // create an Image node to store the mask image
                imageelement = doc->NewElement("Image");
                imageelement->SetAttribute("width", img->width);
                imageelement->SetAttribute("height", img->height);
                imageelement->InsertEndChild(array);
            }
        }
    }
    return imageelement;
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
        for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
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
    xmlCurrent_->SetAttribute("active", n.active());

    if (recursive_) {
        // loop over members of the group
        XMLElement *group = xmlCurrent_;
        for(uint i = 0; i < n.numChildren(); ++i) {
            n.child(i)->accept(*this);
            // revert to group as current
            xmlCurrent_ = group;
        }
    }
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


void SessionVisitor::visit(Surface &)
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

void SessionVisitor::visit(FrameBufferSurface &)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "FrameBufferSurface");
}

void SessionVisitor::visit(MediaPlayer &n)
{
    XMLElement *newelement = xmlDoc_->NewElement("MediaPlayer");
    newelement->SetAttribute("id", n.id());

    if (!n.isImage()) {
        newelement->SetAttribute("play", n.isPlaying());
        newelement->SetAttribute("loop", (int) n.loop());
        newelement->SetAttribute("speed", n.playSpeed());
        newelement->SetAttribute("software_decoding", n.softwareDecodingForced());

        // timeline
        XMLElement *timelineelement = xmlDoc_->NewElement("Timeline");

        // gaps in timeline
        XMLElement *gapselement = xmlDoc_->NewElement("Gaps");
        TimeIntervalSet gaps = n.timeline()->gaps();
        for( auto it = gaps.begin(); it!= gaps.end(); ++it) {
            XMLElement *g = xmlDoc_->NewElement("Interval");
            g->SetAttribute("begin", (*it).begin);
            g->SetAttribute("end", (*it).end);
            gapselement->InsertEndChild(g);
        }
        timelineelement->InsertEndChild(gapselement);

        // fading in timeline
        XMLElement *fadingelement = xmlDoc_->NewElement("Fading");
        XMLElement *array = XMLElementEncodeArray(xmlDoc_, n.timeline()->fadingArray(), MAX_TIMELINE_ARRAY * sizeof(float));
        fadingelement->InsertEndChild(array);
        timelineelement->InsertEndChild(fadingelement);
        newelement->InsertEndChild(timelineelement);
    }

    xmlCurrent_->InsertEndChild(newelement);
}

void SessionVisitor::visit(Shader &n)
{
    // Shader of a simple type
    xmlCurrent_->SetAttribute("type", "Shader");
    xmlCurrent_->SetAttribute("id", n.id());

    XMLElement *color = xmlDoc_->NewElement("color");
    color->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.color) );
    xmlCurrent_->InsertEndChild(color);

    XMLElement *blend = xmlDoc_->NewElement("blending");
    blend->SetAttribute("mode", int(n.blending) );
    xmlCurrent_->InsertEndChild(blend);

}

void SessionVisitor::visit(ImageShader &n)
{
    // Shader of a textured type
    xmlCurrent_->SetAttribute("type", "ImageShader");
    xmlCurrent_->SetAttribute("id", n.id());

    XMLElement *uniforms = xmlDoc_->NewElement("uniforms");
    uniforms->SetAttribute("stipple", n.stipple);
    xmlCurrent_->InsertEndChild(uniforms);
}

void SessionVisitor::visit(MaskShader &n)
{
    // Shader of a mask type
    xmlCurrent_->SetAttribute("type", "MaskShader");
    xmlCurrent_->SetAttribute("id", n.id());
    xmlCurrent_->SetAttribute("mode", n.mode);
    xmlCurrent_->SetAttribute("shape", n.shape);

    XMLElement *uniforms = xmlDoc_->NewElement("uniforms");
    uniforms->SetAttribute("blur", n.blur);
    uniforms->SetAttribute("option", n.option);
    XMLElement *size = xmlDoc_->NewElement("size");
    size->InsertEndChild( XMLElementFromGLM(xmlDoc_, n.size) );
    uniforms->InsertEndChild(size);
    xmlCurrent_->InsertEndChild(uniforms);
}

void SessionVisitor::visit(ImageProcessingShader &n)
{
    // Shader of a textured type
    xmlCurrent_->SetAttribute("type", "ImageProcessingShader");
    xmlCurrent_->SetAttribute("id", n.id());

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
    filter->SetAttribute("filter", n.filterid);
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

    XMLElement *points_node = xmlDoc_->NewElement("points");
    std::vector<glm::vec2> path = n.path();
    for(size_t i = 0; i < path.size(); ++i)
    {
        XMLElement *p = XMLElementFromGLM(xmlDoc_, path[i]);
        p->SetAttribute("index", (int) i);
        points_node->InsertEndChild(p);
    }
    xmlCurrent_->InsertEndChild(points_node);
}

void SessionVisitor::visit(LineSquare &)
{
    // Node of a different type
    xmlCurrent_->SetAttribute("type", "LineSquare");

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
    sourceNode->SetAttribute("id", s.id());
    sourceNode->SetAttribute("name", s.name().c_str() );
    sourceNode->SetAttribute("locked", s.locked() );

    // insert into hierarchy
    xmlCurrent_->InsertFirstChild(sourceNode);

    xmlCurrent_ = xmlDoc_->NewElement( "Mixing" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.groupNode(View::MIXING)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Geometry" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.groupNode(View::GEOMETRY)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Layer" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.groupNode(View::LAYER)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Texture" );
    xmlCurrent_->SetAttribute("mirrored", s.textureMirrored() );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.groupNode(View::TEXTURE)->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Blending" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.blendingShader()->accept(*this);

    xmlCurrent_ = xmlDoc_->NewElement( "Mask" );
    sourceNode->InsertEndChild(xmlCurrent_);
    s.maskShader()->accept(*this);
    // if we are saving a pain mask
    if (s.maskShader()->mode == MaskShader::PAINT) {
        // get the mask previously stored
        XMLElement *imageelement = SessionVisitor::ImageToXML(s.getMask(), xmlDoc_);
        if (imageelement)
            xmlCurrent_->InsertEndChild(imageelement);
    }

    xmlCurrent_ = xmlDoc_->NewElement( "ImageProcessing" );
    xmlCurrent_->SetAttribute("enabled", s.imageProcessingEnabled());
    xmlCurrent_->SetAttribute("follow", s.processingshader_link_.id());
    sourceNode->InsertEndChild(xmlCurrent_);
    s.processingShader()->accept(*this);

    if (s.mixingGroup()) {
        xmlCurrent_ = xmlDoc_->NewElement( "MixingGroup" );
        sourceNode->InsertEndChild(xmlCurrent_);
        s.mixingGroup()->accept(*this);
    }

    xmlCurrent_ = sourceNode;  // parent for next visits (other subtypes of Source)
}

void SessionVisitor::visit (MediaSource& s)
{
    xmlCurrent_->SetAttribute("type", "MediaSource");

    XMLElement *uri = xmlDoc_->NewElement("uri");
    xmlCurrent_->InsertEndChild(uri);
    XMLText *text = xmlDoc_->NewText( s.path().c_str() );
    uri->InsertEndChild( text );

    if (!sessionFilePath_.empty())
        uri->SetAttribute("relative", SystemToolkit::path_relative_to_path(s.path(), sessionFilePath_).c_str());

    s.mediaplayer()->accept(*this);
}

void SessionVisitor::visit (SessionFileSource& s)
{
    xmlCurrent_->SetAttribute("type", "SessionSource");
    if (s.session() != nullptr)
        xmlCurrent_->SetAttribute("fading", s.session()->fading());

    XMLElement *path = xmlDoc_->NewElement("path");
    xmlCurrent_->InsertEndChild(path);
    XMLText *text = xmlDoc_->NewText( s.path().c_str() );
    path->InsertEndChild( text );

    if (!sessionFilePath_.empty())
        path->SetAttribute("relative", SystemToolkit::path_relative_to_path(s.path(), sessionFilePath_).c_str());
}

void SessionVisitor::visit (SessionGroupSource& s)
{
    xmlCurrent_->SetAttribute("type", "GroupSource");

    Session *se = s.session();

    XMLElement *sessionNode = xmlDoc_->NewElement("Session");
    xmlCurrent_->InsertEndChild(sessionNode);

    for (auto iter = se->begin(); iter != se->end(); ++iter){
        setRoot(sessionNode);
        (*iter)->accept(*this);
    }

}

void SessionVisitor::visit (RenderSource&)
{
    xmlCurrent_->SetAttribute("type", "RenderSource");
}

void SessionVisitor::visit (CloneSource& s)
{
    xmlCurrent_->SetAttribute("type", "CloneSource");

    XMLElement *origin = xmlDoc_->NewElement("origin");
    origin->SetAttribute("id", s.origin()->id());
    xmlCurrent_->InsertEndChild(origin);
    XMLText *text = xmlDoc_->NewText( s.origin()->name().c_str() );
    origin->InsertEndChild( text );
}

void SessionVisitor::visit (PatternSource& s)
{
    xmlCurrent_->SetAttribute("type", "PatternSource");

    if (s.pattern()) {
        xmlCurrent_->SetAttribute("pattern", s.pattern()->type() );

        XMLElement *resolution = xmlDoc_->NewElement("resolution");
        resolution->InsertEndChild( XMLElementFromGLM(xmlDoc_, s.pattern()->resolution() ) );
        xmlCurrent_->InsertEndChild(resolution);
    }
}

void SessionVisitor::visit (DeviceSource& s)
{
    xmlCurrent_->SetAttribute("type", "DeviceSource");
    xmlCurrent_->SetAttribute("device", s.device().c_str() );
}

void SessionVisitor::visit (NetworkSource& s)
{
    xmlCurrent_->SetAttribute("type", "NetworkSource");
    xmlCurrent_->SetAttribute("connection", s.connection().c_str() );
}

void SessionVisitor::visit (MixingGroup& g)
{
    xmlCurrent_->SetAttribute("size", g.size());

    for (auto it = g.begin(); it != g.end(); ++it) {
        XMLElement *sour = xmlDoc_->NewElement("source");
        sour->SetAttribute("id", (*it)->id());
        xmlCurrent_->InsertEndChild(sour);
    }
}

void SessionVisitor::visit (MultiFileSource& s)
{
    xmlCurrent_->SetAttribute("type", "MultiFileSource");

    XMLElement *sequence = xmlDoc_->NewElement("Sequence");
    // play properties
    sequence->SetAttribute("fps", s.framerate());
    sequence->SetAttribute("begin", s.begin());
    sequence->SetAttribute("end", s.end());
    sequence->SetAttribute("loop", s.loop());
    // file sequence description
    sequence->SetAttribute("min", s.sequence().min);
    sequence->SetAttribute("max", s.sequence().max);
    sequence->SetAttribute("width", s.sequence().width);
    sequence->SetAttribute("height", s.sequence().height);
    sequence->SetAttribute("codec", s.sequence().codec.c_str());

    if (!sessionFilePath_.empty())
        sequence->SetAttribute("relative", SystemToolkit::path_relative_to_path(s.sequence().location, sessionFilePath_).c_str());

    XMLText *location = xmlDoc_->NewText( s.sequence().location.c_str() );
    sequence->InsertEndChild( location );

    xmlCurrent_->InsertEndChild(sequence);
}

std::string SessionVisitor::getClipboard(const SourceList &list)
{
    std::string x = "";

    if (!list.empty()) {

        // create xml doc and root node
        tinyxml2::XMLDocument xmlDoc;
        tinyxml2::XMLElement *selectionNode = xmlDoc.NewElement(APP_NAME);
        selectionNode->SetAttribute("size", (int) list.size());
        xmlDoc.InsertEndChild(selectionNode);

        // fill doc by visiting sources
        SourceList selection_clones_;
        SessionVisitor sv(&xmlDoc, selectionNode);
        for (auto iter = list.begin(); iter != list.end(); ++iter, sv.setRoot(selectionNode) ){
            // start with clones
            CloneSource *clone = dynamic_cast<CloneSource *>(*iter);
            if (clone)
                (*iter)->accept(sv);
            else
                selection_clones_.push_back(*iter);
        }
        // add others in front
        for (auto iter = selection_clones_.begin(); iter != selection_clones_.end(); ++iter, sv.setRoot(selectionNode) ){
            (*iter)->accept(sv);
        }

        // get compact string
        tinyxml2::XMLPrinter xmlPrint(0, true);
        xmlDoc.Print( &xmlPrint );
        x = xmlPrint.CStr();
    }

    return x;
}

std::string SessionVisitor::getClipboard(Source * const s)
{
    std::string x = "";

    if (s != nullptr) {
        // create xml doc and root node
        tinyxml2::XMLDocument xmlDoc;
        tinyxml2::XMLElement *selectionNode = xmlDoc.NewElement(APP_NAME);
        selectionNode->SetAttribute("size", 1);
        xmlDoc.InsertEndChild(selectionNode);

        // visit source
        SessionVisitor sv(&xmlDoc, selectionNode);
        s->accept(sv);

        // get compact string
        tinyxml2::XMLPrinter xmlPrint(0, true);
        xmlDoc.Print( &xmlPrint );
        x = xmlPrint.CStr();
    }

    return x;
}

std::string SessionVisitor::getClipboard(ImageProcessingShader * const s)
{
    std::string x = "";

    if (s != nullptr) {
        // create xml doc and root node
        tinyxml2::XMLDocument xmlDoc;
        tinyxml2::XMLElement *selectionNode = xmlDoc.NewElement(APP_NAME);
        xmlDoc.InsertEndChild(selectionNode);

        tinyxml2::XMLElement *imgprocNode = xmlDoc.NewElement( "ImageProcessing" );
        selectionNode->InsertEndChild(imgprocNode);

        // visit source
        SessionVisitor sv(&xmlDoc, imgprocNode);
        s->accept(sv);

        // get compact string
        tinyxml2::XMLPrinter xmlPrint(0, true);
        xmlDoc.Print( &xmlPrint );
        x = xmlPrint.CStr();
    }

    return x;
}
