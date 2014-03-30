/*
 * Copyright 2009, The Android Open Source Project
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

#include "config.h"
#include "BitmapAllocatorAndroid.h"

#include "ResourceLimits.h"
#include "SharedBufferStream.h"
#include "SkFlattenable.h"
#include "SkFlattenableBuffers.h"
#include "SkImageEncoder.h"
#include "SkImageRef_GlobalPool.h"
#include "SkImageRef_ashmem.h"

// made this up, so we don't waste a file-descriptor on small images, plus
// we don't want to lose too much on the round-up to a page size (4K)
#define MIN_ASHMEM_ALLOC_SIZE   (32*1024)


static bool should_use_ashmem(const SkBitmap& bm) {
    return bm.getSize() >= MIN_ASHMEM_ALLOC_SIZE;
}

static bool accountedASHMemSkImageRefRegistered = false;

///////////////////////////////////////////////////////////////////////////////

namespace WebCore {

class AccountedASHMemSkImageRef : public SkImageRef_ashmem {
public:
    AccountedASHMemSkImageRef(ResourceLimits::FileDescriptorGrant& adoptFileDescriptorGrant,
                              SkStream* stream, SkBitmap::Config config, int sampleSize)
        : SkImageRef_ashmem(stream, config, sampleSize)
        , m_fileDescriptorGrant(adoptFileDescriptorGrant)
    {
    }

    virtual Factory getFactory() SK_OVERRIDE { return CreateProc; }

    static SkFlattenable* CreateProc(SkFlattenableReadBuffer& buffer)
    {
        ResourceLimits::FileDescriptorGrant fileDescriptorGrant(ResourceLimits::WebContent, 1);
        if (fileDescriptorGrant.isGranted())
            return new AccountedASHMemSkImageRef(fileDescriptorGrant, buffer);
        // ResourceLimits statically allocates some file descriptors for unaccounted usage,
        // this guarantees that we can always deserialize.
        SkImageRef_ashmem* tempImageRef = (SkImageRef_ashmem*)SkImageRef_ashmem::CreateProc(buffer);
        // Skia does not support moving imagerefs directly to other storage.
        // Use re-encoding the image as PNG to different storage as a fallback.
        // Usually there are enough file descriptors so this can be avoided.
        SkBitmap bm;
        tempImageRef->getInfo(&bm);
        bm.setPixelRef(tempImageRef);
        SkDynamicMemoryWStream wStream;
        SkImageEncoder::EncodeStream(&wStream, bm, SkImageEncoder::kPNG_Type, 0);
        SkMemoryStream* stream = new SkMemoryStream(wStream.copyToData());
        // Sample size 1 can always be used, since sampling has been done while encoding
        SkFlattenable* imageRef = new SkImageRef_GlobalPool(stream, bm.getConfig(), 1);
        stream->unref();
        tempImageRef->unref();
        return imageRef;

    }

protected:
    AccountedASHMemSkImageRef(ResourceLimits::FileDescriptorGrant& adoptFileDescriptorGrant,
                              SkFlattenableReadBuffer& buffer)
        : SkImageRef_ashmem(buffer)
        , m_fileDescriptorGrant(adoptFileDescriptorGrant)
    {
    }

private:
    ResourceLimits::FileDescriptorGrant m_fileDescriptorGrant;
};


BitmapAllocatorAndroid::BitmapAllocatorAndroid(SharedBuffer* data,
                                               int sampleSize)
{
    fStream = new SharedBufferStream(data);
    fSampleSize = sampleSize;
}

BitmapAllocatorAndroid::~BitmapAllocatorAndroid()
{
    fStream->unref();
}

bool BitmapAllocatorAndroid::allocPixelRef(SkBitmap* bitmap, SkColorTable*)
{
    SkPixelRef* ref = 0;

    if (should_use_ashmem(*bitmap)) {
        ResourceLimits::FileDescriptorGrant fileDescriptorGrant(ResourceLimits::WebContent, 1);
        if (fileDescriptorGrant.isGranted()) {
            registerAccountedASHMemSkImageRef();
            ref = new AccountedASHMemSkImageRef(fileDescriptorGrant, fStream, bitmap->config(), fSampleSize);
        }
    }

    if (!ref)
        ref = new SkImageRef_GlobalPool(fStream, bitmap->config(), fSampleSize);

    bitmap->setPixelRef(ref)->unref();
    return true;
}

void BitmapAllocatorAndroid::registerAccountedASHMemSkImageRef()
{
    if (!accountedASHMemSkImageRefRegistered) {
        SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(AccountedASHMemSkImageRef)
        accountedASHMemSkImageRefRegistered = true;
    }
}

}
