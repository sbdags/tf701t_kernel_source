/*
 * Copyright 2010, The Android Open Source Project
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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

#define LOG_TAG "ShaderProgram"
#define LOG_NDEBUG 1

#include "config.h"
#include "ShaderProgram.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "DrawQuadData.h"
#include "FloatPoint3D.h"
#include "GLSuccessVerifier.h"
#include "GLUtils.h"
#include "TilesManager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define EPSILON 0.00001f

namespace WebCore {

// fillPortion.xy = starting UV coordinates.
// fillPortion.zw = UV coordinates width and height.
static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "uniform vec4 fillPortion;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vPosition.xy * fillPortion.zw + fillPortion.xy;\n"
    "}\n";

static const char gRepeatTexFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "uniform vec2 repeatScale;\n"
    "void main() {\n"
    "  vec2 repeatedTexCoord; "
    "  repeatedTexCoord.x = v_texCoord.x - floor(v_texCoord.x); "
    "  repeatedTexCoord.y = v_texCoord.y - floor(v_texCoord.y); "
    "  repeatedTexCoord.x = repeatedTexCoord.x * repeatScale.x; "
    "  repeatedTexCoord.y = repeatedTexCoord.y * repeatScale.y; "
    "  gl_FragColor = texture2D(s_texture, repeatedTexCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gRepeatTexFragmentShaderInverted[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform sampler2D s_texture; \n"
    "uniform vec2 repeatScale;\n"
    "void main() {\n"
    "  vec2 repeatedTexCoord; "
    "  repeatedTexCoord.x = v_texCoord.x - floor(v_texCoord.x); "
    "  repeatedTexCoord.y = v_texCoord.y - floor(v_texCoord.y); "
    "  repeatedTexCoord.x = repeatedTexCoord.x * repeatScale.x; "
    "  repeatedTexCoord.y = repeatedTexCoord.y * repeatScale.y; "
    "  vec4 pixel = texture2D(s_texture, repeatedTexCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

// We could pass the pureColor into either Vertex or Frag Shader.
// The reason we passed the color into the Vertex Shader is that some driver
// might create redundant copy when uniforms in fragment shader changed.
static const char gPureColorVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "uniform vec4 inputColor;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_color = inputColor;\n"
    "}\n";

static const char gPureColorFragmentShader[] =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

static const char gFragmentShaderInverted[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

static const char gVideoVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 textureMatrix;\n"
    "uniform mat4 projectionMatrix;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vec2(textureMatrix * vec4(vPosition.x, 1.0 - vPosition.y, 0.0, 1.0));\n"
    "}\n";

static const char gVideoFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES s_yuvTexture;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_yuvTexture, v_texCoord);\n"
    "}\n";

static const char gSurfaceTextureOESFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gSurfaceTextureOESFragmentShaderInverted[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

ShaderProgram::ShaderProgram()
    : m_blendingEnabled(false)
    , m_contrast(1)
    , m_alphaLayer(false)
    , m_currentScale(1.0f)
    , m_needsInit(true)
{
    m_textureBuffer[0] = 0;

    // initialize the matrix to calculate z values correctly, since it can be
    // used for that before setupDrawing is called.
    GLUtils::setOrthographicMatrix(m_visibleContentRectProjectionMatrix,
                                   0,0,1,1,
                                   -1000, 1000);
}

void ShaderProgram::cleanupGLResources()
{
    GLSuccessVerifier glVerifier;
#define CLEANUP_SHADER(type, name) if (name) { name->deleteProgram(); name = 0; }
    FOR_EACH_SHADERPROGRAM_SHADER(CLEANUP_SHADER);

    if (m_textureBuffer[0]) {
        glDeleteBuffers(1, m_textureBuffer);
        m_textureBuffer[0] = 0;
    }

    m_needsInit = true;
    GLUtils::checkGlError("cleanupGLResources");

    m_deferredQuads.cleanupGLResources();
}

void ShaderProgram::didResetRenderingContext()
{
    m_deferredQuads.didResetRenderingContext();
    m_needsInit = true;
}

void ShaderProgram::initGLResources()
{
    GLSuccessVerifier glVerifier;
    // To detect whether or not resources for ShaderProgram allocated
    // successfully, we clean up pre-existing errors here and will check for
    // new errors at the end of this function.
    GLUtils::checkGlError("before initGLResources");

    m_tex2DShader = Tex2DShader::create(gVertexShader, gFragmentShader);
    m_pureColorShader = PureColorShader::create(gPureColorVertexShader, gPureColorFragmentShader);
    m_tex2DInvShader = Tex2DShader::create(gVertexShader, gFragmentShaderInverted, Tex2DShader::HasContrast);
    m_videoShader = VideoShader::create(gVideoVertexShader, gVideoFragmentShader);
    m_texOESShader = Tex2DShader::create(gVertexShader, gSurfaceTextureOESFragmentShader);
    m_texOESInvShader = Tex2DShader::create(gVertexShader, gSurfaceTextureOESFragmentShaderInverted, Tex2DShader::HasContrast);
    m_repeatTex2DShader = RepeatTex2DShader::create(gVertexShader, gRepeatTexFragmentShader);
    m_repeatTex2DInvShader = RepeatTex2DShader::create(gVertexShader, gRepeatTexFragmentShaderInverted, Tex2DShader::HasContrast);

    if (!m_tex2DShader
        || !m_pureColorShader
        || !m_tex2DInvShader
        || !m_videoShader
        || !m_texOESShader
        || !m_texOESInvShader
        || !m_repeatTex2DShader
        || !m_repeatTex2DInvShader) {
        m_needsInit = true;
        return;
    }

    const GLfloat coord[] = {
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D
        0.0f, 1.0f, // A
        1.0f, 1.0f // B
    };

    glGenBuffers(1, m_textureBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), coord, GL_STATIC_DRAW);

    // Map x,y from (0,1) to (-1, 1)
    m_transferProjMtx.scale3d(2, 2, 1);
    m_transferProjMtx.translate3d(-0.5, -0.5, 0);

    m_needsInit = GLUtils::checkGlError("initGLResources");
}

void ShaderProgram::resetGLViewport()
{
    glViewport(m_screenViewport.x(), m_screenViewport.y(), m_screenViewport.width(), m_screenViewport.height());
}

void ShaderProgram::resetBlending()
{
    GLSuccessVerifier glVerifier;
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    m_blendingEnabled = false;
    m_usingPremultipliedAlpha = true;
}

void ShaderProgram::setBlendingState(bool enableBlending, bool usePremultipliedAlpha)
{
    GLSuccessVerifier glVerifier;
    if (enableBlending != m_blendingEnabled) {
        if (enableBlending)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        m_blendingEnabled = enableBlending;
    }

    if (enableBlending && usePremultipliedAlpha != m_usingPremultipliedAlpha) {
        if (usePremultipliedAlpha)
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_usingPremultipliedAlpha = usePremultipliedAlpha;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
// Drawing
/////////////////////////////////////////////////////////////////////////////////////////

// We have multiple coordinates to deal with: first is the screen coordinates,
// second is the view coordinates and the last one is content(document) coordinates.
// Both screen and view coordinates are in pixels.
// All these coordinates start from upper left, but for the purpose of OpenGL
// operations, we may need a inverted Y version of such coordinates which
// start from lower left.
//
// invScreenRect - inv screen coordinates starting from lower left.
// visibleContentRect - local content(document) coordinates starting from upper left.
// screenRect - screen coordinates starting from upper left.
// screenClip - screen coordinates starting from upper left.
//    ------------------------------------------
//    |(origin of screen)                      |
//    |screen                                  |
//    |   ---------------------------------    |
//    |   | (origin of view)              |    |
//    |   | webview                       |    |
//    |   |        --------               |    |
//    |   |        | clip |               |    |
//    |   |        |      |               |    |
//    |   |        --------               |    |
//    |   |                               |    |
//    |   |(origin of inv view)           |    |
//    |   ---------------------------------    |
//    |(origin of inv screen)                  |
//    ------------------------------------------
void ShaderProgram::setupDrawing(const IntRect& invScreenRect,
                                 const SkRect& visibleContentRect,
                                 const IntRect& screenRect, int titleBarHeight,
                                 const IntRect& screenClip, float scale)
{
    m_screenRect = screenRect;
    m_titleBarHeight = titleBarHeight;

    //// viewport ////
    GLUtils::setOrthographicMatrix(m_visibleContentRectProjectionMatrix,
                                   visibleContentRect.fLeft,
                                   visibleContentRect.fTop,
                                   visibleContentRect.fRight,
                                   visibleContentRect.fBottom,
                                   -1000, 1000);

    ALOGV("set m_clipProjectionMatrix, %d, %d, %d, %d",
          screenClip.x(), screenClip.y(), screenClip.x() + screenClip.width(),
          screenClip.y() + screenClip.height());

    // In order to incorporate the animation delta X and Y, using the clip as
    // the GL viewport can save all the trouble of re-position from screenRect
    // to final position.
    GLUtils::setOrthographicMatrix(m_clipProjectionMatrix, screenClip.x(), screenClip.y(),
                                   screenClip.x() + screenClip.width(),
                                   screenClip.y() + screenClip.height(), -1000, 1000);

    m_screenViewport = IntRect(screenClip.x(), m_targetHeight - screenClip.y() - screenClip.height(),
                               screenClip.width(), screenClip.height());
    m_deferredQuads.setViewport(m_screenViewport);
    resetGLViewport();

    m_visibleContentRect = visibleContentRect;
    m_currentScale = scale;


    //// viewRect ////
    m_invScreenRect = invScreenRect;

    // The following matrices transform content coordinates into view coordinates
    // and inv view coordinates.
    // Note that GLUtils::setOrthographicMatrix is inverting the Y.
    TransformationMatrix viewTranslate;
    viewTranslate.translate(1.0, 1.0);

    TransformationMatrix viewScale;
    viewScale.scale3d(m_invScreenRect.width() * 0.5f, m_invScreenRect.height() * 0.5f, 1);

    m_contentToInvViewMatrix = viewScale * viewTranslate * m_visibleContentRectProjectionMatrix;

    viewTranslate.scale3d(1, -1, 1);
    m_contentToViewMatrix = viewScale * viewTranslate * m_visibleContentRectProjectionMatrix;

    IntRect invViewRect(0, 0, m_screenRect.width(), m_screenRect.height());
    m_contentViewport = m_contentToInvViewMatrix.inverse().mapRect(invViewRect);


    //// clipping ////
    IntRect viewClip = screenClip;

    // The incoming screenClip is in screen coordinates, we first
    // translate it into view coordinates.
    // Then we convert it into inverted view coordinates.
    // Therefore, in the clip() function, we need to convert things back from
    // inverted view coordinates to inverted screen coordinates which is used by GL.
    viewClip.setX(screenClip.x() - m_screenRect.x());
    viewClip.setY(screenClip.y() - m_screenRect.y() - m_titleBarHeight);
    FloatRect invViewClip = convertViewCoordToInvViewCoord(viewClip);
    m_invViewClip.setLocation(IntPoint(invViewClip.x(), invViewClip.y()));
    // use ceilf to handle view -> doc -> view coord rounding errors
    m_invViewClip.setSize(IntSize(ceilf(invViewClip.width()), ceilf(invViewClip.height())));

    resetBlending();

    // Set up m_clipProjectionMatrix, m_currentScale and m_webViewMatrix before
    // calling this function.
    setupSurfaceProjectionMatrix();
}

// Calculate the right color value sent into the shader considering the (0,1)
// clamp and alpha blending.
Color ShaderProgram::shaderColor(Color pureColor, float opacity)
{
    float r = pureColor.red() / 255.0;
    float g = pureColor.green() / 255.0;
    float b = pureColor.blue() / 255.0;
    float a = pureColor.alpha() / 255.0;

    if (TilesManager::instance()->invertedScreen()) {
        float intensity = a - (0.2989 * r + 0.5866 * g + 0.1145 * b);
        intensity = ((intensity - a / 2.0) * m_contrast) + a / 2.0;
        intensity *= opacity;
        return Color(intensity, intensity, intensity, a * opacity);
    }
    return Color(r * opacity, g * opacity, b * opacity, a * opacity);
}

// For shaders using texture, it is easy to get the type from the textureTarget.
Tex2DShader* ShaderProgram::textureShaderForTextureQuadData(const TextureQuadData* data)
{
    if (data->textureTarget() == GL_TEXTURE_2D) {
        if (!TilesManager::instance()->invertedScreen())
            return data->hasRepeatScale() ?  m_repeatTex2DShader.get() : m_tex2DShader.get();
        else {
            // With the new GPU texture upload path, we do not use an FBO
            // to blit the texture we receive from the TexturesGenerator thread.
            // To implement inverted rendering, we thus have to do the rendering
            // live, by using a different shader.
            return data->hasRepeatScale() ?  m_repeatTex2DInvShader.get() : m_tex2DInvShader.get();
        }
    } else if (data->textureTarget() == GL_TEXTURE_EXTERNAL_OES) {
        if (!TilesManager::instance()->invertedScreen())
            return m_texOESShader.get();
        else
            return m_texOESInvShader.get();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// This function transform a clip rect extracted from the current layer
// into a clip rect in InvView coordinates -- used by the clipping rects
FloatRect ShaderProgram::rectInInvViewCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_contentToInvViewMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

// used by the partial screen invals
FloatRect ShaderProgram::rectInViewCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_contentToViewMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

FloatRect ShaderProgram::rectInViewCoord(const FloatRect& rect)
{
    return m_contentToViewMatrix.mapRect(rect);
}

FloatRect ShaderProgram::rectInInvViewCoord(const FloatRect& rect)
{
    return m_contentToInvViewMatrix.mapRect(rect);
}

FloatRect ShaderProgram::convertInvViewCoordToContentCoord(const FloatRect& rect)
{
    return m_contentToInvViewMatrix.inverse().mapRect(rect);
}

FloatRect ShaderProgram::convertViewCoordToInvViewCoord(const FloatRect& rect)
{
    FloatRect visibleContentRect = m_contentToViewMatrix.inverse().mapRect(rect);
    return rectInInvViewCoord(visibleContentRect);
}

FloatRect ShaderProgram::convertInvViewCoordToViewCoord(const FloatRect& rect)
{
    FloatRect visibleContentRect = m_contentToInvViewMatrix.inverse().mapRect(rect);
    return rectInViewCoord(visibleContentRect);
}

// clip is in screen coordinates
void ShaderProgram::clip(const FloatRect& clip)
{
    if (clip == m_clipRect)
        return;

    ALOGV("--clipping rect %f %f, %f x %f",
          clip.x(), clip.y(), clip.width(), clip.height());

    // we should only call glScissor in this function, so that we can easily
    // track the current clipping rect.

    IntRect screenClip = enclosingIntRect(clip);

    if (!m_invViewClip.isEmpty())
        screenClip.intersect(m_invViewClip);

    // The previous intersection calculation is using local screen coordinates.
    // Now we need to convert things from local screen coordinates to global
    // screen coordinates and pass to the GL functions.
    screenClip.setX(screenClip.x() + m_invScreenRect.x());
    screenClip.setY(screenClip.y() + m_invScreenRect.y());
    if (screenClip.x() < 0) {
        int w = screenClip.width();
        w += screenClip.x();
        screenClip.setX(0);
        screenClip.setWidth(w);
    }
    if (screenClip.y() < 0) {
        int h = screenClip.height();
        h += screenClip.y();
        screenClip.setY(0);
        screenClip.setHeight(h);
    }

    glScissor(screenClip.x(), screenClip.y(), screenClip.width(), screenClip.height());

    m_clipRect = clip;
}

IntRect ShaderProgram::clippedRectWithVisibleContentRect(const IntRect& rect, int margin)
{
    IntRect viewport(m_visibleContentRect.fLeft - margin, m_visibleContentRect.fTop - margin,
                     m_visibleContentRect.width() + margin,
                     m_visibleContentRect.height() + margin);
    viewport.intersect(rect);
    return viewport;
}

float ShaderProgram::zValue(const TransformationMatrix& drawMatrix, float w, float h)
{
    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.scale3d(w, h, 1);
    TransformationMatrix renderMatrix =
        m_visibleContentRectProjectionMatrix * modifiedDrawMatrix;
    FloatPoint3D point(0.5, 0.5, 0.0);
    FloatPoint3D result = renderMatrix.mapPoint(point);
    return result.z();
}

FloatRect ShaderProgram::viewportClipRect()
{
    FloatRect clipRect(-1, -1, 2, 2);

    GLboolean scissorTest;
    glGetBooleanv(GL_SCISSOR_TEST, &scissorTest);
    if (!scissorTest)
        return clipRect;

    // We should be able to avoid these glGet()s since all glScissor calls
    // should use clip(), but in practice that appears to not be the case.
    int scissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    FloatPoint center = FloatRect(m_screenViewport).center();
    FloatRect scissorRect(scissorBox[0] - center.x(),
                          scissorBox[1] - center.y(),
                          scissorBox[2],
                          scissorBox[3]);
    scissorRect.scale(2.0f / m_screenViewport.width(), 2.0f / m_screenViewport.height());
    clipRect.intersect(scissorRect);

    return clipRect;
}

// Put the common matrix computation at higher level to avoid redundancy.
void ShaderProgram::setupSurfaceProjectionMatrix()
{
    TransformationMatrix scaleMatrix;
    scaleMatrix.scale3d(m_currentScale, m_currentScale, 1);
    m_surfaceProjectionMatrix = m_clipProjectionMatrix * m_webViewMatrix * scaleMatrix;
}

// Calculate the matrix given the geometry.
TransformationMatrix ShaderProgram::getTileProjectionMatrix(const DrawQuadData* data)
{
    DrawQuadType type = data->type();
    if (type == Blit)
        return m_transferProjMtx;

    const TransformationMatrix* matrix = data->drawMatrix();
    const SkRect* geometry = data->geometry();
    FloatRect fillPortion = data->fillPortion();
    ALOGV("fillPortion " FLOAT_RECT_FORMAT, FLOAT_RECT_ARGS(fillPortion));

    // This modifiedDrawMatrix tranform (0,0)(1x1) to the final rect in screen
    // coordinates, before applying the m_webViewMatrix.
    // It first scale and translate the vertex array from (0,0)(1x1) to real
    // tile position and size. Then apply the transform from the layer's.
    // Finally scale to the currentScale to support zooming.
    // Note the geometry contains the tile zoom scale, so visually we will see
    // the tiles scale at a ratio as (m_currentScale/tile's scale).
    TransformationMatrix modifiedDrawMatrix;
    if (type == LayerQuad)
        modifiedDrawMatrix = *matrix;
    modifiedDrawMatrix.translate(geometry->fLeft + geometry->width() * fillPortion.x(),
                                 geometry->fTop + geometry->height() * fillPortion.y());
    modifiedDrawMatrix.scale3d(geometry->width() * fillPortion.width(),
                               geometry->height() * fillPortion.height(), 1);

    // Even when we are on a alpha layer or not, we need to respect the
    // m_webViewMatrix, it may contain the layout offset. Normally it is
    // identity.
    TransformationMatrix renderMatrix;
    renderMatrix = m_surfaceProjectionMatrix * modifiedDrawMatrix;

#if DEBUG_MATRIX
    debugMatrixInfo(m_currentScale, m_clipProjectionMatrix, m_webViewMatrix,
                    modifiedDrawMatrix, matrix);
#endif

    return renderMatrix;
}

void ShaderProgram::drawQuad(const PureColorQuadData* data)
{
    GLSuccessVerifier glVerifier;
    const TransformationMatrix& renderMatrix = getTileProjectionMatrix(data);

    FloatRect destRect = renderMatrix.mapRect(FloatRect(0, 0, 1, 1));
    destRect.intersect(viewportClipRect());
    if (destRect.isEmpty())
        return;

    Color color = shaderColor(data->color(), data->opacity());

    if (!color.alpha())
        return;

    bool canDeferRendering = renderMatrix.isTranslationsAndScales();

    if (canDeferRendering) {
        if (!m_deferredQuads.canAcceptMoreQuads())
            flushDrawing();

        BlendingTree::TransferMode transferMode = color.hasAlpha() ?
            BlendingTree::PremultipliedAlphaBlend : BlendingTree::StraightCopy;

        m_deferredQuads.insert(color.red() / 255.f, color.green() / 255.f,
                               color.blue() / 255.f, color.alpha() / 255.f,
                               destRect, transferMode);
        return;
    }

    flushDrawing();

    setBlendingState(color.hasAlpha(), true);

    m_pureColorShader->useProgram();
    m_pureColorShader->setProjectionMatrix(renderMatrix);
    m_pureColorShader->setColor(color);
    m_pureColorShader->bindPositionBuffer(m_textureBuffer);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::drawQuad(const TextureQuadData* data, PassOwnPtr<FinishedDrawingCallback> finishedDrawingCallback)
{
    GLSuccessVerifier glVerifier;
    const TransformationMatrix& renderMatrix = getTileProjectionMatrix(data);

    const FloatRect& clipRect = viewportClipRect();
    FloatRect destRect = renderMatrix.mapRect(FloatRect(0, 0, 1, 1));
    if (!destRect.intersects(clipRect)) {
        if (finishedDrawingCallback)
            finishedDrawingCallback->didFinishDrawing();
        return;
    }

    Tex2DShader* shader = textureShaderForTextureQuadData(data);

    bool canDeferRendering = renderMatrix.isTranslationsAndScales()
        && shader->canDeferRendering(data);

    if (canDeferRendering) {
        if (!m_deferredQuads.canAcceptMoreQuads())
            flushDrawing();
        FloatRect fillPortion = data->fillPortion();
        FloatRect texgen;
        if (renderMatrix.m11() > 0) {
            texgen.setWidth(fillPortion.width() / destRect.width());
            texgen.setX(fillPortion.x() - destRect.x() * texgen.width());
        } else {
            texgen.setWidth(-fillPortion.width() / destRect.width());
            texgen.setX(fillPortion.x() - destRect.maxX() * texgen.width());
        }
        if (renderMatrix.m22() > 0) {
            texgen.setHeight(fillPortion.height() / destRect.height());
            texgen.setY(fillPortion.y() - destRect.y() * texgen.height());
        } else {
            texgen.setHeight(-fillPortion.height() / destRect.height());
            texgen.setY(fillPortion.y() - destRect.maxY() * texgen.height());
        }

        // This requires that a texture won't be drawn multiple times in the
        // same frame with different filters.
        glBindTexture(GL_TEXTURE_2D, data->textureId());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, data->textureFilter());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, data->textureFilter());

        BlendingTree::TransferMode transferMode;
        if (data->opacity() == 1 && !data->hasAlpha())
            transferMode = BlendingTree::StraightCopy;
        else if (data->hasAlpha() && !data->hasPremultipliedAlpha())
            transferMode = BlendingTree::UnmultipliedAlphaBlend;
        else
            transferMode = BlendingTree::PremultipliedAlphaBlend;

        destRect.intersect(clipRect);
        m_deferredQuads.insert(data->textureId(), destRect, texgen, data->opacity(), transferMode);

        if (finishedDrawingCallback)
            m_finishedDrawingCallbacks.append(finishedDrawingCallback.leakPtr());

        return;
    }

    flushDrawing();

    bool enableBlending = data->hasAlpha() || data->opacity() < 1.0;
    setBlendingState(enableBlending, data->hasPremultipliedAlpha());

    shader->useProgram();
    shader->setProjectionMatrix(renderMatrix);
    shader->applyState(data, this);
    shader->bindPositionBuffer(m_textureBuffer);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (finishedDrawingCallback)
        finishedDrawingCallback->didFinishDrawing();
}

void ShaderProgram::drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                                       const float* textureMatrix, const SkRect& geometry,
                                       int textureId)
{
    GLSuccessVerifier glVerifier;
    flushDrawing();

    // switch to our custom yuv video rendering program
    m_videoShader->useProgram();
    // TODO: Merge drawVideoLayerQuad into drawQuad.
    TransformationMatrix modifiedDrawMatrix;
    modifiedDrawMatrix.scale3d(m_currentScale, m_currentScale, 1);
    modifiedDrawMatrix.multiply(drawMatrix);
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);
    TransformationMatrix renderMatrix =
        m_clipProjectionMatrix * m_webViewMatrix * modifiedDrawMatrix;

    m_videoShader->setProjectionMatrix(renderMatrix);
    m_videoShader->setTextureMatrix(textureMatrix);
    m_videoShader->bindTexture(textureId);
    m_videoShader->bindPositionBuffer(m_textureBuffer);

    setBlendingState(false);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::clear(float red, float green, float blue, float alpha)
{
    if (!m_deferredQuads.canAcceptMoreQuads())
        flushDrawing();

    m_deferredQuads.insert(red, green, blue, alpha, viewportClipRect(), BlendingTree::StraightCopy);
}

void ShaderProgram::flushDrawing()
{
    m_deferredQuads.draw();
    m_deferredQuads.clear();

    for (size_t i = 0; i < m_finishedDrawingCallbacks.size(); i++) {
        OwnPtr<FinishedDrawingCallback> finishedDrawingCallback = adoptPtr(m_finishedDrawingCallbacks[i]);
        finishedDrawingCallback->didFinishDrawing();
    }
    m_finishedDrawingCallbacks.clear();
}

void ShaderProgram::setGLDrawInfo(const android::uirenderer::DrawGlInfo* info)
{
    GLUtils::convertToTransformationMatrix(info->transform, m_webViewMatrix);
    m_alphaLayer = info->isLayer;
    m_targetHeight = info->height;
}

// This function is called per tileGrid to minimize the computation overhead.
// The ortho projection and glViewport will map 1:1, so we don't need to
// worry about them here. Basically, if the current zoom scale / tile's scale
// plus the webview and layer transformation ends up at scale factor 1.0,
// then we can use point sampling.
bool ShaderProgram::usePointSampling(float tileScale,
                                     const TransformationMatrix* layerTransform)
{
    const float testSize = 1.0;
    FloatRect rect(0, 0, testSize, testSize);
    TransformationMatrix matrix;
    matrix.scale3d(m_currentScale, m_currentScale, 1);
    if (layerTransform)
        matrix.multiply(*layerTransform);
    matrix.scale3d(1.0 / tileScale, 1.0 / tileScale, 1);

    matrix = m_webViewMatrix * matrix;

    rect = matrix.mapRect(rect);

    float deltaWidth = abs(rect.width() - testSize);
    float deltaHeight = abs(rect.height() - testSize);

    if (deltaWidth < EPSILON && deltaHeight < EPSILON) {
        ALOGV("Point sampling : deltaWidth is %f, deltaHeight is %f", deltaWidth, deltaHeight);
        return true;
    }
    return false;
}

#if DEBUG_MATRIX
FloatRect ShaderProgram::debugMatrixTransform(const TransformationMatrix& matrix,
                                              const char* matrixName)
{
    FloatRect rect(0.0, 0.0, 1.0, 1.0);
    rect = matrix.mapRect(rect);
    ALOGV("After %s matrix:\n %f, %f rect.width() %f rect.height() %f",
          matrixName, rect.x(), rect.y(), rect.width(), rect.height());
    return rect;

}

void ShaderProgram::debugMatrixInfo(float currentScale,
                                    const TransformationMatrix& clipProjectionMatrix,
                                    const TransformationMatrix& webViewMatrix,
                                    const TransformationMatrix& modifiedDrawMatrix,
                                    const TransformationMatrix* layerMatrix)
{
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    ALOGV("viewport %d, %d, %d, %d , currentScale %f",
          viewport[0], viewport[1], viewport[2], viewport[3], currentScale);
    IntRect currentGLViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    TransformationMatrix scaleMatrix;
    scaleMatrix.scale3d(currentScale, currentScale, 1.0);

    if (layerMatrix)
        debugMatrixTransform(*layerMatrix, "layerMatrix");

    TransformationMatrix debugMatrix = scaleMatrix * modifiedDrawMatrix;
    debugMatrixTransform(debugMatrix, "scaleMatrix * modifiedDrawMatrix");

    debugMatrix = webViewMatrix * debugMatrix;
    debugMatrixTransform(debugMatrix, "webViewMatrix * scaleMatrix * modifiedDrawMatrix");

    debugMatrix = clipProjectionMatrix * debugMatrix;
    FloatRect finalRect =
        debugMatrixTransform(debugMatrix, "all Matrix");
    // After projection, we will be in a (-1, 1) range and now we can map it back
    // to the (x,y) -> (x+width, y+height)
    ALOGV("final convert to screen coord x, y %f, %f width %f height %f , ",
          (finalRect.x() + 1) / 2 * currentGLViewport.width() + currentGLViewport.x(),
          (finalRect.y() + 1) / 2 * currentGLViewport.height() + currentGLViewport.y(),
          finalRect.width() * currentGLViewport.width() / 2,
          finalRect.height() * currentGLViewport.height() / 2);
}
#endif // DEBUG_MATRIX

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
