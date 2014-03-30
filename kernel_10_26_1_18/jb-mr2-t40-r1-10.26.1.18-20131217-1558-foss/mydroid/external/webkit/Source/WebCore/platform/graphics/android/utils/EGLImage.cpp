/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EGLImage.h"

#include "AutoRestoreGLState.h"
#include "GLUtils.h"

namespace WebCore {

PassOwnPtr<EGLImage> EGLImage::createFromTexture(GLuint textureID, GLenum textureTarget)
{
    EGLenum target;
    switch (textureTarget) {
    case GL_TEXTURE_2D: target = EGL_GL_TEXTURE_2D_KHR; break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X: target = EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR; break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: target = EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR; break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: target = EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR; break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: target = EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR; break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: target = EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR; break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: target = EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR; break;
    default: return 0;
    };

    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(textureID);
    EGLDisplay display = eglGetCurrentDisplay();

    static const EGLint attr[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};

    EGLImageKHR image = eglCreateImageKHR(display, eglGetCurrentContext(), target, buffer, attr);
    GLUtils::checkEglError("eglCreateImage", image != EGL_NO_IMAGE_KHR);

    if (image == EGL_NO_IMAGE_KHR)
        return 0;

    return adopt(image, display, textureTarget);
}

EGLImage::~EGLImage()
{
    eglDestroyImageKHR(m_imageDisplay, m_image);
}

GLuint EGLImage::createTexture(GLint filter, GLint wrap) const
{
    GLuint textureID = 0;
    glGenTextures(1, &textureID);

    AutoRestoreTextureBinding restoreTextureBinding(m_textureTarget, textureID);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, wrap);
    writeToTexture(textureID);
    return textureID;

}

void EGLImage::writeToTexture(GLuint textureID) const
{
    glBindTexture(m_textureTarget, textureID);
    glEGLImageTargetTexture2DOES(m_textureTarget, reinterpret_cast<GLeglImageOES>(m_image));
}

} // namespace WebCore
