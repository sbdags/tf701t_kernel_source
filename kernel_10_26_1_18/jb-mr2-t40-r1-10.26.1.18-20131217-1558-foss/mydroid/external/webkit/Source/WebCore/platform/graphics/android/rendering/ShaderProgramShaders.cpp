/*
 * Copyright 2010, The Android Open Source Project
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
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

#include "ShaderProgramShaders.h"

#include "DrawQuadData.h"
#include "GLSuccessVerifier.h"
#include "ShaderProgram.h"

namespace WebCore {

void Tex2DShader::applyState(const TextureQuadData* data, ShaderProgram* shaderProgram)
{
    GLSuccessVerifier glVerifier;

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_texSamplerHandle, 0);
    glBindTexture(data->textureTarget(), data->textureId());
    glTexParameteri(data->textureTarget(), GL_TEXTURE_MIN_FILTER, data->textureFilter());
    glTexParameteri(data->textureTarget(), GL_TEXTURE_MAG_FILTER, data->textureFilter());
    glUniform1f(m_alphaHandle, data->opacity());

    if (m_hasContrast)
        glUniform1f(m_contrastHandle, shaderProgram->contrast());

    FloatRect fillPortion = data->fillPortion();
    glUniform4f(m_fillPortionHandle, fillPortion.x(), fillPortion.y(), fillPortion.width(), fillPortion.height());
}

bool Tex2DShader::canDeferRendering(const TextureQuadData* data) const
{
    return !m_hasContrast && data->textureTarget() == GL_TEXTURE_2D && data->canDeferRendering();
}

void RepeatTex2DShader::applyState(const TextureQuadData* data, ShaderProgram* shader)
{
    GLSuccessVerifier glVerifier;

    Tex2DShader::applyState(data, shader);
    FloatSize repeatScale = data->repeatScale();
    glUniform2f(m_repeatScaleHandle, repeatScale.width(), repeatScale.height());
}

} // namespace WebCore
