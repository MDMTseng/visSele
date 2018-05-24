
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

    Shader ourShader( "shader/shader2/core.vs", "shader/shader2/core.frag" );
    //Establish buffers
    int texSizeX=1024,texSizeY=1024;
    int targetDepth=4;
    GLAcc_Framework GLAcc_f;

    GLAcc_GPU_Buffer tex1(targetDepth,texSizeX,texSizeY);
    printf("tex ID:%d\n",tex1.GetTexID());
    WriteBuffer(tex1);

    GLAcc_GPU_Buffer tex2(targetDepth,texSizeX,texSizeY);
    printf("tex2 ID:%d\n",tex2.GetTexID());
    WriteBuffer2(tex2);
    //ReadBuffer(tex1);
    //ReadBuffer(tex2);
    GLAcc_f.Setup();
    GLAcc_f.SetupShader(ourShader);//actually load vertices(a simple square fill output with depth 0) and use it
    GLAcc_f.SetupViewPort(texSizeX,texSizeY);//Actually setup viewport

    //Setup Input(Texture/Variables)
    {
        ourShader.TextureActivate(ourShader.GetUniformLocation("x1"),tex1.GetTextureTarget(),1);
        ourShader.TextureActivate(ourShader.GetUniformLocation("x2"),tex2.GetTextureTarget(),2);
        glUniform3ui(ourShader.GetUniformLocation("outputDim"), texSizeX,texSizeY,targetDepth);
    }

    GLuint fbo;
    //Setup Output(Multi Render Target (MRT) with FBO)
    {
        GLint maxAtt = 0;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
        printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);
        fbo = initFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);
    }

    //Bind texture to input and output
    {
        GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT0,tex1);
        GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT1,tex2);
        ourShader.TextureActivate(1);
        tex1.BindTexture();
        ourShader.TextureActivate(2);
        tex2.BindTexture();
    }

    clock_t t = clock();
    GLAcc_f.Begin();
    //while (!glfwWindowShouldClose( window ) )
    for(int i=0;i<100;i++)
    {
        GLAcc_f.Compute();//Draw screen
        glFlush();
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
    GLAcc_f.End();
    //
    printf("elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
    ReadBuffer(tex1,0,30);
    ReadBuffer(tex2,0,30);
    deleteFBO(fbo);

    return 0;
}
