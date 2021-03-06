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
#include "ColorBuffer.h"

#include "android/base/memory/ScopedPtr.h"

#include "DispatchTables.h"
#include "GLcommon/GLutils.h"
#include "RenderThreadInfo.h"
#include "TextureDraw.h"
#include "TextureResize.h"
#include "YUVConverter.h"

#include "OpenGLESDispatch/EGLDispatch.h"

#include <stdio.h>
#include <string.h>

namespace {

// Lazily create and bind a framebuffer object to the current host context.
// |fbo| is the address of the framebuffer object name.
// |tex| is the name of a texture that is attached to the framebuffer object
// on creation only. I.e. all rendering operations will target it.
// returns true in case of success, false on failure.
bool bindFbo(GLuint* fbo, GLuint tex) {
    if (*fbo) {
        // fbo already exist - just bind
        s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        return true;
    }

    s_gles2.glGenFramebuffers(1, fbo);
    s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    s_gles2.glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0_OES,
                                   GL_TEXTURE_2D, tex, 0);
    GLenum status = s_gles2.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE_OES) {
        ERR("ColorBuffer::bindFbo: FBO not complete: %#x\n", status);
        s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        s_gles2.glDeleteFramebuffers(1, fbo);
        *fbo = 0;
        return false;
    }
    return true;
}

void unbindFbo() {
    s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Helper class to use a ColorBuffer::Helper context.
// Usage is pretty simple:
//
//     {
//        ScopedHelperContext context(m_helper);
//        if (!context.isOk()) {
//            return false;   // something bad happened.
//        }
//        .... do something ....
//     }   // automatically calls m_helper->teardownContext();
//
class ScopedHelperContext {
public:
    ScopedHelperContext(ColorBuffer::Helper* helper) : mHelper(helper) {
        if (!helper->setupContext()) {
            mHelper = NULL;
        }
    }

    bool isOk() const { return mHelper != NULL; }

    ~ScopedHelperContext() {
        release();
    }

    void release() {
        if (mHelper) {
            mHelper->teardownContext();
            mHelper = NULL;
        }
    }
private:
    ColorBuffer::Helper* mHelper;
};

}  // namespace

// static
ColorBuffer* ColorBuffer::create(EGLDisplay p_display,
                                 int p_width,
                                 int p_height,
                                 GLenum p_internalFormat,
                                 FrameworkFormat p_frameworkFormat,
                                 bool has_eglimage_texture_2d,
                                 Helper* helper) {
    GLenum texInternalFormat = 0;

    switch (p_internalFormat) {
        case GL_RGB:
        case GL_RGB565_OES:
            texInternalFormat = GL_RGB;
            break;

        case GL_RGBA:
        case GL_RGB5_A1_OES:
        case GL_RGBA4_OES:
            texInternalFormat = GL_RGBA;
            break;

        default:
            return NULL;
    }

    const int nComp = (texInternalFormat == GL_RGB ? 3 : 4);
    const unsigned long bufsize = ((unsigned long)nComp) * p_width * p_height;
    android::base::ScopedCPtr<char> initialImage(
                static_cast<char*>(::malloc(bufsize)));
    if (!initialImage) {
        fprintf(stderr,
                "error: failed to allocate initial memory for ColorBuffer "
                "of size %dx%dx%d (%lu KB)\n",
                p_width, p_height, nComp * 8, bufsize / 1024);
        return nullptr;
    }
    memset(initialImage.get(), 0xff, bufsize);

    ScopedHelperContext context(helper);
    if (!context.isOk()) {
        return NULL;
    }

    ColorBuffer *cb = new ColorBuffer(p_display, helper);

    s_gles2.glGenTextures(1, &cb->m_tex);
    s_gles2.glBindTexture(GL_TEXTURE_2D, cb->m_tex);

    s_gles2.glTexImage2D(GL_TEXTURE_2D,
                         0,
                         texInternalFormat,
                         p_width,
                         p_height,
                         0,
                         texInternalFormat,
                         GL_UNSIGNED_BYTE,
                         initialImage.get());
    initialImage.reset();

    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //
    // create another texture for that colorbuffer for blit
    //
    s_gles2.glGenTextures(1, &cb->m_blitTex);
    s_gles2.glBindTexture(GL_TEXTURE_2D, cb->m_blitTex);
    s_gles2.glTexImage2D(GL_TEXTURE_2D,
                         0,
                         texInternalFormat,
                         p_width,
                         p_height,
                         0,
                         texInternalFormat,
                         GL_UNSIGNED_BYTE,
                         NULL);

    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    cb->m_width = p_width;
    cb->m_height = p_height;
    cb->m_internalFormat = texInternalFormat;

    if (has_eglimage_texture_2d) {
        cb->m_eglImage = s_egl.eglCreateImageKHR(
                p_display,
                s_egl.eglGetCurrentContext(),
                EGL_GL_TEXTURE_2D_KHR,
                (EGLClientBuffer)SafePointerFromUInt(cb->m_tex),
                NULL);

        cb->m_blitEGLImage = s_egl.eglCreateImageKHR(
                p_display,
                s_egl.eglGetCurrentContext(),
                EGL_GL_TEXTURE_2D_KHR,
                (EGLClientBuffer)SafePointerFromUInt(cb->m_blitTex),
                NULL);
    }

    cb->m_resizer = new TextureResize(p_width, p_height);

    cb->m_frameworkFormat = p_frameworkFormat;
    switch (cb->m_frameworkFormat) {
    case FRAMEWORK_FORMAT_GL_COMPATIBLE:
        break;
    case FRAMEWORK_FORMAT_YV12:
    case FRAMEWORK_FORMAT_YUV_420_888:
        cb->m_yuv_converter.reset(new YUVConverter(p_width, p_height, cb->m_frameworkFormat));
        break;
    default:
        break;
    }

    s_gles2.glFinish();
    return cb;
}

