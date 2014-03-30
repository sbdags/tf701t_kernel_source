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

#ifndef EGLImageBufferRing_h
#define EGLImageBufferRing_h

#if USE(ACCELERATED_COMPOSITING)

#include <GLES2/gl2.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSafeRefCounted.h>

class SkBitmap;

namespace WebCore {

class EGLImage;
class EGLImageBuffer;
class EGLImageLayer;
class FPSTimer;
class LayerAndroid;

class EGLImageBufferRing : public ThreadSafeRefCounted<EGLImageBufferRing> {
public:
    EGLImageBufferRing();
    ~EGLImageBufferRing();

    class Client {
    public:
        // Return false if the buffer ring should NOT commit the new staged buffer.
        virtual bool onNewFrontBufferReady() = 0;
        virtual ~Client() {}
    };
    void setClient(Client*);

    PassOwnPtr<EGLImageBuffer> takeFrontBufferAndLock();
    void submitFrontBufferAndUnlock(PassOwnPtr<EGLImageBuffer>);

    PassOwnPtr<EGLImageBuffer> takeFreeBuffer();
    void submitBuffer(PassOwnPtr<EGLImageBuffer>);
    void commitStagedBuffer();

    void deleteFreeBuffers();
    void deleteAllBuffers();

    EGLImageBuffer* lockFrontBufferForReadingGL(GLuint* outTextureId);
    void unlockFrontBufferGL(EGLImageBuffer*, GLuint textureId);

    EGLImageBuffer* lockFrontBufferForReading(SkBitmap*, bool premultiplyAlpha);
    void unlockFrontBuffer(EGLImageBuffer*);

private:
    void rotateBuffersLocked();
    static void* fenceWaitThreadEntry(void* self);
    void runFenceWaitThread();

    WTF::Mutex m_clientMutex;
    Client* m_client;

    WTF::Mutex m_mutex;
    WTF::ThreadCondition m_stagedBufferCleared;
    WTF::ThreadCondition m_fenceWaitBufferCleared;
    WTF::ThreadCondition m_fenceWaitThreadReady;
    OwnPtr<EGLImageBuffer> m_freeBuffers[2];
    OwnPtr<EGLImageBuffer> m_frontBuffer;
    OwnPtr<EGLImageBuffer> m_stagedBuffer;
    OwnPtr<EGLImageBuffer> m_fenceWaitBuffer;

    enum FenceWaitThreadStatus {
        NotCreated,
        Created,
        FailedToCreate
    };
    FenceWaitThreadStatus m_fenceWaitThreadStatus;
    ThreadIdentifier m_fenceWaitThreadId;
    bool m_threadExitRequested;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // EGLImageBufferRing_h
