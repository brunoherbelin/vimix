#include "tinyxml2Toolkit.h"
#include "Log.h"

#include <tinyxml2.h>
using namespace tinyxml2;

#include <glm/gtc/matrix_access.hpp>

#include <string>

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


void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::vec3 &vector)
{
    if ( std::string(elem->Name()).find("vec3") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::vec4 &vector)
{
    if ( std::string(elem->Name()).find("vec4") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
    elem->QueryFloatAttribute("w", &vector.w);
}

void tinyxml2::XMLElementToGLM(XMLElement *elem, glm::mat4 &matrix)
{
    if ( std::string(elem->Name()).find("mat4") == std::string::npos )
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

void tinyxml2::XMLCheckResult(int r)
{
    XMLError result = (XMLError) r;
    if ( result != XML_SUCCESS)
    {
        Log::Error("XML error %i: %s", r, tinyxml2::XMLDocument::ErrorIDToName(result));
    }
}
