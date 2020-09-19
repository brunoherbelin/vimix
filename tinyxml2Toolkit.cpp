#include "SystemToolkit.h"
#include "Log.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include <glm/gtc/matrix_access.hpp>

#include <string>

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::ivec2 vector)
{
    XMLElement *newelement = doc->NewElement( "ivec2" );
    newelement->SetAttribute("x", vector.x);
    newelement->SetAttribute("y", vector.y);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::vec3 vector)
{
    XMLElement *newelement = doc->NewElement( "vec3" );
    newelement->SetAttribute("x", vector.x);
    newelement->SetAttribute("y", vector.y);
    newelement->SetAttribute("z", vector.z);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::vec4 vector)
{
    XMLElement *newelement = doc->NewElement( "vec4" );
    newelement->SetAttribute("x", vector.x);
    newelement->SetAttribute("y", vector.y);
    newelement->SetAttribute("z", vector.z);
    newelement->SetAttribute("w", vector.w);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::mat4 matrix)
{
    XMLElement *newelement = doc->NewElement( "mat4" );
    for (int r = 0 ; r < 4 ; r ++)
    {
        glm::vec4 row = glm::row(matrix, r);
        XMLElement *rowxml = XMLElementFromGLM(doc, row);
        rowxml->SetAttribute("row", r);
        newelement->InsertEndChild(rowxml);
    }
    return newelement;
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::ivec2 &vector)
{
    if ( !elem || std::string(elem->Name()).find("ivec2") == std::string::npos )
        return;
    elem->QueryIntAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryIntAttribute("y", &vector.y);
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::vec3 &vector)
{
    if ( !elem || std::string(elem->Name()).find("vec3") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::vec4 &vector)
{
    if ( !elem || std::string(elem->Name()).find("vec4") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
    elem->QueryFloatAttribute("w", &vector.w);
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::mat4 &matrix)
{
    if ( !elem || std::string(elem->Name()).find("mat4") == std::string::npos )
        return;

    // loop over rows of vec4
    XMLElement* row = elem->FirstChildElement("vec4");
    for( ; row ; row = row->NextSiblingElement())
    {
        int r = 0;
        row->QueryIntAttribute("row", &r); // which row index are we reading?
        glm::vec4 vector = glm::row(matrix, r); // use matrix row as default
        XMLElementToGLM(row, vector); // read vec4 values
        matrix[0][r] = vector[0];
        matrix[1][r] = vector[1];
        matrix[2][r] = vector[2];
        matrix[3][r] = vector[3];
    }
}


XMLElement *tinyxml2::XMLElementEncodeArray(XMLDocument *doc, void *array, unsigned int arraysize)
{
    gchar *encoded_string = g_base64_encode( (guchar *) array, arraysize);

    XMLElement *newelement = doc->NewElement( "array" );
    newelement->SetAttribute("len", arraysize);

    XMLText *text = doc->NewText( encoded_string );
    newelement->InsertEndChild( text );

    g_free(encoded_string);
    return newelement;
}

bool tinyxml2::XMLElementDecodeArray(XMLElement *elem, void *array, unsigned int arraysize)
{
    if ( !elem || std::string(elem->Name()).find("array") == std::string::npos )
        return false;

    unsigned int len = 0;
    elem->QueryUnsignedAttribute("len", &len);
    if ( arraysize != len )
        return false;

    gsize declen = 0;
    guchar *decoded_array = g_base64_decode(elem->GetText(), &declen);
    if ( arraysize != declen )
        return false;

    memcpy(array, decoded_array, len);
    return true;
}

bool tinyxml2::XMLSaveDoc(XMLDocument * const doc, std::string filename)
{
    XMLDeclaration *pDec = doc->NewDeclaration();
    doc->InsertFirstChild(pDec);

    std::string s = "Originally saved as " + filename + " by " + SystemToolkit::username();
    XMLComment *pComment = doc->NewComment(s.c_str());
    doc->InsertEndChild(pComment);

    // save session
    XMLError eResult = doc->SaveFile(filename.c_str());
    return !XMLResultError(eResult);
}

bool tinyxml2::XMLResultError(int result)
{
    XMLError xmlresult = (XMLError) result;
    if ( xmlresult != XML_SUCCESS)
    {
        Log::Warning("XML error %i: %s", result, tinyxml2::XMLDocument::ErrorIDToName(xmlresult));
        return true;
    }
    return false;
}
