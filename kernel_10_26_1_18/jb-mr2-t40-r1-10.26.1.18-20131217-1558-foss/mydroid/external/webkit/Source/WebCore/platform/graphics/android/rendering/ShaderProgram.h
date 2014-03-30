/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ShaderProgram_h
#define ShaderProgram_h

#if USE(ACCELERATED_COMPOSITING)

#include "BlendingTree.h"
#include "Color.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "ShaderProgramShaders.h"
#include "SkRect.h"
#include "TransformationMatrix.h"
#include "private/hwui/DrawGlInfo.h"
#include <GLES2/gl2.h>
#include <wtf/PassOwnPtr.h>

#define MAX_CONTRAST 5
#define DEBUG_MATRIX 0

namespace WebCore {

class DrawQuadData;
class PureColorQuadData;
class PureColorShader;
class RepeatTex2DShader;
class Tex2DShader;
class TextureQuadData;
class VideoShader;

#define FOR_EACH_SHADERPROGRAM_SHADER(macro)            \
    macro(PureColorShader, m_pureColorShader)           \
    macro(Tex2DShader, m_tex2DShader)                   \
    macro(Tex2DShader, m_tex2DInvShader)                \
    macro(Tex2DShader, m_texOESShader)                  \
    macro(Tex2DShader, m_texOESInvShader)               \
    macro(VideoShader, m_videoShader)                   \
    macro(RepeatTex2DShader, m_repeatTex2DShader)       \
    macro(RepeatTex2DShader, m_repeatTex2DInvShader)

class ShaderProgram {
public:
    ShaderProgram();
    void initGLResources();
    void cleanupGLResources();
    // Drawing
    void setupDrawing(const IntRect& invScreenRect, const SkRect& visibleContentRect,
                      const IntRect& screenRect, int titleBarHeight,
                      const IntRect& screenClip, float scale);
    FloatRect viewport() const { return m_contentViewport; }
    float viewportScale() const { return m_currentScale; }
    float zValue(const TransformationMatrix& drawMatrix, float w, float h);

    class FinishedDrawingCallback {
    public:
        virtual void didFinishDrawing() = 0;
        virtual ~FinishedDrawingCallback() {}
    };


    void drawQuad(const PureColorQuadData* data);
    void drawQuad(const TextureQuadData* data, PassOwnPtr<FinishedDrawingCallback> = 0);
    void drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                     const float* textureMatrix, const SkRect& geometry, int textureId);
    void clear(float red, float green, float blue, float alpha);
    void flushDrawing();

    FloatRect rectInInvViewCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);
    FloatRect rectInViewCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);

    FloatRect rectInViewCoord(const FloatRect& rect);
    FloatRect rectInInvViewCoord(const FloatRect& rect);
    FloatRect convertInvViewCoordToContentCoord(const FloatRect& rect);
    FloatRect convertViewCoordToInvViewCoord(const FloatRect& rect);
    FloatRect convertInvViewCoordToViewCoord(const FloatRect& rect);

    void clip(const FloatRect& rect);
    IntRect clippedRectWithVisibleContentRect(const IntRect& rect, int margin = 0);
    FloatRect contentViewport() { return m_contentViewport; }

    float contrast() { return m_contrast; }
    void setContrast(float c)
    {
        float contrast = c;
        if (contrast < 0)
            contrast = 0;
        if (contrast > MAX_CONTRAST)
            contrast = MAX_CONTRAST;
        m_contrast = contrast;
    }
    void setGLDrawInfo(const android::uirenderer::DrawGlInfo* info);
    void didResetRenderingContext();
    bool needsInit() { return m_needsInit; }
    bool usePointSampling(float tileScale, const TransformationMatrix* layerTransform);

private:
    TransformationMatrix getTileProjectionMatrix(const DrawQuadData* data);
    void setBlendingState(bool enableBlending, bool usePremultipliedAlpha = true);
    FloatRect viewportClipRect();
    Color shaderColor(Color pureColor, float opacity);
    Tex2DShader* textureShaderForTextureQuadData(const TextureQuadData* data);

    void resetGLViewport();
    void resetBlending();
    void setupSurfaceProjectionMatrix();
#if DEBUG_MATRIX
    FloatRect debugMatrixTransform(const TransformationMatrix& matrix, const char* matrixName);
    void debugMatrixInfo(float currentScale,
                         const TransformationMatrix& clipProjectionMatrix,
                         const TransformationMatrix& webViewMatrix,
                         const TransformationMatrix& modifiedDrawMatrix,
                         const TransformationMatrix* layerMatrix);
#endif // DEBUG_MATRIX

    bool m_blendingEnabled;
    bool m_usingPremultipliedAlpha;

    TransformationMatrix m_surfaceProjectionMatrix;
    TransformationMatrix m_clipProjectionMatrix;
    TransformationMatrix m_visibleContentRectProjectionMatrix;
    GLuint m_textureBuffer[1];

    TransformationMatrix m_contentToInvViewMatrix;
    TransformationMatrix m_contentToViewMatrix;
    SkRect m_visibleContentRect;
    IntRect m_invScreenRect;
    FloatRect m_clipRect;
    IntRect m_invViewClip;
    int m_titleBarHeight;
    // This is the layout position in screen coordinate and didn't contain the
    // animation offset.
    IntRect m_screenRect;

    FloatRect m_contentViewport;
    IntRect m_screenViewport;

    float m_contrast;

    // The height of the render target, either FBO or screen.
    int m_targetHeight;
    bool m_alphaLayer;
    TransformationMatrix m_webViewMatrix;
    float m_currentScale;

    // If there is any GL error happens such that the Shaders are not initialized
    // successfully at the first time, then we need to init again when we draw.
    bool m_needsInit;

    // For transfer queue blitting, we need a special matrix map from (0,1) to
    // (-1,1)
    TransformationMatrix m_transferProjMtx;

#define DEFINE_SHADER_RESOURCE(type, name)      \
    OwnPtr<type> name;

    FOR_EACH_SHADERPROGRAM_SHADER(DEFINE_SHADER_RESOURCE)

    BlendingTree m_deferredQuads;
    Vector<FinishedDrawingCallback*> m_finishedDrawingCallbacks;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // ShaderProgram_h
