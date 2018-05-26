
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLAcc.h>

GLAcc_Framework::GLAcc_Framework()
{
    glGenVertexArrays( 1, &VAO );
    glGenBuffers( 1, &VBO );
}
GLAcc_Framework::~GLAcc_Framework()
{
    glDeleteVertexArrays( 1, &VAO );
    glDeleteBuffers(1,&VBO);
}

void GLAcc_Framework::INIT_ERROR()
{

    if (glewInit()) { // checks if glewInit() is activated
        //cerr << "Unable to initialize GLEW." << endl;
        while (1); // let's use this infinite loop to check the error message before closing the window
        exit(EXIT_FAILURE);
    }


}
void GLAcc_Framework::INIT_MSG()
{
  char *versionGL = "\0";

  versionGL = (char *)(glGetString(GL_VERSION));


  /*cout << endl;
  cout << "OpenGL version: " << versionGL << endl << endl;
  cout << "GLEW version: " << glewGetString(GLEW_VERSION) << endl;


  int maxtexsize;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxtexsize);
  printf("GL_MAX_TEXTURE_SIZE, %d\n",maxtexsize);*/
}


static GLFWwindow* initGLFW(int width,int height) {
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

int GLAcc_Framework::INIT(int width,int height)
{
  GLFWwindow* window = initGLFW( width, height);
  window_GLFW = window;
  if ( nullptr == window )
  {
      std::cout << "Failed to create GLFW window" << std::endl;
      glfwTerminate( );

      return EXIT_FAILURE;
  }
  glfwMakeContextCurrent( window );

  int screenWidth, screenHeight;
  glfwGetFramebufferSize( window, &screenWidth, &screenHeight );
  INIT_ERROR();

  return 0;
}
void GLAcc_Framework::Setup()
{
    float mult=1;
    GLfloat vertices[] =
    {
        // Positions         // Colors
        -mult, -mult, 0.0f,   mult, 0.0f, 0.0f,
        -mult, mult, 0.0f,   0.0f, mult, 0.0f,
        mult, -mult, 0.0f,   0.0f, 0.0f, mult,
        mult, mult, 0.0f,   mult, mult, mult
    };
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
    glBindVertexArray( VAO );

    glBindBuffer( GL_ARRAY_BUFFER, VBO );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );

    glBindVertexArray( 0 ); // Unbind VAO

}


void GLAcc_Framework::SetupShader(Shader &shader)
{  // Position attribute

    shader.Use( );
    glBindVertexArray( VAO );
    int loc = shader.GetAttribLocation("position");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * ) 0 );
    glEnableVertexAttribArray( loc);
    // Color attribute
    loc = shader.GetAttribLocation("color");
    glVertexAttribPointer( loc, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( GLfloat ), ( GLvoid * )( 3 * sizeof( GLfloat ) ) );
    glEnableVertexAttribArray( loc );

    glBindVertexArray( 0 ); // Unbind VAO
}
void GLAcc_Framework::SetupViewPort(int W,int H)
{
    glViewport(0,0,W,H);
}

void GLAcc_Framework::Begin()
{
    glBindVertexArray( VAO );
}
void GLAcc_Framework::Compute()
{
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
}
void GLAcc_Framework::End()
{
    glBindVertexArray( 0 ); // Unbind VAO
}

int GLAcc_Framework::AttachTex2FBO(GLuint fbo,GLenum attachment,GLAcc_GPU_Buffer &gbuf)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, gbuf.GetTextureTarget(), gbuf.GetTexID(), 0);

    int ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if( ret == GL_FRAMEBUFFER_COMPLETE)
    {
      return 0;
    }

    return -1;
}
