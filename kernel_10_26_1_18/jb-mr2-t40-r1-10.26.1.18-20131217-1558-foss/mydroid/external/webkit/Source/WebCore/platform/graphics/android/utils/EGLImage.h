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

#ifndef EGLImage_h
#define EGLImage_h

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class EGLImage {
    WTF_MAKE_NONCOPYABLE(EGLImage);

public:
    static PassOwnPtr<EGLImage> createFromTexture(GLuint textureID,
                                                  GLenum textureTarget = GL_TEXTURE_2D);
    static PassOwnPtr<EGLImage> adopt(EGLImageKHR image, EGLDisplay imageDisplay,
                                      GLenum textureTarget = GL_TEXTURE_2D)
    {
        return adoptPtr(new EGLImage(image, imageDisplay, textureTarget));
    }

    GLenum textureTarget() const { return m_textureTarget; }
    GLuint createTexture(GLint filter = GL_LINEAR, GLint wrap = GL_CLAMP_TO_EDGE) const;
    void writeToTexture(GLuint textureID) const;

    ~EGLImage();

private:
    EGLImage(EGLImageKHR image, EGLDisplay imageDisplay, GLenum textureTarget)
        : m_image(image)
        , m_imageDisplay(imageDisplay)
        , m_textureTarget(textureTarget)
    {
        ASSERT(m_image != EGL_NO_IMAGE_KHR);
    }

    EGLImageKHR m_image;
    EGLDisplay m_imageDisplay;
    GLenum m_textureTarget;
};

} // namespace WebCore

#endif // EGLImage_h
