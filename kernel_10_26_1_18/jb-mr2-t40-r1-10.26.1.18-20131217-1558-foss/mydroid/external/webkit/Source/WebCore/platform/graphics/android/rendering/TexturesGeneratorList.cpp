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
#include "TexturesGeneratorList.h"

#if USE(ACCELERATED_COMPOSITING)

static size_t threadIndexForTile(WebCore::Tile* tile)
{
    return (tile->x() + tile->y()) % WebCore::TexturesGeneratorList::threadCount;
}

namespace WebCore {

TexturesGeneratorList* TexturesGeneratorList::instance()
{
    AtomicallyInitializedStatic(TexturesGeneratorList*, texturesGeneratorList = new TexturesGeneratorList());
    return texturesGeneratorList;
}

void TexturesGeneratorList::flushPendingPaintTileBatches()
{
    for (size_t i = 0; i < threadCount; ++i)
        m_generators[i].flushPendingPaintTileBatches();
}

void TexturesGeneratorList::removeOperationsForFilter(PassRefPtr<OperationFilter> passedFilter)
{
    if (!passedFilter)
        return;

    RefPtr<OperationFilter> filter = passedFilter;

    for (size_t i = 0; i < threadCount; ++i) {
        TexturesGenerator::PaintTileBatch* pendingBatch = pendingBatchForThreadIndex(i);
        ASSERT(!pendingBatch->filter);
        pendingBatch->filter = filter;
    }
}

void TexturesGeneratorList::scheduleOperation(PassOwnPtr<PaintTileOperation> passedOperation)
{
    OwnPtr<PaintTileOperation> operation = passedOperation;

    size_t threadIndex = threadIndexForTile(operation->tile());
    pendingBatchForThreadIndex(threadIndex)->operations.append(operation.release());
}

void TexturesGeneratorList::commitPaintTileBatchIfNeeded()
{
    for (size_t i = 0; i < threadCount; ++i) {
        if (m_pendingBatches[i])
            m_generators[i].scheduleBatch(m_pendingBatches[i].release());
    }
}

size_t TexturesGeneratorList::threadIndexForCurrentThread(size_t indexForOtherThreads) const
{
    WTF::ThreadIdentifier c = currentThread();
    for (size_t i = 0; i < threadCount; ++i) {
        if (c == m_generators[i].threadId())
            return i;
    }
    return indexForOtherThreads;
}

void TexturesGeneratorList::setRendererType(BaseRenderer::RendererType type)
{
    for (size_t i = 0; i < threadCount; ++i)
        m_generators[i].setRendererType(type);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
