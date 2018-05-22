
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

void deleteFBO(GLuint fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}
GLuint initFBO(GLAcc_GPU_Buffer &gbuf) {
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gbuf.GetTextureTarget(), gbuf.GetTexID(), 0);

    int ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if( ret == GL_FRAMEBUFFER_COMPLETE)
    {

    }
    return fbo;
}

void WriteBuffer(GLAcc_GPU_Buffer &tex)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataX = new float[totolLength];
    for (int i=0; i<totolLength; i++)
    {
        if(i%tex.GetChannelCount()==0)
          dataX[i] = 0.01*i+0.0f;
        else
        {
          dataX[i] = 0;
        }
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
        dataX[i] = i*1.0f/totolLength;
    }
    tex.CPU2GPU(dataX, totolLength);
    delete(dataX);
}


void ReadBuffer(GLAcc_GPU_Buffer &tex)
{
    int totolLength=tex.GetBuffSizeX()*tex.GetBuffSizeY()*tex.GetChannelCount();
    float* dataY = new float[totolLength];

    tex.GPU2CPU(dataY, totolLength);
    printf("%s: TexID:%d\n",__func__,tex.GetTexID());
    printf("X:%d Y:%d CH:%d totolLength:%d \n",tex.GetBuffSizeX(),tex.GetBuffSizeY(),tex.GetChannelCount(),totolLength);
    for (int i=0; i<totolLength; i++)
        printf("%f ",dataY[i]);
    delete(dataY);
    printf("\n");
}

void initBaseVertex(Shader &shader,GLuint *pVBO,GLuint *pVAO)
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
    int loc = shader.GetAttribLocation("position");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * ) 0 );
    glEnableVertexAttribArray( loc);
    // Color attribute
    loc = shader.GetAttribLocation("color");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * )( 3 * sizeof( GLfloat ) ) );
    glEnableVertexAttribArray( loc );

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

    Shader ourShader( "shader/core.vs", "shader/core.frag" );
    //Establish buffers
    int texSizeX=3,texSizeY=3;
    glViewport(0,0,texSizeX,texSizeY);
    GLuint VBO;
    GLuint VAO;
    GLAcc_GPU_Buffer tex1(4,texSizeX,texSizeY);
    GLAcc_GPU_Buffer tex2(4,texSizeX,texSizeY);
    printf("tex ID:%d\n",tex1.GetTexID());
    printf("tex2 ID:%d\n",tex2.GetTexID());


    WriteBuffer(tex1);
    WriteBuffer2(tex2);
    ReadBuffer(tex1);
    ReadBuffer(tex2);

    GLuint fbo = initFBO(tex2);
    //glDeleteFramebuffersEXT (1,&fb);

    ourShader.Use( );
    //Setup polygon(square) vertices
    initBaseVertex(ourShader,&VBO,&VAO);


    ourShader.SetTex2Shader("baseTexture1",0);
    tex1.BindTexture();

    ourShader.SetTex2Shader("baseTexture2",1);
    tex2.BindTexture();


    // Game loop
    //while (!glfwWindowShouldClose( window ) )
    for(int i=0;i<2;i++)
    {
        // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
        //glfwPollEvents( );

        // Render
        // Clear the colorbuffer
        //glClearColor( 1.0f, 0.0f, 1.0f, 1.0f );
        //glClear( GL_COLOR_BUFFER_BIT );

        //
        glBindVertexArray( VAO );
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        ReadBuffer(tex2);

        // Swap the screen buffers
        glfwSwapBuffers( window );
    }
    deleteFBO(fbo);

    return 0;
}
