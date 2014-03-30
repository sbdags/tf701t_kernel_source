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

#ifndef GLContext_h
#define GLContext_h

#include "ResourceLimits.h"
#include <EGL/egl.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class GLContext {
    WTF_MAKE_NONCOPYABLE(GLContext);

public:
    enum ContextAttribute {
        EnableRobustness = 1 << 0,
        LowPriority = 1 << 1
    };
    typedef unsigned ContextAttributes;

    static PassOwnPtr<GLContext> create(ResourceLimits::Context resourceContext, ContextAttributes attributes = 0, EGLContext sharedContext = EGL_NO_CONTEXT)
    {
        bool success;
        OwnPtr<GLContext> context(new GLContext(resourceContext, attributes, sharedContext, success));
        if (!success)
            return 0;
        return context.release();
    }
    ~GLContext();

    ContextAttributes attributes() const { return m_attributes; }
    EGLContext context() const { return m_context; }
    EGLDisplay display() const { return m_display; }

    bool isCurrent() const;
    bool makeCurrent();

    bool hasExtension(const char* extensionName);

private:
    GLContext(ResourceLimits::Context, ContextAttributes, EGLContext sharedContext, bool& success);

    ContextAttributes m_attributes;
    EGLContext m_context;
    EGLDisplay m_display;
    HashSet<String> m_extensions;
    ResourceLimits::FileDescriptorGrant m_fileResources;
};

class AutoRestoreGLContext {
public:
    AutoRestoreGLContext();
    AutoRestoreGLContext(const OwnPtr<GLContext>& newContext);
    ~AutoRestoreGLContext();

private:
    void notePreviousContext();
    EGLDisplay m_oldDisplay;
    EGLSurface m_oldDrawSurface;
    EGLSurface m_oldReadSurface;
    EGLContext m_oldContext;
};

} // namespace WebCore

#endif // GLContext_h
