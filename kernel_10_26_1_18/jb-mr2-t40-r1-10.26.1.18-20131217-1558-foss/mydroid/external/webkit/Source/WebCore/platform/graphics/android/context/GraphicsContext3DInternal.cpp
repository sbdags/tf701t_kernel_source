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
#include "GraphicsContext3DInternal.h"

#define LOG_TAG "GraphicsContext3DInternal"
#include <cutils/log.h>

#include "AndroidProperties.h"
#include "AutoRestoreGLState.h"
#include "CanvasRenderingContext.h"
#include "EGLImage.h"
#include "EGLImageBuffer.h"
#include "EGLImageBufferRing.h"
#include "Extensions3D.h"
#include "GLUtils.h"
#include "GraphicsContext3D.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "PlatformGraphicsContext.h"
#include "SkBitmapRef.h"
#include "SkColorPriv.h"
#include "SkDevice.h"
#include "VideoSurface.h"
#include <sys/mman.h>
#include <wtf/CurrentTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

using WTF::makeLambda;

// #define LOG_SHADER_COMPILATION_FAILURES

namespace WebCore {

// DrawFlushThreshold defines how many glDrawArrays, glDrawElements, and
// glClear commands can be executed before the framework forces a glFlush to
// robustness timeouts in valid draw cases.
static const unsigned DrawFlushThreshold = 50;

// The size of the mapped area used to clear allocated memory (64MB).
static const GLsizeiptr staticZeroSize = 4096 * 4096 * 4;

static uint8_t* createReadOnlyZeroArray(size_t size)
{
    void* result = mmap(0, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (result == reinterpret_cast<void*>(-1)) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    return reinterpret_cast<uint8_t*>(result);
}

static uint8_t* readOnlyZeroArray()
{
    AtomicallyInitializedStatic(uint8_t*, staticZero = createReadOnlyZeroArray(staticZeroSize));
    return staticZero;
}

PassOwnArrayPtr<uint8_t> zeroArray(size_t size)
{
    OwnArrayPtr<uint8_t> zero = adoptArrayPtr(new uint8_t[size]);
    memset(zero.get(), 0, size);
    return zero.release();
}

class GraphicsContextLowMemoryKiller {
    WTF_MAKE_NONCOPYABLE(GraphicsContextLowMemoryKiller);

public:
    static GraphicsContextLowMemoryKiller* instance()
    {
        static GraphicsContextLowMemoryKiller* instance = 0;
        if (!instance)
            instance = new GraphicsContextLowMemoryKiller();
        return instance;
    }

    void contextDestroyed(GraphicsContext3DInternal* context)
    {
        removeContext(context);
    }

    void setContextInForeground(GraphicsContext3DInternal* context)
    {
        removeContext(context);
        m_foregroundContexts.append(context);
    }

    void setContextInBackground(GraphicsContext3DInternal* context)
    {
        removeContext(context);
        m_backgroundContexts.append(context);
    }

    GraphicsContext3DInternal* resetLRUContext()
    {
        GraphicsContext3DInternal* candidate = 0;

        // If we have a background context reset the first in the list, which is the LRU context
        if (!m_backgroundContexts.isEmpty()) {
            candidate = m_backgroundContexts[0];
            m_backgroundContexts.remove(0);
            ALOGV("WebGL resetting background context %p due to high memory usage", candidate);
            candidate->forceResetContext(Extensions3D::INNOCENT_CONTEXT_RESET_ARB);
        }

        // If no background context is found reset the oldest foreground context
        if (!m_foregroundContexts.isEmpty()) {
            candidate = m_foregroundContexts[0];
            m_foregroundContexts.remove(0);
            ALOGV("WebGL resetting foreground context %p due to high memory usage, reset not allowed", candidate);
            candidate->forceResetContext(Extensions3D::GUILTY_CONTEXT_RESET_ARB);
        }

        if (candidate && candidate->m_thread)
            candidate->m_thread->finish();

        return candidate;
    }

private:
    GraphicsContextLowMemoryKiller()
    {
    }

    void removeContext(GraphicsContext3DInternal* context)
    {
        for (unsigned int i = 0; i < m_backgroundContexts.size(); i++) {
            if (m_backgroundContexts[i] == context) {
                m_backgroundContexts.remove(i);
                return;
            }
        }

        for (unsigned int i = 0; i < m_foregroundContexts.size(); i++) {
            if (m_foregroundContexts[i] == context) {
                m_foregroundContexts.remove(i);
                return;
            }
        }
    }

    WTF::Vector<GraphicsContext3DInternal*> m_foregroundContexts;
    WTF::Vector<GraphicsContext3DInternal*> m_backgroundContexts;
};

GraphicsContext3DInternal::GraphicsContext3DInternal(GraphicsContext3D* hostContext, const GraphicsContext3D::Attributes& attrs, bool& success)
    : EGLImageSurface(IntSize(0, 0))
    , m_attrs(attrs)
    , m_frameHasContent(false)
    , m_fbo(0)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
    , m_fboBinding(0)
    , m_enabledGLOESStandardDerivatives(false)
    , m_hostContext(hostContext)
    , m_forcedContextLostReason(GL_NO_ERROR)
    , m_inBackground(false)
    , m_drawCount(0)
    , m_contextLostStatus(ContextIntact)
{
    if (!AndroidProperties::getStringProperty("webkit.canvas.webgl", "").contains("noparallel"))
        m_thread = Thread::create("GraphicsContext3DInternal");

    success = call(makeLambda(this, &GraphicsContext3DInternal::initContextT)());

    GraphicsContextLowMemoryKiller::instance()->setContextInForeground(this);
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    call(makeLambda(this, &GraphicsContext3DInternal::destroyGLContextT)()); // We have to call(), because destroyGLContextT depends on m_thread.
    m_thread.clear();
    m_hostContext = 0;
    GraphicsContextLowMemoryKiller::instance()->contextDestroyed(this);
}

bool GraphicsContext3DInternal::initContextT()
{
    // WebGL does not support antialias in this implementation
    m_attrs.antialias = false;

    m_context = GLContext::create(ResourceLimits::WebContent, GLContext::EnableRobustness);
    if (!m_context) {
        ALOGE("Aborting WebGL: Failed to create an OpenGL context");
        return false;
    }

    if (!m_context->hasExtension("GL_EXT_robustness")) {
        ALOGE("Aborting WebGL: No support for GL_EXT_robustness");
        m_context.clear();
        return false;
    }

    m_backBuffer = EGLImageBufferFromTexture::create(IntSize(0, 0), m_attrs.alpha);
    if (!m_backBuffer) {
        m_context.clear();
        return false;
    }
    m_backBuffer->lockSurface();

    if (m_attrs.depth) {
        glGenRenderbuffers(1, &m_depthBuffer);
        ASSERT(m_depthBuffer);
    }

    if (m_attrs.stencil) {
        glGenRenderbuffers(1, &m_stencilBuffer);
        ASSERT(m_stencilBuffer);
    }

    // m_fbo and m_fboBinding both start as 0. m_fbo == 0 means the WebGL backing FBO is invalid. In
    // this case all the operations on WebGL FBO are done with FBO 0.

    // The m_fboBinding always points to the FBO currently intended to be bound by WebGL. If WebGL
    // has bound FBO 0, then m_fboBinding should equal m_fbo. If m_fbo changes when m_fboBinding ==
    // m_fbo, then m_fboBinding should change too. Also, at any given moment entering or exiting
    // GraphicsContext3DInternal function the FBO should be bound to m_fboBinding.
    // The values should be accessed only from the GL thread.
    return true;
}

void GraphicsContext3DInternal::setBackgroundModeCallback(PassOwnPtr<GraphicsContext3D::BackgroundModeCallback> callback)
{
    m_backgroundModeCallback = callback;
}

void GraphicsContext3DInternal::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
{
    m_contextLostCallback = callback;
}

void webglTexImage2DResourceSafe(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, unsigned int imageSize)
{
    if (!width || !height)
        return glTexImage2D(target, level, internalformat, width, height, border, format, type, 0);

    // FIXME: When OES_texture_(half_)float is exposed to WebGL, update this clearing code to accordingly.
    ASSERT((type != GL_FLOAT && type != GL_HALF_FLOAT_OES)
           || (type == GL_HALF_FLOAT_OES && (format == GL_ALPHA || format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA))
           || (type == GL_FLOAT && (format == GL_ALPHA || format == GL_LUMINANCE)));

    if (level) {
        // FBO's can't render to non-zero levels.
        return glTexImage2D(target, level, internalformat, width, height, border, format, type, readOnlyZeroArray());
    }

    glTexImage2D(target, level, internalformat, width, height, border, format, type, 0);

    GLint boundTexture;
    GLint lastFBO;
    GLenum bindingName = (target == GL_TEXTURE_2D) ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_CUBE_MAP;
    glGetIntegerv(bindingName, &boundTexture);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFBO);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, boundTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // This is most likely a sign of robustness timeout having been hit,
        // but fallback to a different clear just in case.
        glTexSubImage2D(target, 0, 0, 0, width, height, format, type, readOnlyZeroArray());
    } else
        GLUtils::clearRect(GL_COLOR_BUFFER_BIT, 0, 0, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, lastFBO);
    glDeleteFramebuffers(1, &fbo);
}

