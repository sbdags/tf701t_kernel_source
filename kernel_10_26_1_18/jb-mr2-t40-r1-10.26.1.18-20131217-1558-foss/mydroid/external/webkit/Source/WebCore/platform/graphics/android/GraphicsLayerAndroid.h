/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef GraphicsLayerAndroid_h
#define GraphicsLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "Frame.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "LayerContent.h"
#include "PicturePile.h"
#include "RefPtr.h"
#include "ScrollableLayerAndroid.h"
#include "SkBitmapRef.h"
#include "Vector.h"

class SkBitmapRef;
class SkRegion;

namespace WebCore {

class FloatPoint3D;
class Image;
class LayerAndroid;
class FixedBackgroundImageLayerAndroid;
class ScrollableLayerAndroid;

class GraphicsLayerAndroid : public GraphicsLayer, PicturePainter {
public:

    GraphicsLayerAndroid(GraphicsLayerClient*);
    virtual ~GraphicsLayerAndroid();

    // PicturePainter

    virtual void paintContents(GraphicsContext* gc, IntRect& dirty);

    /////

    virtual void setName(const String&);

    // for hosting this GraphicsLayer in a native layer hierarchy
    virtual NativeLayer nativeLayer() const;

    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);
    virtual void setReplicatedLayer(GraphicsLayer* layer);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setPreserves3D(bool b);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setBackfaceVisibility(bool b);
    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setMaskLayer(GraphicsLayer*);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);

    virtual void setBackgroundColor(const Color&);
    virtual void clearBackgroundColor();

    virtual void setContentsOpaque(bool);

    virtual void setOpacity(float);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setContentsNeedsDisplay();
    virtual void setContentsRect(const IntRect& rect);

    virtual bool addAnimation(const KeyframeValueList& valueList,
                              const IntSize& boxSize,
                              const Animation* anim,
                              const String& keyframesName,
                              double beginTime);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&,
                                                const Animation*,
                                                const String& keyframesName,
                                                double beginTime,
                                                const IntSize& boxSize);
    bool createAnimationFromKeyframes(const KeyframeValueList&,
                                      const Animation*,
                                      const String& keyframesName,
                                      double beginTime);

    virtual void removeAnimation(const String&);
    virtual void pauseAnimation(const String& keyframesName);

    virtual void suspendAnimations(double time);
    virtual void resumeAnimations();

    virtual void setContentsToImage(Image*);
    virtual void setContentsToMedia(PlatformLayer*);
    virtual void setContentsToCanvas(PlatformLayer*);

    virtual PlatformLayer* platformLayer() const;

    void pauseDisplay(bool state);

#ifndef NDEBUG
    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);
#endif

    virtual void setZPosition(float);

    void gatherRootLayers(Vector<const RenderLayer*>&);
    virtual void syncCompositingState();
    virtual void syncCompositingStateForThisLayerOnly();
    void notifyClientAnimationStarted(double startTime);

    LayerAndroid* contentLayer() { return m_contentLayer; }
    LayerAndroid* foregroundLayer() { return m_foregroundLayer; }

    static int instancesCount();

    virtual void updateScrollOffset();

private:
    void askForSync();
    void askForMediaLayerContentsSync();

    void syncPositionState();
    void syncChildren();
    void syncMask();
    void syncMediaLayerContents();

    void updateMediaLayer();
    void updatePositionedLayers();
    void updateScrollingLayers();
    void updateFixedBackgroundLayers();

    // with SkPicture, we always repaint the entire layer's content.
    bool repaint();
    void needsNotifyClient();

    bool paintContext(LayerAndroid* layer, 
                      PicturePile& picture);

    bool m_needsSyncChildren;
    bool m_needsSyncMask;
    bool m_needsRepaint;
    bool m_needsNotifyClient;
    bool m_needsSyncMediaLayerContents;

    bool m_newImage;
    Image* m_image;

    LayerAndroid* m_contentLayer;
    FixedBackgroundImageLayerAndroid* m_fixedBackgroundLayer;
    LayerAndroid* m_foregroundLayer;
    LayerAndroid* m_foregroundClipLayer;

    PicturePile m_contentLayerContent;
    PicturePile m_foregroundLayerContent;
    LayerAndroid* m_mediaLayer;
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerAndroid_h
