/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef InstrumentedPlatformCanvas_h
#define InstrumentedPlatformCanvas_h

#include "SkCanvas.h"
#if ENABLE(TRACE_TEXGEN_SKIA)
#include "TracingCanvas.h"
#endif

#define DEBUG_SKIA_DRAWING 0
#if DEBUG_SKIA_DRAWING
#include "AndroidLog.h" // NOTE: AndroidLog.h normally shouldn't be included in a header
#include "FloatRect.h"
#define WRAPCANVAS_LOG_ENTRY(...) {ALOGD("non-rect %s, m_isSolidColor %d", __FUNCTION__, m_isSolidColor);}
#else
#define WRAPCANVAS_LOG_ENTRY(...) ((void)0)
#endif

namespace WebCore {

#if ENABLE(TRACE_TEXGEN_SKIA)
class InstrumentedPlatformCanvas : public TracingCanvas {
    typedef TracingCanvas Inherited;
#else
class InstrumentedPlatformCanvas : public SkCanvas {
    typedef SkCanvas Inherited;
#endif

public:
    InstrumentedPlatformCanvas(int width, int height, Color initialColor)
        : m_size(width, height)
        , m_isSolidColor(true)
        , m_solidColor(initialColor)
    {
    }

    virtual ~InstrumentedPlatformCanvas() { }

    bool isSolidColor() const { return m_isSolidColor; }
    Color solidColor() const { return m_solidColor; }

    // overrides from SkCanvas
    virtual int save(SaveFlags flags = kMatrixClip_SaveFlag)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::save(flags);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint, SaveFlags flags)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        return Inherited::saveLayer(bounds, paint, flags);
    }

    virtual void restore()
    {
        WRAPCANVAS_LOG_ENTRY("");
        Inherited::restore();
    }

    virtual bool translate(SkScalar dx, SkScalar dy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::translate(dx, dy);
    }

    virtual bool scale(SkScalar sx, SkScalar sy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::scale(sx, sy);
    }

    virtual bool rotate(SkScalar degrees)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::rotate(degrees);
    }

    virtual bool skew(SkScalar sx, SkScalar sy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::skew(sx, sy);
    }

    virtual bool concat(const SkMatrix& matrix)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::concat(matrix);
    }

    virtual void setMatrix(const SkMatrix& matrix)
    {
        WRAPCANVAS_LOG_ENTRY("");
        Inherited::setMatrix(matrix);
    }

    virtual bool clipRect(const SkRect& rect, SkRegion::Op op)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return Inherited::clipRect(rect, op);
    }

    virtual bool clipPath(const SkPath& path, SkRegion::Op op)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        return Inherited::clipPath(path, op);
    }

    virtual bool clipRegion(const SkRegion& region, SkRegion::Op op)
    {
        WRAPCANVAS_LOG_ENTRY("");
        if (!region.isRect())
            m_isSolidColor = false;
        return Inherited::clipRegion(region, op);
    }

    virtual void clear(SkColor color)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = true;
        m_solidColor = Color(color);
        Inherited::clear(color);
    }

    virtual void drawPaint(const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPaint(paint);
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
            const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPoints(mode, count, pts, paint);
    }

    bool rectFullyOverlaps(const SkRect& rect)
    {
        IntRect canvasRect(IntPoint(), m_size);
        if (getTotalMatrix().rectStaysRect()
            && getTotalClip().contains(canvasRect)) {
            const SkMatrix& matrix = getTotalMatrix();
            SkRect mapped;
            matrix.mapRect(&mapped, rect);
            return mapped.contains(canvasRect);
        }
        return false;
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint)
    {

#if DEBUG_SKIA_DRAWING
        FloatRect rectToDraw = rect;
        ALOGD("drawrect " FLOAT_RECT_FORMAT ", is solid %d", FLOAT_RECT_ARGS(rectToDraw), m_isSolidColor);
#endif

        if (m_isSolidColor) {
            Color color = solidColor(paint);
            if (color != m_solidColor) {
                if (color.isValid() && rectFullyOverlaps(rect))
                    m_solidColor = color;
                else
                    m_isSolidColor = false;
            }
        }

        Inherited::drawRect(rect, paint);
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPath(path, paint);
    }

    virtual void drawBitmap(const SkBitmap& bitmap, SkScalar left,
            SkScalar top, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawBitmap(bitmap, left, top, paint);
    }

    virtual void drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src,
            const SkRect& dst, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawBitmapRectToRect(bitmap, src, dst, paint);
    }

    virtual void drawBitmapMatrix(const SkBitmap& bitmap,
            const SkMatrix& matrix, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawBitmapMatrix(bitmap, matrix, paint);
    }

    virtual void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center,
                                const SkRect& dst, const SkPaint* paint = 0)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawBitmapNine(bitmap, center, dst, paint);
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
            const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawSprite(bitmap, left, top, paint);
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
            SkScalar y, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
            const SkPoint pos[], const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPosText(text, byteLength, pos, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
            const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual void drawTextOnPath(const void* text, size_t byteLength,
            const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawTextOnPath(text, byteLength, path, matrix, paint);
    }

    virtual void drawPicture(SkPicture& picture)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawPicture(picture);
    }

    virtual void drawVertices(VertexMode mode, int vertexCount,
            const SkPoint vertices[], const SkPoint texs[],
            const SkColor colors[], SkXfermode* xfermode,
            const uint16_t indices[], int indexCount, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawVertices(mode, vertexCount, vertices, texs,
                               colors, xfermode, indices, indexCount, paint);
    }

    virtual void drawData(const void* data, size_t size)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        Inherited::drawData(data, size);
    }

private:
    Color solidColor(const SkPaint& paint)
    {
        if (paint.getStyle() != SkPaint::kFill_Style)
            return Color();
        if (paint.getLooper() || paint.getShader())
            return Color();

        SkXfermode::Mode mode;
        SkXfermode::AsMode(paint.getXfermode(), &mode);
        if (mode == SkXfermode::kClear_Mode)
            return Color(0, 0, 0, 0);

        if ((mode == SkXfermode::kSrcOver_Mode && paint.getAlpha() == 255)
            || mode == SkXfermode::kSrc_Mode)
            return Color(paint.getColor());
        return Color();
    }

    IntSize m_size;
    bool m_isSolidColor;
    Color m_solidColor;
    SkPaint m_solidPaint;
};

} // namespace WebCore

#endif // InstrumentedPlatformCanvas_h