bool GraphicsContext3DInternal::texImage2DResourceSafe(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLint unpackAlignment)
{
    unsigned imageSize;
    GLenum error = m_hostContext->computeImageSizeInBytes(format, type, width, height, unpackAlignment, &imageSize, 0);
    if (error != GraphicsContext3D::NO_ERROR) {
        synthesizeGLError(error);
        return false;
    }
    ASSERT(imageSize <= static_cast<unsigned>(staticZeroSize));
    if (!ensureEnoughGraphicsMemory(imageSize) || !readOnlyZeroArray())
        return false;
    push(makeLambda(webglTexImage2DResourceSafe)(target, level, internalformat, width, height, border, format, type, imageSize));

    return true;
}

bool GraphicsContext3DInternal::validateShaderLocation(const String& string)
{
    static const size_t MaxLocationStringLength = 256;
    if (string.length() > MaxLocationStringLength) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return true;
}

static void webglBindAttribLocation(GLuint program, GLuint index, PassOwnArrayPtr<char> name)
{
    OwnArrayPtr<char> ownName = name;
    glBindAttribLocation(program, index, ownName.get());
}

void GraphicsContext3DInternal::bindAttribLocation(GLuint program, GLuint index, const String& name)
{
    if (!validateShaderLocation(name))
        return;

    CString utf8 = name.utf8();
    push(makeLambda(webglBindAttribLocation)(program, index, WTF::makeLambdaArrayArg(utf8.data(), utf8.length() + 1)));
}

// Note for the FBO functions below: FBO 0 is a special case: The default FBO
// in WebGL is actually GraphicsContext3DInternal::m_fbo. If that
// is bound, we wrap FBO ops to behave as if FBO 0 were bound.

void GraphicsContext3DInternal::bindFramebufferT(GLuint fbo)
{
    // This is mostly just a forward to GL with whatever FBO caller wants. The only difference is
    // that if caller asks for 0, we actually bind m_fbo.
    if (!fbo)
        fbo = m_fbo;

    if (m_fboBinding != fbo) {
        m_fboBinding = fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
    }
#if !ASSERT_DISABLED
    else {
        GLuint currentBinding;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&currentBinding));
        ASSERT(currentBinding == m_fboBinding);
    }
#endif
}

void GraphicsContext3DInternal::bindFramebuffer(GLenum target, GLuint fbo)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    push(makeLambda(this, &GraphicsContext3DInternal::bindFramebufferT)(fbo));
}

static void webglBufferDataResourceSafe(GLenum target, GLsizeiptr size, GLenum usage)
{
    // We can clear it in one pass.
    if (staticZeroSize >= size) {
        glBufferData(target, size, readOnlyZeroArray(), usage);
        return;
    }

    // Otherwise clear it in chunks.
    GLintptr offset = 0;
    while (size > 0) {
        GLsizeiptr blockSize = (size < staticZeroSize) ? size : staticZeroSize;

        glBufferSubData(target, offset, blockSize, readOnlyZeroArray());

        offset += blockSize;
        size -= blockSize;
    }
}

void GraphicsContext3DInternal::bufferData(GLenum target, GLsizeiptr size, GLenum usage)
{
    if (!ensureEnoughGraphicsMemory(size) || !readOnlyZeroArray()) {
        synthesizeGLError(GraphicsContext3D::OUT_OF_MEMORY);
        return;
    }

    push(makeLambda(webglBufferDataResourceSafe)(target, size, usage));
}

static void webglBufferData(GLenum target, GLsizeiptr size, PassOwnArrayPtr<char> data, GLenum usage)
{
    OwnArrayPtr<char> ownData = data;
    glBufferData(target, size, ownData.get(), usage);
}

void GraphicsContext3DInternal::bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    push(makeLambda(webglBufferData)(target, size, WTF::makeLambdaArrayArg(data, size), usage));
}

static void webglBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, PassOwnArrayPtr<char> data)
{
    OwnArrayPtr<char> ownData = data;
    glBufferSubData(target, offset, size, ownData.get());
}

void GraphicsContext3DInternal::bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    push(makeLambda(webglBufferSubData)(target, offset, size, WTF::makeLambdaArrayArg(data, size)));
}

GLenum GraphicsContext3DInternal::checkFramebufferStatusT()
{
    if (m_fboBinding == m_fbo)
        return GL_FRAMEBUFFER_COMPLETE;

    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

GLenum GraphicsContext3DInternal::checkFramebufferStatus(GLenum target)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return GraphicsContext3D::NONE;
    }

    return call(makeLambda(this, &GraphicsContext3DInternal::checkFramebufferStatusT)());
}

