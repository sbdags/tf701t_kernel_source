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

#ifndef GraphicsContext3DInternal_h
#define GraphicsContext3DInternal_h

#include "ANGLEWebKitBridge.h"
#include "EGLImageSurface.h"
#include "GLContext.h"
#include "GraphicsContext3D.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <utils/RefBase.h>
#include <utils/threads.h>
#include <wtf/DelegateThread.h>
#include <wtf/Lambda.h>
#include <wtf/ListHashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace android;

namespace WebCore {

class CopyVideoSurface;
class EGLImageBufferFromTexture;
class GraphicsContextLowMemoryKiller;
class ImageData;
class VideoSurface;

typedef struct {
    String source;
    String log;
    bool isValid;
} ShaderSourceEntry;
typedef HashMap<Platform3DObject, ShaderSourceEntry> ShaderSourceMap;

class GraphicsContext3DInternal
    : public EGLImageSurface {
    static const unsigned ThreadQueueCapacity = 2048;
    typedef WTF::DelegateThread<ThreadQueueCapacity> Thread;

public:
    static PassRefPtr<GraphicsContext3DInternal> create(GraphicsContext3D* hostContext, const GraphicsContext3D::Attributes& attrs)
    {
        bool success;
        RefPtr<GraphicsContext3DInternal> surface = adoptRef(new GraphicsContext3DInternal(hostContext, attrs, success));
        if (!success)
            return 0;
        return surface.release();
    }
    virtual ~GraphicsContext3DInternal();

    // GraphicsContext3D implementation
    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback>);
    void setBackgroundModeCallback(PassOwnPtr<GraphicsContext3D::BackgroundModeCallback>);
    bool texImage2DResourceSafe(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLint unpackAlignment);

    bool validateShaderLocation(const String& string);

    void bindAttribLocation(GLuint, GLuint index, const String& name);
    void bindFramebuffer(GLenum target, GLuint);

    void bufferData(GLenum target, GLsizeiptr size, GLenum usage);
    void bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    void bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

    GLenum checkFramebufferStatus(GLenum target);
    void compileShader(GLuint);

    void compressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
    void compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
    void copyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void drawArrays(GLenum mode, GLint first, GLsizei count);
    void drawElements(GLenum mode, GLsizei count, GLenum type, GLintptr offset);

    void flush();
    void finish();

    void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint);
    void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint, GLint level);

    bool getActiveAttrib(GLuint program, GLuint index, ActiveInfo&);
    bool getActiveUniform(GLuint program, GLuint index, ActiveInfo&);
    GLint getAttribLocation(GLuint, const String& name);
    GraphicsContext3D::Attributes getContextAttributes() const { return m_attrs; }
    GLenum getError();
    void getFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* value);
    void getIntegerv(GLenum pname, GLint* value);
    String getProgramInfoLog(GLuint);
    void getShaderiv(GLuint, GLenum pname, GLint* value);
    String getShaderInfoLog(GLuint);

    String getShaderSource(GLuint);
    String getString(GLenum name);
    GLint getUniformLocation(GLuint, const String& name);
    GLsizeiptr getVertexAttribOffset(GLuint index, GLenum pname);

    void releaseShaderCompiler();

    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void shaderSource(GLuint, const String& string);

    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data);

    bool texImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    bool texImage2DVideo(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, VideoSurface*, bool flipY, bool premultiplyAlpha);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    bool texSubImage2DVideo(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, VideoSurface*, bool flipY, bool premultiplyAlpha);

    void vertexAttrib1fv(GLuint index, GLfloat* values);
    void vertexAttrib2fv(GLuint index, GLfloat* values);
    void vertexAttrib3fv(GLuint index, GLfloat* values);
    void vertexAttrib4fv(GLuint index, GLfloat* values);
    void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                             GLsizei stride, GLintptr offset);

    void reshape(int width, int height);

    void markContextChanged();
    void markLayerComposited();
    bool layerComposited() const;

    PassRefPtr<ImageData> paintRenderingResultsToImageData();
    void paintRenderingResultsToCanvas(CanvasRenderingContext* context);

    void clear(GLbitfield);

    GLuint createBuffer();
    GLuint createFramebuffer();
    GLuint createRenderbuffer();
    GLuint createShader(GC3Denum);
    GLuint createTexture();

    void deleteBuffer(GLuint buffer);
    void deleteFramebuffer(GLuint framebuffer);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deleteShader(GLuint shader);
    void deleteTexture(GLuint texture);

    IntSize getInternalFramebufferSize() const;

    // EGLImageSurface overrrides.
    virtual bool isInverted() const { return true; }
    virtual bool hasAlpha() const { return m_attrs.alpha; }
    virtual bool hasPremultipliedAlpha() const { return m_attrs.premultipliedAlpha; }

    virtual void swapBuffers();

    virtual bool supportsQuadBuffering() const { return true; }
    virtual void submitBackBuffer();

    virtual void deleteFreeBuffers();

    // GraphicsContext3DInternal
    enum VerticalOrientation {BottomToTop, TopToBottom};
    enum AlphaMode {AlphaPremultiplied, AlphaNotPremultiplied};

    bool ensureEnoughGraphicsMemory(unsigned requiredBytes);
    void enableGLOESStandardDerivatives();
    void forceResetContext(GLenum contextLostReason);
    GLuint getGraphicsResetStatus();
    PassRefPtr<ImageData> readBackFramebuffer(VerticalOrientation, AlphaMode);
    void synthesizeGLError(GLenum error);
    // Methods used for calling unwrapped GL calls.
    inline void push(PassOwnPtr<WTF::Lambda> func, unsigned minJobsToWakeThread = 8)
    {
        if (m_thread)
            return m_thread->callLater(func, minJobsToWakeThread);

        OwnPtr<WTF::Lambda> ownFunc = func;

        // Make the context current only if it exist. This allows call/push to be used for
        // initialization and deinitialization too.
        if (m_context)
            m_context->makeCurrent();
        ownFunc->call();
    }
    inline void call(PassOwnPtr<WTF::Lambda> func)
    {
        if (m_thread)
            return m_thread->call(func);

        OwnPtr<WTF::Lambda> ownFunc = func;

        // Make the context current only if it exist. This allows call/push to be used for
        // initialization and deinitialization too.
        if (m_context)
            m_context->makeCurrent();
        ownFunc->call();
    }
    template<typename R> inline R call(PassOwnPtr<WTF::ReturnLambda<R> > func)
    {
        if (m_thread)
            return m_thread->call(func);

        OwnPtr<WTF::ReturnLambda<R> > ownFunc = func;

        // Make the context current only if it exist. This allows call/push to be used for
        // initialization and deinitialization too.
        if (m_context)
            m_context->makeCurrent();
        ownFunc->call();
        return ownFunc->ret();
    }

    void updateBackgroundStatus(bool inBackground);
    void didDetachFromView();

