#ifndef GLACC_H
#define GLACC_H

#include <Shader.h>

class GLAcc_GPU_Buffer
{
    GLuint texID;
    int channelCount;
    GLenum texture_target_;
    GLint internal_format_;
    GLenum texture_format_;
    int buffSizeX_;
    int buffSizeY_;
public:
    GLAcc_GPU_Buffer(int channelCount1_4,int buffSizeX,int buffSizeY);

    int CPU2GPU(float *data, int dataL);

    int GPU2CPU(float *data, int dataL);
    ~GLAcc_GPU_Buffer();

    GLuint GetTexID();
    GLenum GetTextureTarget();

    GLuint GetBuffSizeX();

    GLuint GetBuffSizeY();

    GLuint GetChannelCount();

    void BindTexture();

    void SetViewPort();

    static GLuint newBuffer(GLenum texture_target,GLint internal_format,GLenum texture_format,int texSizeX,int texSizeY);

    static void BufferCopy_CPU2GPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);

    static void BufferCopy_GPU2CPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);
    static int BufferProperty(int channelCount1_4,GLenum *ret_texture_target,GLint *ret_internal_format,GLenum *ret_texture_format);
};

class GLAcc_Framework
{
    void* window_GLFW;
public:
    GLuint VBO;
    GLuint VAO;
    GLAcc_Framework();
    ~GLAcc_Framework();

    void INIT_ERROR();
    void INIT_MSG();
    int  INIT(int width,int height);
    void Setup();


    void SetupShader(Shader &shader);
    void SetupViewPort(int W,int H);

    void Begin();
    void Compute();
    void End();

    static int AttachTex2FBO(GLuint fbo,GLenum attachment,GLAcc_GPU_Buffer &gbuf);
};


#endif
