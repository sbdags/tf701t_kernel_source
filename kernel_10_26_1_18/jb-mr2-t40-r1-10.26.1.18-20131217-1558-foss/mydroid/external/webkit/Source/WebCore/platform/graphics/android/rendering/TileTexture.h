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

#ifndef TileTexture_h
#define TileTexture_h

#include "Color.h"
#include "FloatRect.h"
#include "SkBitmap.h"
#include "SkRect.h"
#include "SkSize.h"
#include "TileBacking.h"
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>
#include <GLES2/gl2.h>

class SkCanvas;

namespace WebCore {

class TextureOwner;
class Tile;
class TransformationMatrix;
class MappedTexture;

class TileTexture : public TileBacking {
public:
    // This object is to be constructed on the UI thread.
    TileTexture();
    virtual ~TileTexture();

    // allows UI thread to assign ownership of the texture to the tile.
    void setOwner(TextureOwner* owner);

    virtual void release(TextureOwner* owner);

    virtual bool isReadyFor(TextureOwner* owner) const { return m_owner == owner; }

    // private member accessor functions
    TextureOwner* owner() { return m_owner; } // only used by the consumer thread

    // only call this from the UI thread, since it needs to delete the GL texture.
    void discardBackingStore();

    void discardGLTexture();

    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return m_hasAlpha; }
    void drawGL(bool isLayer, const SkRect& rect, float opacity,
                const TransformationMatrix* transform, bool forceBlending, bool usePointSampling,
                const FloatRect& fillPortion);

    virtual bool reservesTexture() const { return true; }

    // FIXME: REMOVE
    virtual unsigned int getImageTextureId() { return m_ownTextureId; }

    virtual bool canBlitUpdate() const { return m_ownTextureId; }
    virtual void blitUpdate(const SkBitmap& subset, const SkIRect& textureInval);

    void swapBuffer(OwnPtr<MappedTexture>& buffer, bool needsFlipY);

    bool hasTexture() const { return m_texture; }

    void didResetRenderingContext();

private:
    // OpenGL ID of texture, 0 when there's no GL texture.
    GLuint m_ownTextureId;

    // Tile owning the texture.
    WTF::Mutex m_ownerMutex;
    TextureOwner* m_owner;

    OwnPtr<MappedTexture> m_texture;
    bool m_textureNeedsFlipY;

    bool m_hasAlpha;
};

} // namespace WebCore

#endif // TileTexture_h
