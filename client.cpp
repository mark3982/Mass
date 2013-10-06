#include <stdio.h>

#include "client.h"
#include "GL\glut.h"

int init() {
   return 1;
}

void display() {
}

void keyboard(unsigned char key, int x, int y) {
   printf("key:%c x:%i y:%i\n", key, x, y);
}

DWORD WINAPI mass_client_entry(void *arg) {
   // initialize Lua
   // initialize OpenGL
   // implement support for Lua window drawing to create the UI
   // Lua scripts run

   MASS_CLIENT_ARGS        *args;

   args = (MASS_CLIENT_ARGS*)arg;

   glutInit(&args->argc, args->argv);
   glutInitWindowPosition(50, 50);
   glutInitWindowSize(300, 300);
   glutCreateWindow("Mass");

   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);

   if (!init())
      return 1;

   glutMainLoop();
   return 0;
}