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

#include "config.h"
#include "MessageHandler.h"

#include <JNIHelp.h>
#include <JNIUtility.h>

static const char* messageHandlerClassName = "android/webkit/NativeMessageHandler";

namespace android {

struct MessageHandler::JavaGlue {
    jmethodID newInstance;
    jmethodID detachNativePointer;
    jmethodID hasMessages;
    jmethodID removeMessages;
    jmethodID removeAllMessages;
    jmethodID sendMessage;
    jmethodID sendMessageDelayed;
    jobject javaInstance;
};

MessageHandler::MessageHandler()
    : m_glue(0)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(messageHandlerClassName);
    m_glue.set(new JavaGlue());
    m_glue->newInstance = env->GetMethodID(clazz, "<init>", "(I)V");
    m_glue->detachNativePointer = env->GetMethodID(clazz, "detachNativePointer", "()V");
    m_glue->hasMessages = env->GetMethodID(clazz, "hasMessages", "(I)Z");
    m_glue->removeMessages = env->GetMethodID(clazz, "removeMessages", "(I)V");
    m_glue->removeAllMessages = env->GetMethodID(clazz, "removeAllMessages", "()V");
    m_glue->sendMessage = env->GetMethodID(clazz, "sendMessage", "(IIII)V");
    m_glue->sendMessageDelayed = env->GetMethodID(clazz, "sendMessageDelayed", "(IJIII)V");

    jobject localInstance = env->NewObject(clazz, m_glue->newInstance, this);
    m_glue->javaInstance = env->NewGlobalRef(localInstance);
}

MessageHandler::~MessageHandler()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->detachNativePointer);
    env->DeleteGlobalRef(m_glue->javaInstance);
}

bool MessageHandler::hasMessages(unsigned what)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->hasMessages, what);
}

void MessageHandler::removeMessages(unsigned what)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->removeMessages, what);
}

void MessageHandler::removeAllMessages()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->removeAllMessages);
}

void MessageHandler::sendMessage(unsigned what, int arg1, int arg2, void* obj)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->sendMessage, what, arg1, arg2, obj);
}

void MessageHandler::sendMessageDelayed(unsigned what, double delaySeconds, int arg1, int arg2, void* obj)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    long delayMillis = static_cast<long>(1000.0 * delaySeconds + 0.5);
    env->CallVoidMethod(m_glue->javaInstance, m_glue->sendMessageDelayed, what, delayMillis, arg1, arg2, obj);
}

class MessageHandler::JNICallbacks {
public:
    static void handleMessage(JNIEnv*, jobject, jint pointer, jint what, jint arg1, jint arg2, jint obj)
    {
        if (MessageHandler* messageHandler = reinterpret_cast<MessageHandler*>(pointer))
            messageHandler->handleMessage(what, arg1, arg2, reinterpret_cast<void*>(obj));
    }
};

static JNINativeMethod messageHandlerRegistration[] = {
    { "nativeHandleMessage", "(IIIII)V",
        (void*) &MessageHandler::JNICallbacks::handleMessage },
};

int registerMessageHandler(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, messageHandlerClassName,
                                    messageHandlerRegistration,
                                    NELEM(messageHandlerRegistration));
}

} // namespace android
