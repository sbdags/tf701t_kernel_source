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

#ifndef TexturesGenerator_h
#define TexturesGenerator_h

#if USE(ACCELERATED_COMPOSITING)

#include "PaintTileOperation.h"
#include "BaseRenderer.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

class TexturesGenerator {
public:
    struct PaintTileBatch {
        RefPtr<OperationFilter> filter;
        Vector<OwnPtr<PaintTileOperation> > operations;
    };

    TexturesGenerator();
    ~TexturesGenerator();

    void flushPendingPaintTileBatches();

    void scheduleBatch(PassOwnPtr<PaintTileBatch>);

    void setRendererType(BaseRenderer::RendererType);

    WTF::ThreadIdentifier threadId() const { return m_threadId; }

    void runScheduledOperations();
private:
    void addBatches(Vector<OwnPtr<PaintTileBatch> >&);
    void addOrUpdatePaintTileOperation(PassOwnPtr<PaintTileOperation> passedOperation);
    void removeOperationsForFilter(OperationFilter*);

    PassOwnPtr<PaintTileOperation> popNext();
    void updateRendererIfNeeded(BaseRenderer::RendererType);

    WTF::ThreadIdentifier m_threadId;
    WTF::Vector<OwnPtr<PaintTileBatch> > m_requestedOperations;
    WTF::Mutex m_requestedOperationsLock;
    WTF::ThreadCondition m_requestedOperationsCond;
    WTF::ThreadCondition m_pendingBatchFlushCond;

    typedef WTF::HashMap<Tile*, PaintTileOperation* > OperationsHash;
    OperationsHash m_operations;

    bool m_deferredMode;
    OwnPtr<BaseRenderer> m_renderer;
    bool m_exitRequested;
    bool m_batchFlushRequested;
    BaseRenderer::RendererType m_rendererType;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TexturesGenerator_h
