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

    GLAcc_GPU_Buffer(int channelCount1_4,int buffSizeX,int buffSizeY,int filter_type, int warp_type);
    int CPU2GPU(float *data, int dataL);

    int CPU2GPU(int glDataType, void *data, int dataL);
    int GPU2CPU(float *data, int dataL);
    int GPU2CPU(int glDataType, void *data, int dataL);
    ~GLAcc_GPU_Buffer();

    GLuint GetTexID();
    GLenum GetTextureTarget();

    GLuint GetBuffSizeX();

    GLuint GetBuffSizeY();

    GLuint GetChannelCount();

    void BindTexture();

    void SetViewPort();

    static GLuint newBuffer(GLenum texture_target,GLint internal_format,GLenum texture_format,int texSizeX,int texSizeY,
      int format_type,int filter_type, int warp_type);

    static void BufferCopy_CPU2GPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);

    static void BufferCopy_CPU2GPU(GLuint texID, GLenum data_type, void *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);
    static void BufferCopy_GPU2CPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);
    static void BufferCopy_GPU2CPU(GLuint texID, GLenum data_type, void *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format);
    static int BufferProperty(int channelCount1_4,GLenum *ret_texture_target,GLint *ret_internal_format,GLenum *ret_texture_format);
  private:
    void INIT(int channelCount1_4,int buffSizeX,int buffSizeY,int format_type,int filter_type, int warp_type);
};

class GLAcc_Framework
{
    void* window_GLFW;
public:
    GLuint VBO;
    GLuint VAO;
    GLAcc_Framework(int width,int height);
    ~GLAcc_Framework();

    void INIT_ERROR();
    void INIT_MSG();
    int  INIT(int width,int height);
    void Setup();


    void SetupShader(Shader &shader);
    void SetupViewPort(int W,int H);
    void *getWindow();
    void Begin();
    void Compute();
    void End();

    static int AttachTex2FBO(GLuint fbo,GLenum attachment,GLAcc_GPU_Buffer &gbuf);
};


#endif