void GraphicsContext3DInternal::initCompilerT()
{
    if (m_compiler)
        return;

    m_compiler = adoptPtr(new ANGLEWebKitBridge());
    ShBuiltInResources resources;
    ShInitBuiltInResources(&resources);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &resources.MaxVertexAttribs);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &resources.MaxVertexUniformVectors);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &resources.MaxVaryingVectors);
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &resources.MaxVertexTextureImageUnits);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &resources.MaxCombinedTextureImageUnits);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &resources.MaxTextureImageUnits);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &resources.MaxFragmentUniformVectors);
    resources.OES_standard_derivatives = m_enabledGLOESStandardDerivatives ? 1 : 0;
    // Always set to 1 for OpenGL ES.
    resources.MaxDrawBuffers = 1;
    m_compiler->setResources(resources);
}

void GraphicsContext3DInternal::compileShaderT(GLuint shader)
{
    int GLshaderType;
    ANGLEShaderType shaderType;

    glGetShaderiv(shader, GL_SHADER_TYPE, &GLshaderType);
    if (GLshaderType == GL_VERTEX_SHADER)
        shaderType = SHADER_TYPE_VERTEX;
    else if (GLshaderType == GL_FRAGMENT_SHADER)
        shaderType = SHADER_TYPE_FRAGMENT;
    else
        return; // Invalid shader type.

    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
        return;

    ShaderSourceEntry& entry = result->second;
    String translatedShaderSource;
    String shaderInfoLog;

    initCompilerT();
    bool isValid = m_compiler->validateShaderSource(entry.source.utf8().data(), shaderType, translatedShaderSource, shaderInfoLog, SH_ESSL_OUTPUT);

    entry.log = shaderInfoLog;
    entry.isValid = isValid;

    if (!isValid)
        return; // Shader didn't validate, don't move forward with compiling translated source

    int translatedShaderLength = translatedShaderSource.length();

    const CString& translatedShaderCString = translatedShaderSource.utf8();
    const char* translatedShaderPtr = translatedShaderCString.data();

    glShaderSource(shader, 1, &translatedShaderPtr, &translatedShaderLength);

    glCompileShader(shader);

    int GLCompileSuccess;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &GLCompileSuccess);

#if LOG_SHADER_COMPILATION_FAILURES
    // OpenGL might not accept the shader even though it was validated by ANGLE, probably
    // due to usage of functionality not supported by the hardware.
    if (GLCompileSuccess != GL_TRUE)
        ALOGE("OpenGL shader compilation failed for an ANGLE validated %s shader", (shaderType == SHADER_TYPE_VERTEX) ? "vertex" : "fragment");
#endif
}

void GraphicsContext3DInternal::compileShader(GLuint shader)
{
    push(makeLambda(this, &GraphicsContext3DInternal::compileShaderT)(shader));
}

void GraphicsContext3DInternal::compressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* pixels)
{
    ASSERT(pixels);
    if (!ensureEnoughGraphicsMemory(imageSize))
        return;
    call(makeLambda(glCompressedTexImage2D)(target, level, internalformat, width, height, border, imageSize, pixels));
}

void GraphicsContext3DInternal::compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* pixels)
{
    if (pixels) {
        if (!ensureEnoughGraphicsMemory(imageSize))
            return;
        call(makeLambda(glCompressedTexSubImage2D)(target, level, xoffset, yoffset, width, height, format, imageSize, pixels));
    }
}

void GraphicsContext3DInternal::copyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    if (!ensureEnoughGraphicsMemory(0))
        return;
    call(makeLambda(glCopyTexImage2D)(target, level, internalformat, x, y, width, height, border));
}

void GraphicsContext3DInternal::drawArraysT(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
    incrementDrawCountT();
}

void GraphicsContext3DInternal::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    // Send "1" to make sure the GL thread wakes up and starts drawing immediately.
    push(makeLambda(this, &GraphicsContext3DInternal::drawArraysT)(mode, first, count), 1);
}

void GraphicsContext3DInternal::drawElementsT(GLenum mode, GLsizei count, GLenum type, GLintptr offset)
{
    glDrawElements(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
    incrementDrawCountT();
}

void GraphicsContext3DInternal::drawElements(GLenum mode, GLsizei count, GLenum type, GLintptr offset)
{
    // Send "1" to make sure the GL thread wakes up and starts drawing immediately.
    push(makeLambda(this, &GraphicsContext3DInternal::drawElementsT)(mode, count, type, offset), 1);
}

void GraphicsContext3DInternal::flushT()
{
    glFlush();
    m_drawCount = 0;
}

void GraphicsContext3DInternal::flush()
{
    push(makeLambda(this, &GraphicsContext3DInternal::flushT)(), 1);
}

void GraphicsContext3DInternal::finishT()
{
    glFinish();
    m_drawCount = 0;
}

void GraphicsContext3DInternal::finish()
{
    push(makeLambda(this, &GraphicsContext3DInternal::finishT)(), 1);
}

void GraphicsContext3DInternal::framebufferRenderbufferT(GLenum attachment, GLuint renderbuffertarget, GLuint rbo)
{
    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, renderbuffertarget, rbo);

    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
}

void GraphicsContext3DInternal::framebufferRenderbuffer(GLenum target, GLenum attachment, GLuint renderbuffertarget, GLuint rbo)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    push(makeLambda(this, &GraphicsContext3DInternal::framebufferRenderbufferT)(attachment, renderbuffertarget, rbo));
}

void GraphicsContext3DInternal::framebufferTexture2DT(GLenum attachment, GLuint textarget, GLuint texture, GLint level)
{
    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, texture, level);

    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
}

void GraphicsContext3DInternal::framebufferTexture2D(GLenum target, GLenum attachment, GLuint textarget, GLuint texture, GLint level)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    push(makeLambda(this, &GraphicsContext3DInternal::framebufferTexture2DT)(attachment, textarget, texture, level));
}

static bool webglGetActiveAttrib(GLuint program, GLuint index, ActiveInfo& info)
{
    GLint maxAttributeSize = 0;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    GLchar name[maxAttributeSize]; // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength).crossThreadString();
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3DInternal::getActiveAttrib(GLuint program, GLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return call(makeLambda(webglGetActiveAttrib)(program, index, info));
}

static bool webglGetActiveUniform(GLuint program, GLuint index, ActiveInfo& info)
{
    GLint maxUniformSize = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize]; // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength).crossThreadString();
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3DInternal::getActiveUniform(GLuint program, GLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return call(makeLambda(webglGetActiveUniform)(program, index, info));
}

int GraphicsContext3DInternal::getAttribLocation(GLuint program, const String& name)
{
    if (!validateShaderLocation(name))
        return -1;
    CString utf8 = name.utf8();
    return call(makeLambda(glGetAttribLocation)(program, utf8.data()));
}

GLenum GraphicsContext3DInternal::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        GLenum err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }
    return call(makeLambda(glGetError)());
}