ColorBuffer::ColorBuffer(EGLDisplay display, Helper* helper) :
        m_display(display),
        m_helper(helper) {}

ColorBuffer::~ColorBuffer() {
    ScopedHelperContext context(m_helper);

    if (m_blitEGLImage) {
        s_egl.eglDestroyImageKHR(m_display, m_blitEGLImage);
    }
    if (m_eglImage) {
        s_egl.eglDestroyImageKHR(m_display, m_eglImage);
    }

    if (m_fbo) {
        s_gles2.glDeleteFramebuffers(1, &m_fbo);
    }

    if (m_yuv_conversion_fbo) {
        s_gles2.glDeleteFramebuffers(1, &m_yuv_conversion_fbo);
    }

    m_yuv_converter.reset();

    GLuint tex[2] = {m_tex, m_blitTex};
    s_gles2.glDeleteTextures(2, tex);

    delete m_resizer;
}

void ColorBuffer::readPixels(int x,
                             int y,
                             int width,
                             int height,
                             GLenum p_format,
                             GLenum p_type,
                             void* pixels) {
    ScopedHelperContext context(m_helper);
    if (!context.isOk()) {
        return;
    }

    if (bindFbo(&m_fbo, m_tex)) {
        s_gles2.glReadPixels(x, y, width, height, p_format, p_type, pixels);
        unbindFbo();
    }
}

void ColorBuffer::subUpdate(int x,
                            int y,
                            int width,
                            int height,
                            GLenum p_format,
                            GLenum p_type,
                            void* pixels) {
    ScopedHelperContext context(m_helper);
    if (!context.isOk()) {
        return;
    }

    if (m_frameworkFormat == FRAMEWORK_FORMAT_YV12 ||
        m_frameworkFormat == FRAMEWORK_FORMAT_YUV_420_888) {
        assert(m_yuv_converter.get());

        // This FBO will convert the YUV frame to RGB
        // and render it to |m_tex|.
        bindFbo(&m_yuv_conversion_fbo, m_tex);
        m_yuv_converter->drawConvert(x, y, width, height, (char*)pixels);
        unbindFbo();

        // |m_tex| still needs to be bound afterwards
        s_gles2.glBindTexture(GL_TEXTURE_2D, m_tex);
    } else {
        s_gles2.glBindTexture(GL_TEXTURE_2D, m_tex);
        s_gles2.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        s_gles2.glTexSubImage2D(
                GL_TEXTURE_2D, 0, x, y, width, height, p_format, p_type, pixels);
    }
}


