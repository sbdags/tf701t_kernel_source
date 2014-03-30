/*
 * Copyright 2010, The Android Open Source Project
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#define LOG_TAG "GLUtils"
#define LOG_NDEBUG 1

#include "config.h"
#include "GLUtils.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseRenderer.h"
#include "GLSuccessVerifier.h"
#include "Tile.h"
#include "TilesManager.h"

#include <android/native_window.h>
#include <gui/GLConsumer.h>
#include <wtf/CurrentTime.h>

// We will limit GL error logging for LOG_VOLUME_PER_CYCLE times every
// LOG_VOLUME_PER_CYCLE seconds.
#define LOG_CYCLE 30.0
#define LOG_VOLUME_PER_CYCLE 20

struct ANativeWindowBuffer;

namespace WebCore {

using namespace android;

void GLUtils::initializeEGLDisplayIfNeeded()
{
    // Initialize EGL to be able to create EGLImages.
    static bool hasInitialized = false;
    if (!hasInitialized) {
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            GLUtils::checkEglError("eglGetDisplay", EGL_FALSE);
            return;
        }

        if (!eglInitialize(display, 0, 0)) {
            GLUtils::checkEglError("eglInitialize", EGL_FALSE);
            return;
        }

        hasInitialized = true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
// Matrix utilities
/////////////////////////////////////////////////////////////////////////////////////////

void GLUtils::toGLMatrix(GLfloat* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11(); // scaleX
    flattened[1] = m.m12(); // skewY
    flattened[2] = m.m13();
    flattened[3] = m.m14(); // persp0
    flattened[4] = m.m21(); // skewX
    flattened[5] = m.m22(); // scaleY
    flattened[6] = m.m23();
    flattened[7] = m.m24(); // persp1
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41(); // transX
    flattened[13] = m.m42(); // transY
    flattened[14] = m.m43();
    flattened[15] = m.m44(); // persp2
}

void GLUtils::toSkMatrix(SkMatrix& matrix, const TransformationMatrix& m)
{
    matrix[0] = m.m11(); // scaleX
    matrix[1] = m.m21(); // skewX
    matrix[2] = m.m41(); // transX
    matrix[3] = m.m12(); // skewY
    matrix[4] = m.m22(); // scaleY
    matrix[5] = m.m42(); // transY
    matrix[6] = m.m14(); // persp0
    matrix[7] = m.m24(); // persp1
    matrix[8] = m.m44(); // persp2
}

void GLUtils::setOrthographicMatrix(TransformationMatrix& ortho, float left, float top,
                                    float right, float bottom, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    if (!deltaX || !deltaY || !deltaZ)
        return;

    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
}

bool GLUtils::has3dTransform(const TransformationMatrix& matrix)
{
    return matrix.m13() != 0 || matrix.m23() != 0
        || matrix.m31() != 0 || matrix.m32() != 0
        || matrix.m33() != 1 || matrix.m34() != 0
        || matrix.m43() != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL error checks
/////////////////////////////////////////////////////////////////////////////////////////

double GLUtils::m_previousLogTime = 0;
int GLUtils::m_currentLogCounter = 0;

bool GLUtils::allowGLLog()
{
    if (m_currentLogCounter < LOG_VOLUME_PER_CYCLE) {
        m_currentLogCounter++;
        return true;
    }

    // when we are in Log cycle and over the log limit, just return false
    double currentTime = WTF::currentTime();
    double delta = currentTime - m_previousLogTime;
    bool inLogCycle = (delta <= LOG_CYCLE) && (delta > 0);
    if (inLogCycle)
        return false;

    // When we are out of Log Cycle and over the log limit, we need to reset
    // the counter and timer.
    m_previousLogTime = currentTime;
    m_currentLogCounter = 0;
    return false;
}

static void crashIfOOM(GLint errorCode)
{
    const GLint OOM_ERROR_CODE = 0x505;
    if (errorCode == OOM_ERROR_CODE || errorCode == EGL_BAD_ALLOC) {
        ALOGE("ERROR: Fatal OOM detected.");
        CRASH();
    }
}

void GLUtils::checkEglError(const char* op, EGLBoolean returnVal)
{
    if (returnVal != EGL_TRUE) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("EGL ERROR - %s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("after %s() eglError (0x%x)\n", op, error);
        crashIfOOM(error);
    }
}

bool GLUtils::checkGlError(const char* op)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR - after %s() glError (0x%x)\n", op, error);
        crashIfOOM(error);
        ret = true;
    }
    return ret;
}

bool GLUtils::checkGlErrorOn(void* p, const char* op)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR on %x - after %s() glError (0x%x)\n", p, op, error);
        crashIfOOM(error);
        ret = true;
    }
    return ret;
}

void GLUtils::checkSurfaceTextureError(const char* functionName, int status)
{
    if (status !=  NO_ERROR) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("ERROR at calling %s status is (%d)", functionName, status);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL extension checks
/////////////////////////////////////////////////////////////////////////////////////////

bool GLUtils::isEGLImageSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    return eglExtensions && glExtensions
        && strstr(eglExtensions, "EGL_KHR_image_base")
        && strstr(eglExtensions, "EGL_KHR_gl_texture_2D_image")
        && strstr(glExtensions, "GL_OES_EGL_image");
}

bool GLUtils::isEGLFenceSyncSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    return eglExtensions && strstr(eglExtensions, "EGL_KHR_fence_sync");
}

/////////////////////////////////////////////////////////////////////////////////////////
// Shader utilities
/////////////////////////////////////////////////////////////////////////////////////////

GLuint GLUtils::loadShader(GLenum shaderType, const char* pSource)
{
    GLSuccessVerifier glVerifier;

    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, 0);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                glGetShaderInfoLog(shader, infoLen, 0, buf);
                ALOGE("could not compile shader %d:\n%s\n", shaderType, buf);
                free(buf);
            }
            glDeleteShader(shader);
            shader = 0;
            }
        }
    }
    return shader;
}

GLint GLUtils::createProgram(const char* pVertexSource, const char* pFragmentSource)
{
    GLSuccessVerifier glVerifier;
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        ALOGE("couldn't load the vertex shader!");
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        ALOGE("couldn't load the pixel shader!");
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        GLUtils::checkGlError("glAttachShader vertex");
        glAttachShader(program, pixelShader);
        GLUtils::checkGlError("glAttachShader pixel");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, 0, buf);
                    ALOGE("could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }

    glDeleteShader(vertexShader);
    glDeleteShader(pixelShader);
    return program;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Textures utilities
/////////////////////////////////////////////////////////////////////////////////////////

static GLenum getInternalFormat(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_ALPHA;
    case SkBitmap::kARGB_4444_Config:
        return GL_RGBA;
    case SkBitmap::kARGB_8888_Config:
        return GL_RGBA;
    case SkBitmap::kRGB_565_Config:
        return GL_RGB;
    default:
        return -1;
    }
}

static GLenum getType(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kARGB_4444_Config:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case SkBitmap::kARGB_8888_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kIndex8_Config:
        return -1; // No type for compressed data.
    case SkBitmap::kRGB_565_Config:
        return GL_UNSIGNED_SHORT_5_6_5;
    default:
        return -1;
    }
}

void GLUtils::deleteTexture(GLuint* texture)
{
    glDeleteTextures(1, texture);
    GLUtils::checkGlError("glDeleteTexture");
    *texture = 0;
}

GLuint GLUtils::createSampleColorTexture(int r, int g, int b)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        r, g, b,
        r, g, b,
        r, g, b,
        r, g, b
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createSampleTexture()
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 0
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createTileGLTexture(int width, int height)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte* pixels = 0;
#ifdef DEBUG
    int length = width * height * 4;
    pixels = new GLubyte[length];
    for (int i = 0; i < length; i++)
        pixels[i] = i % 256;
#endif
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef DEBUG
    delete pixels;
#endif
    return texture;
}

void GLUtils::createTextureWithBitmap(GLuint texture, const SkBitmap& bitmap, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, bitmap.width(), bitmap.height(),
                 0, internalformat, type, bitmap.getPixels());
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexImage2D")) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR: glTexImage2D parameters are : textureId %d,"
              " bitmap.width() %d, bitmap.height() %d,"
              " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
              texture, bitmap.width(), bitmap.height(), internalformat, type,
              bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::updateTextureWithBitmap(GLuint texture, const SkBitmap& bitmap,
                                      const IntRect& inval, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    if (inval.isEmpty()) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap.width(), bitmap.height(),
                        internalformat, type, bitmap.getPixels());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, inval.x(), inval.y(), inval.width(), inval.height(),
                        internalformat, type, bitmap.getPixels());
    }
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexSubImage2D")) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR: glTexSubImage2D parameters are : textureId %d,"
              " bitmap.width() %d, bitmap.height() %d,"
              " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
              texture, bitmap.width(), bitmap.height(), internalformat, type,
              bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::createEGLImageFromTexture(GLuint texture, EGLImageKHR* image)
{
    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(texture);
    static const EGLint attr[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    *image = eglCreateImageKHR(eglGetCurrentDisplay(), eglGetCurrentContext(),
                               EGL_GL_TEXTURE_2D_KHR, buffer, attr);
    GLUtils::checkEglError("eglCreateImage", (*image != EGL_NO_IMAGE_KHR));
}

void GLUtils::createTextureFromEGLImage(GLuint texture, EGLImageKHR image, GLint filter, GLint wrap)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}

void GLUtils::convertToTransformationMatrix(const float* matrix, TransformationMatrix& transformMatrix)
{
    transformMatrix.setMatrix(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
}

bool GLUtils::deepCopyBitmapSubset(const SkBitmap& sourceBitmap,
                                   SkBitmap& subset, int leftOffset, int topOffset)
{
    sourceBitmap.lockPixels();
    subset.lockPixels();
    char* srcPixels = (char*) sourceBitmap.getPixels();
    char* dstPixels = (char*) subset.getPixels();
    if (!dstPixels || !srcPixels || !subset.lockPixelsAreWritable()) {
        ALOGD("no pixels :( %p, %p (writable=%d)", srcPixels, dstPixels,
              subset.lockPixelsAreWritable());
        subset.unlockPixels();
        sourceBitmap.unlockPixels();
        return false;
    }
    int srcRowSize = sourceBitmap.rowBytes();
    int destRowSize = subset.rowBytes();
    for (int i = 0; i < subset.height(); i++) {
        int srcOffset = (i + topOffset) * srcRowSize;
        srcOffset += (leftOffset * sourceBitmap.bytesPerPixel());
        int dstOffset = i * destRowSize;
        memcpy(dstPixels + dstOffset, srcPixels + srcOffset, destRowSize);
    }
    subset.unlockPixels();
    sourceBitmap.unlockPixels();
    return true;
}

void GLUtils::clearRect(GLenum buffers, int x, int y, int width, int height)
{
    ASSERT(!(buffers & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)));

    GLint oldViewport[4];
    GLint oldScissor;
    GLfloat oldColor[4];
    GLclampf oldDepth;
    GLint oldStencil;

    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glViewport(x, y, width, height);
    glGetIntegerv(GL_SCISSOR_TEST, &oldScissor);
    if (oldScissor)
        glDisable(GL_SCISSOR_TEST);

    if (buffers & GL_COLOR_BUFFER_BIT) {
        glGetFloatv(GL_COLOR_CLEAR_VALUE, oldColor);
        glClearColor(0, 0, 0, 0);
    }
    if (buffers & GL_DEPTH_BUFFER_BIT) {
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &oldDepth);
        glClearDepthf(1.0);
    }
    if (buffers & GL_STENCIL_BUFFER_BIT) {
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &oldStencil);
        glClearStencil(0);
    }

    glClear(buffers);

    if (buffers & GL_STENCIL_BUFFER_BIT)
        glClearStencil(oldStencil);

    if (buffers & GL_DEPTH_BUFFER_BIT)
        glClearDepthf(oldDepth);

    if (buffers & GL_COLOR_BUFFER_BIT)
        glClearColor(oldColor[0], oldColor[1], oldColor[2], oldColor[3]);

    if (oldScissor)
        glEnable(GL_SCISSOR_TEST);

    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

static void flipImage(void* imageData, size_t width, size_t height)
{
    uint32_t* line1 = reinterpret_cast<uint32_t*>(imageData);
    uint32_t* line2 = line1 + (height - 1) * width;
    for (; line2 > line1; line1 += width, line2 -= width) {
        for (size_t x = 0; x < width; x++)
            std::swap(line1[x], line2[x]);
    }
}

static inline uint8_t div_by_255_in_65025_range(unsigned x)
{
    ASSERT(x <= 255*255);
    // For all x = 0..(255*255):
    uint8_t result = (x + 1u + (x >> 8)) >> 8;
    ASSERT(x / 255u == result);
    return result;
}

static void premultiplyAlpha(void* pixelData, size_t numPixels)
{
    size_t numChannels = 4 * numPixels;
    uint8_t* pixels = reinterpret_cast<uint8_t*>(pixelData);
    for (size_t i = 0; i < numChannels; i += 4) {
        uint8_t alpha = pixels[3 + i];
        pixels[0 + i] = div_by_255_in_65025_range(pixels[0 + i] * alpha);
        pixels[1 + i] = div_by_255_in_65025_range(pixels[1 + i] * alpha);
        pixels[2 + i] = div_by_255_in_65025_range(pixels[2 + i] * alpha);
    }
}

static void unmultiplyAlpha(void* pixelData, size_t numPixels)
{
    size_t numChannels = 4 * numPixels;
    uint8_t* pixels = reinterpret_cast<uint8_t*>(pixelData);
    for (size_t i = 0; i < numChannels; i += 4) {
        if (uint8_t alpha = pixels[3 + i]) {
            pixels[0 + i] = std::min(255u, (pixels[0 + i] * 255u) / alpha);
            pixels[1 + i] = std::min(255u, (pixels[1 + i] * 255u) / alpha);
            pixels[2 + i] = std::min(255u, (pixels[2 + i] * 255u) / alpha);
        } else
            pixels[0 + i] = pixels[1 + i] = pixels[2 + i] = 0;
    }
}

void GLUtils::readPixels(const IntRect& rect, void* data, VerticalOrientation verticalOrientation, AlphaOp alphaOp)
{
    GLint oldPackAlignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(rect.x(), rect.y(), rect.width(), rect.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (verticalOrientation == TopToBottom)
        flipImage(data, rect.width(), rect.height());

    if (alphaOp == AlphaDoPremultiply)
        premultiplyAlpha(data, rect.width() * rect.height());
    else if (alphaOp == AlphaDoUnmultiply)
        unmultiplyAlpha(data, rect.width() * rect.height());

    glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