void GraphicsContext3DInternal::getFramebufferAttachmentParameterivT(GLenum attachment, GLenum pname, int* value)
{
    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, pname, value);

    if (m_fboBinding == m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
}

void GraphicsContext3DInternal::getFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, int* value)
{
    if (target != GL_FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    call(makeLambda(this, &GraphicsContext3DInternal::getFramebufferAttachmentParameterivT)(attachment, pname, value));
}

void GraphicsContext3DInternal::getIntegervT(GLenum pname, GLint* params)
{
    glGetIntegerv(pname, params);
    if (pname == GL_FRAMEBUFFER_BINDING && static_cast<GLuint>(*params) == m_fbo)
        *params = 0;
}

void GraphicsContext3DInternal::getIntegerv(GLenum pname, GLint* params)
{
    call(makeLambda(this, &GraphicsContext3DInternal::getIntegervT)(pname, params));
}

static void webglGetProgramInfoLog(GLuint program, String& retValue)
{
    GLint logSize = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize > 1) {
        GLsizei returnedLength;
        OwnArrayPtr<GLchar> log = adoptArrayPtr(new GLchar[logSize]);
        glGetProgramInfoLog(program, logSize, &returnedLength, log.get());
        ASSERT(logSize == 1 + returnedLength);
        retValue = String(log.get()).crossThreadString();
    }
}

String GraphicsContext3DInternal::getProgramInfoLog(GLuint program)
{
    String retValue;
    call(makeLambda(webglGetProgramInfoLog)(program, retValue));
    if (retValue.isNull())
        return "";
    return retValue;
}

bool GraphicsContext3DInternal::getShaderivT(GLuint shader, GLenum pname, GLint* value)
{
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    switch (pname) {
    case GraphicsContext3D::DELETE_STATUS:
    case GraphicsContext3D::SHADER_TYPE:
        glGetShaderiv(shader, pname, value);
        break;

    case GraphicsContext3D::COMPILE_STATUS:
        if (result == m_shaderSourceMap.end()) {
            *value = static_cast<int>(false);
            break;
        }

        *value = static_cast<int>(result->second.isValid);
        break;

    case GraphicsContext3D::INFO_LOG_LENGTH: {
        if (result == m_shaderSourceMap.end()) {
            *value = 0;
            break;
        }
        String retValue;
        getShaderInfoLogT(shader, retValue);
        *value = retValue.length();
        break;
    }

    case GraphicsContext3D::SHADER_SOURCE_LENGTH: {
        String retValue;
        getShaderSourceT(shader, retValue);
        *value = retValue.length();
        break;
    }

    default:
        return false;
    }

    return true;
}

void GraphicsContext3DInternal::getShaderiv(GLuint shader, GLuint pname, GLint* value)
{
    if (!call(makeLambda(this, &GraphicsContext3DInternal::getShaderivT)(shader, pname, value)))
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
}

void GraphicsContext3DInternal::getShaderInfoLogT(GLuint shader, String& retValue)
{
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return;

    ShaderSourceEntry entry = result->second;
    if (!entry.isValid) {
        retValue = entry.log.crossThreadString();
        return;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return;

    GLsizei size = 0;
    OwnArrayPtr<GLchar> info = adoptArrayPtr(new GLchar[length]);
    glGetShaderInfoLog(shader, length, &size, info.get());

    retValue = String(info.get()).crossThreadString();
}

String GraphicsContext3DInternal::getShaderInfoLog(GLuint shader)
{
    String retValue;
    call(makeLambda(this, &GraphicsContext3DInternal::getShaderInfoLogT)(shader, retValue));
    if (retValue.isNull())
        return "";
    return retValue;
}

void GraphicsContext3DInternal::getShaderSourceT(GLuint shader, String& retValue)
{
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return;

    retValue = result->second.source.crossThreadString();
}

String GraphicsContext3DInternal::getShaderSource(GLuint shader)
{
    String retValue;
    call(makeLambda(this, &GraphicsContext3DInternal::getShaderSourceT)(shader, retValue));
    if (retValue.isNull())
        return "";
    return retValue;
}

String GraphicsContext3DInternal::getString(GLenum name)
{
    // might want to consider returning our own strings in the future
    return reinterpret_cast<const char*>(call(makeLambda(glGetString)(name)));
}

GLint GraphicsContext3DInternal::getUniformLocation(GLuint program, const String& name)
{
    if (!validateShaderLocation(name))
        return -1;
    CString utf8 = name.utf8();
    return call(makeLambda(glGetUniformLocation)(program, utf8.data()));
}

long GraphicsContext3DInternal::getVertexAttribOffset(GLuint index, GLenum pname)
{
    void* ret = 0;
    call(makeLambda(glGetVertexAttribPointerv)(index, pname, &ret));
    return static_cast<long>(reinterpret_cast<intptr_t>(ret));
}

void GraphicsContext3DInternal::releaseShaderCompilerT()
{
    m_compiler.clear();
    glReleaseShaderCompiler();
}

void GraphicsContext3DInternal::releaseShaderCompiler()
{
    push(makeLambda(this, &GraphicsContext3DInternal::releaseShaderCompilerT)());
}

static void webglRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);

    // WebGL security dictates that we clear the buffer after allocation.
    if (!width || !height)
        return;

    GLuint oldFbo, tempFbo, rbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&oldFbo));
    glGetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint*>(&rbo));
    glGenFramebuffers(1, &tempFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, tempFbo);

    GLenum attachment = 0, clearBuffer = 0;
    switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
        attachment = GL_DEPTH_ATTACHMENT;
        clearBuffer = GL_DEPTH_BUFFER_BIT;
        break;
    case GL_STENCIL_INDEX8:
        attachment = GL_STENCIL_ATTACHMENT;
        clearBuffer = GL_STENCIL_BUFFER_BIT;
        break;
    case GL_RGBA4:
    case GL_RGB565:
    case GL_RGB5_A1:
        attachment = GL_COLOR_ATTACHMENT0;
        clearBuffer = GL_COLOR_BUFFER_BIT;
        break;
    }
    ASSERT(attachment && clearBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        GLUtils::clearRect(clearBuffer, 0, 0, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, oldFbo);
    glDeleteFramebuffers(1, &tempFbo);
}

void GraphicsContext3DInternal::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    push(makeLambda(webglRenderbufferStorage)(target, internalformat, width, height));
}

// Use 'String' instead of 'const String&' to get a copy as the invocation is parallel.
void GraphicsContext3DInternal::shaderSourceT(GLuint shader, String source)
{
    ShaderSourceEntry entry;
    entry.source = source;
    m_shaderSourceMap.set(shader, entry);
}

