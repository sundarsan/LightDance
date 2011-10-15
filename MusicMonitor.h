// -*- C++ -*-

#ifndef MUSICMONITOR_H
#define MUSICMONITOR_H

#include "AudioMonitor.h"

/// \brief Delegate class for a music monitor.
class MusicMonitorHandler {
public:
  enum BeatKind {
    kBeatLow = 0,
    kBeatHi
  };

protected:
  MusicMonitorHandler();

public:
  virtual ~MusicMonitorHandler();

  virtual void HandleBeat(BeatKind kind, double time) = 0;
};

class MusicMonitor : public AudioMonitorHandler {
protected:
  MusicMonitor();
  virtual void HandleSample(double time, double left, double right) = 0;
  
public:
  virtual ~MusicMonitor();
};

MusicMonitor *CreateAubioMusicMonitor(MusicMonitorHandler *handler);

#endif // MUSICMONITOR_H
