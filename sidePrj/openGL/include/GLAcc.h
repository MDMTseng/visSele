#ifndef GLACC_H
#define GLACC_H

#define GLEW_STATIC
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>

#include <stdio.h>
#include "Shader.h"


#include <iostream>
#include <string>
#include <sstream>

class GLAcc_GPU_Buffer
{
public:
    GLuint texID;
    int channelCount;
    GLenum texture_target_;
    GLint internal_format_;
    GLenum texture_format_;
    int buffSizeX_;
    int buffSizeY_;
    GLAcc_GPU_Buffer(int channelCount1_4,int buffSizeX,int buffSizeY)
    {
      int ret = BufferProperty(channelCount1_4,
        &texture_target_,
        &internal_format_,
        &texture_format_);

      if(ret !=0)
      {
        throw std::invalid_argument( "Error: GLAcc_GPU_Buffer constructor error, most likely the channelCount1_4 problem" );
      }
      channelCount = channelCount1_4;
      texID = newBuffer( texture_target_, internal_format_, texture_format_, buffSizeX, buffSizeY);

      buffSizeX_=buffSizeX;
      buffSizeY_=buffSizeY;

    }

    int CPU2GPU(float *data, int dataL)
    {
      if(dataL!=buffSizeX_*buffSizeY_*channelCount)
      {
        return -1;
      }
      BufferCopy_CPU2GPU(texID, data, buffSizeX_, buffSizeY_,
        texture_target_,texture_format_);
      return 0;
    }

    int GPU2CPU(float *data, int dataL)
    {
      if(dataL!=buffSizeX_*buffSizeY_*channelCount)
      {
        return -1;
      }
      BufferCopy_GPU2CPU(texID, data, buffSizeX_, buffSizeY_,
        texture_target_,texture_format_);
      return 0;
    }

    static GLuint newBuffer(GLenum texture_target,GLint internal_format,GLenum texture_format,int texSizeX,int texSizeY)
    {
      GLuint texID;
        // create a new texture name
      glEnable( GL_TEXTURE_2D );
      glGenTextures (1, &texID);
      // bind the texture name to a texture target
      glBindTexture(texture_target,texID);
      // turn off filtering and set proper wrap mode
      // (obligatory for float textures atm)
      glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP);
      // set texenv to replace instead of the default modulate
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      // and allocate graphics memory
      glTexImage2D(texture_target, 0, internal_format,
                   texSizeX, texSizeY, 0, texture_format, GL_FLOAT, 0);

      return texID;
    }

    static void BufferCopy_CPU2GPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format)
    {
      glBindTexture(texture_target, texID);
      glTexSubImage2D(texture_target,0,0,0,texSizeX,texSizeY,
                      texture_format,GL_FLOAT,data);//data CPU to GPU
    }

    static void BufferCopy_GPU2CPU(GLuint texID, float *data, int texSizeX, int texSizeY,
      GLenum texture_target,GLenum texture_format)
    {
      glBindTexture(texture_target,texID);
      glGetTexImage(texture_target,0,texture_format,GL_FLOAT,data);//data GPU to CPU
    }
    static int BufferProperty(int channelCount1_4,GLenum *ret_texture_target,GLint *ret_internal_format,GLenum *ret_texture_format)
    {
      if(channelCount1_4<1 || channelCount1_4>4)return -1;
      if(!ret_texture_target || !ret_internal_format || !ret_texture_format)return -1;

      GLenum _texture_target = GL_TEXTURE_RECTANGLE_ARB;
      GLint _internal_format = GL_RGBA32F;
      GLenum _texture_format = GL_RGBA;

      if(channelCount1_4 == 1)
      {
        _internal_format = GL_R32F;
        _texture_format = GL_RED;
      }

      if(channelCount1_4 == 2)
      {
        _internal_format = GL_RG32F ;
        _texture_format = GL_RG;
      }

      if(channelCount1_4 == 3)
      {
        _internal_format = GL_RGB32F ;
        _texture_format = GL_RGB;
      }
      *ret_texture_target=_texture_target;
      *ret_internal_format=_internal_format;
      *ret_texture_format=_texture_format;
      return 0;
    }
};

#endif
