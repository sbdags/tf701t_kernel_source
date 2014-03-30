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

#define LOG_TAG "TexturesGenerator"
#define LOG_NDEBUG 1

#include "config.h"
#include "TexturesGenerator.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLUtils.h"
#include "GaneshRenderer.h"
#include "PaintTileOperation.h"
#include "RasterRenderer.h"
#include "TexturesGeneratorList.h"
#include "TilesManager.h"
#include <sys/resource.h>
#include <sys/time.h>
#include <utils/ThreadDefs.h>

static void* threadLoop(void* self)
{
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DEFAULT + 3 * ANDROID_PRIORITY_LESS_FAVORABLE);
    reinterpret_cast<WebCore::TexturesGenerator*>(self)->runScheduledOperations();
    return 0;
}

// Defer painting for one second if best in queue has priority
// PaintTileOperation::gDeferPriorityCutoff or higher
static double processDeferredWait = 1.0;

namespace WebCore {

TexturesGenerator::TexturesGenerator()
    : m_deferredMode(true)
    , m_exitRequested(false)
    , m_batchFlushRequested(false)
    , m_rendererType(BaseRenderer::Raster)
{
    WTF::MutexLocker locker(m_requestedOperationsLock);
    m_threadId = WTF::createThread(threadLoop, this, "WKTexGen");
}

TexturesGenerator::~TexturesGenerator()
{
    {
        WTF::MutexLocker lock(m_requestedOperationsLock);
        m_exitRequested = true;
    }

    m_requestedOperationsCond.signal();
    waitForThreadCompletion(m_threadId, 0);
    detachThread(m_threadId);

    deleteAllValues(m_operations);
}

void TexturesGenerator::flushPendingPaintTileBatches()
{
    WTF::MutexLocker lock(m_requestedOperationsLock);
    m_batchFlushRequested = true;
    m_requestedOperationsCond.signal();
    while(m_batchFlushRequested)
        m_pendingBatchFlushCond.wait(m_requestedOperationsLock);
}

void TexturesGenerator::setRendererType(BaseRenderer::RendererType type)
{
    WTF::MutexLocker lock(m_requestedOperationsLock);
    m_rendererType = type;
}

void TexturesGenerator::scheduleBatch(PassOwnPtr<PaintTileBatch> paintUpdate)
{
    WTF::MutexLocker lock(m_requestedOperationsLock);
    m_requestedOperations.append(paintUpdate);
    m_requestedOperationsCond.signal();
}

void TexturesGenerator::addOrUpdatePaintTileOperation(PassOwnPtr<PaintTileOperation> passedOperation)
{
    OwnPtr<PaintTileOperation> operation = passedOperation;

    // Move to non-deferred mode if the new operation was not a deferrable.
    const unsigned long long currentDrawGLCount = TilesManager::instance()->getDrawGLCount();
    bool deferrable = operation->priority(currentDrawGLCount) >= PaintTileOperation::gDeferPriorityCutoff;
    m_deferredMode &= deferrable;

    delete m_operations.take(operation->tile());
    m_operations.set(operation->tile(), operation.leakPtr());
}

void TexturesGenerator::removeOperationsForFilter(OperationFilter* filter)
{
    Vector<Tile*> removed;
    for (OperationsHash::const_iterator end = m_operations.end(), it = m_operations.begin();
         it != end; ++it) {
        if (filter->check(it->second)) {
            ASSERT(it->second->tile() == it->first);
            removed.append(it->first);
        }
    }

    for (size_t i = 0; i < removed.size(); ++i)
        delete m_operations.take(removed[i]);
}

void TexturesGenerator::addBatches(Vector<OwnPtr<PaintTileBatch> >& batches)
{
    for (size_t i = 0; i < batches.size(); ++i) {
        if (batches[i]->filter)
            removeOperationsForFilter(batches[i]->filter.get());
        Vector<OwnPtr<PaintTileOperation> >& operations = batches[i]->operations;
        for (size_t j = 0; j < operations.size(); ++j)
            addOrUpdatePaintTileOperation(operations[j].release());
    }
}

PassOwnPtr<PaintTileOperation> TexturesGenerator::popNext()
{
    // Priority can change between when it was added and now
    // Hence why the entire queue is rescanned

    PaintTileOperation* current = 0;
    int currentPriority = std::numeric_limits<int>::max();
    unsigned long long currentDrawGLCount = TilesManager::instance()->getDrawGLCount();

    for (OperationsHash::const_iterator end = m_operations.end(), it = m_operations.begin();
         it != end; ++it) {
        PaintTileOperation* next = it->second;

        int nextPriority = next->priority(currentDrawGLCount);
        if (!m_deferredMode && nextPriority >= PaintTileOperation::gDeferPriorityCutoff)
            continue;

        // Otherwise pick items preferrably by priority, or if equal, by order of
        // insertion (as we add items at the back of the queue)
        if (!current || nextPriority <= currentPriority) {
            current = next;
            currentPriority = nextPriority;

            // Found a very high priority item, go ahead and just handle it now.
            if (currentPriority < 0)
                break;
        }
    }

    m_deferredMode = !current || currentPriority >= PaintTileOperation::gDeferPriorityCutoff;

    if (!current)
        return 0;

    ASSERT(current->tile());
    return adoptPtr(m_operations.take(current->tile()));
}

void TexturesGenerator::updateRendererIfNeeded(BaseRenderer::RendererType type)
{
    if (!m_renderer || type != m_renderer->type()) {
        if (type == BaseRenderer::Ganesh)
            m_renderer.set(new GaneshRenderer());
        else
            m_renderer.set(new RasterRenderer());
    }
}

void TexturesGenerator::runScheduledOperations()
{
    while (true) {
        Vector<OwnPtr<PaintTileBatch> > batches;
        BaseRenderer::RendererType rendererType;
        bool shouldSignalWhenBatchesFlushed;

        {
            WTF::MutexLocker locker(m_requestedOperationsLock);

            if (m_exitRequested)
                return;

            batches.swap(m_requestedOperations);

            rendererType = m_rendererType;
            shouldSignalWhenBatchesFlushed = m_batchFlushRequested;
        }

        addBatches(batches);
        batches.clear();
        if (shouldSignalWhenBatchesFlushed) {
            {
                WTF::MutexLocker locker(m_requestedOperationsLock);
                m_batchFlushRequested = false;
            }
            m_pendingBatchFlushCond.signal();
        }

        if (m_operations.isEmpty()) {
            WTF::MutexLocker locker(m_requestedOperationsLock);
            if (!m_batchFlushRequested)
                m_requestedOperationsCond.wait(m_requestedOperationsLock);
            continue;
        }

        OwnPtr<PaintTileOperation> currentOperation = popNext();
        if (!currentOperation) {
            // Moved from non-deferred to deferred mode. Wait for the deferred amount.
            WTF::MutexLocker locker(m_requestedOperationsLock);
            if (!m_batchFlushRequested)
                m_requestedOperationsCond.timedWait(m_requestedOperationsLock, currentTime() + processDeferredWait);
            continue;
        }

        updateRendererIfNeeded(rendererType);
        currentOperation->run(m_renderer.get());
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
