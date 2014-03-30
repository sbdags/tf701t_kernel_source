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
#include "EGLImageBufferRing.h"

#if USE(ACCELERATED_COMPOSITING)

#include "EGLImageBuffer.h"
#include "JNIUtility.h"

#define LOG_TAG "EGLImageBufferRing"
#include <cutils/log.h>

namespace WebCore {

EGLImageBufferRing::EGLImageBufferRing()
    : m_client(0)
    , m_fenceWaitThreadStatus(NotCreated)
    , m_fenceWaitThreadId(0)
    , m_threadExitRequested(false)
{
}

EGLImageBufferRing::~EGLImageBufferRing()
{
    if (m_fenceWaitThreadId) {
        ASSERT(m_fenceWaitThreadStatus == Created);

        {
            MutexLocker lock(m_mutex);
            m_threadExitRequested = true;
        }

        m_fenceWaitThreadReady.signal();
        waitForThreadCompletion(m_fenceWaitThreadId, 0);
        detachThread(m_fenceWaitThreadId);
    }
}

void EGLImageBufferRing::setClient(Client* client)
{
    MutexLocker lock(m_clientMutex);
    m_client = client;
}

PassOwnPtr<EGLImageBuffer> EGLImageBufferRing::takeFrontBufferAndLock()
{
    m_mutex.lock();

    // Let the pipe clear before allowing direct front buffer access.
    while (m_fenceWaitBuffer)
        m_fenceWaitBufferCleared.wait(m_mutex);

    if (m_stagedBuffer)
        rotateBuffersLocked();

    return m_frontBuffer.release();
}

void EGLImageBufferRing::submitFrontBufferAndUnlock(PassOwnPtr<EGLImageBuffer> buffer)
{
    ASSERT(!m_frontBuffer && !m_stagedBuffer && !m_fenceWaitBuffer);
    m_frontBuffer = buffer;
    m_mutex.unlock();
}

PassOwnPtr<EGLImageBuffer> EGLImageBufferRing::takeFreeBuffer()
{
    MutexLocker lock(m_mutex);

    // Wait until the buffer submitted previously is staged. That way the caller
    // won't ever have to allocate more than 4.
    while (m_fenceWaitBuffer)
        m_fenceWaitBufferCleared.wait(m_mutex);

    if (m_freeBuffers[1])
        return m_freeBuffers[1].release();

    return m_freeBuffers[0].release();
}

void EGLImageBufferRing::submitBuffer(PassOwnPtr<EGLImageBuffer> buffer)
{
    MutexLocker lock(m_mutex);

    if (m_fenceWaitThreadStatus == NotCreated) {
        m_fenceWaitThreadId = createThread(fenceWaitThreadEntry, this, "FenceWait");
        if (m_fenceWaitThreadId)
            m_fenceWaitThreadStatus = Created;
        else {
            ALOGE("Failed to create a thread to wait on buffer fences. Falling back on triple buffering.");
            m_fenceWaitThreadStatus = FailedToCreate;
        }
    }

    if (m_fenceWaitThreadStatus == FailedToCreate) {
        // We couldn't create a thread. Fall back on triple buffering.
        ASSERT(!m_freeBuffers[1] && !m_freeBuffers[0] && !m_stagedBuffer && !m_fenceWaitBuffer);
        m_freeBuffers[0] = m_frontBuffer.release();
        m_frontBuffer = buffer;
        return;
    }

    // takeFreeBuffer waits for m_fenceWaitBuffer to clear.
    ASSERT(!m_fenceWaitBuffer);
    m_fenceWaitBuffer = buffer;

    m_fenceWaitThreadReady.signal();
}

void EGLImageBufferRing::commitStagedBuffer()
{
    MutexLocker lock(m_mutex);
    if (!m_stagedBuffer)
        return;

    rotateBuffersLocked();
}

void EGLImageBufferRing::rotateBuffersLocked()
{
    ASSERT(m_stagedBuffer && !m_freeBuffers[1]);
    m_freeBuffers[1] = m_freeBuffers[0].release();
    m_freeBuffers[0] = m_frontBuffer.release();
    m_frontBuffer = m_stagedBuffer.release();
    m_stagedBufferCleared.signal();
}

void* EGLImageBufferRing::fenceWaitThreadEntry(void* self)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    JSC::Bindings::getJavaVM()->AttachCurrentThread(&env, 0);

