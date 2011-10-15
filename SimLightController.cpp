#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>

#include "SimLightController.h"

namespace {
class GLUTSimLightController : public SimLightController {
  static GLUTSimLightController *the_controller;

  static void idle_callback() {
    the_controller->idle();
  }
  static void draw_callback() {
    the_controller->draw();
  }
  static void keypress_callback(unsigned char key, int x, int y) {
    the_controller->keypress(key, x, y);
  }

  void idle();
  void draw();  
  void keypress(unsigned char key, int x, int y) {
    if (key=='q' || key=='Q' || key==27) {
      exit(0);
    }
  }

  bool lights_enabled[4];

public:
  GLUTSimLightController() : lights_enabled() {
    int argc = 0;
    char *argv = 0;

    the_controller = this;

    glutInit(&argc, &argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(640, 480);
    glutCreateWindow("SimLightController");

    glutIdleFunc(idle_callback);
    glutKeyboardFunc(keypress_callback);
    glutDisplayFunc(draw_callback);
  }

  virtual void SetLight(unsigned Index, bool Enable) {
    // FIXME: Irritatingly, we are on the wrong thread here to post a
    // redisplay. We just draw in a loop, to solve this.
    lights_enabled[Index] = Enable;
  }
  
  virtual void MainLoop() {
    glutMainLoop();
  }
};
}

GLUTSimLightController *GLUTSimLightController::the_controller = 0;

void draw_circle_filled(float cx, float cy, float radius,
                        unsigned N = 32) {
  float k = 2.*M_PI/N;

  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  for (unsigned i = 0; i != N + 1; ++i) {
    float t = i*k;
    glVertex2f(cx + cos(t)*radius, cy + sin(t)*radius);
  }
  glEnd();
}

void GLUTSimLightController::idle() {
  static int count = 0;
  if ((++count % 64) == 0)
    glutPostRedisplay();
}

void GLUTSimLightController::draw() {
  int w = glutGet(GLUT_WINDOW_WIDTH);
  int h = glutGet(GLUT_WINDOW_HEIGHT);
  float light_position[4] = {0.0, 0.0, -1.0, 0.0};

  glViewport(0, 0, w, h);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(35, (double) w/h, 0.1, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glLightfv(GL_LIGHT0, GL_POSITION, light_position);

  glTranslatef(0, 0, -3);

  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_POINT_SMOOTH);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_LIGHT0);
  glEnable(GL_NORMALIZE);

  struct Light {
    float radius;
    float color[3];
    float position[2];
  } lights[4] = {
    { .2, {  0,.8, 0 }, { -.5, -.5 } },
    { .2, {  0, 0,.8 }, {  .5,  .5 } },
    { .2, { .8, 0, 0 }, {  .5, -.5 } },
    { .2, { .8,.8, 0 }, { -.5,  .5 } },
  };
  for (unsigned i = 0; i != 4; ++i) {
    Light &l = lights[i];

    if (lights_enabled[i]) {
      glColor3fv(l.color);
      draw_circle_filled(l.position[0], l.position[1], l.radius);
    }
  }

  glFlush();

  glutSwapBuffers();
}

SimLightController *CreateSimLightController() {
  return new GLUTSimLightController();
}