bool ColorBuffer::blitFromCurrentReadBuffer()
{
    RenderThreadInfo *tInfo = RenderThreadInfo::get();
    if (!tInfo->currContext.get()) {
        // no Current context
        return false;
    }

    // Copy the content of the current read surface into m_blitEGLImage.
    // This is done by creating a temporary texture, bind it to the EGLImage
    // then call glCopyTexSubImage2D().
    GLuint tmpTex;
    GLint currTexBind;
    if (tInfo->currContext->version() > GLESApi_CM) {
        s_gles2.glGetIntegerv(GL_TEXTURE_BINDING_2D, &currTexBind);
        s_gles2.glGenTextures(1,&tmpTex);
        s_gles2.glBindTexture(GL_TEXTURE_2D, tmpTex);
        s_gles2.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_blitEGLImage);

        // If the read buffer is multisampled, we need to resolve.
        GLint samples;
        s_gles2.glGetIntegerv(GL_SAMPLE_BUFFERS, &samples);
        if (tInfo->currContext->version() > GLESApi_2 && samples > 0) {
            s_gles2.glBindTexture(GL_TEXTURE_2D, 0);

            GLuint resolve_fbo;
            GLint prev_draw_fbo;
            s_gles2.glGenFramebuffers(1, &resolve_fbo);
            s_gles2.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

            s_gles2.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo);
            s_gles2.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmpTex, 0);
            s_gles2.glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            s_gles2.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);

            s_gles2.glDeleteFramebuffers(1, &resolve_fbo);
            s_gles2.glBindTexture(GL_TEXTURE_2D, tmpTex);
        } else {
            s_gles2.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                    m_width, m_height);
        }
        s_gles2.glDeleteTextures(1, &tmpTex);
        s_gles2.glBindTexture(GL_TEXTURE_2D, currTexBind);
    }
    else {
        s_gles1.glGetIntegerv(GL_TEXTURE_BINDING_2D, &currTexBind);
        s_gles1.glGenTextures(1,&tmpTex);
        s_gles1.glBindTexture(GL_TEXTURE_2D, tmpTex);
        s_gles1.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_blitEGLImage);
        s_gles1.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                                 m_width, m_height);
        s_gles1.glDeleteTextures(1, &tmpTex);
        s_gles1.glBindTexture(GL_TEXTURE_2D, currTexBind);
    }

    ScopedHelperContext context(m_helper);
    if (!context.isOk()) {
        return false;
    }

    if (!bindFbo(&m_fbo, m_tex)) {
        return false;
    }

    // Save current viewport and match it to the current colorbuffer size.
    GLint vport[4] = { 0, };
    s_gles2.glGetIntegerv(GL_VIEWPORT, vport);
    s_gles2.glViewport(0, 0, m_width, m_height);

    // render m_blitTex
    m_helper->getTextureDraw()->draw(m_blitTex, 0., 0, 0);

    // Restore previous viewport.
    s_gles2.glViewport(vport[0], vport[1], vport[2], vport[3]);
    unbindFbo();

    return true;
}

bool ColorBuffer::bindToTexture() {
    if (!m_eglImage) {
        return false;
    }
    RenderThreadInfo *tInfo = RenderThreadInfo::get();
    if (!tInfo->currContext.get()) {
        return false;
    }
    if (tInfo->currContext->version() > GLESApi_CM) {
        s_gles2.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_eglImage);
    }
    else {
        s_gles1.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_eglImage);
    }
    return true;
}

bool ColorBuffer::bindToRenderbuffer() {
    if (!m_eglImage) {
        return false;
    }
    RenderThreadInfo *tInfo = RenderThreadInfo::get();
    if (!tInfo->currContext.get()) {
        return false;
    }
    if (tInfo->currContext->version() > GLESApi_CM) {
        s_gles2.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER_OES, m_eglImage);
    }
    else {
        s_gles1.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER_OES, m_eglImage);
    }
    return true;
}

bool ColorBuffer::post(float rotation, float dx, float dy) {
    // NOTE: Do not call m_helper->setupContext() here!
    return m_helper->getTextureDraw()->draw(m_resizer->update(m_tex), rotation, dx, dy);
}

void ColorBuffer::readback(unsigned char* img) {
    ScopedHelperContext context(m_helper);
    if (!context.isOk()) {
        return;
    }
    if (bindFbo(&m_fbo, m_tex)) {
        s_gles2.glReadPixels(
                0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, img);
        unbindFbo();
    }
}
