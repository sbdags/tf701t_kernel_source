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
#include "TracingCanvas.h"

#if USE(ACCELERATED_COMPOSITING)
#if ENABLE(TRACE_TEXGEN_SKIA)

#include "AndroidLog.h" // for TRACE_METHOD

namespace WebCore {

int TracingCanvas::save(SaveFlags flags)
{
    TRACE_METHOD();
    return SkCanvas::save(flags);
}

int TracingCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint,
                          SaveFlags flags)
{
    TRACE_METHOD();
    return SkCanvas::saveLayer(bounds, paint, flags);
}
void TracingCanvas::restore()
{
    TRACE_METHOD();
    SkCanvas::restore();
}

bool TracingCanvas::translate(SkScalar dx, SkScalar dy)
{
    TRACE_METHOD();
    return SkCanvas::translate(dx, dy);
}

bool TracingCanvas::scale(SkScalar sx, SkScalar sy)
{
    TRACE_METHOD();
    return SkCanvas::scale(sx, sy);
}

bool TracingCanvas::rotate(SkScalar degrees)
{
    TRACE_METHOD();
    return SkCanvas::rotate(degrees);
}

bool TracingCanvas::skew(SkScalar sx, SkScalar sy)
{
    TRACE_METHOD();
    return SkCanvas::skew(sx, sy);
}

bool TracingCanvas::concat(const SkMatrix& matrix)
{
    TRACE_METHOD();
    return SkCanvas::concat(matrix);
}

void TracingCanvas::setMatrix(const SkMatrix& matrix)
{
    TRACE_METHOD();
    SkCanvas::setMatrix(matrix);
}

bool TracingCanvas::clipRect(const SkRect& rect, SkRegion::Op op, bool b)
{
    TRACE_METHOD();
    return SkCanvas::clipRect(rect, op, b);
}

bool TracingCanvas::clipPath(const SkPath& path, SkRegion::Op op, bool b)
{
    TRACE_METHOD();
    return SkCanvas::clipPath(path, op, b);
}

bool TracingCanvas::clipRegion(const SkRegion& deviceRgn,
                            SkRegion::Op op)
{
    TRACE_METHOD();
    return SkCanvas::clipRegion(deviceRgn, op);
}


void TracingCanvas::drawPaint(const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawPaint(paint);
}

void TracingCanvas::drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawPoints(mode, count, pts, paint);
}

void TracingCanvas::drawRect(const SkRect& rect, const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawRect(rect, paint);
}

void TracingCanvas::drawPath(const SkPath& path, const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawPath(path, paint);
}

void TracingCanvas::drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top,
                            const SkPaint* paint)
{
    TRACE_METHOD();
    SkCanvas::drawBitmap(bitmap, left, top, paint);
}

void TracingCanvas::drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                                const SkRect& dst, const SkPaint* paint)
{
    TRACE_METHOD();
    SkCanvas::drawBitmapRect(bitmap, src, dst, paint);
}

void TracingCanvas::drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m,
                                  const SkPaint* paint)
{
    TRACE_METHOD();
    SkCanvas::drawBitmapMatrix(bitmap, m, paint);
}

void TracingCanvas::drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint)
{
    TRACE_METHOD();
    SkCanvas::drawSprite(bitmap, left, top, paint);
}

void TracingCanvas::drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawText(text, byteLength, x, y, paint);
}

void TracingCanvas::drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawPosText(text, byteLength, pos, paint);
}

void TracingCanvas::drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawPosTextH(text, byteLength, xpos, constY, paint);
}

void TracingCanvas::drawTextOnPath(const void* text, size_t byteLength,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawTextOnPath(text, byteLength, path, matrix, paint);
}

void TracingCanvas::drawPicture(SkPicture& pic)
{
    TRACE_METHOD();
    SkCanvas::drawPicture(pic);
}

void TracingCanvas::drawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint vertices[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint)
{
    TRACE_METHOD();
    SkCanvas::drawVertices(vmode, vertexCount, vertices, texs, colors, xmode, indices, indexCount, paint);
}

SkBounder* TracingCanvas::setBounder(SkBounder* b)
{
    TRACE_METHOD();
    return SkCanvas::setBounder(b);
}

SkDrawFilter* TracingCanvas::setDrawFilter(SkDrawFilter* f)
{
    TRACE_METHOD();
    return SkCanvas::setDrawFilter(f);
}


} // namespace WebCore

#endif // ENABLE(TRACE_TEXGEN_SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