void GraphicsContext3DInternal::shaderSource(GLuint shader, const String& source)
{
    push(makeLambda(this, &GraphicsContext3DInternal::shaderSourceT)(shader, source.crossThreadString()));
}

bool GraphicsContext3DInternal::texImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
{
    ASSERT(pixels);
    unsigned imageSize;
    GLenum error = m_hostContext->computeImageSizeInBytes(format, type, width, height, 1 /*unpackAlignment*/, &imageSize, 0);
    if (error != GraphicsContext3D::NO_ERROR) {
        synthesizeGLError(error);
        return false;
    }
    if (!ensureEnoughGraphicsMemory(imageSize))
        return false;

    call(makeLambda(glTexImage2D)(target, level, internalformat, width, height, border, format, type, pixels));

    return true;
}

bool GraphicsContext3DInternal::texImage2DVideo(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, VideoSurface* videoSurface, bool flipY, bool premultiplyAlpha)
{
    ASSERT(videoSurface);
    // "If a packed pixel format is specified which would imply loss of bits of
    // precision from the image data, this loss of precision must occur."
    if (type != GL_UNSIGNED_BYTE)
        return false;

    // Framebuffers can only render to level 0.
    if (level != 0)
        return false;

    unsigned imageSize;
    GLenum error = m_hostContext->computeImageSizeInBytes(format, type, width, height, 1 /*unpackAlignment*/, &imageSize, 0);
    if (error != GraphicsContext3D::NO_ERROR) {
        synthesizeGLError(error);
        return false;
    }

    if (!ensureEnoughGraphicsMemory(imageSize))
        return false;

    // Ignore premultiplyAlpha since android doesn't support any video formats
    // that allow transparency.
    push(makeLambda(this, &GraphicsContext3DInternal::texImage2DVideoT)(target, internalformat, width, height, border, format, videoSurface, flipY));

    return true;
}

void GraphicsContext3DInternal::texImage2DVideoT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, android::sp<VideoSurface> videoSurface, bool flipY)
{
    glTexImage2D(target, 0, internalformat, width, height, border, format, GL_UNSIGNED_BYTE, 0);

    GLint textureId;
    glGetIntegerv(target == GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_CUBE_MAP, &textureId);

    IntRect destRect;
    if (flipY)
        destRect = IntRect(0, 0, width, height);
    else
        destRect = IntRect(0, height, width, -height);

    if (!m_copyVideoSurface) {
        m_copyVideoSurface = CopyVideoSurface::create(m_context->context());
        if (!m_copyVideoSurface)
            return;
    }

    m_copyVideoSurface->copyCurrentFrame(target, textureId, videoSurface, destRect);
}

void GraphicsContext3DInternal::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
{
    // We could copy the array here and not have to block, but this will be faster as the image gets
    // larger.
    if (pixels)
        call(makeLambda(glTexSubImage2D)(target, level, xoffset, yoffset, width, height, format, type, pixels));
}

bool GraphicsContext3DInternal::texSubImage2DVideo(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, VideoSurface* videoSurface, bool flipY, bool premultiplyAlpha)
{
    ASSERT(videoSurface);
    // "If a packed pixel format is specified which would imply loss of bits of
    // precision from the image data, this loss of precision must occur."
    if (type != GL_UNSIGNED_BYTE)
        return false;

    // Framebuffers can only render to level 0.
    if (level != 0)
        return false;

    // Ignore premultiplyAlpha since android doesn't support any video formats
    // that allow transparency.
    push(makeLambda(this, &GraphicsContext3DInternal::texSubImage2DVideoT)(target, xoffset, yoffset, width, height, videoSurface, flipY));

    return true;
}

void GraphicsContext3DInternal::texSubImage2DVideoT(GLenum target, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, android::sp<VideoSurface> videoSurface, bool flipY)
{
    GLint textureId;
    glGetIntegerv(target == GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_CUBE_MAP, &textureId);

    IntRect destRect;
    if (flipY)
        destRect = IntRect(xoffset, yoffset, width, height);
    else
        destRect = IntRect(xoffset, yoffset + height, width, -height);

    if (!m_copyVideoSurface) {
        m_copyVideoSurface = CopyVideoSurface::create(m_context->context());
        if (!m_copyVideoSurface)
            return;
    }

    m_copyVideoSurface->copyCurrentFrame(target, textureId, videoSurface, destRect);
}

void GraphicsContext3DInternal::vertexAttrib1fv(GLuint index, GLfloat* v)
{
    push(makeLambda(glVertexAttrib1f)(index, v[0]));
}

void GraphicsContext3DInternal::vertexAttrib2fv(GLuint index, GLfloat* v)
{
    push(makeLambda(glVertexAttrib2f)(index, v[0], v[1]));
}

void GraphicsContext3DInternal::vertexAttrib3fv(GLuint index, GLfloat* v)
{
    push(makeLambda(glVertexAttrib3f)(index, v[0], v[1], v[2]));
}

void GraphicsContext3DInternal::vertexAttrib4fv(GLuint index, GLfloat* v)
{
    push(makeLambda(glVertexAttrib4f)(index, v[0], v[1], v[2], v[3]));
}

void GraphicsContext3DInternal::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
    push(makeLambda(glVertexAttribPointer)(index, size, type, normalized, stride,
                                                     reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
}

static GLuint reshapeRenderbufferStorage(GLuint rbo, GLenum format, int width, int height)
{
    GLuint oldRbo;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint*>(&oldRbo));
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, oldRbo);
    return rbo;
}

