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
#include "WakeLock.h"

#include "WebCoreJni.h"
#include <JNIHelp.h>
#include <JNIUtility.h>

namespace android {

struct WakeLock::JavaGlue {
    jmethodID setReferenceCounted;
    jmethodID acquire;
    jmethodID acquireTimeout;
    jmethodID release;
    jmethodID isHeld;
    jobject javaInstance;
};

PassOwnPtr<WakeLock> WakeLock::create(jobject context, const char* tag)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return 0;

    if (!context)
        return 0;

    jclass permissionClass = env->FindClass("android/Manifest$permission");
    jfieldID WAKE_LOCK = env->GetStaticFieldID(permissionClass, "WAKE_LOCK", "Ljava/lang/String;");
    jclass packageManagerClass = env->FindClass("android/content/pm/PackageManager");
    jfieldID PERMISSION_GRANTED = env->GetStaticFieldID(packageManagerClass, "PERMISSION_GRANTED", "I");
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID checkCallingOrSelfPermission = env->GetMethodID(contextClass, "checkCallingOrSelfPermission", "(Ljava/lang/String;)I");
    jint hasWakeLockPermission = env->CallIntMethod(context, checkCallingOrSelfPermission,
                                                    env->GetStaticObjectField(permissionClass, WAKE_LOCK));
    if (hasWakeLockPermission != env->GetStaticIntField(packageManagerClass, PERMISSION_GRANTED))
        return 0;

    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jfieldID POWER_SERVICE = env->GetStaticFieldID(contextClass, "POWER_SERVICE", "Ljava/lang/String;");
    jobject powerManager = env->CallObjectMethod(context, getSystemService,
                                                 env->GetStaticObjectField(contextClass, POWER_SERVICE));

    jclass powerManagerClass = env->FindClass("android/os/PowerManager");
    jmethodID newWakeLock = env->GetMethodID(powerManagerClass, "newWakeLock", "(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
    jfieldID FULL_WAKE_LOCK = env->GetStaticFieldID(powerManagerClass, "FULL_WAKE_LOCK", "I");
    jfieldID ON_AFTER_RELEASE = env->GetStaticFieldID(powerManagerClass, "ON_AFTER_RELEASE", "I");
    jstring jtag = wtfStringToJstring(env, tag);
    jobject wakeLock = env->CallObjectMethod(powerManager, newWakeLock,
                                             env->GetStaticIntField(powerManagerClass, FULL_WAKE_LOCK)
                                             | env->GetStaticIntField(powerManagerClass, ON_AFTER_RELEASE),
                                             jtag);

    return adoptPtr(new WakeLock(wakeLock));
}

WakeLock::WakeLock(jobject wakeLock)
    : m_glue(0)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass wakeLockClass = env->FindClass("android/os/PowerManager$WakeLock");
    m_glue.set(new JavaGlue());
    m_glue->setReferenceCounted = env->GetMethodID(wakeLockClass, "setReferenceCounted", "(Z)V");
    m_glue->acquire = env->GetMethodID(wakeLockClass, "acquire", "()V");
    m_glue->acquireTimeout = env->GetMethodID(wakeLockClass, "acquire", "(J)V");
    m_glue->release = env->GetMethodID(wakeLockClass, "release", "()V");
    m_glue->isHeld = env->GetMethodID(wakeLockClass, "isHeld", "()Z");
    m_glue->javaInstance = env->NewGlobalRef(wakeLock);
}

WakeLock::~WakeLock()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    if (isHeld())
        release();
    env->DeleteGlobalRef(m_glue->javaInstance);
}

void WakeLock::setReferenceCounted(bool referenceCounted)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->setReferenceCounted, referenceCounted);
}

void WakeLock::acquire()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->acquire);
}

void WakeLock::acquire(long timeout)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->acquireTimeout, timeout);
}

void WakeLock::release()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return;

    env->CallVoidMethod(m_glue->javaInstance, m_glue->release);
}

bool WakeLock::isHeld() const
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue)
        return false;

    return env->CallBooleanMethod(m_glue->javaInstance, m_glue->isHeld);
}

} // namespace android
