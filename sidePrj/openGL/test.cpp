
#define GLEW_STATIC
#include "GLAcc.h"

#include <GL/glew.h>
#include "Shader.h"

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <unistd.h>
using namespace std;
/*
* Message
*/
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
        dataX[i] = 0.1;
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

void runShaderSetup(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2)
{

  GLAcc_f.SetupShader(shader);//actually load vertices(a simple square fill output with depth 0) and use it
  GLAcc_f.SetupViewPort(y1.GetBuffSizeX(),y1.GetBuffSizeY());//Actually setup viewport

  //Setup Input(Texture/Variables)
  {
      shader.TextureActivate(shader.GetUniformLocation("x1"),x1.GetTextureTarget(),1);
      shader.TextureActivate(shader.GetUniformLocation("x2"),x2.GetTextureTarget(),2);
      glUniform3ui(shader.GetUniformLocation("outputDim"), y1.GetBuffSizeX(),y1.GetBuffSizeY(),y1.GetChannelCount());

  }

  //Setup Output(Multi Render Target (MRT) with FBO)
  {
      /**/
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
      glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);
  }

}
void runShader(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,int loopCount)
{
  shader.Use( );
  //Bind texture to input and output
  {
      GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT0,y1);
      shader.TextureActivate(1);
      x1.BindTexture();
      shader.TextureActivate(2);
      x2.BindTexture();
  }

  //clock_t t = clock();
  GLAcc_f.Begin();
  for(int i=0;i<loopCount;i++)
  {
      GLAcc_f.Compute();//Draw screen
      glFlush();
  }
  GLAcc_f.End();
  //
  //printf("elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
}
/**
* Main, what else?
*/
int main(int argc, char** argv) {
    int width=800, height=800;
    //Init window

    GLAcc_Framework GLAcc_f(width,height);

    int texSizeX=1024/1,texSizeY=1024/1,targetDepth=1;

    GLAcc_GPU_Buffer tex1(targetDepth,texSizeX,texSizeY);
    GLAcc_GPU_Buffer tex2(targetDepth,texSizeX,texSizeY);
    //ReadBuffer(tex1);
    //ReadBuffer(tex2);
    GLAcc_f.Setup();
    GLuint fbo= initFBO();

    /*GLint maxAtt = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
    printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);*/

    Shader ourShader1( "shader/shader1/core.vs", "shader/shader1/core.frag" );
    runShaderSetup(GLAcc_f,ourShader1,fbo,tex1,tex1,tex2);
    {
      WriteBuffer(tex1);
      WriteBuffer2(tex2);
      clock_t t = clock();
      runShader(GLAcc_f,ourShader1,fbo,tex1,tex1,tex2,10000);
      glFinish();
      printf("runShader>>elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
      ReadBuffer(tex1,0,4);
    }


    Shader ourShader2( "shader/shader2/core.vs", "shader/shader2/core.frag" );
    runShaderSetup(GLAcc_f,ourShader2,fbo,tex1,tex1,tex2);
    {
      WriteBuffer(tex1);
      WriteBuffer2(tex2);
      clock_t t = clock();
      for(int i=0;i<5000;i++)
      {
        runShader(GLAcc_f,ourShader1,fbo,tex1,tex1,tex2,1);
        runShader(GLAcc_f,ourShader2,fbo,tex1,tex1,tex2,1);
      }
      glFinish();
      printf("runShader>>elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
      ReadBuffer(tex1,0,4);
    }



    //ReadBuffer(tex2,0,30);
    deleteFBO(fbo);

    return 0;
}
