/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <GLcommon/TextureUtils.h>
#include <GLcommon/GLESmacros.h>
#include <GLcommon/GLDispatch.h>
#include <GLcommon/GLESvalidate.h>
#include <stdio.h>
#include <cmath>
#include <memory>

int getCompressedFormats(int* formats){
    if(formats){
        //Palette
        formats[0] = GL_PALETTE4_RGBA8_OES;
        formats[1] = GL_PALETTE4_RGBA4_OES;
        formats[2] = GL_PALETTE8_RGBA8_OES;
        formats[3] = GL_PALETTE8_RGBA4_OES;
        formats[4] = GL_PALETTE4_RGB8_OES;
        formats[5] = GL_PALETTE8_RGB8_OES;
        formats[6] = GL_PALETTE4_RGB5_A1_OES;
        formats[7] = GL_PALETTE8_RGB5_A1_OES;
        formats[8] = GL_PALETTE4_R5_G6_B5_OES;
        formats[9] = GL_PALETTE8_R5_G6_B5_OES;
        //ETC
        formats[MAX_SUPPORTED_PALETTE] = GL_ETC1_RGB8_OES;
        formats[MAX_SUPPORTED_PALETTE + 1] = GL_COMPRESSED_RGB8_ETC2;
        formats[MAX_SUPPORTED_PALETTE + 2] = GL_COMPRESSED_SIGNED_R11_EAC;
        formats[MAX_SUPPORTED_PALETTE + 3] = GL_COMPRESSED_RG11_EAC;
        formats[MAX_SUPPORTED_PALETTE + 4] = GL_COMPRESSED_SIGNED_RG11_EAC;
        formats[MAX_SUPPORTED_PALETTE + 5] = GL_COMPRESSED_RGB8_ETC2;
        formats[MAX_SUPPORTED_PALETTE + 6] = GL_COMPRESSED_SRGB8_ETC2;
        formats[MAX_SUPPORTED_PALETTE + 7] = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
        formats[MAX_SUPPORTED_PALETTE + 8] = GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
        formats[MAX_SUPPORTED_PALETTE + 9] = GL_COMPRESSED_RGBA8_ETC2_EAC;
        formats[MAX_SUPPORTED_PALETTE + 10] = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
        formats[MAX_SUPPORTED_PALETTE + 11] = GL_COMPRESSED_R11_EAC;
    }
    return MAX_SUPPORTED_PALETTE + MAX_ETC_SUPPORTED;
}

#define GL_R16                            0x822A
#define GL_RG16                           0x822C
#define GL_R16_SNORM                      0x8F98
#define GL_RG16_SNORM                     0x8F99

