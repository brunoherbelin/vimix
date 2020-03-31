#ifndef TINYXML2TOOLKIT_H
#define TINYXML2TOOLKIT_H

#include <glm/glm.hpp>

namespace tinyxml2 {

class XMLDocument;
class XMLElement;

XMLElement *XMLElementGLM(XMLDocument *doc, glm::vec3 vector);
XMLElement *XMLElementGLM(XMLDocument *doc, glm::vec4 vector);
XMLElement *XMLElementGLM(XMLDocument *doc, glm::mat4 matrix);

}

#endif // TINYXML2TOOLKIT_H
