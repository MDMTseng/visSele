
#define GLEW_STATIC
#include "GLAcc.h"

#include <GL/glew.h>
#include "Shader.h"
#include <cmath>
// GLFW
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <unistd.h>
#include <lodepng.h>
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
        dataX[idx+0] = dataX[idx+1] =0;//0.01*distortionF;
        if(distortionF!=0 && i>1 && j>1 && i<mesh.GetBuffSizeY()-1 && j<mesh.GetBuffSizeX()-1)
        {
          float diffX=((float)j-mesh.GetBuffSizeX()/2);
          float diffY=((float)i-mesh.GetBuffSizeY()/2);
          float dist=hypot(diffX,diffY)/hypot(mesh.GetBuffSizeX()/2,mesh.GetBuffSizeY()/2);
          float theta=atan2(diffX,diffY);
          float offset=sin(dist*M_PI);
          dataX[idx+0] +=sin(theta+dist*M_PI)*offset*0.3*distortionF;
          dataX[idx+1] +=cos(theta+dist*M_PI)*offset*0.3*distortionF;

        }

    }

    //dataX[(2*mesh.GetBuffSizeX()+2)*mesh.GetChannelCount()]+=1;
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
        //dataX[idx] =((i*j)%gridSize <gridSize/2)?0: (float)1;
        dataX[idx] =(ygrid^xgrid)?0: (float)1;
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

void runShaderSetupInput(GLAcc_Framework &GLAcc_f,Shader &shader)
  {
    GLAcc_f.SetupShader(shader);//actually load vertices(a simple square fill output with depth 0) and use it

    //Setup Input(Texture/Variables)
    {
        shader.TextureActivate("x1",0,1);
        shader.TextureActivate("x2",0,2);
        shader.TextureActivate("x3",0,3);
        shader.TextureActivate("x4",0,4);
        shader.TextureActivate("offset_mesh",0,13);
    }

  }

void runShaderSetupOut(GLAcc_Framework &GLAcc_f,Shader &shader, GLAcc_GPU_Buffer &y1)
{
    glUniform3ui(shader.GetUnif("outputDim"), y1.GetBuffSizeX(),y1.GetBuffSizeY(),y1.GetChannelCount());


}
void runShaderSetup(GLAcc_Framework &GLAcc_f,Shader &shader,
  GLAcc_GPU_Buffer &y1)
{
  runShaderSetupInput(GLAcc_f,shader);
  runShaderSetupOut(GLAcc_f,shader,y1);
}


void runDisplayShaderSetup(GLAcc_Framework &GLAcc_f,Shader &shader,int outputWidth,int outputHeight)
{
  runShaderSetupInput(GLAcc_f,shader);

  glUniform3ui(shader.GetUniformLocation("outputDim"),outputWidth,outputHeight,4);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDrawBuffer(GL_BACK);

}

