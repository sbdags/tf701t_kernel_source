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
#include "GLContext.h"

#define LOG_TAG "GLContext"
#include <cutils/log.h>

#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <wtf/Vector.h>

namespace WebCore {

class AttribList {
public:
    AttribList()
    {
        m_attribs.append(EGL_NONE);
    }
    const EGLint* get() const
    {
        return m_attribs.data();
    }
    void append(EGLint name, EGLint value)
    {
        m_attribs.last() = name;
        m_attribs.append(value);
        m_attribs.append(EGL_NONE);
    }
private:
    Vector<EGLint> m_attribs;
};

static void checkEGLError(const char* operation)
{
    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError())
        ALOGE("EGL error after %s: 0x%x)\n", operation, error);
}


GLContext::GLContext(ResourceLimits::Context resourceContext, ContextAttributes attributes, EGLContext sharedContext, bool& success)
    : m_attributes(attributes)
    , m_context(EGL_NO_CONTEXT)
    , m_display(EGL_NO_DISPLAY)
    , m_fileResources(resourceContext, 2)
{
    success = false;
    checkEGLError("constructor entrypoint");
    if (!m_fileResources.isGranted())
        return;

    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEGLError("eglGetDisplay");
    if (m_display == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay returned EGL_NO_DISPLAY");
        return;
    }

    EGLConfig config;
    EGLint numConfigs;
    EGLBoolean returnValue = eglGetConfigs(m_display, &config, 1, &numConfigs);
    checkEGLError("eglGetConfigs");
    if (returnValue != EGL_TRUE || numConfigs != 1) {
        ALOGE("eglGetConfigs failed\n");
        return;
    }

    AttribList contextAttribs;
    contextAttribs.append(EGL_CONTEXT_CLIENT_VERSION, 2);
    if (m_attributes & EnableRobustness)
        contextAttribs.append(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT);
    if (m_attributes & LowPriority)
        contextAttribs.append(EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_LOW_IMG);
    m_context = eglCreateContext(m_display, config, sharedContext, contextAttribs.get());
    checkEGLError("eglCreateContext");
    if (m_context == EGL_NO_CONTEXT) {
        ALOGE("eglCreateContext failed\n");
        return;
    }

    success = makeCurrent();
}

GLContext::~GLContext()
{
    if (m_context != EGL_NO_CONTEXT) {
        ASSERT(m_display != EGL_NO_DISPLAY);
        if (eglGetCurrentContext() == m_context)
            eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_display, m_context);
    }
}

bool GLContext::isCurrent() const
{
    return eglGetCurrentContext() == m_context;
}

bool GLContext::makeCurrent()
{
    EGLBoolean returnValue = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
    checkEGLError("eglMakeCurrent");
    if (!returnValue)
        ALOGE("eglMakeCurrent failed\n");
    return returnValue;
}

bool GLContext::hasExtension(const char* extensionName)
{
    if (!m_extensions.size()) {
        String extensionsString = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        Vector<String> individualExtensions;
        extensionsString.split(" ", individualExtensions);
        for (size_t i = 0; i < individualExtensions.size(); ++i)
            m_extensions.add(individualExtensions[i]);
    }
    return m_extensions.contains(extensionName);
}

AutoRestoreGLContext::AutoRestoreGLContext()
{
    notePreviousContext();
}

AutoRestoreGLContext::AutoRestoreGLContext(const OwnPtr<GLContext>& newContext)
{
    notePreviousContext();
    newContext->makeCurrent();
}

void AutoRestoreGLContext::notePreviousContext()
{
    if (eglGetCurrentDisplay() == EGL_NO_DISPLAY) {
        // EGL_NO_DISPLAY is not a valid dpy parameter for eglMakeCurrent. Even
        // to make no context current, we have to send in a real display.
        m_oldDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        m_oldDrawSurface = EGL_NO_SURFACE;
        m_oldReadSurface = EGL_NO_SURFACE;
        m_oldContext = EGL_NO_CONTEXT;
        return;
    }

    m_oldDisplay = eglGetCurrentDisplay();
    m_oldDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    m_oldReadSurface = eglGetCurrentSurface(EGL_READ);
    m_oldContext = eglGetCurrentContext();
}

AutoRestoreGLContext::~AutoRestoreGLContext()
{
    eglMakeCurrent(m_oldDisplay, m_oldDrawSurface, m_oldReadSurface, m_oldContext);
}

} // namespace WebCore
