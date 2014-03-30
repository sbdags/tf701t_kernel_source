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
#include "MediaMetadata.h"

#include <binder/Parcel.h>
#include <JNIUtility.h>

namespace android {

struct MediaMetadata::JavaGlue {
    jmethodID newInstance;
    jmethodID parse;
    jmethodID has;
    jmethodID getBoolean;
    jint PAUSE_AVAILABLE;
    jint SEEK_BACKWARD_AVAILABLE;
    jint SEEK_FORWARD_AVAILABLE;
    jint SEEK_AVAILABLE;
    jobject javaInstance;
};

MediaMetadata::MediaMetadata()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass("android/media/Metadata");
    m_glue.set(new JavaGlue());
    m_glue->newInstance = env->GetMethodID(clazz, "<init>", "()V");
    m_glue->parse = env->GetMethodID(clazz, "parse", "(Landroid/os/Parcel;)Z");
    m_glue->has = env->GetMethodID(clazz, "has", "(I)Z");
    m_glue->getBoolean = env->GetMethodID(clazz, "getBoolean", "(I)Z");

    m_glue->PAUSE_AVAILABLE = env->GetStaticIntField(clazz, env->GetStaticFieldID(clazz, "PAUSE_AVAILABLE", "I"));
    m_glue->SEEK_BACKWARD_AVAILABLE = env->GetStaticIntField(clazz, env->GetStaticFieldID(clazz, "SEEK_BACKWARD_AVAILABLE", "I"));
    m_glue->SEEK_FORWARD_AVAILABLE = env->GetStaticIntField(clazz, env->GetStaticFieldID(clazz, "SEEK_FORWARD_AVAILABLE", "I"));
    m_glue->SEEK_AVAILABLE = env->GetStaticIntField(clazz, env->GetStaticFieldID(clazz, "SEEK_AVAILABLE", "I"));

    jobject localInstance = env->NewObject(clazz, m_glue->newInstance, this);
    m_glue->javaInstance = env->NewGlobalRef(localInstance);
}

MediaMetadata::~MediaMetadata()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->DeleteGlobalRef(m_glue->javaInstance);
}

bool MediaMetadata::parse(PassOwnPtr<Parcel> parcel)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    jclass parcelClass = env->FindClass("android/os/Parcel");
    jmethodID obtain = env->GetStaticMethodID(parcelClass, "obtain", "(I)Landroid/os/Parcel;");
    jobject javaParcel = env->CallStaticObjectMethod(parcelClass, obtain, reinterpret_cast<int>(parcel.get()));

    if (!env->CallBooleanMethod(m_glue->javaInstance, m_glue->parse, javaParcel))
        return false;

    m_parcel = parcel;

    return true;
}

bool MediaMetadata::has(Key key)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->has, javaId(key));
}

bool MediaMetadata::getBool(Key key)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->getBoolean, javaId(key));
}

int MediaMetadata::javaId(Key key) const
{
    if (!m_glue)
        return 0;

    switch (key) {
    case PauseAvailable: return m_glue->PAUSE_AVAILABLE;
    case SeekBackwardAvailable: return m_glue->SEEK_BACKWARD_AVAILABLE;
    case SeekForwardAvailable: return m_glue->SEEK_FORWARD_AVAILABLE;
    case SeekAvailable: return m_glue->SEEK_AVAILABLE;
    default: return 0;
    }
}

} // namespace android
