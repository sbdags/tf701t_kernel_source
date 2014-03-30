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
#include "GraphicsContext3D.h"

#if ENABLE(WEBGL)

#include "EGLImageLayer.h"
#include "Extensions3DAndroid.h"
#include "GraphicsContext3DInternal.h"
#include "HostWindow.h"
#include "ImageData.h"
#include "WebFrameView.h"
#include "WebViewCore.h"
#include <GLES2/gl2.h>
#include <wtf/Closure.h>
#include <wtf/OwnArrayPtr.h>

// Uncomment to enable logging. The logging the api will cause a significant drop in performance.
// #define LOG_API

#if defined(LOG_API)
#define LOG_TAG "GraphicsContext3DAndroid"
#include <cutils/log.h>
#include <sstream>
#include <wtf/text/CString.h>


class APILog {
public:
    APILog(const char* name)
        : m_divider("")
    {
        m_str << "gl." << name << '(';
    }

    ~APILog()
    {
        ALOGD("webgl> %s)", m_str.str().c_str());
    }

    template<typename T> APILog& operator <<(const T& t)
    {
        m_str << m_divider << t;
        m_divider = ",";
        return *this;
    }

private:
    std::ostringstream m_str;
    const char* m_divider;
};

std::ostream& operator <<(std::ostream& out, const String& str)
{
    out << str.utf8().data();
    return out;
}

std::ostream& operator <<(std::ostream& out, const ActiveInfo& info)
{
    out << &info;
    return out;
}

#define WEBGL_LOG(code) APILog code
#else
#define WEBGL_LOG(code)
#endif

PassOwnArrayPtr<uint8_t> zeroArray(size_t size)
{
    OwnArrayPtr<uint8_t> zero = adoptArrayPtr(new uint8_t[size]);
    memset(zero.get(), 0, size);
    return zero.release();
}

using WTF::makeLambda;

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, true));
    if (!context->m_internal)
        return 0;
    return context.release();
}

GraphicsContext3D::GraphicsContext3D(Attributes attrs, HostWindow* hostWindow, bool renderToWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
{
    m_internal = GraphicsContext3DInternal::create(this, attrs);
    if (!m_internal)
        return;
    m_layer = new EGLImageLayer(m_internal, "webgl");
    m_layer->unref();
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_layer.get();
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DAndroid(m_internal.get()));
    return m_extensions.get();
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return true;
}

void GraphicsContext3D::makeContextCurrent()
{
    // The context lives in its own thread, so this call is a NOP.
}

// Exmaple: ARGLIST5(=,+) becomes "= arg1 + arg2 + arg3 + arg4 + arg5"
// use __VA_ARGS__ instead of "dividerN" so we can pass in a comma
#define ARGLIST0(divider0, ...)
#define ARGLIST1(divider0, ...) ARGLIST0(divider0, __VA_ARGS__) divider0 arg1
#define ARGLIST2(divider0, ...) ARGLIST1(divider0, __VA_ARGS__) __VA_ARGS__ arg2
#define ARGLIST3(divider0, ...) ARGLIST2(divider0, __VA_ARGS__) __VA_ARGS__ arg3
#define ARGLIST4(divider0, ...) ARGLIST3(divider0, __VA_ARGS__) __VA_ARGS__ arg4
#define ARGLIST5(divider0, ...) ARGLIST4(divider0, __VA_ARGS__) __VA_ARGS__ arg5
#define ARGLIST6(divider0, ...) ARGLIST5(divider0, __VA_ARGS__) __VA_ARGS__ arg6
#define ARGLIST7(divider0, ...) ARGLIST6(divider0, __VA_ARGS__) __VA_ARGS__ arg7
#define ARGLIST8(divider0, ...) ARGLIST7(divider0, __VA_ARGS__) __VA_ARGS__ arg8
#define ARGLIST9(divider0, ...) ARGLIST8(divider0, __VA_ARGS__) __VA_ARGS__ arg9
#define ARGLIST10(divider0, ...) ARGLIST9(divider0, __VA_ARGS__) __VA_ARGS__ arg10
#define ARGLIST11(divider0, ...) ARGLIST10(divider0, __VA_ARGS__) __VA_ARGS__ arg11

