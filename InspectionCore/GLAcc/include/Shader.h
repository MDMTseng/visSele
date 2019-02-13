#ifndef SHADER_H
#define SHADER_H


#include <GL/glew.h>
#include <map>
#include <string>

class Shader
{
private:
    struct UniformInfo{
      int shaderLoc;
      int shaderTexID;
    };
    std::map<std::string,struct UniformInfo> *uniformMap;
    std::map<std::string,int> *attrMap;
public:
    int Program;
    // Constructor generates the shader on the fly
    static std::string LOADFILE(const char* path);

    Shader( const GLchar *vertexPath, const GLchar *fragmentPath );
    ~Shader();

    void LoadShader(const char* vcode, const char* fcode);
    // Uses the current shader
    void Use( );
    int GetID( );
    int GetAttr (const std::string &name);
    int GetAttribLocation (const std::string &name);

    int GetUnif (const std::string &name);
    int GetUniformLocation(const std::string &name);
    // Uses the current shader
    int TextureActivate(const std::string name,int target, int idx);
    int TextureActivate( int location,int target,int idx);
    int TextureActivate(int idx);
    int TextureActivate(const std::string name);

    void DeactivateTexture( int target, int idx);
    void SetFloat2Shader( std::string name, float  data);
    void SetFragData( std::string name, int idx);
};
#endif
