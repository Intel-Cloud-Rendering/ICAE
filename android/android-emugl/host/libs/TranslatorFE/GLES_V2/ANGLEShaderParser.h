// Copyright 2016 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#pragma once

#include <string>

#include <GLES2/gl2.h>

namespace ANGLEShaderParser {
    bool globalInitialize(
            int attribs,
            int uniformVectors,
            int varyingVectors,
            int vertexTextureImageUnits,
            int combinedTexImageUnits,
            int textureImageUnits,
            int fragmentUniformVectors,
            int drawBuffers,
            int fragmentPrecisionHigh,
            int vertexOutputComponents,
            int fragmentInputComponents,
            int minProgramTexelOffset,
            int maxProgramTexelOffset,
            int maxDualSourceDrawBuffers);
    bool translate(int esslVersion, const char* src, GLenum shaderType,
                   std::string* outInfolog, std::string* outObjCode);
}