    reinterpret_cast<EGLImageBufferRing*>(self)->runFenceWaitThread();

    JSC::Bindings::getJavaVM()->DetachCurrentThread();

    EGLBoolean ret = eglReleaseThread();
    ASSERT_UNUSED(ret, ret == EGL_TRUE);

    return 0;
}

void EGLImageBufferRing::runFenceWaitThread()
{
    for (;;) {
        {
            MutexLocker lock(m_mutex);
            for (;;) {
                if (m_threadExitRequested)
                    return;
                if (m_fenceWaitBuffer)
                    break;
                m_fenceWaitThreadReady.wait(m_mutex);
            }
        }

        m_fenceWaitBuffer->lockSurface();
        m_fenceWaitBuffer->finish();
        m_fenceWaitBuffer->unlockSurface();

        if (m_fenceWaitBuffer->isIntact()) {
            MutexLocker lock(m_mutex);

            while (m_stagedBuffer)
                m_stagedBufferCleared.wait(m_mutex);

            m_stagedBuffer = m_fenceWaitBuffer.release();
        } else {
            MutexLocker lock(m_mutex);
            m_fenceWaitBuffer.clear();
        }

        m_fenceWaitBufferCleared.broadcast();

        {
            MutexLocker lock(m_clientMutex);
            if (!m_client || m_client->onNewFrontBufferReady())
                commitStagedBuffer();
        }
    }
}

static inline void deleteBuffer(OwnPtr<EGLImageBuffer>& buffer)
{
    if (!buffer)
        return;
    buffer->lockSurface();
    buffer.clear();
}

void EGLImageBufferRing::deleteFreeBuffers()
{
    MutexLocker lock(m_mutex);

    while (m_fenceWaitBuffer)
        m_fenceWaitBufferCleared.wait(m_mutex);

    deleteBuffer(m_freeBuffers[1]);
    deleteBuffer(m_freeBuffers[0]);
    if (m_stagedBuffer) {
        deleteBuffer(m_frontBuffer);
        m_frontBuffer = m_stagedBuffer.release();
        m_stagedBufferCleared.signal();
    }
}

void EGLImageBufferRing::deleteAllBuffers()
{
    MutexLocker lock(m_mutex);

    while (m_fenceWaitBuffer)
        m_fenceWaitBufferCleared.wait(m_mutex);

    deleteBuffer(m_freeBuffers[1]);
    deleteBuffer(m_freeBuffers[0]);
    deleteBuffer(m_stagedBuffer);
    deleteBuffer(m_frontBuffer);
}

EGLImageBuffer* EGLImageBufferRing::lockFrontBufferForReadingGL(GLuint* outTextureId)
{
    EGLImageBuffer* frontBuffer = 0;
    {
        MutexLocker lock(m_mutex);
        if (!m_frontBuffer)
            return 0;
        frontBuffer = m_frontBuffer.get();
        frontBuffer->lockSurface();
    }

    frontBuffer->finish();

    GLuint textureId = 0;
    if (!frontBuffer->lockBufferForReadingGL(&textureId)) {
        frontBuffer->unlockSurface();
        return 0;
    }

    *outTextureId = textureId;
    return frontBuffer;
}

void EGLImageBufferRing::unlockFrontBufferGL(EGLImageBuffer* lockedFrontBuffer, GLuint textureId)
{
    lockedFrontBuffer->unlockBufferGL(textureId);
    lockedFrontBuffer->setFence();
    lockedFrontBuffer->unlockSurface();
}

EGLImageBuffer* EGLImageBufferRing::lockFrontBufferForReading(SkBitmap* bitmap, bool premultiplyAlpha)
{
    EGLImageBuffer* frontBuffer = 0;
    {
        MutexLocker lock(m_mutex);
        if (!m_frontBuffer)
            return 0;
        frontBuffer = m_frontBuffer.get();
        frontBuffer->lockSurface();
    }

    frontBuffer->finish();

    if (frontBuffer->lockBufferForReading(bitmap, premultiplyAlpha))
        return frontBuffer;

    frontBuffer->unlockSurface();
    return 0;
}

void EGLImageBufferRing::unlockFrontBuffer(EGLImageBuffer* lockedFrontBuffer)
{
    lockedFrontBuffer->unlockBuffer();
    lockedFrontBuffer->unlockSurface();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
