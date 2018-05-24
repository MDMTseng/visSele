#ifndef GLACC_H
#define GLACC_H

#include <GL/glew.h>
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
public:
    GLuint VBO;
    GLuint VAO;
    GLAcc_Framework()
    {
        glGenVertexArrays( 1, &VAO );
        glGenBuffers( 1, &VBO );
    }
    ~GLAcc_Framework()
    {
        glDeleteVertexArrays( 1, &VAO );
        glDeleteBuffers(1,&VBO);
    }
    void Setup()
    {
        float mult=1;
        GLfloat vertices[] =
        {
            // Positions         // Colors
            -mult, -mult, 0.0f,   mult, 0.0f, 0.0f,
            -mult, mult, 0.0f,   0.0f, mult, 0.0f,
            mult, -mult, 0.0f,   0.0f, 0.0f, mult,
            mult, mult, 0.0f,   mult, mult, mult
        };
        // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
        glBindVertexArray( VAO );

        glBindBuffer( GL_ARRAY_BUFFER, VBO );
        glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );

        glBindVertexArray( 0 ); // Unbind VAO

    }


    void SetupShader(Shader &shader)
    {  // Position attribute

        shader.Use( );
        glBindVertexArray( VAO );
        int loc = shader.GetAttribLocation("position");
        glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * ) 0 );
        glEnableVertexAttribArray( loc);
        // Color attribute
        loc = shader.GetAttribLocation("color");
        glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * )( 3 * sizeof( GLfloat ) ) );
        glEnableVertexAttribArray( loc );

        glBindVertexArray( 0 ); // Unbind VAO
    }
    void SetupViewPort(int W,int H)
    {
        glViewport(0,0,W,H);
    }

    void Begin()
    {
        glBindVertexArray( VAO );
    }
    void Compute()
    {
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
    }
    void End()
    {
        glBindVertexArray( 0 ); // Unbind VAO
    }

    static int AttachTex2FBO(GLuint fbo,GLenum attachment,GLAcc_GPU_Buffer &gbuf)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, gbuf.GetTextureTarget(), gbuf.GetTexID(), 0);

        int ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if( ret == GL_FRAMEBUFFER_COMPLETE)
        {
          return 0;
        }

        return -1;
    }
};


#endif
