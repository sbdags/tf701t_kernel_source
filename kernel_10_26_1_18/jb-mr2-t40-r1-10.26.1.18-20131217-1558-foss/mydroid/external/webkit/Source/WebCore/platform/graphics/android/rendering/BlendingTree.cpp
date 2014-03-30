/*
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
#include "BlendingTree.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AutoRestoreGLState.h"
#include "GLSuccessVerifier.h"
#include "GLUtils.h"
#include "ShaderProgram.h"

#include <sstream>
#include <wtf/FastAllocBase.h>

#if USE(NV_DRAW_TEXTURE)
#include <GLES2/gl2ext_nv.h>
#endif
// BlendingTree is a specialized binary space partitioning tree
// (http://en.wikipedia.org/wiki/Binary_space_partitioning).

// Instead of thinking in terms of which rectangles to blend, BlendingTree
// thinks in terms of subspaces. The 2d screen gets divided up into rectangular
// 2d subspaces, and each subspace has a stack of layers (a layer is either a
// pure color or a texture). At draw time, it draws each subspace and blends
// its layer stack together.

// There are two ways to subdivide space: a horizontal line and a vertical line
// (HorizontalSplitNode and VerticalSplitNode). These space divisions happen
// recursively. When inserting a quad into the tree, if it crosses a subspace
// boundary, it gets split in two along the boundary line and each half gets
// inserted recursively into its respective subspace.

// Once at a leaf node, the tree expresses a quad by 4 space divisions (one for
// each edge of the rectangle), and the middle subspace gets the quad's layer
// (color or texture) on its stack for blending.
// See the diagram below.

//
//                       Subspace 1
//
//  <----------------------------------------------------->
//                  |                  |
//                  |       QUAD       |
//                  |    Subspace 5    |
//      Subspace 4  |   (add layer     |
//                  |    to blending   |
//                  |    stack)        |     Subspace 2
//                  |                  |
//     <-------------------------------|
//                                     |
//                Subspace 3           |
//                                     |
//                                     V

static const float epsilon = 1e-5;
static const size_t maxQuadCount = 128;

#if USE(NV_DRAW_TEXTURE)
static PFNGLDRAWTEXTURENVPROC s_glDrawTextureNV;
#endif

namespace WebCore {

struct BlendingLayer {
    unsigned textureID;
    union {
        float color[4];
        float texgen[4];
    };
    float opacity;
    BlendingTree::TransferMode transferMode;
};

struct BlendingLayerNode {
    const BlendingLayerNode* previousLayer;
    uint64_t shaderKey;
    const BlendingLayer* layer;
};

struct GraphicsState {
    GraphicsState(const IntRect& viewport_, size_t maxBlendingDepth, bool initialIsBlending)
        : currentShader(0)
        , boundTextures(adoptArrayPtr(new GLuint[maxBlendingDepth]))
        , isBlending(initialIsBlending)
        , viewport(viewport_)
    {
    }
    const BlendingShader* currentShader;
    OwnArrayPtr<GLuint> boundTextures;
    bool isBlending;
    IntRect viewport;
};

class BlendingShader {
public:
    // A shader is defined by a 64-bit int, 4 bits per layer.
    static const uint8_t layerTypeMask = 0b0011;
    static const uint8_t emptyLayer = 0b0000;
    static const uint8_t colorLayer = 0b0001;
    static const uint8_t textureLayer = 0b0010;
    static const uint8_t hasOpacity = 0b0100;
    static const uint8_t needsMultiplyAlpha = 0b1000;

    static size_t maxLayerDepth()
    {
        int maxTextures, maxVaryings, maxVertexUniforms, maxFragmentUniforms;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextures);
        glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniforms);
        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxFragmentUniforms);

        // A shader is defined by a 64-bit int, 4 bits per layer.
        int maxDepth = 2 * sizeof(uint64_t);
        maxDepth = std::min(maxDepth, maxTextures);
        // We pack 2 sets of texture coords into each varying vector.
        maxDepth = std::min(maxDepth, 2 * maxVaryings);
        // We need a vertex uniform per texture plus one for the quad position.
        maxDepth = std::min(maxDepth, maxVertexUniforms - 1);
        // We need at most one fragment uniform per layer (color or opacity).
        maxDepth = std::min(maxDepth, maxFragmentUniforms);

        return maxDepth;
    }

    BlendingShader(uint64_t key)
        : m_key(key)
        , m_prev(0)
        , m_next(0)
        , m_layerCount(0)
        , m_colorCount(0)
        , m_textureCount(0)
        , m_opacityCount(0)
    {
        ASSERT(m_key);
        for (uint64_t key = m_key; key; key >>= 4) {
            if ((key & layerTypeMask) == colorLayer)
                m_colorCount++;
            else
                m_textureCount++;

            if (key & hasOpacity)
                m_opacityCount++;

            m_layerCount++;
        }

        std::ostringstream vertexSource;
        vertexSource << "uniform vec4 position;\n";
        vertexSource << "attribute vec2 corner;\n";
        if (m_textureCount) {
            vertexSource << "uniform vec4 texgen[" << m_textureCount << "];\n";
            vertexSource << "varying vec4 texcoords[" << ((1 + m_textureCount) / 2) << "];\n";
        }

        vertexSource << "void main()\n";
        vertexSource << "{\n";
        vertexSource << "vec4 vertex = vec4(corner * position.xy + position.zw, 0, 1);\n";
        for (size_t i = 0; i < m_textureCount; i++) {
            vertexSource << "texcoords[" << (i / 2) << "]." << ((i & 1) ? "zw" : "xy") << " = "
                << "vertex.xy * texgen[" << i << "].xy + texgen[" << i << "].zw;\n";
        }
        vertexSource << "gl_Position = vertex;\n";
        vertexSource << "}\n";

        std::ostringstream fragmentSource;
        fragmentSource << "precision lowp float;\n";
        if (m_colorCount)
            fragmentSource << "uniform vec4 colors[" << m_colorCount << "];\n";
        if (m_textureCount) {
            fragmentSource << "uniform sampler2D textures[" << m_textureCount << "];\n";
            fragmentSource << "varying vec4 texcoords[" << ((1 + m_textureCount) / 2) << "];\n";
        }
        if (m_opacityCount)
            fragmentSource << "uniform float opacities[" << m_opacityCount << "];\n";

        if (m_layerCount > 1) {
            fragmentSource << "vec4 blend(vec4 color1, vec4 color2)\n";
            fragmentSource << "{\n";
            fragmentSource << "return (1.0 - color2.a) * color1 + color2;\n";
            fragmentSource << "}\n";
        }

        fragmentSource << "void main()\n";
        fragmentSource << "{\n";
        fragmentSource << "vec4 layerColor, accumulatedColor;\n";
        size_t textureIndex = 0, colorIndex = 0, opacityIndex = 0;
        for (size_t i = 0; i < m_layerCount; i++) {
            uint8_t layerKey = (m_key >> (4 * (m_layerCount - i - 1))) & 0xf;
            if ((layerKey & layerTypeMask) == colorLayer)
                fragmentSource << "layerColor = colors[" << colorIndex++ << "];\n";
            else {
                fragmentSource << "layerColor = texture2D(textures[" << textureIndex << "], "
                    << "texcoords[" << (textureIndex / 2) << "]." << ((textureIndex & 1) ? "zw" : "xy") << ");\n";
                textureIndex++;
            }

            if (layerKey & needsMultiplyAlpha)
                fragmentSource << "layerColor = vec4(layerColor.a * layerColor.rgb, layerColor.a);\n";
            if (layerKey & hasOpacity)
                fragmentSource << "layerColor = opacities[" << opacityIndex++ << "] * layerColor;\n";

            if (i)
                fragmentSource << "accumulatedColor = blend(accumulatedColor, layerColor);\n";
            else
                fragmentSource << "accumulatedColor = layerColor;\n";
        }
        fragmentSource << "gl_FragColor = accumulatedColor;\n";
        fragmentSource << "}\n";

        m_id = GLUtils::createProgram(vertexSource.str().c_str(), fragmentSource.str().c_str());
        ASSERT(m_id);
        glBindAttribLocation(m_id, 0, "corner");
        glLinkProgram(m_id);

        glUseProgram(m_id);
        m_positionLocation = glGetUniformLocation(m_id, "position");
        if (m_colorCount)
            m_colorLocations = getUniformArrayLocations("colors", m_colorCount);
        if (m_textureCount) {
            m_texturesLocation = glGetUniformLocation(m_id, "textures");
            int textureIndices[m_textureCount];
            for (size_t i = 0; i < m_textureCount; i++)
                textureIndices[i] = i;
            glUniform1iv(m_texturesLocation, m_textureCount, textureIndices);
            m_texgenLocations = getUniformArrayLocations("texgen", m_textureCount);
        }
        if (m_opacityCount)
            m_opacityLocations = getUniformArrayLocations("opacities", m_opacityCount);
    }

    ~BlendingShader()
    {
        if (m_id)
            glDeleteProgram(m_id);
    }

    void didResetRenderingContext() { m_id = 0; }

    void useProgram() const { glUseProgram(m_id); }
    uint64_t key() const { return m_key; }
    BlendingShader* prev() const { return m_prev; }
    void setPrev(BlendingShader* prev) { m_prev = prev; }
    BlendingShader* next() const { return m_next; }
    void setNext(BlendingShader* next) { m_next = next; }
    size_t colorCount() const { return m_colorCount; }
    size_t textureCount() const { return m_textureCount; }
    size_t opacityCount() const { return m_opacityCount; }

    void setColor(size_t index, const float* color) const
    {
        ASSERT(index < m_colorCount);
        glUniform4fv(m_colorLocations[index], 1, color);
    }

    void setTexgen(size_t index, const float* texgen) const
    {
        ASSERT(index < m_textureCount);
        glUniform4fv(m_texgenLocations[index], 1, texgen);
    }

    void setOpacity(size_t index, float opacity) const
    {
        ASSERT(index < m_opacityCount);
        glUniform1f(m_opacityLocations[index], opacity);
    }

    void setQuadPosition(float x1, float y1, float x2, float y2) const
    {
        glUniform4f(m_positionLocation, x2 - x1, y2 - y1, x1, y1);
    }

private:
    PassOwnArrayPtr<int> getUniformArrayLocations(const char* arrayName, size_t arraySize)
    {
        OwnArrayPtr<int> locations = adoptArrayPtr(new int[arraySize]);
        size_t bufferSize = strlen(arrayName) + 3 * sizeof(size_t) + 3;
        char arrayElementName[bufferSize];
        for (size_t i = 0; i < arraySize; i++) {
            snprintf(arrayElementName, bufferSize, "%s[%u]", arrayName, i);
            locations[i] = glGetUniformLocation(m_id, arrayElementName);
        }
        return locations.release();
    }

    uint64_t m_key;
    BlendingShader* m_prev;
    BlendingShader* m_next;
    size_t m_layerCount;
    size_t m_colorCount;
    size_t m_textureCount;
    size_t m_opacityCount;
    int m_id;
    int m_positionLocation;
    int m_texturesLocation;
    OwnArrayPtr<int> m_colorLocations;
    OwnArrayPtr<int> m_texgenLocations;
    OwnArrayPtr<int> m_opacityLocations;
};

class BlendingNode : public RefCounted<BlendingNode> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum Edge {
        RightEdge = 1 << 0,
        TopEdge = 1 << 1,
        LeftEdge = 1 << 2,
        BottomEdge = 1 << 3
    };
    typedef unsigned Edges;
    typedef BlendingTree::ShaderCache ShaderCache;
    virtual PassRefPtr<BlendingNode> insert(const BlendingLayer*, float x1, float y1, float x2, float y2, Edges degenerateEdges, size_t layerDepth, size_t* maxLayerDepth) = 0;
    virtual void draw(float x1, float y1, float x2, float y2, const BlendingLayerNode*, GraphicsState*, ShaderCache&) = 0;
    virtual ~BlendingNode() {}
};

class HorizontalSplitNode : public BlendingNode {
public:
    HorizontalSplitNode(float splitX, PassRefPtr<BlendingNode> left, PassRefPtr<BlendingNode> right)
        : m_splitX(splitX)
        , m_left(left)
        , m_right(right)
    {}

    virtual PassRefPtr<BlendingNode> insert(const BlendingLayer* layer, float x1, float y1, float x2, float y2, Edges degenerateEdges, size_t layerDepth, size_t* maxLayerDepth)
    {
        if (fabs(x2 - m_splitX) < epsilon)
            m_left = m_left->insert(layer, x1, y1, x2, y2, RightEdge | degenerateEdges, layerDepth, maxLayerDepth);
        else if (fabs(x1 - m_splitX) < epsilon)
            m_right = m_right->insert(layer, x1, y1, x2, y2, LeftEdge | degenerateEdges, layerDepth, maxLayerDepth);
        else if (x2 < m_splitX)
            m_left = m_left->insert(layer, x1, y1, x2, y2, degenerateEdges, layerDepth, maxLayerDepth);
        else if (x1 > m_splitX)
            m_right = m_right->insert(layer, x1, y1, x2, y2, degenerateEdges, layerDepth, maxLayerDepth);
        else {
            m_left = m_left->insert(layer, x1, y1, m_splitX, y2, RightEdge | degenerateEdges, layerDepth, maxLayerDepth);
            m_right = m_right->insert(layer, m_splitX, y1, x2, y2, LeftEdge | degenerateEdges, layerDepth, maxLayerDepth);
        }

        return this;
    }

    virtual void draw(float x1, float y1, float x2, float y2, const BlendingLayerNode* topLayer, GraphicsState* graphicsState, ShaderCache& shaderCache)
    {
        if (x1 < m_splitX)
            m_left->draw(x1, y1, m_splitX, y2, topLayer, graphicsState, shaderCache);
        if (x2 > m_splitX)
            m_right->draw(m_splitX, y1, x2, y2, topLayer, graphicsState, shaderCache);
    }

private:
    float m_splitX;
    RefPtr<BlendingNode> m_left;
    RefPtr<BlendingNode> m_right;
};

class VerticalSplitNode : public BlendingNode {
public:
    VerticalSplitNode(float splitY, PassRefPtr<BlendingNode> top, PassRefPtr<BlendingNode> bottom)
        : m_splitY(splitY)
        , m_top(top)
        , m_bottom(bottom)
    {}

    virtual PassRefPtr<BlendingNode> insert(const BlendingLayer* layer, float x1, float y1, float x2, float y2, Edges degenerateEdges, size_t layerDepth, size_t* maxLayerDepth)
    {
        if (fabs(y2 - m_splitY) < epsilon)
            m_top = m_top->insert(layer, x1, y1, x2, y2, BottomEdge | degenerateEdges, layerDepth, maxLayerDepth);
        else if (fabs(y1 - m_splitY) < epsilon)
            m_bottom = m_bottom->insert(layer, x1, y1, x2, y2, TopEdge | degenerateEdges, layerDepth, maxLayerDepth);
        else if (y2 < m_splitY)
            m_top = m_top->insert(layer, x1, y1, x2, y2, degenerateEdges, layerDepth, maxLayerDepth);
        else if (y1 > m_splitY)
            m_bottom = m_bottom->insert(layer, x1, y1, x2, y2, degenerateEdges, layerDepth, maxLayerDepth);
        else {
            m_top = m_top->insert(layer, x1, y1, x2, m_splitY, BottomEdge | degenerateEdges, layerDepth, maxLayerDepth);
            m_bottom = m_bottom->insert(layer, x1, m_splitY, x2, y2, TopEdge | degenerateEdges, layerDepth, maxLayerDepth);
        }

        return this;
    }

    virtual void draw(float x1, float y1, float x2, float y2, const BlendingLayerNode* topLayer, GraphicsState* graphicsState, ShaderCache& shaderCache)
    {
        if (y1 < m_splitY)
            m_top->draw(x1, y1, x2, m_splitY, topLayer, graphicsState, shaderCache);
        if (y2 > m_splitY)
            m_bottom->draw(x1, m_splitY, x2, y2, topLayer, graphicsState, shaderCache);
    }

private:
    float m_splitY;
    RefPtr<BlendingNode> m_top;
    RefPtr<BlendingNode> m_bottom;
};

class PushLayerNode : public BlendingNode {
public:
    PushLayerNode(PassRefPtr<BlendingNode> childNode, const BlendingLayer* layer)
        : m_childNode(childNode)
        , m_layer(layer)
    {}

    virtual PassRefPtr<BlendingNode> insert(const BlendingLayer* layer, float x1, float y1, float x2, float y2, Edges degenerateEdges, size_t layerDepth, size_t* maxLayerDepth)
    {
        m_childNode = m_childNode->insert(layer, x1, y1, x2, y2, degenerateEdges, 1 + layerDepth, maxLayerDepth);
        return this;
    }

    virtual void draw(float x1, float y1, float x2, float y2, const BlendingLayerNode* topLayer, GraphicsState* graphicsState, ShaderCache& shaderCache)
    {
        BlendingLayerNode nextLayer;
        nextLayer.previousLayer = (m_layer->transferMode != BlendingTree::StraightCopy) ? topLayer : 0;
        nextLayer.shaderKey = nextLayer.previousLayer ? nextLayer.previousLayer->shaderKey << 4 : 0;
        nextLayer.layer = m_layer;

        if (!m_layer->textureID) {
            nextLayer.shaderKey |= BlendingShader::colorLayer;
            ASSERT(m_layer->opacity == 1);
            ASSERT(m_layer->transferMode != BlendingTree::UnmultipliedAlphaBlend);
        } else {
            nextLayer.shaderKey |= BlendingShader::textureLayer;
            if (m_layer->opacity != 1)
                nextLayer.shaderKey |= BlendingShader::hasOpacity;
            if (m_layer->transferMode == BlendingTree::UnmultipliedAlphaBlend)
                nextLayer.shaderKey |= BlendingShader::needsMultiplyAlpha;
        }

        m_childNode->draw(x1, y1, x2, y2, &nextLayer, graphicsState, shaderCache);
    }

private:
    RefPtr<BlendingNode> m_childNode;
    const BlendingLayer* m_layer;
};

class DrawQuadNode : public BlendingNode {
public:
    virtual PassRefPtr<BlendingNode> insert(const BlendingLayer* layer, float x1, float y1, float x2, float y2, Edges degenerateEdges, size_t layerDepth, size_t* maxLayerDepth)
    {
        RefPtr<BlendingNode> node = adoptRef(new PushLayerNode(this, layer));
        if (!(degenerateEdges & LeftEdge))
            node = adoptRef(new HorizontalSplitNode(x1, this, node.release()));
        if (!(degenerateEdges & BottomEdge))
            node = adoptRef(new VerticalSplitNode(y2, node.release(), this));
        if (!(degenerateEdges & RightEdge))
            node = adoptRef(new HorizontalSplitNode(x2, node.release(), this));
        if (!(degenerateEdges & TopEdge))
            node = adoptRef(new VerticalSplitNode(y1, this, node.release()));

        *maxLayerDepth = std::max(*maxLayerDepth, 1 + layerDepth);
        return node.release();
    }

    virtual void draw(float x1, float y1, float x2, float y2, const BlendingLayerNode* topLayer, GraphicsState* graphicsState, ShaderCache& shaderCache)
    {
        if (!topLayer)
            return;

#if USE(NV_DRAW_TEXTURE)
        if (s_glDrawTextureNV && !topLayer->previousLayer) {
            const BlendingLayer* layer = topLayer->layer;
            if (layer->textureID && layer->opacity == 1) {
                bool shouldBlendIntoFramebuffer = layer->transferMode != BlendingTree::StraightCopy;

                if (shouldBlendIntoFramebuffer != graphicsState->isBlending) {
                    if (shouldBlendIntoFramebuffer)
                        glEnable(GL_BLEND);
                    else
                        glDisable(GL_BLEND);

                    graphicsState->isBlending = shouldBlendIntoFramebuffer;
                }

                if (layer->transferMode == BlendingTree::UnmultipliedAlphaBlend) {
                    ASSERT(graphicsState->isBlending);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }

                // Map destination vertices to texture coordinates. Keep in sync with the shaders.
                // Unit square corner (0, 0).
                float u1 = x1 * layer->texgen[0] + layer->texgen[2];
                float v1 = y1 * layer->texgen[1] + layer->texgen[3];

                // Unit square corner (1, 1).
                float u2 = x2 * layer->texgen[0] + layer->texgen[2];
                float v2 = y2 * layer->texgen[1] + layer->texgen[3];

                // The (x1, y1) and (x2, y2) are in GL viewport coordinates, currently [-1..1].
                // Map them to GL window coordinates used by the glDrawTextureNV.
                const IntRect& v = graphicsState->viewport;
                float cx = v.center().x();
                float cy = v.center().y();
                float w = v.width() / 2;
                float h = v.height() / 2;

                x1 = cx + w * x1;
                y1 = cy + h * y1;
                x2 = cx + w * x2;
                y2 = cy + h * y2;

                s_glDrawTextureNV(layer->textureID, /* sampler */ 0, x1, y1, x2, y2, /* z */ 0, u1, v1, u2, v2);

                if (layer->transferMode == BlendingTree::UnmultipliedAlphaBlend)
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                return;
            }
        }
