
#define GLEW_STATIC
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>

#include <stdio.h>
#include "Shader.h"


#include <iostream>
#include <string>
#include <sstream>

#include "GLAcc.h"
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

GLuint initCalcBuffer(int texSizeX,int texSizeY, int ChannelCount)
{
    float* dataX = new float[texSizeX*texSizeY*ChannelCount];
    for (int i=0; i<texSizeX*texSizeY*ChannelCount; i++)
    {
        if(i%4==0)
          dataX[i] = 0.01*i+0.0f;
    }
    float* dataY = new float[texSizeX*texSizeY*ChannelCount];

//Do what ever

    GLAcc_GPU_Buffer tex(ChannelCount,texSizeX,texSizeY);
    printf("New BufferID:%d\n",tex.texID);
    tex.CPU2GPU(dataX, texSizeX*texSizeY*ChannelCount);

    tex.GPU2CPU(dataY, texSizeX*texSizeY*ChannelCount);

    for (int i=0; i<texSizeX*texSizeY*ChannelCount; i++)
        printf("%f ",dataY[i]);
    delete(dataY);
    delete(dataX);

    //glDeleteTextures (1,&texID);
    return tex.texID;
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
    GLuint TexID = initCalcBuffer(texSizeX,texSizeY,4);

    GLuint fb = initFBO();
    glDeleteFramebuffersEXT (1,&fb);
    Shader ourShader( "shader/core.vs", "shader/core.frag" );
    ourShader.SetTex2Shader("baseTexture2",TexID);
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
        //
        glBindVertexArray( VAO );
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // Swap the screen buffers
        glfwSwapBuffers( window );
    }
    return 0;
}
