/*
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

#ifndef MessageHandler_h
#define MessageHandler_h

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace android {

class MessageHandler {
    WTF_MAKE_NONCOPYABLE(MessageHandler);
    struct JavaGlue;

public:
    class JNICallbacks;
    friend class JNICallbacks;

    MessageHandler();
    virtual ~MessageHandler();

    bool hasMessages(unsigned what);
    void removeMessages(unsigned what);
    void removeAllMessages();

    void sendMessage(unsigned what, int = 0, int = 0, void* = 0);
    void sendMessage(unsigned what, void* obj)
    {
        sendMessage(what, 0, 0, obj);
    }

    void sendMessageDelayed(unsigned what, double delaySeconds, int = 0, int = 0, void* = 0);
    void sendMessageDelayed(unsigned what, double delaySeconds, void* obj)
    {
        sendMessageDelayed(what, delaySeconds, 0, 0, obj);
    }

protected:
    virtual void handleMessage(unsigned what, int, int, void*) = 0;

private:
    OwnPtr<JavaGlue> m_glue;
};

} // namespace android

#endif // MessageHandler_h
