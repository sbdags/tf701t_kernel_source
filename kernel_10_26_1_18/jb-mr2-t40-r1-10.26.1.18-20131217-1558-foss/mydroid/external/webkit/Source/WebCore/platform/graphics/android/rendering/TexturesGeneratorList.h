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

#ifndef TexturesGeneratorList_h
#define TexturesGeneratorList_h

#if USE(ACCELERATED_COMPOSITING)

#include "TexturesGenerator.h"

namespace WebCore {

class TexturesGeneratorList {
public:
    static const size_t threadCount = 2;
    static TexturesGeneratorList* instance();

    void flushPendingPaintTileBatches();
    void removeOperationsForFilter(PassRefPtr<OperationFilter>);
    void scheduleOperation(PassOwnPtr<PaintTileOperation>);
    void commitPaintTileBatchIfNeeded();

    void setRendererType(BaseRenderer::RendererType);

    template<class T>
    class PerThread {
    public:
        static const size_t count = threadCount + 1;

        class Locker {
            WTF_MAKE_NONCOPYABLE(Locker);
        public:
            Locker(PerThread<T>& objects)
            {
                size_t index = TexturesGeneratorList::instance()->threadIndexForCurrentThread(threadCount);
                if (index < threadCount)
                    m_mutex = 0;
                else {
                    m_mutex = &objects.m_sharedInstanceLock;
                    m_mutex->lock();
                }
                m_object = &objects.m_objects[index];
            }

            ~Locker()
            {
                if (m_mutex)
                    m_mutex->unlock();
            }

            T& instance() { return *m_object; }

        private:
            T* m_object;
            WTF::Mutex* m_mutex;
        };
        friend class Locker;

        T& operator[](int i) { return m_objects[i]; }
        const T& operator[](int i) const { return m_objects[i]; }

        PerThread()
        {
        }

    private:
        PerThread(const PerThread<T>&);
        PerThread<T>& operator=(const PerThread<T>&);

        T m_objects[count];
        WTF::Mutex m_sharedInstanceLock;
    };

    size_t threadIndexForCurrentThread(size_t indexForOtherThreads) const;

private:
    TexturesGenerator::PaintTileBatch* pendingBatchForThreadIndex(size_t threadIndex)
    {
        if (!m_pendingBatches[threadIndex])
            m_pendingBatches[threadIndex] = adoptPtr(new TexturesGenerator::PaintTileBatch);

        return m_pendingBatches[threadIndex].get();
    }

    TexturesGenerator m_generators[threadCount];
    OwnPtr<TexturesGenerator::PaintTileBatch> m_pendingBatches[threadCount];
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TexturesGeneratorList_h
