//
// ObjLoader.h -- modified for glm
//  modified from https://github.com/mortennobel/OpenGL_3_2_Utils
//  William.Thibault@csueastbay.edu
//

/*!
 * OpenGL 3.2 Utils - Extension to the Angel library (from the book Interactive Computer Graphics 6th ed
 * https://github.com/mortennobel/OpenGL_3_2_Utils
 *
 * New BSD License
 *
 * Copyright (c) 2011, Morten Nobel-Joergensen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __OBJ_LOADER_H
#define __OBJ_LOADER_H

#include <string>
#include <vector>
#include <glm/glm.hpp>


struct Material
{
Material() :
    ambient (0.1, 0.1, 0.1, 1.0),
    diffuse (0.8, 0.8, 0.8, 1.0),
    specular(1.0, 1.0, 1.0, 1.0 ),
    shininess(75.0)
  {}
  virtual ~Material() {  }

  glm::vec4 ambient;
  glm::vec4 diffuse;
  glm::vec4 specular;
  float shininess;

  std::string diffuseTexture;
  std::string bumpTexture;
};


// The original version, modified for glm.
// Load an OBJ model into the out parameters.
// Note only simple OBJ files are supported.
bool loadObject (std::string filename,
                std::vector<glm::vec3> &outPositions,
		std::vector<glm::vec3> &outNormal,
		std::vector<glm::vec2> &outUv,
                std::vector<int> &outIndices,
                Material *outMaterial,
		float scale = 1.0f
 );

// this tries to handle multiple materials (w/textures) and groups of faces as per pcl_kinfu_largeScale, etc
bool loadObjectGroups (std::string filename,
		std::vector<glm::vec3> &outPositions, 
		std::vector<glm::vec3> &outNormal, 
		std::vector<glm::vec2> &outUv,
                std::vector<std::vector<int> > &outIndices, // per group
                std::vector<Material *> &outMaterials,  // per group (w/textures)
		float scale=1.0f
);

#endif
