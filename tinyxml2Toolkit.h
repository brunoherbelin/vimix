#ifndef TINYXML2TOOLKIT_H
#define TINYXML2TOOLKIT_H

#include <string>
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

XMLElement *XMLElementEncodeArray(XMLDocument *doc, void *array, uint64_t arraysize);
bool XMLElementDecodeArray(XMLElement *elem, void *array, uint64_t arraysize);

bool XMLSaveDoc(tinyxml2::XMLDocument * const doc, std::string filename);
bool XMLResultError(int result);

}

#endif // TINYXML2TOOLKIT_H
