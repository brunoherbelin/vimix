/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <zlib.h>
#include <glib.h>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include <glm/gtc/matrix_access.hpp>

#include <string>

#include "SystemToolkit.h"
#include "Log.h"


XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::ivec2 vector)
{
    XMLElement *newelement = doc->NewElement( "ivec2" );
    newelement->SetAttribute("x", vector.x);
    newelement->SetAttribute("y", vector.y);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::vec2 vector)
{
    XMLElement *newelement = doc->NewElement( "vec2" );
    newelement->SetAttribute("x", (float) vector.x);
    newelement->SetAttribute("y", (float) vector.y);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::vec3 vector)
{
    XMLElement *newelement = doc->NewElement( "vec3" );
    newelement->SetAttribute("x", (float) vector.x);
    newelement->SetAttribute("y", (float) vector.y);
    newelement->SetAttribute("z", (float) vector.z);
    return newelement;
}

XMLElement *tinyxml2::XMLElementFromGLM(XMLDocument *doc, glm::vec4 vector)
{
    XMLElement *newelement = doc->NewElement( "vec4" );
    newelement->SetAttribute("x", (float) vector.x);
    newelement->SetAttribute("y", (float) vector.y);
    newelement->SetAttribute("z", (float) vector.z);
    newelement->SetAttribute("w", (float) vector.w);
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

void tinyxml2::XMLElementToGLM(const XMLElement *elem, glm::ivec2 &vector)
{
    if ( !elem || std::string(elem->Name()).find("ivec2") == std::string::npos )
        return;
    elem->QueryIntAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryIntAttribute("y", &vector.y);
}

void tinyxml2::XMLElementToGLM(const XMLElement *elem, glm::vec2 &vector)
{
    if ( !elem || std::string(elem->Name()).find("vec2") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
}

void tinyxml2::XMLElementToGLM(const XMLElement *elem, glm::vec3 &vector)
{
    if ( !elem || std::string(elem->Name()).find("vec3") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
}

void tinyxml2::XMLElementToGLM(const XMLElement *elem, glm::vec4 &vector)
{
    if ( !elem || std::string(elem->Name()).find("vec4") == std::string::npos )
        return;
    elem->QueryFloatAttribute("x", &vector.x); // If this fails, original value is left as-is
    elem->QueryFloatAttribute("y", &vector.y);
    elem->QueryFloatAttribute("z", &vector.z);
    elem->QueryFloatAttribute("w", &vector.w);
}

void tinyxml2::XMLElementToGLM(const XMLElement *elem, glm::mat4 &matrix)
{
    if ( !elem || std::string(elem->Name()).find("mat4") == std::string::npos )
        return;

    // loop over rows of vec4
    const XMLElement* row = elem->FirstChildElement("vec4");
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


XMLElement *tinyxml2::XMLElementEncodeArray(XMLDocument *doc, const void *array, uint arraysize)
{
    // create <array> node
    XMLElement *newelement = doc->NewElement( "array" );
    newelement->SetAttribute("len", arraysize);

    // prepare an array for compressing data
    uLong  compressed_size = compressBound(arraysize);
    gchar *compressed_array = g_new(gchar, compressed_size);

    // encoded string will hold the base64 encoding of the array
    gchar *encoded_array = nullptr;

    // zlib  compress ((Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen));
    if ( Z_OK == compress((Bytef *)compressed_array, &compressed_size,
                          (Bytef *)array, arraysize) ) {

        // compression suceeded : add a zbytes field to indicate how much data was compressed
        newelement->SetAttribute("zbytes", (uint) compressed_size);

        // encode the compressed array
        encoded_array = g_base64_encode( (guchar *) compressed_array, (gsize) compressed_size);

    }
    // failed compression
    else {
        // encode the raw array
        encoded_array = g_base64_encode( (guchar *) array, (gsize) arraysize);
    }

    // save the encoded string as text
    XMLText *text = doc->NewText( encoded_array );
    newelement->InsertEndChild( text );

    // free temporary arrays
    g_free(compressed_array);
    g_free(encoded_array);

    return newelement;
}

bool tinyxml2::XMLElementDecodeArray(const XMLElement *elem, void *array, uint arraysize)
{
    bool ret = false;

    // sanity check
    if (array==nullptr || arraysize==0)
        return ret;

    // make sure we have the good type of XML node
    if ( !elem || std::string(elem->Name()).compare("array") != 0 )
        return ret;

    // make sure the stored array is of the requested array size
    uint len = 0;
    elem->QueryUnsignedAttribute("len", &len);
    if (len == arraysize)
    {
        // read and decode the text field in <array>
        gsize   decoded_size = 0;
        guchar *decoded_array = g_base64_decode(elem->GetText(), &decoded_size);

        // if data is z-compressed (zbytes size is indicated)
        uint zbytes = 0;
        elem->QueryUnsignedAttribute("zbytes", &zbytes);
        if ( zbytes > 0) {
            // sanity check 1: decoded data size must match the buffer size
            if ( decoded_array && zbytes == (uint) decoded_size ) {

                // allocate a temporary array for decompressing data
                uLong  uncompressed_size = len;
                gchar *uncompressed_array = g_new(gchar, uncompressed_size);

                // zlib uncompress ((Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen));
                uncompress((Bytef *)uncompressed_array, &uncompressed_size,
                           (Bytef *)decoded_array, (uLong) zbytes) ;

                // sanity check 2: decompressed data size must match array size
                if ( uncompressed_array && len == uncompressed_size ){
                    // copy to target array
                    memcpy(array, uncompressed_array, len);
                    // success
                    ret = true;
                }

                // free temp decompression buffer
                g_free(uncompressed_array);
            }
        }
        // data is not z-compressed
        else {
            // copy the decoded data
            if ( decoded_array && len == decoded_size ) {
                // copy to target array
                memcpy(array, decoded_array, len);
                // success
                ret = true;
            }
        }

        // free temporary array
        g_free(decoded_array);
    }

    return ret;
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

bool tinyxml2::XMLResultError(int result, bool verbose)
{
    XMLError xmlresult = (XMLError) result;
    if ( xmlresult != XML_SUCCESS)
    {
        if (verbose)
            Log::Info("XML error %i: %s", result, tinyxml2::XMLDocument::ErrorIDToName(xmlresult));
        return true;
    }
    return false;
}
