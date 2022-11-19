#ifndef TINYXML2TOOLKIT_H
#define TINYXML2TOOLKIT_H

#include <string>
#include <sys/types.h>

#include <glm/glm.hpp>

namespace tinyxml2 {

class XMLDocument;
class XMLElement;

XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::ivec2 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::vec2 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::vec3 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::vec4 vector);
XMLElement *XMLElementFromGLM(XMLDocument *doc, glm::mat4 matrix);

void XMLElementToGLM(const XMLElement *elem, glm::ivec2 &vector);
void XMLElementToGLM(const XMLElement *elem, glm::vec2 &vector);
void XMLElementToGLM(const XMLElement *elem, glm::vec3 &vector);
void XMLElementToGLM(const XMLElement *elem, glm::vec4 &vector);
void XMLElementToGLM(const XMLElement *elem, glm::mat4 &matrix);

XMLElement *XMLElementEncodeArray(XMLDocument *doc, const void *array, uint arraysize);
bool XMLElementDecodeArray(const tinyxml2::XMLElement *elem, void *array, uint arraysize);

bool XMLSaveDoc(tinyxml2::XMLDocument * const doc, std::string filename);
bool XMLResultError(int result, bool verbose = true);

}

#endif // TINYXML2TOOLKIT_H