void runShader3(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,GLAcc_GPU_Buffer &y2,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &x3,GLAcc_GPU_Buffer &x4,GLAcc_GPU_Buffer &offset_mesh,
  int loopCount)
{
  shader.Use( );
  //Bind texture to input and output
  {
      GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT0,y1);
      GLAcc_f.AttachTex2FBO(fbo,GL_COLOR_ATTACHMENT1,y2);
      if(0 == shader.TextureActivate("x1"))
        x1.BindTexture();
      if(0 == shader.TextureActivate("x2"))
        x2.BindTexture();
      if(0 == shader.TextureActivate("x3"))
        x3.BindTexture();
      if(0 == shader.TextureActivate("x4"))
        x4.BindTexture();
      if(0 == shader.TextureActivate("offset_mesh"))
        offset_mesh.BindTexture();
      //GLAcc_f.SetupViewPort(y1.GetBuffSizeX(),y1.GetBuffSizeY());//Actually setup viewport
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

void runShader2(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,GLAcc_GPU_Buffer &y2,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &x3,GLAcc_GPU_Buffer &offset_mesh,
  int loopCount)
  {
    runShader3(GLAcc_f,shader,fbo,y1,y2,x1,x2,x3,x3,offset_mesh,loopCount);
  }
void runShader(GLAcc_Framework &GLAcc_f,Shader &shader,GLuint fbo,
  GLAcc_GPU_Buffer &y1,
  GLAcc_GPU_Buffer &x1,GLAcc_GPU_Buffer &x2,GLAcc_GPU_Buffer &x3,GLAcc_GPU_Buffer &offset_mesh,
  int loopCount)
  {
    runShader2(GLAcc_f,shader,fbo,y1,y1,x1,x2,x3,offset_mesh,loopCount);
  }
void runDisplayShader(GLAcc_Framework &GLAcc_f,Shader &shader,int outputWidth,int outputHeight,GLAcc_GPU_Buffer &x1)
{
  shader.Use( );
  GLAcc_f.SetupViewPort(outputWidth,outputHeight);//Actually setup viewport
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

int Save2PNG(uint8_t *data, int width, int height,int channelCount, char* filePath)
{
    // we're going to encode with a state rather than a convenient function, because enforcing a color type requires setting options
    lodepng::State state;
    // input color type
    state.info_raw.colortype = LCT_GREY;
    switch(channelCount)
    {
      case 1:state.info_raw.colortype = LCT_GREY;break;
      case 2:state.info_raw.colortype = LCT_GREY_ALPHA;break;//Weird but what ever
      case 3:state.info_raw.colortype = LCT_RGB;break;
      case 4:state.info_raw.colortype = LCT_RGBA;break;
      default:return -1;
    }
    state.info_raw.bitdepth = 8;
    // output color type
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 1; // without this, it would ignore the output color type specified above and choose an optimal one instead

    std::vector<unsigned char> buffer;
    unsigned error = lodepng::encode(buffer, data, width, height,state);
    if(error)
    {
      std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
      return -1;
    }


    return lodepng::save_file(buffer,filePath);
}

void ImageLoaderTest()
{
    /*int width,height,channel;
    unsigned char *image = SOIL_load_image("test_data/img_test.png", &width, &height, &channel, SOIL_LOAD_L);

    printf("%s: pixAddr = %p>> %d,%d,%d\n",__func__,image,width,height,channel);
    SOIL_save_image
    ("test_data/img_test_save.tga",
      SOIL_SAVE_TYPE_TGA,
       width,  height,  channel,image);
    SOIL_free_image_data(image);*/


    std::vector<unsigned char> image;
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height, "test_data/img_test.png");
    printf("%s: size= %d, pixDepth:%d>> %d,%d\n",__func__,image.size(),image.size()/width/height,width,height);

    Save2PNG(&image[0],  width,  height,4, "test_data/img_test_save2.png");
}


void ReadBufferToFile(GLAcc_GPU_Buffer &tex,char* path)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();

    uint8_t* dataY = new uint8_t[totolLength];
    tex.GPU2CPU(GL_UNSIGNED_BYTE,dataY, totolLength);
    printf("%s: TexID:%d\n",__func__,tex.GetTexID());
    printf("X:%d Y:%d CH:%d totolLength:%d \n",tex.GetBuffSizeX(),tex.GetBuffSizeY(),tex.GetChannelCount(),totolLength);

    Save2PNG(dataY,tex.GetBuffSizeX(), tex.GetBuffSizeY(),tex.GetChannelCount(), path);

    delete[] dataY;
}

GLAcc_GPU_Buffer* ReadFileToBuffer(char* path)
{
    std::vector<unsigned char> image;
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height,path);
    int channels = image.size()/width/height;//

    GLAcc_GPU_Buffer *tex = new GLAcc_GPU_Buffer(channels,width,height,GL_LINEAR,GL_MIRRORED_REPEAT);
    tex->CPU2GPU(GL_UNSIGNED_BYTE,&(image[0]), image.size());
    return tex;
}

