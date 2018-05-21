
#define GLEW_STATIC
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>

#include <stdio.h>
#include "Shader.h"


#include <iostream>
#include <string>
#include <sstream>

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
* Format the string as expected
*/
void formatMe(string *text) {
    string dot = ".";

    text->insert(1, dot); // transforms 30000 into 3.0000
    text->insert(4, dot); // transforms 3.0000 into 3.00.00
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
* Manage display (to be implemented)
*/
void managerDisplay(void)
{
    glClear(GL_COLOR_BUFFER_BIT);                // clear the screen
    //glutSwapBuffers();
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

/**
* Manage keyboard
*/
void managerKeyboard(unsigned char key, int x, int y) {
    if (key == 27) { // 27 = ESC key
        exit(0);
    }
}


GLuint initFBO(void) {
    GLuint fb;
    // create FBO (off-screen framebuffer)
    glGenFramebuffersEXT(1, &fb);
    // bind offscreen buffer
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);

    return fb;
}

GLuint initGraphicBuffer(int texture_target,GLint internal_format,GLenum texture_format,int texSizeX,int texSizeY)
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

GLuint initCalcBuffer(int texSizeX,int texSizeY, bool singleChannel)
{
    int texture_target = GL_TEXTURE_RECTANGLE_ARB;
    int internal_format = GL_RGBA32F;
    int texture_format = GL_RGBA;
    int channelNum=4;

    if(singleChannel)//Select single channel
    {
      internal_format=GL_R32F;
      texture_format=GL_RED;
      channelNum=1;
    }

    float* dataX = new float[texSizeX*texSizeY*channelNum];
    for (int i=0; i<texSizeX*texSizeY*channelNum; i++)
    {
        if(i%4==0)
          dataX[i] = 0.01*i+0.0f;
    }
    float* dataY = new float[texSizeX*texSizeY*channelNum];
    for (int i=0; i<texSizeX*texSizeY*channelNum; i++)
        dataY[i] = 0;
    float alpha;

    GLuint texID = initGraphicBuffer(texture_target,internal_format,texture_format,texSizeX,texSizeY);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              texture_target, texID, 0);
    printf("New BufferID:%d\n",texID);
    glBindTexture(texture_target, texID);
    glTexSubImage2D(texture_target,0,0,0,texSizeX,texSizeY,
                    texture_format,GL_FLOAT,dataX);//data CPU to GPU

//Do what ever

    glBindTexture(texture_target,texID);
    glGetTexImage(texture_target,0,texture_format,GL_FLOAT,dataY);//data GPU to CPU
    /*glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glReadPixels(0,0,texSizeX,texSizeY,texture_format,GL_FLOAT,dataY);*/


    for (int i=0; i<texSizeX*texSizeY*channelNum; i++)
        printf("%f ",dataY[i]);
    delete(dataY);
    delete(dataX);

    //glDeleteTextures (1,&texID);

    return texID;
}

void initBaseVertex(GLuint *pVBO,GLuint *pVAO)
{
    GLfloat vertices[] =
    {
        // Positions         // Colors
        -1.0f, -1.0f, 0.0f,   1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f
    };
    glGenVertexArrays( 1, pVAO );
    glGenBuffers( 1, pVBO );
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
    glBindVertexArray( *pVAO );

    glBindBuffer( GL_ARRAY_BUFFER, *pVBO );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );

    // Position attribute
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * ) 0 );
    glEnableVertexAttribArray( 0 );
    // Color attribute
    glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * )( 3 * sizeof( GLfloat ) ) );
    glEnableVertexAttribArray( 1 );

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

    //Establish buffers
    int texSizeX=3,texSizeY=3;
    GLuint VBO;
    GLuint VAO;
    initBaseVertex(&VBO,&VAO);
    GLuint TexID = initCalcBuffer(texSizeX,texSizeY,false);

    GLuint fb = initFBO();
    glDeleteFramebuffersEXT (1,&fb);
    Shader ourShader( "shader/core.vs", "shader/core.frag" );
    // Game loop
    while (!glfwWindowShouldClose( window ) )
    {
        // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
        glfwPollEvents( );

        // Render
        // Clear the colorbuffer
        glClearColor( 1.0f, 0.0f, 1.0f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        // Draw the triangle
        ourShader.Use( );
        //ourShader.SetTex2Shader("baseTexture",TexID);
        glBindVertexArray( VAO );
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // Swap the screen buffers
        glfwSwapBuffers( window );
    }
    return 0;
}
