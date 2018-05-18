#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
using namespace std ;

#define _USE_MATH_DEFINES
#include <math.h>

// REMEMBER TO INSTALL GLEW! http://glew.sourceforge.net/install.html
//
// 2 WAYS TO COMPILE GLEW:
// 1)  AS A STATIC LIB.
//     #defining GLEW_STATIC means to compile the glew library as a static lib.
//     This means your prog WILL NOT require the glew32.dll file to accompany your executable.
//
// THIS IS HOW YOU COMPILE GLEW AS A STATIC LIB:
#define GLEW_STATIC
#include <GL/glew.h>
#pragma comment( lib, "glew32s.lib" )   // the 's' is for 'static'

// 2)  AS A DYNAMIC LIB:
//     THIS REQUIRES YOU INCLUDE glew32.dll WITH YOUR FINAL EXECUTABLE/HAVE IT INSTALLED.
//#include <GL/glew.h>
//#pragma comment( lib, "glew32.lib" )

// I'm using the STATIC LIB way above, because I prefer it.

#include <GL/glut.h>

GLuint glslProgId ;     // GLSL shader PROGRAM identifier
GLuint vshId, pshId ;   // GLSL vertex shader id, pixel shader id

// These 2 are "handles" that connect the POSITION attribute in
// the GLSL shader with __AN ACTUAL SET OF VERTICES__ from the C++ source code.
// In this program each VERTEX has a position and a color (only).
GLuint positionAttribute, colorAttribute;

// This is the connector between the uniform variable modelViewMatrix,
// used in the glsl shader code.  We need this variable to LOAD
// a specific modelview matrix INTO the GLSL shader before rendering the verts.
GLuint uniformModelViewMatrix;

// The vertex array object identifier.  This single number
// identifies EVERYTHING about the vertex buffer we are
// about to create and render, from its attribute set (positions, colors)
// to where to fetch the data from when it's time to draw.
GLuint vao ;

// window width and height.
float w=512.f, h=512.f;

struct VertexPC
{
  float x,y,z, r,g,b,a ;
  VertexPC( float ix, float iy, float iz, float ir, float ig, float ib, float ia ):
    x(ix),y(iy),z(iz), r(ir),g(ig),b(ib),a(ia)
  {}
} ;

// The data for the vertex and index buffers.
// I'm just using index buffers to show how they are used.
static VertexPC verts[] = {
  VertexPC(0,0,0, 1,0,0,1),
  VertexPC(1,0,0, 0,1,0,1),
  VertexPC(0,1,0, 0,0,1,1)
} ;
static int indices[] = {0,1,2};

