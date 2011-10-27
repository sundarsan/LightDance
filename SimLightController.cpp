#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>

#include "LightManager.h"
#include "SimLightController.h"
#include "Util.h"

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
    if (key == 'q' || key == 'Q' || key == 27) {
      exit(0);
    } else if (key == 'n' || key == 'N') {
      if (light_manager)
        light_manager->ChangePrograms();
    }
  }

  bool lights_enabled[4];
  double last_beat_time;
  unsigned num_frames;

  LightManager *light_manager;

public:
  GLUTSimLightController() : lights_enabled(), light_manager(0) {
    int argc = 0;
    char *argv = 0;

    the_controller = this;
    last_beat_time = -1;
    num_frames = 0;

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
    lights_enabled[Index] = Enable;
  }

  virtual void BeatNotification(unsigned Index, double Time) {
    // FIXME: It is not reliable to use the time here.
    last_beat_time = get_elapsed_time_in_seconds();
  }

  virtual void MainLoop() {
    glutMainLoop();
  }

  virtual void RegisterLightManager(LightManager &Manager) {
    light_manager = &Manager;
  }
};
}

GLUTSimLightController *GLUTSimLightController::the_controller = 0;

namespace {

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

void glutDrawString(float x, float y, char *string) {
  char c;

  glRasterPos2f(x, y);
  while ((c = *string++)) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
}

}

void GLUTSimLightController::idle() {
  usleep(2000);
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
    { .2, { .9,.9,.9 }, { -.5, -.5 } },
    { .2, { .8, 0, 0 }, {  .5, -.5 } },
    { .2, {  0,.8, 0 }, {  .5,  .5 } },
    { .2, { .8,.8, 0 }, { -.5,  .5 } },
  };
  for (unsigned i = 0; i != 4; ++i) {
    Light &l = lights[i];

    if (lights_enabled[i]) {
      glColor3fv(l.color);
      draw_circle_filled(l.position[0], l.position[1], l.radius);
    }
  }

  // Draw the beat monitor.
  const double beat_monitor_dim_time = .05;
  double elapsed = get_elapsed_time_in_seconds();
  if (elapsed - last_beat_time <= beat_monitor_dim_time) {
    double v = 1 - (elapsed - last_beat_time) / beat_monitor_dim_time;
    glColor3f(v, v, v);
    glRectf(-.9, -.9, -.7, -.8);
  }

  // Draw the 2D overlays.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  const int textHeight = 15;
  char buffer[256];
  int y = 0;
  sprintf(buffer, "Time: %.4fs\n", elapsed);
  glColor3f(1, 1, 1);
  glutDrawString(10, 10 + textHeight*y++, buffer);

  if (true) {
    num_frames++;
    double fps = num_frames / elapsed;
    sprintf(buffer, "FPS: %.4fs\n", fps);
    glColor3f(1, 1, 1);
    glutDrawString(10, 10 + textHeight*y++, buffer);
  }

  if (light_manager) {
    double bpm = light_manager->GetRecentBPM();
    sprintf(buffer, "BPM: %.4fs\n", bpm);
    glColor3f(1, 1, 1);
    glutDrawString(10, 10 + textHeight*y++, buffer);
  }

  glFlush();

  glutSwapBuffers();
}

SimLightController *CreateSimLightController() {
  return new GLUTSimLightController();
}
