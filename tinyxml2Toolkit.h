#ifndef TINYXML2TOOLKIT_H
#define TINYXML2TOOLKIT_H

#include <glm/glm.hpp>

namespace tinyxml2 {

class XMLDocument;
class XMLElement;

XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::vec3 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::vec4 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::mat4 matrix);

void XMLElementToGLM(XMLElement *elem, glm::vec3 &vector);
void XMLElementToGLM(XMLElement *elem, glm::vec4 &vector);
void XMLElementToGLM(XMLElement *elem, glm::mat4 &matrix);

void XMLCheckResult(int r);

}

#endif // TINYXML2TOOLKIT_H
