// -*- C++ -*-

#ifndef SIMLIGHTCONTROLLER_H
#define SIMLIGHTCONTROLLER_H

#include "LightController.h"

class SimLightController : public LightController {
public:
  virtual void MainLoop() = 0;
};

SimLightController *CreateSimLightController();

#endif // SIMLIGHTCONTROLLER_H