int test1(int argc, char** argv);
int main(int argc, char** argv)
{

    //return test1( argc, argv);
    int width=800, height=800;
    //Init window

    GLAcc_Framework GLAcc_f(width,height);

    int texSizeX=800/1,texSizeY=800/1,targetDepth=1;
    GLAcc_GPU_Buffer offset_mesh(2,texSizeX/5,texSizeY/5,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer offset_mesh_gradient(2,offset_mesh.GetBuffSizeX(),
    offset_mesh.GetBuffSizeY(),GL_LINEAR,GL_CLAMP);



    GLAcc_GPU_Buffer _NTex(1,1,1,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer inputImg(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer offsetMap(3,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer offsetMap2(3,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer inputMorphImg(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer ref_img(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer sobel_edge(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer gradient(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer gradOptV(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    InitInputImage(ref_img,150);
    GLAcc_f.Setup();
    GLuint fbo= initFBO();

    //Setup Output(Multi Render Target (MRT) with FBO)
    {
        /**/
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);
    }


    int screenWidth, screenHeight;
    glfwGetFramebufferSize( (GLFWwindow*)GLAcc_f.getWindow(), &screenWidth, &screenHeight );

    /*GLint maxAtt = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
    printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);*/

    Shader morphRegularizerShader( "shader/morphRegularizer/core.vs", "shader/morphRegularizer/core.frag" );
    Shader morphingShader( "shader/morph/core.vs", "shader/morph/core.frag" );
    Shader ourDisplayShader( "shader/display/core.vs", "shader/display/core.frag" );
    Shader sobelNormShader( "shader/spfilters/core.vs", "shader/spfilters/sobelNorm.frag" );
    Shader sobelShader( "shader/spfilters/core.vs", "shader/spfilters/sobel.frag" );
    Shader uniBlurShader( "shader/spfilters/core.vs", "shader/spfilters/uniblurAll.frag" );
    Shader crossBlurShader( "shader/spfilters/core.vs", "shader/spfilters/crossblurAll.frag" );
    Shader minSearchShader( "shader/exp/core.vs", "shader/exp/minSearch.frag" );
    runShaderSetup(GLAcc_f,morphingShader,inputMorphImg);
    runShaderSetup(GLAcc_f,uniBlurShader,ref_img);
    runShaderSetup(GLAcc_f,crossBlurShader,ref_img);
    runShaderSetup(GLAcc_f,sobelShader,sobel_edge);
    runShaderSetup(GLAcc_f,sobelNormShader,sobel_edge);

    printf(">>>>\n");
    runShaderSetup(GLAcc_f,minSearchShader,ref_img);
    printf(">>>>\n");

    runDisplayShaderSetup(GLAcc_f,ourDisplayShader,screenWidth,screenHeight);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if(1)
    {//Pre processing, soften image and create it's sobel gradient field
        GLAcc_f.SetupViewPort(ref_img.GetBuffSizeX(),ref_img.GetBuffSizeY());//Actually setup viewport
        uniBlurShader.Use( );
        for(int i=0;i<7;i++)
        {
            glUniform1i(uniBlurShader.GetUnif("blur_size"),1);
            runShader(GLAcc_f,uniBlurShader,fbo,ref_img,ref_img,_NTex,_NTex,_NTex,5);
            glUniform1i(uniBlurShader.GetUnif("blur_size"),-1);
            runShader(GLAcc_f,uniBlurShader,fbo,ref_img,ref_img,_NTex,_NTex,_NTex,5);
        }
        runShader(GLAcc_f,sobelShader,fbo,sobel_edge,ref_img,_NTex,_NTex,_NTex,1);
        runShader(GLAcc_f,sobelNormShader,fbo,sobel_edge,sobel_edge,_NTex,_NTex,_NTex,1);
        runShader(GLAcc_f,crossBlurShader,fbo,sobel_edge,sobel_edge,_NTex,_NTex,_NTex,1);
        runShader(GLAcc_f,crossBlurShader,fbo,sobel_edge,sobel_edge,_NTex,_NTex,_NTex,1);
        runShader(GLAcc_f,crossBlurShader,fbo,sobel_edge,sobel_edge,_NTex,_NTex,_NTex,1);
        runShader(GLAcc_f,crossBlurShader,fbo,sobel_edge,sobel_edge,_NTex,_NTex,_NTex,1);

    }

    initOffsetMeshBuffer(offset_mesh,0.05);
    GLAcc_f.SetupViewPort(inputImg.GetBuffSizeX(),inputImg.GetBuffSizeY());//Actually setup viewport
    runShader(GLAcc_f,morphingShader,fbo,inputImg,ref_img,_NTex,_NTex,offset_mesh,1);//Fake a input image by morph refrence image
    initOffsetMeshBuffer(offset_mesh,0);

    int loopTotal=5;
    int iterC=loopTotal;
    loopTotal=(loopTotal/iterC)*iterC;
    for(int i=0;i<iterC;i++)
    {
        for(int i=0;i<loopTotal/iterC;i++)
        {
          glBindFramebuffer(GL_FRAMEBUFFER, fbo);
          GLAcc_f.SetupViewPort(1,1,offsetMap.GetBuffSizeX()-2,offsetMap.GetBuffSizeY()-2);

          runShader3(GLAcc_f,minSearchShader,fbo,
            offsetMap,offsetMap2,
            offsetMap,
            inputImg,
            ref_img,
            sobel_edge,
            _NTex,
            1);

          if(0){
            runShader(GLAcc_f,crossBlurShader,fbo,offsetMap,offsetMap,_NTex,_NTex,_NTex,1);
          }
        }
        if(1){
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          glfwPollEvents( );
          runDisplayShader(GLAcc_f,ourDisplayShader,screenWidth,screenHeight,offsetMap);
          glfwSwapBuffers( (GLFWwindow*)GLAcc_f.getWindow() );
        }
    }




    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    ReadBufferToFile(offsetMap2,"test_data/offsetMap2.png");
    ReadBufferToFile(inputImg,"test_data/inputImg.png");
    ReadBufferToFile(offsetMap,"test_data/offsetMap.png");
    ReadBufferToFile(sobel_edge,"test_data/sobel_edge.png");
    deleteFBO(fbo);
    return 0;
}

int test1(int argc, char** argv) {
    //ImageLoaderTest();

    int width=800, height=800;
    //Init window

    GLAcc_Framework GLAcc_f(width,height);

    int texSizeX=800/1,texSizeY=800/1,targetDepth=1;

    GLAcc_GPU_Buffer offset_mesh(2,texSizeX/5,texSizeY/5,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer offset_mesh_gradient(2,offset_mesh.GetBuffSizeX(),
    offset_mesh.GetBuffSizeY(),GL_LINEAR,GL_CLAMP);

    GLAcc_GPU_Buffer _NTex(1,1,1,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer inputImg(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer inputMorphImg(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_CLAMP);
    GLAcc_GPU_Buffer ref_img(targetDepth,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer sobel_edge(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer gradient(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    GLAcc_GPU_Buffer gradOptV(3,texSizeX,texSizeY,GL_LINEAR,GL_MIRRORED_REPEAT);
    InitInputImage(ref_img,150);
    GLAcc_f.Setup();
    GLuint fbo= initFBO();

    //Setup Output(Multi Render Target (MRT) with FBO)
    {
        /**/
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum bufs[] = {GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        glDrawBuffers(sizeof(bufs)/sizeof(GLenum), bufs);
    }


    int screenWidth, screenHeight;
    glfwGetFramebufferSize( (GLFWwindow*)GLAcc_f.getWindow(), &screenWidth, &screenHeight );

    /*GLint maxAtt = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
    printf("Test start.... GL_MAX_COLOR_ATTACHMENTS:%d\n",maxAtt);*/

    Shader morphRegularizerShader( "shader/morphRegularizer/core.vs", "shader/morphRegularizer/core.frag" );
    Shader morphingShader( "shader/morph/core.vs", "shader/morph/core.frag" );
    Shader ourDisplayShader( "shader/display/core.vs", "shader/display/core.frag" );
    Shader sobelShader( "shader/spfilters/core.vs", "shader/spfilters/sobel.frag" );
    Shader uniBlurShader( "shader/spfilters/core.vs", "shader/spfilters/uniblur.frag" );
    Shader downSumShader( "shader/spfilters/core.vs", "shader/spfilters/downSum.frag" );
    Shader subMulShader( "shader/spfilters/core.vs", "shader/spfilters/subMul.frag" );
    Shader gradOptShader( "shader/spfilters/core.vs", "shader/spfilters/gradOpt.frag" );
    runShaderSetup(GLAcc_f,morphRegularizerShader,offset_mesh);
    runShaderSetup(GLAcc_f,morphingShader,inputMorphImg);
    runShaderSetup(GLAcc_f,uniBlurShader,ref_img);
    runShaderSetup(GLAcc_f,sobelShader,sobel_edge);
    runShaderSetup(GLAcc_f,downSumShader,offset_mesh_gradient);
    runShaderSetup(GLAcc_f,subMulShader,gradient);
    runShaderSetup(GLAcc_f,gradOptShader,offset_mesh);

    runDisplayShaderSetup(GLAcc_f,ourDisplayShader,screenWidth,screenHeight);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if(1)
    {//Pre processing, soften image and create it's sobel gradient field
        GLAcc_f.SetupViewPort(ref_img.GetBuffSizeX(),ref_img.GetBuffSizeY());//Actually setup viewport
        uniBlurShader.Use( );
        for(int i=0;i<8;i++)
        {
            glUniform1i(uniBlurShader.GetUnif("blur_size"),5);
            runShader(GLAcc_f,uniBlurShader,fbo,ref_img,ref_img,_NTex,_NTex,_NTex,1);
            glUniform1i(uniBlurShader.GetUnif("blur_size"),-5);
            runShader(GLAcc_f,uniBlurShader,fbo,ref_img,ref_img,_NTex,_NTex,_NTex,1);
        }

        runShader(GLAcc_f,sobelShader,fbo,sobel_edge,ref_img,_NTex,_NTex,_NTex,1);

    }

    initOffsetMeshBuffer(offset_mesh,0.1);
    GLAcc_f.SetupViewPort(inputImg.GetBuffSizeX(),inputImg.GetBuffSizeY());//Actually setup viewport
    runShader(GLAcc_f,morphingShader,fbo,inputImg,ref_img,_NTex,_NTex,offset_mesh,1);//Fake a input image by morph refrence image

    initOffsetMeshBuffer(offset_mesh,0);

    int loopTotal=5000;
    int iterC=loopTotal;
    loopTotal=(loopTotal/iterC)*iterC;
    for(int i=0;i<iterC;i++)
    {
        for(int j=0;j<loopTotal/iterC;j++)
        {
          glBindFramebuffer(GL_FRAMEBUFFER, fbo);
          //Use viewport(1,1,W-2,H-2) to avoid changing morph edge
          GLAcc_f.SetupViewPort(1,1,offset_mesh.GetBuffSizeX()-2,offset_mesh.GetBuffSizeY()-2);
          runShader(GLAcc_f,morphRegularizerShader,fbo,offset_mesh,offset_mesh,_NTex,_NTex,_NTex,1);
          GLAcc_f.SetupViewPort(inputMorphImg.GetBuffSizeX(),inputMorphImg.GetBuffSizeY());//Actually setup viewport
          runShader(GLAcc_f,morphingShader,fbo,inputMorphImg,inputImg,_NTex,_NTex,offset_mesh,1);
          runShader(GLAcc_f,subMulShader,fbo,gradient,ref_img,inputMorphImg,sobel_edge,_NTex,1);



          GLAcc_f.SetupViewPort(1,1,offset_mesh_gradient.GetBuffSizeX()-2,offset_mesh_gradient.GetBuffSizeY()-2);
          runShader(GLAcc_f,downSumShader,fbo,offset_mesh_gradient,gradient,_NTex,_NTex,_NTex,1);
          runShader2(GLAcc_f,gradOptShader,fbo,offset_mesh,gradOptV,offset_mesh,offset_mesh_gradient,gradOptV,_NTex,1);
        }
        //glFinish();
        if(1){
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          glfwPollEvents( );
          runDisplayShader(GLAcc_f,ourDisplayShader,screenWidth,screenHeight,gradient);
          glfwSwapBuffers( (GLFWwindow*)GLAcc_f.getWindow() );
        }
    }
    glFinish();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    ReadBufferToFile(inputMorphImg,"test_data/inputMorphImg.png");
    ReadBufferToFile(inputImg,"test_data/inputImg.png");
    ReadBufferToFile(ref_img,"test_data/ref_img.png");
    deleteFBO(fbo);
    return 0;
}