void GraphicsContext3DInternal::reshapeT(IntSize newSize)
{
    if (m_contextLostStatus != ContextIntact)
        return;

    if (m_backBuffer->size() == newSize)
        return;

    bufferRing()->deleteFreeBuffers();

    GLenum clearBuffers = GL_COLOR_BUFFER_BIT;
    if (m_depthBuffer) {
        reshapeRenderbufferStorage(m_depthBuffer, GL_DEPTH_COMPONENT16, newSize.width(), newSize.height());
        clearBuffers |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_stencilBuffer) {
        reshapeRenderbufferStorage(m_stencilBuffer, GL_STENCIL_INDEX8, newSize.width(), newSize.height());
        clearBuffers |= GL_STENCIL_BUFFER_BIT;
    }

    m_backBuffer.clear();
    m_backBuffer = EGLImageBufferFromTexture::create(newSize, m_attrs.alpha);
    if (!m_backBuffer) {
        // Either insufficient memory or context is broken
        m_contextLostStatus = ContextLost;
        return;
    }
    m_backBuffer->lockSurface();

    if (m_backBuffer->size().isEmpty()) {
        glDeleteFramebuffers(1, &m_fbo); // If framebuffer binding was to m_fbo, this causes it to revert to 0.

        // When the m_fbo is not valid, the default fbo is 0.
        // If the current binding was to the default fbo (m_fbo), change it to point to new value of default fbo.
        if (m_fboBinding == m_fbo)
            m_fboBinding = 0;
        m_fbo = 0;
        return;
    }

    if (!m_fbo) {
        glGenFramebuffers(1, &m_fbo);
        // When the m_fbo is not valid, the default fbo is 0. Thus if the current binding was to the default fbo,
        // change it to point to the new default fbo == m_fbo.
        if (!m_fboBinding)
            m_fboBinding = m_fbo;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backBuffer->sourceContextTextureId(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ASSERT_NOT_REACHED();
        m_contextLostStatus = ContextLost;
        return;
    } else
        GLUtils::clearRect(clearBuffers, 0, 0, newSize.width(), newSize.height());

    if (m_fboBinding != m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
}

void GraphicsContext3DInternal::reshape(int width, int height)
{
    IntSize newSize(width, height);

    if (newSize == size())
        return;

    call(makeLambda(this, &GraphicsContext3DInternal::reshapeT)(newSize));

    if (m_contextLostStatus != ContextIntact) {
        handleContextLossIfNeeded();
        return;
    }

    markContextChanged();

    updateSize(newSize);
}

void GraphicsContext3DInternal::markContextChanged()
{
    m_frameHasContent = true;
}

void GraphicsContext3DInternal::markLayerComposited()
{
    // This will only be reached if we aren't doing accelerated compositing
    // into a layer (so never). However, if we swap buffers buffer here
    // WebGL will still have the correct behavior (with just a little bit of
    // unnecessary extra work) if somebody does do compositing that way.
    swapBuffers();
}

bool GraphicsContext3DInternal::layerComposited() const
{
    // Since the surface is double-buffered, "layerComposited" really isn't
    // what WebGLRenderingContext wants to know. What it's asking is if the
    // frame is brand new (it clears each new frame).
    return !m_frameHasContent;
}

PassRefPtr<ImageData> GraphicsContext3DInternal::paintRenderingResultsToImageData()
{
    return readBackFramebuffer(GraphicsContext3DInternal::TopToBottom, GraphicsContext3DInternal::AlphaNotPremultiplied);
}

void GraphicsContext3DInternal::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    RefPtr<ImageData> image = readBackFramebuffer(GraphicsContext3DInternal::TopToBottom, GraphicsContext3DInternal::AlphaPremultiplied);

    if (!image)
        return;

    SkBitmap sourceBitmap;
    sourceBitmap.setConfig(SkBitmap::kARGB_8888_Config, image->width(), image->height(), 4 * image->width());
    sourceBitmap.setPixels(image->data()->data()->data());
    sourceBitmap.setIsOpaque(!hasAlpha());

    context->canvas()->buffer()->context()->platformContext()->prepareForDrawing();
    PlatformGraphicsContext* canvas = context->canvas()->buffer()->context()->platformContext();
    canvas->writePixels(sourceBitmap, 0, 0, SkCanvas::kNative_Premul_Config8888);
}

void GraphicsContext3DInternal::clearT(GLbitfield buffers)
{
    glClear(buffers);
    incrementDrawCountT();
}

void GraphicsContext3DInternal::clear(GLbitfield buffers)
{
    push(makeLambda(this, &GraphicsContext3DInternal::clearT)(buffers), 1);
}

GLuint GraphicsContext3DInternal::createBuffer()
{
    GLuint o;
    call(makeLambda(glGenBuffers)(1, &o));
    return o;
}

GLuint GraphicsContext3DInternal::createFramebuffer()
{
    GLuint o;
    call(makeLambda(glGenFramebuffers)(1, &o));
    return o;
}

GLuint GraphicsContext3DInternal::createRenderbuffer()
{
    GLuint o;
    call(makeLambda(glGenRenderbuffers)(1, &o));
    return o;
}

GLuint GraphicsContext3DInternal::createShader(GLenum type)
{
    return call(makeLambda(glCreateShader)(type));
}

GLuint GraphicsContext3DInternal::createTexture()
{
    GLuint o;
    call(makeLambda(glGenTextures)(1, &o));
    return o;
}

void webglDeleteBuffer(GLuint o)
{
    glDeleteBuffers(1, &o);
}

void GraphicsContext3DInternal::deleteBuffer(GLuint buffer)
{
    push(makeLambda(webglDeleteBuffer)(buffer));
}

void webglDeleteFramebuffer(GLuint o)
{
    glDeleteFramebuffers(1, &o);
}

void GraphicsContext3DInternal::deleteFramebuffer(GLuint framebuffer)
{
    push(makeLambda(webglDeleteFramebuffer)(framebuffer));
}

void webglDeleteRenderbuffer(GLuint o)
{
    glDeleteRenderbuffers(1, &o);
}

void GraphicsContext3DInternal::deleteRenderbuffer(GLuint renderbuffer)
{
    push(makeLambda(webglDeleteRenderbuffer)(renderbuffer));
}

void GraphicsContext3DInternal::deleteShader(GLuint shader)
{
    push(makeLambda(glDeleteShader)(shader));
}

void webglDeleteTexture(GLuint o)
{
    glDeleteTextures(1, &o);
}

void GraphicsContext3DInternal::deleteTexture(GLuint texture)
{
    push(makeLambda(webglDeleteTexture)(texture));
}

IntSize GraphicsContext3DInternal::getInternalFramebufferSize() const
{
    return size();
}

PassOwnPtr<EGLImageBufferFromTexture> GraphicsContext3DInternal::createBackBufferT(PassOwnPtr<EGLImageBufferFromTexture> failedCandidate)
{
    failedCandidate.clear();

    if (m_contextLostStatus != ContextIntact)
        return 0;

    OwnPtr<EGLImageBufferFromTexture> newBackBuffer;
    do {
        newBackBuffer = static_pointer_cast<EGLImageBufferFromTexture>(bufferRing()->takeFreeBuffer());
    } while (newBackBuffer && newBackBuffer->size() != m_backBuffer->size());

    if (!newBackBuffer)
        newBackBuffer = EGLImageBufferFromTexture::create(m_backBuffer->size(), m_attrs.alpha);

    if (!newBackBuffer) {
        // Couldn't create a new back buffer, the caller should not submit the old one.
        m_contextLostStatus = ContextLost;
        return 0;
    }
    return newBackBuffer.release();
}

void GraphicsContext3DInternal::swapBuffersT()
{
    if (m_contextLostStatus != ContextIntact)
        return;

    flushT();

    if (glGetGraphicsResetStatusEXT() != GL_NO_ERROR) {
        m_contextLostStatus = ContextLost;
        return;
    }

    m_backBuffer->setFence();

    m_backBuffer->unlockSurface();

    OwnPtr<EGLImageBufferFromTexture> newBackBuffer =
        static_pointer_cast<EGLImageBufferFromTexture>(bufferRing()->takeFrontBufferAndLock());

    if (!newBackBuffer || newBackBuffer->size() != m_backBuffer->size()) {
        newBackBuffer.clear();
        newBackBuffer = EGLImageBufferFromTexture::create(m_backBuffer->size(), m_attrs.alpha);
    }

    bufferRing()->submitFrontBufferAndUnlock(m_backBuffer.release());

    m_backBuffer = newBackBuffer.release();

    if (!m_backBuffer) {
        m_contextLostStatus = ContextLost;
        return;
    }

    m_backBuffer->lockSurface();
}

void GraphicsContext3DInternal::setupNextBackBufferT(EGLImageBufferFromTexture* previousBackBuffer)
{
    flushT();

    if (m_contextLostStatus != ContextIntact || glGetGraphicsResetStatusEXT() != GL_NO_ERROR) {
        if (m_contextLostStatus == ContextIntact)
            m_contextLostStatus = ContextLost;
        previousBackBuffer->onSourceContextReset();
        previousBackBuffer->unlockSurface();
        return;
    }

    previousBackBuffer->setFence();

    m_backBuffer->lockSurface();

    // After this, the caller is free to do anything it wants to the old back buffer. The buffer
    // will be deleted in this thread, so it will be valid at least during the copy below.
    previousBackBuffer->unlockSurface();

    updateRenderTargetT();
}

void GraphicsContext3DInternal::updateRenderTargetT()
{
    ASSERT(m_contextLostStatus == ContextIntact);

    if (m_fboBinding != m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    m_backBuffer->finish();

    if (m_attrs.preserveDrawingBuffer) {
        // Copy the previous backbuffer (attached to m_fbo) to m_backBuffer.
        AutoRestoreTextureBinding2D bindTex2D(m_backBuffer->sourceContextTextureId());
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_backBuffer->size().width(), m_backBuffer->size().height());
    }

    // Attach m_backBuffer to m_fbo.
    if (!m_backBuffer->size().isEmpty()) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backBuffer->sourceContextTextureId(), 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            m_contextLostStatus = ContextLost;
            return;
        }
    }

    if (m_fboBinding != m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);
}