#endif

        const BlendingShader* shader = graphicsState->currentShader;
        if (!shader || shader->key() != topLayer->shaderKey) {
            shader = graphicsState->currentShader = shaderCache[topLayer->shaderKey];
            shader->useProgram();
        }

        size_t colorIndex = shader->colorCount() - 1;
        size_t textureIndex = shader->textureCount() - 1;
        size_t opacityIndex = shader->opacityCount() - 1;
        bool shouldBlendIntoFramebuffer = true;
        for (const BlendingLayerNode* layerNode = topLayer; layerNode; layerNode = layerNode->previousLayer) {
            const BlendingLayer* layer = layerNode->layer;
            if (!layer->textureID)
                shader->setColor(colorIndex--, layer->color);
            else {
                if (graphicsState->boundTextures[textureIndex] != layer->textureID) {
                    glActiveTexture(textureIndex + GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, layer->textureID);
                    graphicsState->boundTextures[textureIndex] = layer->textureID;
                }
                shader->setTexgen(textureIndex--, layer->texgen);
                if (layer->opacity != 1)
                    shader->setOpacity(opacityIndex--, layer->opacity);
            }
            if (layer->transferMode == BlendingTree::StraightCopy) {
                ASSERT(!layerNode->previousLayer);
                shouldBlendIntoFramebuffer = false;
            }
        }

        if (shouldBlendIntoFramebuffer != graphicsState->isBlending) {
            if (shouldBlendIntoFramebuffer)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);

            graphicsState->isBlending = shouldBlendIntoFramebuffer;
        }

        shader->setQuadPosition(x1, y1, x2, y2);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
};

