
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

/**
* Main, what else?
*/
int main(int argc, char** argv) {
    initFreeGlut(argc, argv);    // inits freeglut
    managerError();                // manages errors
    consoleMessage();            // displays message on the console

    // C. register the display callback function
    glutDisplayFunc(managerDisplay);                        // 8. callback function
    glutKeyboardFunc(managerKeyboard);

    // D. main loop
    glutMainLoop();                                    // 9. infinite loop
    return 0;
}