#define PUSH_TO_GL(argCount, name, glFuncName, params) \
void GraphicsContext3D::name(params) \
{ \
    WEBGL_LOG((#name) ARGLIST##argCount(<< , <<)); \
    m_internal->push(makeLambda(gl##glFuncName)(ARGLIST##argCount(, ,))); \
}

#define CALL_TO_GL(argCount, GLReturnType, name, glFuncName, params) \
GLReturnType GraphicsContext3D::name(params) \
{ \
    WEBGL_LOG((#name) ARGLIST##argCount(<< , <<)); \
    return m_internal->call(makeLambda(gl##glFuncName)(ARGLIST##argCount(, ,))); \
}

#define FORWARD_TO_INTERNAL(argCount, GLReturnType, name, params) \
GLReturnType GraphicsContext3D::name(params) \
{ \
    WEBGL_LOG((#name) ARGLIST##argCount(<< , <<)); \
    return m_internal->name(ARGLIST##argCount(, ,)); \
}

#define FORWARD_TO_INTERNAL_UNLOGGED(argCount, GLReturnType, name, params) \
GLReturnType GraphicsContext3D::name(params) \
{ \
    return m_internal->name(ARGLIST##argCount(, ,)); \
}

#define FORWARD_TO_INTERNAL_UNLOGGED_CONST(argCount, GLReturnType, name, params) \
GLReturnType GraphicsContext3D::name(params) const \
{ \
    return m_internal->name(ARGLIST##argCount(, ,)); \
}

#define PUSH_UNIFORM_TO_GL(name, size, GLType) \
static void webglUniform##name(GLint location, GLsizei count, PassOwnArrayPtr<GLType> v) \
{ \
    OwnArrayPtr<GLType> ownV = v; \
    glUniform##name(location, count, ownV.get()); \
} \
void GraphicsContext3D::uniform##name(GLint location, GLType* v, GLsizei count) \
{ \
    WEBGL_LOG(("uniform"#name) << location << v << count); \
    m_internal->push(makeLambda(webglUniform##name)(location, count, WTF::makeLambdaArrayArg(v, size * count))); \
}

#define PUSH_UNIFORM_MATRIX_TO_GL(name, size) \
static void webglUniformMatrix##name(GLint location, GLsizei count, GLboolean transpose, PassOwnArrayPtr<GLfloat> v) \
{ \
    OwnArrayPtr<GLfloat> ownV = v; \
    glUniformMatrix##name(location, count, transpose, ownV.get()); \
} \
void GraphicsContext3D::uniformMatrix##name(GLint location, GLboolean transpose, GLfloat* v, GLsizei count) \
{ \
    WEBGL_LOG(("uniformMatrix"#name) << location << transpose << v << count); \
    m_internal->push(makeLambda(webglUniformMatrix##name)(location, count, transpose, WTF::makeLambdaArrayArg(v, size * size * count))); \
}

#define PARAMS0()
#define PARAMS1(T1) T1 arg1
#define PARAMS2(T1, T2) PARAMS1(T1), T2 arg2
#define PARAMS3(T1, T2, T3) PARAMS2(T1, T2), T3 arg3
#define PARAMS4(T1, T2, T3, T4) PARAMS3(T1, T2, T3), T4 arg4
#define PARAMS5(T1, T2, T3, T4, T5) PARAMS4(T1, T2, T3, T4), T5 arg5
#define PARAMS6(T1, T2, T3, T4, T5, T6) PARAMS5(T1, T2, T3, T4, T5), T6 arg6
#define PARAMS7(T1, T2, T3, T4, T5, T6, T7) PARAMS6(T1, T2, T3, T4, T5, T6), T7 arg7
#define PARAMS8(T1, T2, T3, T4, T5, T6, T7, T8) PARAMS7(T1, T2, T3, T4, T5, T6, T7), T8 arg8
#define PARAMS9(T1, T2, T3, T4, T5, T6, T7, T8, T9) PARAMS8(T1, T2, T3, T4, T5, T6, T7, T8), T9 arg9
#define PARAMS10(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10) PARAMS9(T1, T2, T3, T4, T5, T6, T7, T8, T9), T10 arg10
#define PARAMS11(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11) PARAMS10(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10), T11 arg11

PUSH_TO_GL(2, attachShader, AttachShader, PARAMS2(GLuint, GLuint))
PUSH_TO_GL(2, bindRenderbuffer, BindRenderbuffer, PARAMS2(GLuint, GLuint))
PUSH_TO_GL(2, bindTexture, BindTexture, PARAMS2(GLenum, GLuint))
PUSH_TO_GL(4, blendColor, BlendColor, PARAMS4(GLclampf, GLclampf, GLclampf, GLclampf))
PUSH_TO_GL(1, blendEquation, BlendEquation, PARAMS1(GLenum))
PUSH_TO_GL(2, blendEquationSeparate, BlendEquationSeparate, PARAMS2(GLenum, GLenum))
PUSH_TO_GL(2, blendFunc, BlendFunc, PARAMS2(GLenum, GLenum))
PUSH_TO_GL(4, blendFuncSeparate, BlendFuncSeparate, PARAMS4(GLenum, GLenum, GLenum, GLenum))
PUSH_TO_GL(4, clearColor, ClearColor, PARAMS4(GLclampf, GLclampf, GLclampf, GLclampf))
PUSH_TO_GL(1, clearDepth, ClearDepthf, PARAMS1(GLclampf))
PUSH_TO_GL(1, clearStencil, ClearStencil, PARAMS1(GLint))
PUSH_TO_GL(4, colorMask, ColorMask, PARAMS4(GLboolean, GLboolean, GLboolean, GLboolean))
PUSH_TO_GL(1, cullFace, CullFace, PARAMS1(GLenum))
PUSH_TO_GL(1, depthFunc, DepthFunc, PARAMS1(GLenum))
PUSH_TO_GL(1, depthMask, DepthMask, PARAMS1(GLboolean))
PUSH_TO_GL(2, depthRange, DepthRangef, PARAMS2(GLclampf, GLclampf))
PUSH_TO_GL(2, detachShader, DetachShader, PARAMS2(GLuint, GLuint))
PUSH_TO_GL(1, disable, Disable, PARAMS1(GLenum))
PUSH_TO_GL(1, enable, Enable, PARAMS1(GLenum))
PUSH_TO_GL(1, frontFace, FrontFace, PARAMS1(GLenum))
PUSH_TO_GL(2, hint, Hint, PARAMS2(GLenum, GLenum))
PUSH_TO_GL(1, lineWidth, LineWidth, PARAMS1(GLfloat))
PUSH_TO_GL(1, linkProgram, LinkProgram, PARAMS1(GLuint))
PUSH_TO_GL(2, pixelStorei, PixelStorei, PARAMS2(GLenum, GLint))
PUSH_TO_GL(2, polygonOffset, PolygonOffset, PARAMS2(GLfloat, GLfloat))
PUSH_TO_GL(2, sampleCoverage, SampleCoverage, PARAMS2(GLclampf, GLboolean))
PUSH_TO_GL(4, scissor, Scissor, PARAMS4(GLint, GLint, GLsizei, GLsizei))
PUSH_TO_GL(3, stencilFunc, StencilFunc, PARAMS3(GLenum, GLint, GLuint))
PUSH_TO_GL(4, stencilFuncSeparate, StencilFuncSeparate, PARAMS4(GLenum, GLenum, GLint, GLuint))
PUSH_TO_GL(1, stencilMask, StencilMask, PARAMS1(GLuint))
PUSH_TO_GL(2, stencilMaskSeparate, StencilMaskSeparate, PARAMS2(GLenum, GLuint))
PUSH_TO_GL(3, stencilOp, StencilOp, PARAMS3(GLenum, GLenum, GLenum))
PUSH_TO_GL(4, stencilOpSeparate, StencilOpSeparate, PARAMS4(GLenum, GLenum, GLenum, GLenum))
PUSH_TO_GL(3, texParameterf, TexParameterf, PARAMS3(GLenum, GLenum, GLfloat))
PUSH_TO_GL(3, texParameteri, TexParameteri, PARAMS3(GLenum, GLenum, GLint))
PUSH_TO_GL(2, uniform1f, Uniform1f, PARAMS2(GLint, GLfloat))
PUSH_TO_GL(2, uniform1i, Uniform1i, PARAMS2(GLint, GLint))
PUSH_TO_GL(3, uniform2f, Uniform2f, PARAMS3(GLint, GLfloat, GLfloat))
PUSH_TO_GL(3, uniform2i, Uniform2i, PARAMS3(GLint, GLint, GLint))
PUSH_TO_GL(4, uniform3f, Uniform3f, PARAMS4(GLint, GLfloat, GLfloat, GLfloat))
PUSH_TO_GL(4, uniform3i, Uniform3i, PARAMS4(GLint, GLint, GLint, GLint))
PUSH_TO_GL(5, uniform4f, Uniform4f, PARAMS5(GLint, GLfloat, GLfloat, GLfloat, GLfloat))
PUSH_TO_GL(5, uniform4i, Uniform4i, PARAMS5(GLint, GLint, GLint, GLint, GLint))
PUSH_TO_GL(1, useProgram, UseProgram, PARAMS1(GLuint))
PUSH_TO_GL(1, validateProgram, ValidateProgram, PARAMS1(GLuint))
PUSH_TO_GL(2, vertexAttrib1f, VertexAttrib1f, PARAMS2(GLuint, GLfloat))
PUSH_TO_GL(3, vertexAttrib2f, VertexAttrib2f, PARAMS3(GLuint, GLfloat, GLfloat))
PUSH_TO_GL(4, vertexAttrib3f, VertexAttrib3f, PARAMS4(GLuint, GLfloat, GLfloat, GLfloat))
PUSH_TO_GL(5, vertexAttrib4f, VertexAttrib4f, PARAMS5(GLuint, GLfloat, GLfloat, GLfloat, GLfloat))
PUSH_TO_GL(4, viewport, Viewport, PARAMS4(GLint, GLint, GLsizei, GLsizei))
PUSH_TO_GL(8, copyTexSubImage2D, CopyTexSubImage2D, PARAMS8(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei))
PUSH_TO_GL(1, generateMipmap, GenerateMipmap, PARAMS1(GLenum))
PUSH_TO_GL(2, bindBuffer, BindBuffer, PARAMS2(GLenum, GLuint))
PUSH_TO_GL(1, disableVertexAttribArray, DisableVertexAttribArray, PARAMS1(GLuint))
PUSH_TO_GL(1, enableVertexAttribArray, EnableVertexAttribArray, PARAMS1(GLuint))
PUSH_TO_GL(1, activeTexture, ActiveTexture, PARAMS1(GLenum))
PUSH_TO_GL(1, deleteProgram, DeleteProgram, PARAMS1(GLuint))

PUSH_UNIFORM_TO_GL(1fv, 1, GLfloat)
PUSH_UNIFORM_TO_GL(1iv, 1, GLint)
PUSH_UNIFORM_TO_GL(2fv, 2, GLfloat)
PUSH_UNIFORM_TO_GL(2iv, 2, GLint)
PUSH_UNIFORM_TO_GL(3fv, 3, GLfloat)
PUSH_UNIFORM_TO_GL(3iv, 3, GLint)
PUSH_UNIFORM_TO_GL(4fv, 4, GLfloat)
PUSH_UNIFORM_TO_GL(4iv, 4, GLint)

PUSH_UNIFORM_MATRIX_TO_GL(2fv, 2)
PUSH_UNIFORM_MATRIX_TO_GL(3fv, 3)
PUSH_UNIFORM_MATRIX_TO_GL(4fv, 4)

CALL_TO_GL(4, void, getAttachedShaders, GetAttachedShaders, PARAMS4(GLuint, GLsizei, GLsizei*, GLuint*))
CALL_TO_GL(2, void, getBooleanv, GetBooleanv, PARAMS2(GLenum, GLboolean*))
CALL_TO_GL(3, void, getBufferParameteriv, GetBufferParameteriv, PARAMS3(GLenum, GLenum, GLint*))
CALL_TO_GL(2, void, getFloatv, GetFloatv, PARAMS2(GLenum, GLfloat*))
CALL_TO_GL(3, void, getProgramiv, GetProgramiv, PARAMS3(GLuint, GLenum, GLint*))
CALL_TO_GL(3, void, getRenderbufferParameteriv, GetRenderbufferParameteriv, PARAMS3(GLenum, GLenum, GLint*))
CALL_TO_GL(4, void, getShaderPrecisionFormat, GetShaderPrecisionFormat, PARAMS4(GLenum, GLenum, GLint*, GLint*))
CALL_TO_GL(3, void, getTexParameterfv, GetTexParameterfv, PARAMS3(GLenum, GLenum, GLfloat*))
CALL_TO_GL(3, void, getTexParameteriv, GetTexParameteriv, PARAMS3(GLenum, GLenum, GLint*))
CALL_TO_GL(3, void, getUniformfv, GetUniformfv, PARAMS3(GLuint, GLint, GLfloat*))
CALL_TO_GL(3, void, getUniformiv, GetUniformiv, PARAMS3(GLuint, GLint, GLint*))
CALL_TO_GL(3, void, getVertexAttribfv, GetVertexAttribfv, PARAMS3(GLuint, GLenum, GLfloat*))
CALL_TO_GL(3, void, getVertexAttribiv, GetVertexAttribiv, PARAMS3(GLuint, GLenum, GLint*))
CALL_TO_GL(1, GLboolean, isBuffer, IsBuffer, PARAMS1(GLuint))
CALL_TO_GL(1, GLboolean, isEnabled, IsEnabled, PARAMS1(GLenum))
CALL_TO_GL(1, GLboolean, isFramebuffer, IsFramebuffer, PARAMS1(GLuint))
CALL_TO_GL(1, GLboolean, isProgram, IsProgram, PARAMS1(GLuint))
CALL_TO_GL(1, GLboolean, isRenderbuffer, IsRenderbuffer, PARAMS1(GLuint))
CALL_TO_GL(1, GLboolean, isShader, IsShader, PARAMS1(GLuint))
CALL_TO_GL(1, GLboolean, isTexture, IsTexture, PARAMS1(GLuint))
CALL_TO_GL(0, GLenum, createProgram, CreateProgram, PARAMS0())

FORWARD_TO_INTERNAL(1, void, clear, PARAMS1(GLbitfield))
FORWARD_TO_INTERNAL(0, GLuint, createBuffer, PARAMS0())
FORWARD_TO_INTERNAL(0, GLuint, createFramebuffer, PARAMS0())
FORWARD_TO_INTERNAL(0, GLuint, createRenderbuffer, PARAMS0())
FORWARD_TO_INTERNAL(0, GLuint, createTexture, PARAMS0())
FORWARD_TO_INTERNAL(1, void, deleteBuffer, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(1, void, deleteFramebuffer, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(1, void, deleteRenderbuffer, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(1, void, deleteTexture, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(3, void, bufferData, PARAMS3(GLenum, GLintptr, GLenum))
FORWARD_TO_INTERNAL(4, void, bufferData, PARAMS4(GLenum, GLintptr, const void*, GLenum))
FORWARD_TO_INTERNAL(3, void, bindAttribLocation, PARAMS3(GLuint, GLuint, const String&))
FORWARD_TO_INTERNAL(2, void, bindFramebuffer, PARAMS2(GLenum, GLuint))
FORWARD_TO_INTERNAL(4, void, bufferSubData, PARAMS4(GLenum, GLintptr, GLsizeiptr, const void*))
FORWARD_TO_INTERNAL(1, GLuint, createShader, PARAMS1(GLenum))
FORWARD_TO_INTERNAL(8, void, copyTexImage2D, PARAMS8(GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint))
FORWARD_TO_INTERNAL(1, void, compileShader, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(8, void, compressedTexImage2D, PARAMS8(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*))
FORWARD_TO_INTERNAL(9, void, compressedTexSubImage2D, PARAMS9(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*))
FORWARD_TO_INTERNAL(1, void, deleteShader, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(4, void, drawElements, PARAMS4(GLenum, GLsizei, GLenum, GLintptr))
FORWARD_TO_INTERNAL(3, void, drawArrays, PARAMS3(GLenum, GLint, GLsizei))
FORWARD_TO_INTERNAL(1, GLenum, checkFramebufferStatus, PARAMS1(GLenum))
FORWARD_TO_INTERNAL(0, void, finish, PARAMS0())
FORWARD_TO_INTERNAL(0, void, flush, PARAMS0())
FORWARD_TO_INTERNAL(5, void, framebufferTexture2D, PARAMS5(GLenum, GLenum, GLuint, GLuint, GLint))
FORWARD_TO_INTERNAL(4, void, framebufferRenderbuffer, PARAMS4(GLenum, GLenum, GLuint, GLuint))
FORWARD_TO_INTERNAL(3, bool, getActiveAttrib, PARAMS3(GLuint, GLuint, ActiveInfo&))
FORWARD_TO_INTERNAL(3, bool, getActiveUniform, PARAMS3(GLuint, GLuint, ActiveInfo&))
FORWARD_TO_INTERNAL(2, int, getAttribLocation, PARAMS2(GLuint, const String&))
FORWARD_TO_INTERNAL(2, long, getVertexAttribOffset, PARAMS2(GLuint, GLenum))
FORWARD_TO_INTERNAL(2, GLint, getUniformLocation, PARAMS2(GLuint, const String&))
FORWARD_TO_INTERNAL(4, void, getFramebufferAttachmentParameteriv, PARAMS4(GLenum, GLenum, GLenum, int*))
FORWARD_TO_INTERNAL(2, void, getIntegerv, PARAMS2(GLenum, GLint*))
FORWARD_TO_INTERNAL(0, GLenum, getError, PARAMS0())
FORWARD_TO_INTERNAL(1, String, getProgramInfoLog, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(1, String, getShaderSource, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(1, String, getShaderInfoLog, PARAMS1(GLuint))
FORWARD_TO_INTERNAL(3, void, getShaderiv, PARAMS3(GLuint, GLuint, GLint*))
FORWARD_TO_INTERNAL(1, String, getString, PARAMS1(GLenum))
FORWARD_TO_INTERNAL(7, void, readPixels, PARAMS7(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))
FORWARD_TO_INTERNAL(0, void, releaseShaderCompiler, PARAMS0())
FORWARD_TO_INTERNAL(4, void, renderbufferStorage, PARAMS4(GLenum, GLenum, GLsizei, GLsizei))
FORWARD_TO_INTERNAL(2, void, shaderSource, PARAMS2(GLuint, const String&))
FORWARD_TO_INTERNAL(1, void, synthesizeGLError, PARAMS1(GLenum))
FORWARD_TO_INTERNAL(9, bool, texImage2D, PARAMS9(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))
FORWARD_TO_INTERNAL(11, bool, texImage2DVideo, PARAMS11(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLenum, GLenum, PlatformVideoSurface*, bool, bool))
FORWARD_TO_INTERNAL(9, bool, texImage2DResourceSafe, PARAMS9(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLenum, GLenum, GLint))
FORWARD_TO_INTERNAL(9, void, texSubImage2D, PARAMS9(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))
FORWARD_TO_INTERNAL(11, bool, texSubImage2DVideo, PARAMS11(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, PlatformVideoSurface*, bool, bool))
FORWARD_TO_INTERNAL(2, void, vertexAttrib1fv, PARAMS2(GLuint, GLfloat*))
FORWARD_TO_INTERNAL(2, void, vertexAttrib2fv, PARAMS2(GLuint, GLfloat*))
FORWARD_TO_INTERNAL(2, void, vertexAttrib3fv, PARAMS2(GLuint, GLfloat*))
FORWARD_TO_INTERNAL(2, void, vertexAttrib4fv, PARAMS2(GLuint, GLfloat*))
FORWARD_TO_INTERNAL(6, void, vertexAttribPointer, PARAMS6(GLuint, GLint, GLenum, GLboolean, GLsizei, GLintptr))

FORWARD_TO_INTERNAL_UNLOGGED(0, void, markContextChanged, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED(1, bool, validateShaderLocation, PARAMS1(const String&))
FORWARD_TO_INTERNAL_UNLOGGED(1, void, setContextLostCallback, PARAMS1(WTF::PassOwnPtr<WebCore::GraphicsContext3D::ContextLostCallback>))
FORWARD_TO_INTERNAL_UNLOGGED(1, void, setBackgroundModeCallback, PARAMS1(WTF::PassOwnPtr<WebCore::GraphicsContext3D::BackgroundModeCallback>))
FORWARD_TO_INTERNAL_UNLOGGED(0, GraphicsContext3D::Attributes, getContextAttributes, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED_CONST(0, IntSize, getInternalFramebufferSize, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED_CONST(0, bool, layerComposited, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED(0, void, markLayerComposited, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED(0, PassRefPtr<ImageData>, paintRenderingResultsToImageData, PARAMS0())
FORWARD_TO_INTERNAL_UNLOGGED(1, void, paintRenderingResultsToCanvas, PARAMS1(CanvasRenderingContext*))
FORWARD_TO_INTERNAL_UNLOGGED(2, void, reshape, PARAMS2(int, int))


} // namespace WebCore

#endif // ENABLE(WEBGL)
