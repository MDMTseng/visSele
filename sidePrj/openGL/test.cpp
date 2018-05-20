
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <stdio.h>


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
    GLint versionFreeGlutInt = 0;

    versionGL = (char *)(glGetString(GL_VERSION));
    versionFreeGlutInt = (glutGet(GLUT_VERSION));

    string versionFreeGlutString = makeMeString(versionFreeGlutInt);
    formatMe(&versionFreeGlutString);

    cout << endl;
    cout << "OpenGL version: " << versionGL << endl << endl;
    cout << "FreeGLUT version: " << versionFreeGlutString << endl << endl;

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
    glutSwapBuffers();
}


/**
* Initialize FREEGLUT
*/
void initFreeGlut(int ac, char *av[]) {
    // A. init
    glutInit(&ac, av);                                // 1. inits glut with arguments from the shell
    glutInitDisplayString("");                        // 2a. sets display parameters with a string (obsolete)
    glutInitDisplayMode(GLUT_SINGLE);                // 2b. sets display parameters with defines
    glutInitWindowSize(600, 600);                    // 3. window size
    glutInitContextVersion(3, 3);                    // 4. sets the version 3.3 as current version (so some functions of 1.x and 2.x could not work properly)
    glutInitContextProfile(GLUT_CORE_PROFILE);        // 5. sets the version 3.3 for the profile core
    glutInitWindowPosition(500, 500);                // 6. distance from the top-left screen

                                                    // B. create window
    glutCreateWindow("BadproG - Hello world :D");    // 7. message displayed on top bar window

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

void initCalcBuffer(int texSizeX,int texSizeY, bool singleChannel)
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
        dataX[i] = i;
    float* dataY = new float[texSizeX*texSizeY*channelNum];
    for (int i=0; i<texSizeX*texSizeY*channelNum; i++)
        dataY[i] = 0;
    float alpha;

    GLuint fb = initFBO();
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

    glDeleteFramebuffersEXT (1,&fb);
    glDeleteTextures (1,&texID);
}

void InitProjection(int texSizeX,int texSizeY)
{
    // viewport for 1:1 pixel=texel=geometry mapping
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, texSizeX, 0.0, texSizeY,-1,1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glViewport(0, 0, texSizeX, texSizeY);
}
/**
* Main, what else?
*/
int main(int argc, char** argv) {
    initFreeGlut(argc, argv);    // inits freeglut
    managerError();                // manages errors
    consoleMessage();            // displays message on the console


    int texSizeX=3,texSizeY=3;
    InitProjection(texSizeX,texSizeY);
    initCalcBuffer(texSizeX,texSizeY,false);

    // C. register the display callback function
    glutDisplayFunc(managerDisplay);                        // 8. callback function
    glutKeyboardFunc(managerKeyboard);
    // D. main loop
    glutMainLoop();                                    // 9. infinite loop
    return 0;
}
