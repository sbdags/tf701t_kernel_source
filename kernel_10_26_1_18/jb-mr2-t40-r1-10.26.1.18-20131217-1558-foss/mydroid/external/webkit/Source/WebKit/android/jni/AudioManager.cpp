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
#include "AudioManager.h"

#include <JNIHelp.h>
#include <JNIUtility.h>

namespace android {

static const char* audioManagerClassName = "android/webkit/NativeAudioManager";

struct AudioManager::JavaGlue {
    jmethodID newInstance;
    jmethodID detachNativePointer;
    jmethodID requestAudioFocus;
    jmethodID abandonAudioFocus;
    jobject javaInstance;
};

AudioManager::AudioManager(jobject context, AudioFocusListener* listener)
    : m_listener(listener)
    , m_glue(0)
{
    ASSERT(m_listener);

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(audioManagerClassName);
    m_glue.set(new JavaGlue());
    m_glue->newInstance = env->GetMethodID(clazz, "<init>", "(ILandroid/content/Context;)V");
    m_glue->detachNativePointer = env->GetMethodID(clazz, "detachNativePointer", "()V");
    m_glue->requestAudioFocus = env->GetMethodID(clazz, "requestAudioFocus", "(I)Z");
    m_glue->abandonAudioFocus = env->GetMethodID(clazz, "abandonAudioFocus", "()Z");

    jobject localInstance = env->NewObject(clazz, m_glue->newInstance, this, context);
    m_glue->javaInstance = env->NewGlobalRef(localInstance);

    env->DeleteLocalRef(localInstance);
    env->DeleteLocalRef(clazz);
}

AudioManager::~AudioManager()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    abandonAudioFocus();
    env->CallVoidMethod(m_glue->javaInstance, m_glue->detachNativePointer);
    env->DeleteGlobalRef(m_glue->javaInstance);
}

bool AudioManager::requestAudioFocus(StreamType streamType)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->requestAudioFocus, streamType);
}

bool AudioManager::abandonAudioFocus()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->abandonAudioFocus);
}

void AudioManager::onAudioFocusChange(AudioFocusChange focusChange)
{
    m_listener->onAudioFocusChange(focusChange);
}

class AudioManager::JNICallbacks {
public:
    static void onAudioFocusChange(JNIEnv*, jobject, jint pointer, jint focusChange)
    {
        if (AudioManager* audioManager = reinterpret_cast<AudioManager*>(pointer))
            audioManager->onAudioFocusChange(static_cast<AudioFocusChange>(focusChange));
    }
};

static JNINativeMethod audioManagerRegistration[] = {
    { "nativeOnAudioFocusChange", "(II)V",
        (void*) &AudioManager::JNICallbacks::onAudioFocusChange },
};

int registerAudioManager(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, audioManagerClassName,
                                    audioManagerRegistration,
                                    NELEM(audioManagerRegistration));
}

} // namespace android