void GraphicsContext3DInternal::handleContextLossIfNeeded()
{
    ASSERT(m_contextLostStatus != ContextIntact);

    if (m_contextLostStatus < LostBuffersFreed) {
        call(makeLambda(this, &GraphicsContext3DInternal::deleteLostBuffersT)());
        // Front buffer was deleted, so layer contents changed
        m_hostContext->platformLayer()->viewInvalidate();
        m_contextLostStatus = LostBuffersFreed;
    }

    if (m_contextLostCallback && m_contextLostStatus < ContextLostCallbackNotified) {
        m_contextLostCallback->onContextLost();
        m_contextLostStatus = ContextLostCallbackNotified;
    }
}

void GraphicsContext3DInternal::swapBuffers()
{
    if (m_contextLostStatus != ContextIntact) {
        handleContextLossIfNeeded();
        return;
    }

    if (!m_frameHasContent)
        return;

    call(makeLambda(this, &GraphicsContext3DInternal::swapBuffersT)());

    if (m_contextLostStatus != ContextIntact) {
        handleContextLossIfNeeded();
        return;
    }

    push(makeLambda(this, &GraphicsContext3DInternal::updateRenderTargetT)(), 1);

    if (!m_attrs.preserveDrawingBuffer)
        m_frameHasContent = false;
}

void GraphicsContext3DInternal::submitBackBuffer()
{
    if (m_contextLostStatus != ContextIntact) {
        handleContextLossIfNeeded();
        return;
    }

    if (!m_frameHasContent)
        return;

    // Try to see if the buffer ring has an applicable back buffer. We can only peek one buffer, because
    // the unapplicable buffer needs to be destroyed or resized in the GL thread.
    OwnPtr<EGLImageBufferFromTexture*> newBackBuffer = static_pointer_cast<EGLImageBufferFromTexture>(bufferRing()->takeFreeBuffer());


    // It is unclear if the swap will succeed. Thus we need to wait for the result.
    // We are allowed to access m_backBuffer. We know previous setupNextBackBufferT
    // has finished modifying m_backBuffer because 'takeFreeBuffer' waits for it.
    if (!newBackBuffer || newBackBuffer->size() != m_backBuffer->size())
        newBackBuffer = call(makeLambda(this, &GraphicsContext3DInternal::createBackBufferT)(newBackBuffer.release()));
    if (!newBackBuffer) {
        ASSERT(m_contextLostStatus != ContextIntact);
        handleContextLossIfNeeded();
        return;
    }

    OwnPtr<EGLImageBufferFromTexture> previousBackBuffer = m_backBuffer.release();
    m_backBuffer = newBackBuffer.release();
    push(makeLambda(this, &GraphicsContext3DInternal::setupNextBackBufferT)(previousBackBuffer.get()), 1);

    if (!m_attrs.preserveDrawingBuffer)
        m_frameHasContent = false;

    bufferRing()->submitBuffer(previousBackBuffer.release());
}

void GraphicsContext3DInternal::deleteFreeBuffers()
{
    call(makeLambda(bufferRing(), &EGLImageBufferRing::deleteFreeBuffers)());
}

bool GraphicsContext3DInternal::ensureEnoughGraphicsMemory(unsigned requiredBytes)
{
    bool canSatisfyGraphicsMemoryAllocation;

    while (true) {
        canSatisfyGraphicsMemoryAllocation = ResourceLimits::canSatisfyGraphicsMemoryAllocation(requiredBytes);
        if (canSatisfyGraphicsMemoryAllocation)
            break;

        GraphicsContext3DInternal* resetContext = GraphicsContextLowMemoryKiller::instance()->resetLRUContext();

        if (!resetContext || resetContext == this)
            break;
    }

    return canSatisfyGraphicsMemoryAllocation;
}

void GraphicsContext3DInternal::enableGLOESStandardDerivativesT()
{
    if (m_compiler && !m_enabledGLOESStandardDerivatives) {
        m_enabledGLOESStandardDerivatives = true;
        ShBuiltInResources ANGLEResources = m_compiler->getResources();
        if (!ANGLEResources.OES_standard_derivatives) {
            ANGLEResources.OES_standard_derivatives = 1;
            m_compiler->setResources(ANGLEResources);
        }
    }
}


void GraphicsContext3DInternal::enableGLOESStandardDerivatives()
{
    push(makeLambda(this, &GraphicsContext3DInternal::enableGLOESStandardDerivativesT)());
}

void GraphicsContext3DInternal::synthesizeGLError(GLenum error)
{
    m_syntheticErrors.add(error);
}