BlendingTree::BlendingTree()
    : m_drawQuadNode(adoptRef(new DrawQuadNode()))
    , m_unitSquareBuffer(0)
    , m_maxLayerDepth(0)
{
#if USE(NV_DRAW_TEXTURE)
    if (!s_glDrawTextureNV)
        s_glDrawTextureNV = reinterpret_cast<PFNGLDRAWTEXTURENVPROC>(eglGetProcAddress("glDrawTextureNV"));
#endif

    m_blendingLayers = adoptArrayPtr(new BlendingLayer[maxQuadCount]);
    clear();
}

BlendingTree::~BlendingTree()
{
    cleanupGLResources();
}

void BlendingTree::clear()
{
    m_root = m_drawQuadNode;
    m_quadCount = 0;
    m_layerDepth = 0;
}

size_t BlendingTree::maxLayerDepth()
{
    if (!m_maxLayerDepth)
        m_maxLayerDepth = BlendingShader::maxLayerDepth();
    return m_maxLayerDepth;
}

bool BlendingTree::canAcceptMoreQuads()
{
    return m_quadCount < maxQuadCount
           && m_layerDepth < maxLayerDepth();
}

void BlendingTree::insert(float red, float green, float blue, float alpha,
                          FloatRect destRect, TransferMode transferMode)
{
    destRect.intersect(FloatRect(-1, -1, 2, 2));
    if (destRect.isEmpty())
        return;

    ASSERT(m_quadCount < maxQuadCount);
    ASSERT(m_layerDepth < maxLayerDepth());

    if (alpha == 1)
        transferMode = StraightCopy;
    else if (transferMode == UnmultipliedAlphaBlend) {
        red *= alpha;
        green *= alpha;
        blue *= alpha;
        transferMode = PremultipliedAlphaBlend;
    }

    BlendingLayer* layer = &m_blendingLayers[m_quadCount++];
    layer->textureID = 0;
    layer->color[0] = red;
    layer->color[1] = green;
    layer->color[2] = blue;
    layer->color[3] = alpha;
    layer->opacity = 1;
    layer->transferMode = transferMode;
    m_root = m_root->insert(layer, destRect.x(), destRect.y(), destRect.maxX(), destRect.maxY(), 0, 0, &m_layerDepth);
}

