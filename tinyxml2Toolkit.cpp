#include "tinyxml2Toolkit.h"

#include <tinyxml2.h>
using namespace tinyxml2;

#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>


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
