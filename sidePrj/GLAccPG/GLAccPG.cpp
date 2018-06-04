
#define GLEW_STATIC
#include "GLAcc.h"

#include <GL/glew.h>
#include "Shader.h"

// GLFW
#include <GLFW/glfw3.h>
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



int initOffsetMeshBuffer(GLAcc_GPU_Buffer &mesh,float distortionF)
{
    if(mesh.GetChannelCount()<2)
    {
      printf("%s: Error: the mesh channel is less than 2 ",__func__);
      return -1;
    }
    int totalLength=mesh.GetBuffSizeX()*mesh.GetBuffSizeY()*mesh.GetChannelCount();
    float* dataX = new float[totalLength];
    for (int i=0; i<mesh.GetBuffSizeY(); i++)
      for (int j=0; j<mesh.GetBuffSizeX(); j++)
    {
        int idx = (i*mesh.GetBuffSizeX()+j)*mesh.GetChannelCount();
        //dataX[idx+0] = (float)j/(mesh.GetBuffSizeX()-1);//From 0~1
        //dataX[idx+1] = (float)i/(mesh.GetBuffSizeY()-1);//From 0~1
        dataX[idx+0] = dataX[idx+1] =0;
        dataX[idx+0] +=((rand()%1000)/1000.0-0.5)/mesh.GetBuffSizeX()*distortionF;
        dataX[idx+1] +=((rand()%1000)/1000.0-0.5)/mesh.GetBuffSizeY()*distortionF;
    }

    //dataX[(2*mesh.GetBuffSizeX()+2)*mesh.GetChannelCount()]+=0.1;
    mesh.CPU2GPU(dataX, totalLength);
    delete(dataX);

    return 0;
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

void InitInputImage(GLAcc_GPU_Buffer &tex,int gridSize)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataX = new float[totolLength];
    for (int i=0; i<tex.GetBuffSizeY(); i++)
      for (int j=0; j<tex.GetBuffSizeX(); j++)
    {
        int idx = (i*tex.GetBuffSizeX()+j)*tex.GetChannelCount();
        bool ygrid = i%gridSize <gridSize/2;
        bool xgrid = j%gridSize <gridSize/2;
        dataX[idx] =((i*j)%gridSize <gridSize/2)?0: (float)1;
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

void runShaderSetupInput(GLAcc_Framework &GLAcc_f,Shader &shader,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &offset_mesh)
  {
    GLAcc_f.SetupShader(shader);//actually load vertices(a simple square fill output with depth 0) and use it

    //Setup Input(Texture/Variables)
    {
        shader.TextureActivate(shader.GetUniformLocation("x1"),x1.GetTextureTarget(),1);
        shader.TextureActivate(shader.GetUniformLocation("x2"),x2.GetTextureTarget(),2);
        shader.TextureActivate(shader.GetUniformLocation("offset_mesh"),offset_mesh.GetTextureTarget(),3);
    }

  }

void runShaderSetupOut(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo, GLAcc_GPU_Buffer &y1)
  {
      glUniform3ui(shader.GetUniformLocation("outputDim"), y1.GetBuffSizeX(),y1.GetBuffSizeY(),y1.GetChannelCount());

      //Setup Output(Multi Render Target (MRT) with FBO)
      {
          /**/
          glBindFramebuffer(GL_FRAMEBUFFER, fbo);
          GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
          glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);
      }
      GLAcc_f.SetupViewPort(y1.GetBuffSizeX(),y1.GetBuffSizeY());//Actually setup viewport

  }
void runShaderSetup(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &offset_mesh)
{
  runShaderSetupInput(GLAcc_f,shader,x1, x2,offset_mesh);
  runShaderSetupOut(GLAcc_f,shader,fbo,y1);
}


void runDisplayShaderSetup(GLAcc_Framework &GLAcc_f,Shader &shader,int outputWidth,int outputHeight,
GLAcc_GPU_Buffer &x1)
{
  runShaderSetupInput(GLAcc_f,shader,x1, x1,x1);

  glUniform3ui(shader.GetUniformLocation("outputDim"),outputWidth,outputHeight,4);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDrawBuffer(GL_BACK);
  GLAcc_f.SetupViewPort(outputWidth,outputHeight);//Actually setup viewport

}
void runShader(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &offset_mesh,
  int loopCount)
{
  shader.Use( );
  //Bind texture to input and output
  {
      GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT0,y1);
      shader.TextureActivate(1);
      x1.BindTexture();
      shader.TextureActivate(2);
      x2.BindTexture();
      shader.TextureActivate(3);
      offset_mesh.BindTexture();
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
void runDisplayShader(GLAcc_Framework &GLAcc_f,Shader &shader,GLAcc_GPU_Buffer &x1)
{
  shader.Use( );
  //Bind texture to input and output
  {
      shader.TextureActivate(1);
      x1.BindTexture();
  }

  //clock_t t = clock();
  GLAcc_f.Begin();
  GLAcc_f.Compute();//Draw screen
  glFlush();
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

    int texSizeX=800/1,texSizeY=800/1,targetDepth=1;

    GLAcc_GPU_Buffer offset_mesh(2,texSizeX/100,texSizeY/100,GL_LINEAR,GL_MIRRORED_REPEAT);
    initOffsetMeshBuffer(offset_mesh,0.5);

    GLAcc_GPU_Buffer tex1(targetDepth,texSizeX,texSizeY);
    GLAcc_GPU_Buffer tex2(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_f.Setup();
    GLuint fbo= initFBO();

    /*GLint maxAtt = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
    printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);*/

    Shader ourShader1( "shader/morph/core.vs", "shader/morph/core.frag" );
    /*ourShader1.LoadShader(
      Shader::LOADFILE("shader/shader1/core.vs").c_str( ),
      Shader::LOADFILE("shader/shader1/core.frag").c_str( ));*/
    runShaderSetup(GLAcc_f,ourShader1,fbo,tex1,tex1,tex2,offset_mesh);
    {
      //WriteBuffer(tex1);
      InitInputImage(tex2,80);
      clock_t t = clock();
      runShader(GLAcc_f,ourShader1,fbo,tex1,tex1,tex2,offset_mesh,1);
      glFinish();
      printf("runShader>>elapse:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
      ReadBuffer(tex1,0,40);
    }
    deleteFBO(fbo);

    {
      Shader ourDisplayShader( "shader/display/core.vs", "shader/display/core.frag" );

      int screenWidth, screenHeight;
      glfwGetFramebufferSize( (GLFWwindow*)GLAcc_f.getWindow(), &screenWidth, &screenHeight );

      //InitInputImage(tex1);
      runDisplayShaderSetup(GLAcc_f,ourDisplayShader,screenWidth,screenHeight,tex1);
      glfwPollEvents( );
      runDisplayShader(GLAcc_f,ourDisplayShader,tex1);
      glfwSwapBuffers( (GLFWwindow*)GLAcc_f.getWindow() );
    }
    getchar();
    return 0;
}
