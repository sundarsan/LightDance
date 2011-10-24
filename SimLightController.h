// -*- C++ -*-

#ifndef SIMLIGHTCONTROLLER_H
#define SIMLIGHTCONTROLLER_H

#include "LightController.h"

class LightManager;

class SimLightController : public LightController {
public:
  virtual void MainLoop() = 0;

  virtual void RegisterLightManager(LightManager &) = 0;
};

SimLightController *CreateSimLightController();

#endif // SIMLIGHTCONTROLLER_H
