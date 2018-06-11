#include <Shader.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

std::string Shader::LOADFILE(const char* path)
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

Shader::Shader( const GLchar *vertexPath, const GLchar *fragmentPath )
{
    uniformMap=new std::map<std::string,struct UniformInfo>();
    attrMap=new std::map<std::string,int>();

    LoadShader(LOADFILE(vertexPath).c_str( ), LOADFILE(fragmentPath).c_str( ));
}
Shader::~Shader()
{
    if(Program!=-1)
    {
      glDeleteProgram(Program);
    }
    Program=-1;
    delete uniformMap;
    delete attrMap;
}

void Shader::LoadShader(const char* vcode, const char* fcode)
{
    if(Program!=-1)
    {
      glDeleteProgram(Program);
    }
    Program=-1;
    uniformMap->erase(uniformMap->begin(), uniformMap->end());
    attrMap->erase(attrMap->begin(), attrMap->end());

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
void Shader::Use( )
{
    glUseProgram( this->Program );
}


int Shader::GetID( )
{
    return this->Program;
}


int Shader::GetAttr (const std::string &name)
{
  return GetAttribLocation (name);
}
int Shader::GetAttribLocation (const std::string &name)
{
    auto ele = attrMap->find(name);
    if( ele != attrMap->end() )
    {
      return ele->second;
    }
    int tex_location = glGetAttribLocation (this->Program, name.c_str());
    printf("%s:LOC:%d :%s\n",__func__,tex_location,name.c_str());
    (*attrMap)[name] = tex_location;
    return tex_location;
}

int Shader::GetUnif (const std::string &name)
{
  return GetUniformLocation (name);
}
int Shader::GetUniformLocation(const std::string &name)
{
    auto ele = uniformMap->find(name);
    if( ele != uniformMap->end() )
    {
      return ele->second.shaderLoc;
    }
    int tex_location = glGetUniformLocation(this->Program, name.c_str());
    printf("%s:LOC:%d :%s\n",__func__,tex_location,name.c_str());
    (*uniformMap)[name].shaderLoc = tex_location;
    (*uniformMap)[name].shaderTexID = -1;
    return tex_location;
}
// Uses the current shader
int Shader::TextureActivate(const std::string name,int target, int idx)
{
    int tex_location=GetUniformLocation(name);
    if(tex_location<0)return tex_location;
    (*uniformMap)[name].shaderTexID = idx;
    TextureActivate(tex_location,target, idx);
    return 0;
}
int Shader::TextureActivate( int location,int target,int idx)
{
    //printf("%s: idx:%d \n",__func__,idx);
    glUniform1i(location, idx);
    TextureActivate(idx);
    glEnable(target);
    return 0;
}
int Shader::TextureActivate(int idx)
{
    glActiveTexture(GL_TEXTURE0+idx);
    return 0;
}
int Shader::TextureActivate(const std::string name)
{
    auto ele = uniformMap->find(name);
    if( ele == uniformMap->end() || ele->second.shaderTexID==-1)
    {
      return -1;
    }
    TextureActivate(ele->second.shaderTexID);
    return 0;
}

void Shader::DeactivateTexture( int target, int idx)
{
    glActiveTexture(GL_TEXTURE0+idx);
    glDisable(target);
}

void Shader::SetFloat2Shader( std::string name, float  data)
{
  GLint loc = GetUniformLocation (name);
  if (loc != -1)
  {
     glUniform1f(loc, data);
  }
}
void Shader::SetFragData( std::string name, int idx)
{
  glBindFragDataLocation(this->Program, idx, name.c_str());
}