private:
    GraphicsContext3DInternal(GraphicsContext3D* hostContext, const GraphicsContext3D::Attributes& attrs, bool& success);
    void handleContextLossIfNeeded();

    // "T" means these methods are only meant to be called on the delegate thread.
    void bindFramebufferT(GLuint fbo);
    GLenum checkFramebufferStatusT();
    void compileShaderT(GLuint shader);
    void destroyGLContextT();
    void drawArraysT(GLenum mode, GLint first, GLsizei count);
    void drawElementsT(GLenum mode, GLsizei count, GLenum type, GLintptr offset);
    void enableGLOESStandardDerivativesT();
    void framebufferRenderbufferT(GLenum attachment, GLuint renderbuffertarget, GLuint rbo);
    void framebufferTexture2DT(GLenum attachment, GLuint textarget, GLuint texture, GLint level);
    void getFramebufferAttachmentParameterivT(GLenum attachment, GLenum pname, int* value);
    GLuint getGraphicsResetStatusT();
    void getIntegervT(GLenum pname, GLint* params);
    void getShaderInfoLogT(GLuint shade, String& retValuer);
    bool getShaderivT(GLuint shader, GLuint pname, GLint* value);
    void getShaderSourceT(GLuint shader, String& retValue);
    bool initContextT();
    void initCompilerT();
    PassRefPtr<ImageData> readBackFramebufferT(VerticalOrientation, AlphaMode);
    void readPixelsT(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data);
    void releaseShaderCompilerT();
    void texImage2DVideoT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, android::sp<VideoSurface>, bool flipY);
    void texSubImage2DVideoT(GLenum target, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, android::sp<VideoSurface>, bool flipY);
    void reshapeT(IntSize);
    PassOwnPtr<EGLImageBufferFromTexture> createBackBufferT(PassOwnPtr<EGLImageBufferFromTexture> failedCandidate);
    void swapBuffersT();
    void setupNextBackBufferT(EGLImageBufferFromTexture* previousBackBuffer);
    void updateRenderTargetT();
    void shaderSourceT(GLuint shader, String source);
    void incrementDrawCountT();
    void flushT();
    void finishT();
    void clearT(GLbitfield);
    void forceResetContextT(GLenum contextLostReason);
    void deleteLostBuffersT();

    OwnPtr<Thread> m_thread;
    GraphicsContext3D::Attributes m_attrs;
    bool m_frameHasContent;
    OwnPtr<GLContext> m_context;
    OwnPtr<EGLImageBufferFromTexture> m_backBuffer;
    GLuint m_fbo;
    GLuint m_depthBuffer;
    GLuint m_stencilBuffer;
    GLuint m_fboBinding;
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
    OwnPtr<GraphicsContext3D::BackgroundModeCallback> m_backgroundModeCallback;
    bool m_enabledGLOESStandardDerivatives;
    ShaderSourceMap m_shaderSourceMap;
    OwnPtr<ANGLEWebKitBridge> m_compiler;
    ListHashSet<unsigned long> m_syntheticErrors;
    GraphicsContext3D* m_hostContext;
    GLenum m_forcedContextLostReason;
    bool m_inBackground;
    unsigned m_drawCount;
    OwnPtr<CopyVideoSurface> m_copyVideoSurface;

    enum ContextLostStatus {
        ContextIntact,
        ContextLost,
        LostBuffersFreed,
        ContextLostCallbackNotified
    };
    ContextLostStatus m_contextLostStatus;

    friend class GraphicsContextLowMemoryKiller;
};

} // namespace WebCore

#endif // GraphicsContext3DInternal_h