void BlendingTree::insert(unsigned textureID, FloatRect destRect, const FloatRect& texgen,
                          float opacity, TransferMode transferMode)
{
    if (!textureID) {
        insert(0, 0, 0, 1, destRect, StraightCopy);
        return;
    }

    destRect.intersect(FloatRect(-1, -1, 2, 2));
    if (destRect.isEmpty())
        return;

    ASSERT(m_quadCount < maxQuadCount);
    ASSERT(m_layerDepth < maxLayerDepth());

    BlendingLayer* layer = &m_blendingLayers[m_quadCount++];
    layer->textureID = textureID;
    layer->texgen[0] = texgen.width();
    layer->texgen[1] = texgen.height();
    layer->texgen[2] = texgen.x();
    layer->texgen[3] = texgen.y();
    layer->opacity = opacity;
    layer->transferMode = transferMode;
    m_root = m_root->insert(layer, destRect.x(), destRect.y(), destRect.maxX(), destRect.maxY(), 0, 0, &m_layerDepth);
}

void BlendingTree::draw()
{
    if (!m_layerDepth)
        return;
    GLSuccessVerifier glVerifier;

    AutoRestoreCurrentProgram restoreCurrentProgram;
    AutoRestoreArrayBufferBinding restoreArrayBuffer;
    AutoRestoreScissorTest restoreScissorTest;
    AutoRestoreBlend restoreBlend;
    AutoRestoreBlendFunc restoreBlendFunc;
    AutoRestoreBlendEquation restoreBlendEquation;
    AutoRestoreVertexAttribPointer restoreVertexAttribPointer(0);
    AutoRestoreEnabledVertexArrays restoreEnabledVertexArrays;
    AutoRestoreActiveTexture restoreActiveTexture;
    AutoRestoreMultiTextureBindings2D restoreMultiTextureBindings(m_layerDepth);

    glDisable(GL_SCISSOR_TEST);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glEnableVertexAttribArray(0);
    for (int i = 1; i < restoreEnabledVertexArrays.vertexArrayCount(); i++)
        glDisableVertexAttribArray(i);

    if (!m_unitSquareBuffer) {
        float unitSquare[] = {0,0, 1,0, 1,1, 0,1};
        glGenBuffers(1, &m_unitSquareBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_unitSquareBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(unitSquare), unitSquare, GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_unitSquareBuffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GraphicsState graphicsState(m_viewport, m_layerDepth, restoreBlend.blend());
    for (size_t i = 0; i < m_layerDepth; i++)
        graphicsState.boundTextures[i] = restoreMultiTextureBindings.textureBinding(i);

    if (!m_shaderCache)
        m_shaderCache = adoptPtr(new ShaderCache);

    m_root->draw(-1, -1, 1, 1, 0, &graphicsState, *(m_shaderCache.get()));
}

void BlendingTree::cleanupGLResources()
{
    m_shaderCache.clear();
    if (m_unitSquareBuffer) {
        glDeleteBuffers(1, &m_unitSquareBuffer);
        m_unitSquareBuffer = 0;
    }
}

void BlendingTree::didResetRenderingContext()
{
    m_unitSquareBuffer = 0;

    if (!m_shaderCache)
        return;

    for (ShaderCache::iterator it = m_shaderCache->begin(); it != m_shaderCache->end(); it = it->next())
        it->didResetRenderingContext();

    m_shaderCache.clear();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
