// -*- C++ -*-

#ifndef LIGHTPROGRAM_H
#define LIGHTPROGRAM_H

#include "MusicMonitor.h"
#include <string>
#include <vector>

class LightManager;
struct LightInfo;

class LightProgram {
  friend class LightManager;

protected:
  LightProgram();
  
public:
  static void LoadAllPrograms(std::vector<LightProgram *> &Result);

  virtual ~LightProgram();

  virtual std::string GetName() const = 0;

  virtual bool WorksWithSetup(const std::vector<LightInfo> &Lights) const = 0;

  virtual void Start(LightManager &Manager) = 0;
  virtual void Stop() = 0;

  virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double time) = 0;
};

#endif // LIGHTPROGRAM_H
