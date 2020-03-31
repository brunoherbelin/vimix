#ifndef TINYXML2TOOLKIT_H
#define TINYXML2TOOLKIT_H

#include <glm/glm.hpp>

namespace tinyxml2 {

class XMLDocument;
class XMLElement;

tinyxml2::XMLElement *XMLElementGLM(tinyxml2::XMLDocument *doc, glm::vec3 vector);
tinyxml2::XMLElement *XMLElementGLM(tinyxml2::XMLDocument *doc, glm::vec4 vector);
tinyxml2::XMLElement *XMLElementGLM(tinyxml2::XMLDocument *doc, glm::mat4 matrix);

}

#endif // TINYXML2TOOLKIT_H
