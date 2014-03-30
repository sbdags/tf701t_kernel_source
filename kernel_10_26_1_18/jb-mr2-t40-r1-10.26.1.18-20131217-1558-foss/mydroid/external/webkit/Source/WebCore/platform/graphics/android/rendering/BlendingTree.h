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

#ifndef BlendingTree_h
#define BlendingTree_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatRect.h"
#include "IntRect.h"
#include <wtf/LRUCache.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class BlendingLayer;
class BlendingNode;
class BlendingShader;

class BlendingTree {
public:
    typedef LRUCache<uint64_t, BlendingShader, 128> ShaderCache;

    BlendingTree();
    ~BlendingTree();

    void setViewport(const IntRect& viewport) { m_viewport = viewport; }
    void cleanupGLResources();
    void didResetRenderingContext();

    void clear();

    bool canAcceptMoreQuads();

    enum TransferMode {
        StraightCopy,
        PremultipliedAlphaBlend,
        UnmultipliedAlphaBlend
    };

    // Inserts a colored quad to the tree to be blended later.
    void insert(float red, float green, float blue, float alpha, FloatRect destRect, TransferMode);

    // Inserts a textured quad to the tree to be blended later.
    // \destRect The quad in GL viewport coordinates to fill with the texture.
    // \texgen Scale and translate values which map GL viewport coordinates of destRect
    //         to texture coordinates [0..1].
    void insert(unsigned textureID, FloatRect destRect, const FloatRect& texgen, float opacity, TransferMode);

    void draw();

private:
    unsigned maxLayerDepth();

    RefPtr<BlendingNode> m_root;
    RefPtr<BlendingNode> m_drawQuadNode;
    OwnArrayPtr<BlendingLayer> m_blendingLayers;
    OwnPtr<ShaderCache> m_shaderCache;
    unsigned m_unitSquareBuffer;
    size_t m_quadCount;
    size_t m_layerDepth;
    size_t m_maxLayerDepth;
    IntRect m_viewport;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // BlendingTree_h