void GraphicsContext3DInternal::destroyGLContextT()
{
    if (!m_context)
        return;

    m_compiler.clear();

    // Unbind fbo before destruction.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteRenderbuffers(1, &m_stencilBuffer);
    glDeleteRenderbuffers(1, &m_depthBuffer);
    glDeleteFramebuffers(1, &m_fbo);

    bufferRing()->deleteFreeBuffers();

    OwnPtr<EGLImageBuffer> frontBuffer = bufferRing()->takeFrontBufferAndLock();
    if (frontBuffer)
        frontBuffer->deleteBufferSource();
    bufferRing()->submitFrontBufferAndUnlock(frontBuffer.release());

    if (m_backBuffer) {
        m_backBuffer->unlockSurface();
        m_backBuffer.clear();
    }

    m_context.clear();
    m_copyVideoSurface.clear();

    if (m_thread) {
        EGLBoolean ret = eglReleaseThread();
        ASSERT_UNUSED(ret, ret == EGL_TRUE);
    }
}

GLuint GraphicsContext3DInternal::getGraphicsResetStatus()
{
    if (m_forcedContextLostReason != GL_NO_ERROR)
        return m_forcedContextLostReason;
    return call(makeLambda(glGetGraphicsResetStatusEXT)());
}

PassRefPtr<ImageData> GraphicsContext3DInternal::readBackFramebufferT(VerticalOrientation verticalOrientation, AlphaMode alphaMode)
{
    if (m_contextLostStatus != ContextIntact)
        return 0;

    RefPtr<ImageData> image = ImageData::create(m_backBuffer->size());
    ByteArray* array = image->data()->data();
    if (array->length() != 4u * m_backBuffer->size().width() * m_backBuffer->size().height())
        return 0;

    // It's OK to use m_frameHasContent here because we're in a blocking call.
    if (!m_frameHasContent) {
        if (m_attrs.alpha)
            memset(array->data(), 0, array->length());
        else {
            const int value = makeRGB(0, 0, 0);
            int* const data = reinterpret_cast<int*>(array->data());
            const int length = array->length() / sizeof(data[0]);
            for (int i = 0; i < length; ++i)
                data[i] = value;
        }
        return image;
    }

    GLUtils::AlphaOp alphaOp;
    if (!m_attrs.premultipliedAlpha && alphaMode == AlphaPremultiplied)
        alphaOp = GLUtils::AlphaDoPremultiply;
    else if (m_attrs.premultipliedAlpha && alphaMode == AlphaNotPremultiplied)
        alphaOp = GLUtils::AlphaDoUnmultiply;
    else
        alphaOp = GLUtils::AlphaDoNothing;

    if (m_fboBinding != m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    GLUtils::readPixels(IntRect(IntPoint(), image->size()), array->data(),
                        verticalOrientation == BottomToTop ? GLUtils::BottomToTop : GLUtils::TopToBottom,
                        alphaOp);

    // Need to check whether context was reset due to the calls that were waiting for execution at
    // the GPU, so we don't return uninitialized memory or corrupted rendering results in image in
    // case readPixels failed.
    if (glGetGraphicsResetStatusEXT() != GL_NO_ERROR) {
        m_contextLostStatus = ContextLost;
        return 0;
    }

    if (m_fboBinding != m_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBinding);

    return image.release();
}

PassRefPtr<ImageData> GraphicsContext3DInternal::readBackFramebuffer(VerticalOrientation verticalOrientation, AlphaMode alphaMode)
{
    RefPtr<ImageData> image = call(makeLambda(this, &GraphicsContext3DInternal::readBackFramebufferT)(verticalOrientation, alphaMode));
    if (m_contextLostStatus != ContextIntact)
        handleContextLossIfNeeded();

    return image.release();
}

void GraphicsContext3DInternal::readPixelsT(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    // Need to check whether context is reset due to the calls waiting for execution at the GPU,
    // so we avoid reading back a corrupted buffer. glFinish does not cost a significant amount of
    // performance, since readPixels causes a synchronization anyway.
    glFinish();
    if (glGetGraphicsResetStatusEXT() != GL_NO_ERROR) {
        m_contextLostStatus = ContextLost;
        return;
    }

    glReadPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3DInternal::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    call(makeLambda(this, &GraphicsContext3DInternal::readPixelsT)(x, y, width, height, format, type, data));
    if (m_contextLostStatus != ContextIntact)
        handleContextLossIfNeeded();
}

void GraphicsContext3DInternal::incrementDrawCountT()
{
    if (m_drawCount++ >= DrawFlushThreshold)
        flushT();
}

void GraphicsContext3DInternal::forceResetContext(GLenum contextLostReason)
{
    call(makeLambda(this, &GraphicsContext3DInternal::forceResetContextT)(contextLostReason));

    // This will call deleteLostBuffersT.
    handleContextLossIfNeeded();
}

void GraphicsContext3DInternal::forceResetContextT(GLenum contextLostReason)
{
    if (m_contextLostStatus != ContextIntact)
        return;

    m_contextLostStatus = ContextLost;

    if (glGetGraphicsResetStatusEXT() != GL_NO_ERROR)
        return;

    m_forcedContextLostReason = contextLostReason;

    glDeleteFramebuffers(1, &m_fbo);
    m_fboBinding = 0;
    m_fbo = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteRenderbuffers(1, &m_stencilBuffer);
    m_stencilBuffer = 0;
    glDeleteRenderbuffers(1, &m_depthBuffer);
    m_depthBuffer = 0;
}

void GraphicsContext3DInternal::deleteLostBuffersT()
{
    // After a hard context reset, the driver implicitly calls glDeleteX on all
    // GL resources. But the surface buffers have EGL images, so the actual
    // pixel data won't be freed until the EGL images are also destroyed. And
    // EGL images don't get implicitly deleted on a hard reset. We call this
    // method to delete the EGL images and free as much memory as we can.

    // Also, previous front buffer might be corrupted, since our context lost
    // check at the end of the frame does not cover commands that will still be
    // executed asynchronously on the GPU. Delete the front buffer as well to
    // stop it from displaying.

    ASSERT(m_contextLostStatus == ContextLost);

    // We can access m_backBuffer here because it's a blocking call. However, we must
    // check if the backbuffer exists, as context reset might have happened during
    // backbuffer allocation.
    if (m_backBuffer)
        m_backBuffer->onSourceContextReset();

    bufferRing()->deleteAllBuffers();
}

void GraphicsContext3DInternal::updateBackgroundStatus(bool inBackground)
{
    if (m_inBackground == inBackground)
        return;

    m_inBackground = inBackground;

    if (inBackground)
        GraphicsContextLowMemoryKiller::instance()->setContextInBackground(this);
    else
        GraphicsContextLowMemoryKiller::instance()->setContextInForeground(this);

    if (m_backgroundModeCallback)
        m_backgroundModeCallback->onBackgroundModeChanged(inBackground);
}

void GraphicsContext3DInternal::didDetachFromView()
{
    updateBackgroundStatus(false);
}

} // namespace WebCore
