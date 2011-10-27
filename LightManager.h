// -*- C++ -*-

#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include "MusicMonitor.h"
#include <vector>

struct LightInfo;
class LightController;
class LightProgram;
class MusicMonitorHandler;

struct LightState {
  bool Enabled;
  double TotalEnabledTime;
  double LastEnableTime;
};

class LightManager : public MusicMonitorHandler {
protected:
  LightManager();
public:
  virtual ~LightManager();

  virtual const std::vector<LightInfo> &GetSetup() const = 0;

  virtual void ChangePrograms() = 0;

  virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double time) = 0;

  virtual void SetLight(unsigned Index, bool Enable) = 0;

  virtual const LightState &GetLightState(unsigned Index) const = 0;

  virtual double GetRecentBPM() const = 0;
  
  virtual std::string GetProgramName() const = 0;
};

LightManager *CreateLightManager(LightController *Controller,
                                 std::vector<LightInfo> LightSetup);

#endif // LIGHTMANAGER_H
