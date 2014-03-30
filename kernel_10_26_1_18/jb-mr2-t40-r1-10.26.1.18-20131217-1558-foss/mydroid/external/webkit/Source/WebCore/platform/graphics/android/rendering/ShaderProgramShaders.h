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
#ifndef ShaderProgramShaders_h
#define ShaderProgramShaders_h

#include "GLUtils.h"

#include <GLES2/gl2.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class TextureQuadData;
class ShaderProgram;

class ShaderProgramShader {
public:
    virtual ~ShaderProgramShader()
    {
        // m_program is not deleted automatically because we want to support broken contexts.
    }

    void setProjectionMatrix(const TransformationMatrix& renderMatrix)
    {
        GLfloat matrix[16];
        GLUtils::toGLMatrix(matrix, renderMatrix);
        glUniformMatrix4fv(m_projectionMatrixHandle, 1, GL_FALSE, matrix);
    }

    void bindPositionBuffer(GLuint* textureBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, *textureBuffer);
        glEnableVertexAttribArray(m_positionHandle);
        glVertexAttribPointer(m_positionHandle, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    GLuint id() const { return m_program; }

    void useProgram() const { glUseProgram(m_program); }

    void deleteProgram()
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }

protected:
    ShaderProgramShader(GLuint program)
        : m_program(program)
        , m_positionHandle(glGetAttribLocation(program, "vPosition"))
        , m_projectionMatrixHandle(glGetUniformLocation(program, "projectionMatrix"))
    {
        useProgram();
        glEnableVertexAttribArray(m_positionHandle);
    }

private:
    GLuint m_program;
    GLuint m_positionHandle;
    GLuint m_projectionMatrixHandle;
};

class PureColorShader : public ShaderProgramShader {
public:
    static PassOwnPtr<PureColorShader> create(const char* vertexSource, const char* fragmentSource)
    {
        GLuint id = GLUtils::createProgram(vertexSource, fragmentSource);
        if (!id)
            return 0;

        return adoptPtr(new PureColorShader(id));
    }

    void setColor(const Color& pureColor)
    {
        glUniform4f(m_pureColorHandle, pureColor.red() / 255.0, pureColor.green() / 255.0,
                    pureColor.blue() / 255.0, pureColor.alpha() / 255.0);
    }
protected:
    PureColorShader(GLuint program)
        : ShaderProgramShader(program)
        , m_pureColorHandle(glGetUniformLocation(program, "inputColor"))
    {
    }

private:
    GLuint m_pureColorHandle;
};

class Tex2DShader : public ShaderProgramShader {
public:
    enum ContrastType {
        NoContrast,
        HasContrast
    };

    static PassOwnPtr<Tex2DShader> create(const char* vertexSource, const char* fragmentSource, ContrastType contrast = NoContrast)
    {
        GLuint id = GLUtils::createProgram(vertexSource, fragmentSource);
        if (!id)
            return 0;
        return adoptPtr(new Tex2DShader(id, contrast));
    }

    // Apply state from TextureQuadData and ShaderProgram into GL state.
    virtual void applyState(const TextureQuadData*, ShaderProgram*);

    // Return true if BlendingTree supports deferring quads drawn with this shader
    // specified by TextureQuadData data.
    virtual bool canDeferRendering(const TextureQuadData*) const;

protected:
    Tex2DShader(GLuint program, ContrastType contrast)
        : ShaderProgramShader(program)
        , m_alphaHandle(glGetUniformLocation(program, "alpha"))
        , m_texSamplerHandle(glGetUniformLocation(program, "s_texture"))
        , m_fillPortionHandle(glGetUniformLocation(program, "fillPortion"))
        , m_contrastHandle(contrast == NoContrast ? 0 : glGetUniformLocation(program, "contrast"))
        , m_hasContrast(contrast != NoContrast)
     {
         glUniform1i(m_texSamplerHandle, 0);
     }

private:
    GLuint m_alphaHandle;
    GLuint m_texSamplerHandle;
    GLuint m_fillPortionHandle;
    GLuint m_contrastHandle;
    bool m_hasContrast;
};


class RepeatTex2DShader : public Tex2DShader {
public:
    static PassOwnPtr<RepeatTex2DShader> create(const char* vertexSource, const char* fragmentSource, ContrastType contrast = NoContrast)
    {
        GLuint id = GLUtils::createProgram(vertexSource, fragmentSource);
        if (!id)
            return 0;
        return adoptPtr(new RepeatTex2DShader(id, contrast));
    }

    virtual void applyState(const TextureQuadData*, ShaderProgram*);

    virtual bool canDeferRendering(const TextureQuadData*) const { return false; }
protected:
    RepeatTex2DShader(GLuint program, ContrastType contrast)
        : Tex2DShader(program, contrast)
        , m_repeatScaleHandle(glGetUniformLocation(program, "repeatScale"))
    {
    }

private:
    GLuint m_repeatScaleHandle;
};


class VideoShader : public ShaderProgramShader {
public:
    static PassOwnPtr<VideoShader> create(const char* vertexSource, const char* fragmentSource)
    {
        GLuint id = GLUtils::createProgram(vertexSource, fragmentSource);
        if (!id)
            return 0;
        return adoptPtr(new VideoShader(id));
    }

    // Exposed until drawVideoQuadData is fixed somehow.
    void setTextureMatrix(const float* textureMatrix)
    {
        glUniformMatrix4fv(m_videoMatrixHandle, 1, GL_FALSE, textureMatrix);
    }

    void bindTexture(int textureId)
    {
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_texSamplerHandle, 0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    }

protected:
    VideoShader(GLuint program)
        : ShaderProgramShader(program)
        , m_texSamplerHandle(glGetUniformLocation(program, "s_yuvTexture"))
        , m_videoMatrixHandle(glGetUniformLocation(program, "textureMatrix"))
    {
         glUniform1i(m_texSamplerHandle, 0);
    }

private:
    GLuint m_texSamplerHandle;
    GLuint m_videoMatrixHandle;
};

} // namespace WebCore

#endif