void  doCompressedTexImage2D(GLEScontext * ctx, GLenum target, GLint level, 
                                          GLenum internalformat, GLsizei width, 
                                          GLsizei height, GLint border, 
                                          GLsizei imageSize, const GLvoid* data, void * funcPtr)
{
    /* XXX: This is just a hack to fix the resolve of glTexImage2D problem
       It will be removed when we'll no longer link against ligGL */
    typedef void (GLAPIENTRY *glTexImage2DPtr_t ) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
    glTexImage2DPtr_t glTexImage2DPtr;
    glTexImage2DPtr =  (glTexImage2DPtr_t)funcPtr; 

    switch (internalformat) {
        case GL_ETC1_RGB8_OES:
        case GL_COMPRESSED_RGB8_ETC2:
        case GL_COMPRESSED_SRGB8_ETC2:
        case GL_COMPRESSED_RGBA8_ETC2_EAC:
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        case GL_COMPRESSED_R11_EAC:
        case GL_COMPRESSED_SIGNED_R11_EAC:
        case GL_COMPRESSED_RG11_EAC:
        case GL_COMPRESSED_SIGNED_RG11_EAC:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            {
                GLint format = GL_RGB;
                GLint type = GL_UNSIGNED_BYTE;
                GLint convertedInternalFormat = GL_RGB8;
                ETC2ImageFormat etcFormat = EtcRGB8;
                switch (internalformat) {
                    case GL_COMPRESSED_RGB8_ETC2:
                    case GL_ETC1_RGB8_OES:
                        break;
                    case GL_COMPRESSED_RGBA8_ETC2_EAC:
                        convertedInternalFormat = GL_RGBA8;
                        etcFormat = EtcRGBA8;
                        format = GL_RGBA;
                        break;
                    case GL_COMPRESSED_SRGB8_ETC2:
                        convertedInternalFormat = GL_SRGB8;
                        break;
                    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
                        convertedInternalFormat = GL_SRGB8_ALPHA8;
                        etcFormat = EtcRGBA8;
                        format = GL_RGBA;
                        break;
                    case GL_COMPRESSED_R11_EAC:
                        convertedInternalFormat = GL_R32F;
                        etcFormat = EtcR11;
                        format = GL_RED;
                        type = GL_FLOAT;
                        break;
                    case GL_COMPRESSED_SIGNED_R11_EAC:
                        convertedInternalFormat = GL_R32F;
                        etcFormat = EtcSignedR11;
                        format = GL_RED;
                        type = GL_FLOAT;
                        break;
                    case GL_COMPRESSED_RG11_EAC:
                        convertedInternalFormat = GL_RG32F;
                        etcFormat = EtcRG11;
                        format = GL_RG;
                        type = GL_FLOAT;
                        break;
                    case GL_COMPRESSED_SIGNED_RG11_EAC:
                        convertedInternalFormat = GL_RG32F;
                        etcFormat = EtcSignedRG11;
                        format = GL_RG;
                        type = GL_FLOAT;
                        break;
                    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
                        convertedInternalFormat = GL_RGBA8;
                        etcFormat = EtcRGB8A1;
                        format = GL_RGBA;
                        break;
                    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
                        convertedInternalFormat = GL_SRGB8_ALPHA8;
                        etcFormat = EtcRGB8A1;
                        format = GL_RGBA;
                        break;
                }

                int pixelSize = etc_get_decoded_pixel_size(etcFormat);
                GLsizei compressedSize = etc_get_encoded_data_size(etcFormat, width, height);
                SET_ERROR_IF((compressedSize > imageSize), GL_INVALID_VALUE);
                SET_ERROR_IF(!data,GL_INVALID_OPERATION);

                const int32_t align = ctx->getUnpackAlignment()-1;
                const int32_t bpr = ((width * pixelSize) + align) & ~align;
                const size_t size = bpr * height;

                std::unique_ptr<etc1_byte[]> pOut(new etc1_byte[size]);
                int res = etc2_decode_image((const etc1_byte*)data, etcFormat, pOut.get(), width, height, bpr);
                SET_ERROR_IF(res!=0, GL_INVALID_VALUE);

                glTexImage2DPtr(target,level,convertedInternalFormat,width,height,border,format,type,pOut.get());
            }
            break;
        case GL_PALETTE4_RGB8_OES:
        case GL_PALETTE4_RGBA8_OES:
        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
        case GL_PALETTE8_RGB8_OES:
        case GL_PALETTE8_RGBA8_OES:
        case GL_PALETTE8_R5_G6_B5_OES:
        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
            {
                SET_ERROR_IF(level > log2(ctx->getMaxTexSize()) || 
                             border !=0 || level > 0 || 
                             !GLESvalidate::texImgDim(width,height,ctx->getMaxTexSize()+2),GL_INVALID_VALUE)
                SET_ERROR_IF(!data,GL_INVALID_OPERATION);

                int nMipmaps = -level + 1;
                GLsizei tmpWidth  = width;
                GLsizei tmpHeight = height;

                for(int i = 0; i < nMipmaps ; i++)
                {
                   GLenum uncompressedFrmt;
                   unsigned char* uncompressed = uncompressTexture(internalformat,uncompressedFrmt,width,height,imageSize,data,i);
                   glTexImage2DPtr(target,i,uncompressedFrmt,tmpWidth,tmpHeight,border,uncompressedFrmt,GL_UNSIGNED_BYTE,uncompressed);
                   tmpWidth/=2;
                   tmpHeight/=2;
                   delete[] uncompressed;
                }
            }
            break;

        default:
            SET_ERROR_IF(1, GL_INVALID_ENUM);
            break;
    }
}
