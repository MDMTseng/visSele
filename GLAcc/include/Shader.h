#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <GL/glew.h>

class Shader
{
public:
    int Program;
    // Constructor generates the shader on the fly
    static std::string LOADFILE(const char* path)
    {
      std::string content;
      std::ifstream file;
      file.exceptions ( std::ifstream::badbit );
      try
      {
        file.open( path );
        std::stringstream fStream;
        fStream << file.rdbuf( );
        file.close( );
        content = fStream.str( );
      }
      catch ( std::ifstream::failure e )
      {
          std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
      }
      return content;
    }

    Shader( const GLchar *vertexPath, const GLchar *fragmentPath )
    {
        LoadShader(LOADFILE(vertexPath).c_str( ), LOADFILE(fragmentPath).c_str( ));
    }
    void LoadShader(const char* vcode, const char* fcode)
    {
        if(Program!=-1)
        {
          glDeleteProgram(Program);
        }
        Program=-1;
        // 1. Retrieve the vertex/fragment source code from filePath


        const GLchar *vShaderCode = vcode;
        const GLchar *fShaderCode = fcode;
        // 2. Compile shaders
        GLuint vertex, fragment;
        GLint success;
        GLchar infoLog[512];
        // Vertex Shader
        vertex = glCreateShader( GL_VERTEX_SHADER );
        glShaderSource( vertex, 1, &vShaderCode, NULL );
        glCompileShader( vertex );
        // Print compile errors if any
        glGetShaderiv( vertex, GL_COMPILE_STATUS, &success );
        if ( !success )
        {
            glGetShaderInfoLog( vertex, 512, NULL, infoLog );
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // Fragment Shader
        fragment = glCreateShader( GL_FRAGMENT_SHADER );
        glShaderSource( fragment, 1, &fShaderCode, NULL );
        glCompileShader( fragment );
        // Print compile errors if any
        glGetShaderiv( fragment, GL_COMPILE_STATUS, &success );
        if ( !success )
        {
            glGetShaderInfoLog( fragment, 512, NULL, infoLog );
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // Shader Program
        this->Program = glCreateProgram( );
        glAttachShader( this->Program, vertex );
        glAttachShader( this->Program, fragment );
        glLinkProgram( this->Program );
        // Print linking errors if any
        glGetProgramiv( this->Program, GL_LINK_STATUS, &success );
        if (!success)
        {
            glGetProgramInfoLog( this->Program, 512, NULL, infoLog );
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }
        // Delete the shaders as they're linked into our program now and no longer necessery
        glDeleteShader( vertex );
        glDeleteShader( fragment );

    }
    // Uses the current shader
    void Use( )
    {
        glUseProgram( this->Program );
    }


    int GetID( )
    {
        return this->Program;
    }

    int GetAttribLocation ( char* name)
    {
        int tex_location = glGetAttribLocation (this->Program, name);
        printf("%s:LOC:%d :%s\n",__func__,tex_location,name);
        return tex_location;
    }

    int GetUniformLocation( char* name)
    {
        int tex_location = glGetUniformLocation(this->Program, name);
        printf("%s:LOC:%d :%s\n",__func__,tex_location,name);
        return tex_location;
    }
    // Uses the current shader
    int TextureActivate( char* name,int target, int idx)
    {
        int tex_location=GetUniformLocation(name);
        if(tex_location<0)return tex_location;
        TextureActivate(tex_location,target, idx);
        return 0;
    }
    int TextureActivate( int location,int target,int idx)
    {
        //printf("%s: idx:%d \n",__func__,idx);
        glUniform1i(location, idx);
        TextureActivate(idx);
        glEnable(target);
        return 0;
    }
    int TextureActivate(int idx)
    {
        glActiveTexture(GL_TEXTURE0+idx);
        return 0;
    }

    void DeactivateTexture( int target, int idx)
    {
        glActiveTexture(GL_TEXTURE0+idx);
        glDisable(target);
    }

    void SetFloat2Shader( char* name, float  data)
    {
      GLint loc = glGetUniformLocation(this->Program, name);
      if (loc != -1)
      {
         glUniform1f(loc, data);
      }
    }
    void SetFragData( char* name, int idx)
    {
      glBindFragDataLocation(this->Program, idx, name);
    }
};

#endif
