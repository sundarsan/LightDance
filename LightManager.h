// -*- C++ -*-

#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include "MusicMonitor.h"
#include <vector>

struct LightInfo;
class LightController;
class LightProgram;
class MusicMonitorHandler;

class LightManager : public MusicMonitorHandler {
protected:
  LightManager();
public:
  virtual ~LightManager();

  virtual LightController &GetController() const = 0;

  virtual const std::vector<LightInfo> &GetSetup() const = 0;

  virtual void ChangePrograms() = 0;

  virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double time) = 0;
};

LightManager *CreateLightManager(LightController *Controller,
                                 std::vector<LightInfo> LightSetup);

#endif // LIGHTMANAGER_H
