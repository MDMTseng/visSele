
#define GLEW_STATIC
#include "GLAcc.h"
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>

#include "Shader.h"

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <unistd.h>
using namespace std;

/**
* analyse the version
*/
string makeMeString(GLint versionRaw) {
    stringstream ss;
    string str = "\0";

    ss << versionRaw;    // transfers versionRaw value into "ss"
    str = ss.str();        // sets the "str" string as the "ss" value
    return str;
}


/**
* Message
*/
void consoleMessage() {
    char *versionGL = "\0";

    versionGL = (char *)(glGetString(GL_VERSION));


    cout << endl;
    cout << "OpenGL version: " << versionGL << endl << endl;
    cout << "GLEW version: " << glewGetString(GLEW_VERSION) << endl;


    int maxtexsize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxtexsize);
    printf("GL_MAX_TEXTURE_SIZE, %d\n",maxtexsize);
}

/**
* Manager error
*/
void managerError() {
    if (glewInit()) { // checks if glewInit() is activated
        cerr << "Unable to initialize GLEW." << endl;
        while (1); // let's use this infinite loop to check the error message before closing the window
        exit(EXIT_FAILURE);
    }
    // FreeConsole();
}

/**
* Initialize FREEGLUT
*/
GLFWwindow* initGLFW(int width,int height) {
  // Init GLFW
  glfwInit( );

  // Set all the required options for GLFW
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );

  glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );

  // Create a GLFWwindow object that we can use for GLFW's functions
  return glfwCreateWindow( width, height, "LearnOpenGL", nullptr, nullptr );

}

void deleteFBO(GLuint fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}
GLuint initFBO() {
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    return fbo;
}


int attachTex2FBO(GLuint fbo,GLenum attachment,GLAcc_GPU_Buffer &gbuf)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, gbuf.GetTextureTarget(), gbuf.GetTexID(), 0);

    int ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if( ret == GL_FRAMEBUFFER_COMPLETE)
    {
      return 0;
    }

    return -1;
}



void WriteBuffer(GLAcc_GPU_Buffer &tex)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataX = new float[totolLength];
    for (int i=0; i<totolLength; i++)
    {
        dataX[i] = 0;
    }
    tex.CPU2GPU(dataX, totolLength);
    delete(dataX);
}

void WriteBuffer2(GLAcc_GPU_Buffer &tex)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataX = new float[totolLength];
    for (int i=0; i<totolLength; i++)
    {
        dataX[i] = 100;
    }
    tex.CPU2GPU(dataX, totolLength);
    delete(dataX);
}


void ReadBuffer(GLAcc_GPU_Buffer &tex,int idxStart,int readL)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataY = new float[totolLength];

    tex.GPU2CPU(dataY, totolLength);
    printf("%s: TexID:%d\n",__func__,tex.GetTexID());
    printf("X:%d Y:%d CH:%d totolLength:%d \n",tex.GetBuffSizeX(),tex.GetBuffSizeY(),tex.GetChannelCount(),totolLength);

    if(idxStart<0)idxStart+=totolLength;

    int idxEnd=idxStart+readL;
    for (int i=idxStart; i<totolLength && i< idxEnd; i++)
        printf("%f ",dataY[i]);
    delete(dataY);
    printf("\n");
}

void initBaseVertex(Shader &shader,GLuint *pVBO,GLuint *pVAO)
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
    glGenVertexArrays( 1, pVAO );
    glGenBuffers( 1, pVBO );
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
    glBindVertexArray( *pVAO );

    glBindBuffer( GL_ARRAY_BUFFER, *pVBO );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );

    // Position attribute
    int loc = shader.GetAttribLocation("position");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * ) 0 );
    glEnableVertexAttribArray( loc);
    // Color attribute
    loc = shader.GetAttribLocation("color");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * )( 3 * sizeof( GLfloat ) ) );
    glEnableVertexAttribArray( loc );

    glBindVertexArray( 0 ); // Unbind VAO
}
/**
* Main, what else?
*/
int main(int argc, char** argv) {
    int width=800, height=800;
    //Init window
    GLFWwindow* window = initGLFW( width, height);
    if ( nullptr == window )
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate( );

        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent( window );

    int screenWidth, screenHeight;
    glfwGetFramebufferSize( window, &screenWidth, &screenHeight );


    //LOGOSOGO
    managerError();                // manages errors
    consoleMessage();            // displays message on the console

    Shader ourShader( "shader/core.vs", "shader/core.frag" );
    //Establish buffers
    int texSizeX=1024,texSizeY=1024;
    glViewport(0,0,texSizeX,texSizeY);
    GLuint VBO;
    GLuint VAO;
    GLAcc_GPU_Buffer tex1(4,texSizeX,texSizeY);
    GLAcc_GPU_Buffer tex2(4,texSizeX,texSizeY);
    printf("tex ID:%d\n",tex1.GetTexID());
    printf("tex2 ID:%d\n",tex2.GetTexID());


    WriteBuffer(tex1);
    WriteBuffer2(tex2);
    ReadBuffer(tex1,0,10);
    ReadBuffer(tex2,0,10);
    //ReadBuffer(tex1);
    //ReadBuffer(tex2);

    GLuint fbo = initFBO();
    //glDeleteFramebuffersEXT (1,&fb);

    ourShader.Use( );
    //Setup polygon(square) vertices
    initBaseVertex(ourShader,&VBO,&VAO);


    ourShader.ActivateTexture(55,tex1.GetTextureTarget(),0);
    tex1.BindTexture();

    ourShader.ActivateTexture(66,tex2.GetTextureTarget(),1);
    tex2.BindTexture();

    glBindVertexArray( VAO );
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLint maxAtt = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
    printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);

    //tex3.BindTexture();

    attachTex2FBO(fbo,GL_COLOR_ATTACHMENT0,tex1);
    attachTex2FBO(fbo,GL_COLOR_ATTACHMENT1,tex2);

    GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
    glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);

    clock_t t = clock();

    // Game loop
    //while (!glfwWindowShouldClose( window ) )
    for(int i=0;i<99;i++)
    {
        // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
        //glfwPollEvents( );

        // Render
        // Clear the colorbuffer
        //glClearColor( 1.0f, 0.0f, 1.0f, 1.0f );
        //glClear( GL_COLOR_BUFFER_BIT );

        //
        /*if(i%2==0)
        {
          tex1.BindTexture();
          attachTex2FBO(fbo,tex2);
        }
        else
        {
          tex2.BindTexture();
          attachTex2FBO(fbo,tex1);
        }*/

        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
        glFlush();
        // Swap the screen buffers
        //glfwSwapBuffers( window );
    }
    glFinish();
    if(0)
    {
        GLsync fence= glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        glFlush();
        glFinish();
        while (true)
        {
            GLenum syncRes = glClientWaitSync(fence, 0, 1000*000*000);
            switch (syncRes)
            {
              case GL_ALREADY_SIGNALED: cout << "ALREADY" << std::endl; break;
              case GL_CONDITION_SATISFIED: cout << "EXECUTED"  << std::endl; break;
              case GL_TIMEOUT_EXPIRED: cout << "TIMEOUT" << std::endl; break;
              case GL_WAIT_FAILED: cout << "FAIL" << std::endl; break;
            }
            if (syncRes == GL_CONDITION_SATISFIED || syncRes == GL_ALREADY_SIGNALED) break;
        }
        glDeleteSync(fence);
    }
    //
    printf("elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
    glBindVertexArray(0);
    ReadBuffer(tex1,0,10);
    ReadBuffer(tex2,0,10);
    deleteFBO(fbo);

    return 0;
}