map<int,const char*> glErrName ;
// GL ERR
map<int,const char*> createErrMap()
{
  map<int,const char*> errmap ;
  errmap.insert( make_pair( 0x0000, "GL_NO_ERROR" ) ) ;
  errmap.insert( make_pair( 0x0500, "GL_INVALID_ENUM" ) ) ;
  errmap.insert( make_pair( 0x0501, "GL_INVALID_VALUE" ) ) ;
  errmap.insert( make_pair( 0x0502, "GL_INVALID_OPERATION" ) ) ;
  errmap.insert( make_pair( 0x0503, "GL_STACKOVERFLOW" ) ) ;
  errmap.insert( make_pair( 0x0504, "GL_STACK_UNDERFLOW" ) ) ;
  errmap.insert( make_pair( 0x0505, "GL_OUTOFMEMORY" ) ) ;

  errmap.insert( make_pair( 0x8CD5, "GL_FRAMEBUFFER_COMPLETE" ) ) ;
  errmap.insert( make_pair( 0x8CD6, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" ) ) ;
  errmap.insert( make_pair( 0x8CD7, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" ) ) ;
  errmap.insert( make_pair( 0x8CD9, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS" ) ) ;
  errmap.insert( make_pair( 0x8CDD, "GL_FRAMEBUFFER_UNSUPPORTED" ) ) ;
  return errmap ;
}

inline bool GL_OK()
{
  GLenum err = glGetError() ;
  if( err != GL_NO_ERROR )
    printf( "GLERROR %d %s\n", err, glErrName[ err ] ) ;
  return err == GL_NO_ERROR ;
}

inline bool GL_OK( int line, const char* file )
{
  GLenum err = glGetError() ;
  if( err != GL_NO_ERROR )
    printf( "GLERROR %d %s, line=%d of file=%s\n", err, glErrName[ err ], line, file ) ;
  return err == GL_NO_ERROR ;
}

inline bool CHECK( bool cond, const char* errMsg )
{
  if( !cond )  puts( errMsg ) ;
  return cond ;
}

// WITH LINE NUMBERS
#define CHECK_GL GL_OK( __LINE__, __FILE__ )

// WITHOUT LINE #s
//#define CHECK_GL GL_OK()

void printShaderInfoLog(GLuint obj)
{
  int infologLength = 0;

  glGetShaderiv( obj, GL_INFO_LOG_LENGTH, &infologLength ) ;
  if( infologLength )
  {
    char *infoLog = (char *)malloc(infologLength);
    int charsWritten ;
    glGetShaderInfoLog( obj, infologLength, &charsWritten, infoLog ) ;
    puts( infoLog ) ;
    free( infoLog ) ;
  }
}

void printProgramInfoLog( GLuint obj )
{
  int infologLength = 0;

  glGetProgramiv( obj, GL_INFO_LOG_LENGTH, &infologLength ) ;
  if( infologLength )
  {
    char *infoLog = (char *)malloc( infologLength ) ;
    int charsWritten ;
    glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
    puts( infoLog ) ;
    free( infoLog ) ;
  }
}

void setupShaders()
{
  // The text of the shaders.  I put it as an inline string
  // just so I could post this easily w/o external files,
  // but you could easily load the shader from an external file.
  // The point is, you must pass your shader to OpenGL as
  // just a NULL terminated string.
  const char *vs =
    "uniform mat4 modelViewMatrix ;                         "
    "                                                       "
    "attribute vec3 position;                               "
    "attribute vec4 color;                                  "
    "                                                       "
    "varying vec4 Color;                                    "
    "                                                       "
    "void main()                                            "
    "{                                                      "
    "  Color = color;                                       "
    "  gl_Position = modelViewMatrix * vec4(position,1.0) ; "
    "}                                                      " ;

  const char *fs =
    "in vec4 Color;            "
    "                          "
    "void main()               "
    "{                         "
    "  gl_FragColor = Color ;  "
    "}                         " ;

  vshId = glCreateShader(GL_VERTEX_SHADER) ;  CHECK_GL ;
  glShaderSource( vshId, 1, &vs, NULL ) ;  CHECK_GL ;
  glCompileShader( vshId ) ;  CHECK_GL ;
  printShaderInfoLog( vshId );

  pshId = glCreateShader(GL_FRAGMENT_SHADER) ;  CHECK_GL ;
  glShaderSource( pshId, 1, &fs, NULL ) ;  CHECK_GL ;
  glCompileShader( pshId ) ;  CHECK_GL ;
  printShaderInfoLog( pshId );

  glslProgId = glCreateProgram();
  glAttachShader( glslProgId, vshId );
  glAttachShader( glslProgId, pshId );

  glLinkProgram( glslProgId );
  printProgramInfoLog( glslProgId );

  positionAttribute = glGetAttribLocation( glslProgId, "position" ) ;
  colorAttribute = glGetAttribLocation( glslProgId, "color" ) ;

  uniformModelViewMatrix = glGetUniformLocation( glslProgId, "modelViewMatrix" ) ;



  // Make a "vertex buffer"
  GLuint vertexBuffer ;
  glGenBuffers( 1, &vertexBuffer ) ;

  // LOAD THE DATA INTO IT:
  glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer ) ;
  glBufferData( GL_ARRAY_BUFFER, sizeof( verts ), verts, GL_STATIC_DRAW ) ;

  // Make an "index buffer"
  GLuint indexBuffer ;
  glGenBuffers( 1, &indexBuffer ) ;
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer ) ;
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW ) ;



  //// SET UP THE VAO
  // You know, this is weird, but a VERTEX ARRAY OBJECT
  // DOESN'T ACTUALLY STORE DATA.
  // Vertex BUFFERS store the data.
  // A VAO stores the SETUP of how a group of
  // vertices is to be rendered.
  // That is, it stores:
  // 1)  The DATA (in vertex BUFFERS) to be drawn
  // 2)  THE SEMANTICS of that data (vertex format)
  // 3)  THE LINKAGE TO THE SHADER THAT WILL BE USED TO DRAW THIS DATA.

  // So you can see, it's all very intermingled and messy.
  // TO draw vertex data with GLSL, you have to:
  //   1) Load the raw vertex data into a vertex buffer
  //   2) set up vertex attributes (via glVertexAttribPointer)
  //      WHICH does 2 things:
  //      a)  It tells OpenGL the VERTEX FORMAT (floats? 3 elements? 4 elements? stride?)
  //      b)  It tells OpenGL THE SHADER VARIABLE THAT THIS VERTEX ATTRIBUTE LINKS INTO.
  //   3) Call glDrawArrays or glDrawElements( ..., 0 )
  //
  // INIT THE VERTEX ARRAY (stuff to draw)
  // The vertex array OBJECT remembers info about
  // WHAT VERTEX ATTRIBS ARE TO BE BOUND, to what
  // indices, and the actual vertex buffer object
  // from which to pull data when rendering.
  glGenVertexArrays( 1, &vao ) ;
  glBindVertexArray( vao ) ;

  glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer ) ;
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer ) ;

  glEnableVertexAttribArray( positionAttribute ) ;
  glVertexAttribPointer(
    positionAttribute,  // index. NOTE THIS HAS BEEN CONNECTED UP EARLIER
    // WITH THE VERTEX SHADER THAT WILL BE USED TO RENDER THIS VERTEX ARRAY
    // IN THE LINE:
    //   > positionAttribute = glGetAttribLocation( glslProgId, "position" ) ; // C++ code line
    // and "position" is used in the GLSL vertex shader as:
    //   > attribute vec3 position; // GLSL shader line
    // SO. Don't be confused, the `positionAttribute` variable is simply connected up
    // to the vertex shader's INCOMING "position" attribute.  Naked girls.
    3,          // size. 3 floats/vertex
    GL_FLOAT,   // type. floats!
    0,          // normalized? No.
    sizeof(VertexPC),          // stride
    0           // pointer.  0 says to use the CURRENTLY BOUND
    // VERTEX BUFFER (WHATEVER THAT IS AT THE TIME OF THE DRAW CALL).
    // If you want to draw from CLIENT MEMORY you would pass AN ACTUAL ADDRESS
    // here (BUT YOU MUST MAKE SURE TO HAVE NO VERTEX BUFFER BOUND BECAUSE THEN
    // THE MEMORY ADDRESS YOU PASS HERE WOULD BE INTERPRETTED AS AN __OFFSET__
    // INTO THAT VERTEX BUFFER, TRIGGERING A SEGFAULT/OUT OF BOUNDS EXCEPTION TYPE THING.)
  ) ;

  glEnableVertexAttribArray( colorAttribute ) ;

  int offset = 3*sizeof(float) ;
  glVertexAttribPointer( colorAttribute, 4, GL_FLOAT, 0, sizeof( VertexPC ),

    (void*)offset  // RE: THE LAST PARAMETER: I know it looks ridiculous!! to pass a NUMBER as cast to VOID* here,
    // but that's C api's for you.  "Wait! Shouldn't it be &offset?"
    // NO. Your eyes kid you not.  You take the INTEGER NUMBER (which amounts to 12 here)
    // AND JUST PASS THAT AS IF IT WERE A "VOID*" POINTER. Even though you and I know it is not.
  ) ;

}

void keyboard(unsigned char key, int x, int y)
{
  switch(key)
  {
    case 27: //esc
      glDeleteVertexArrays( 1,&vao ) ;
      glDeleteProgram( glslProgId );
      glDeleteShader( vshId );
      glDeleteShader( pshId );
      exit(0);
      break ;
  }
}

void resizeWindow(int width, int height)
{
  w=width,h=height;
}

void draw()
{
  glViewport( 0, 0, w, h ) ;

  glClearColor( 0.1f, 0.1f, 0.1f, 0.1f ) ;
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

  // Bind the shader I created earlier
  glUseProgram( glslProgId );

  // Update the uniform variable.  YOU CAN
  // ONLY UPDATE THE UNIFORMS OF __BOUND SHADERS__ --
  // IE glUniformMatrix4fv must be called AFTER glUseProgram
  float m[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } ;
  glUniformMatrix4fv(uniformModelViewMatrix, 1, false, m);

  // Here we bind the vertex array "object".  This is EQUIVALENT
  // to "replaying"
  glBindVertexArray( vao );

  // Now we could draw in one of 2 ways:

  // DRAW USING THE VERTEX BUFFER (ONLY):
  //glDrawArrays( GL_TRIANGLES, 0, 3 ) ; // draw as vertex buffer (with no index buffer)

  // DRAW USING INDEX BUFFER + VERTEX BUFFER
  glDrawElements(
    GL_TRIANGLES,
    3, //# INDICES in the index buffer
    GL_UNSIGNED_INT, // I used 4-byte ints. Some other people use shorts, but I never do.
    0  // 0 means, use the index buffer bound
  ) ;

  glutSwapBuffers();
}
/*
int main(int argc, char **argv)
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
  glutInitWindowPosition(100,100);
  glutInitWindowSize( w,h ) ;
  glutCreateWindow("GLSL with GLUT");

  glutDisplayFunc(draw);
  glutIdleFunc(draw);
  glutReshapeFunc(resizeWindow);
  glutKeyboardFunc(keyboard);

  glewInit();
  //if( glewIsSupported("GL_VERSION_3_0") )

  glEnable(GL_DEPTH_TEST);
  glClearColor(1.0,1.0,1.0,1.0);

  setupShaders() ;

  glutMainLoop() ;

  return 0 ;
}*/
