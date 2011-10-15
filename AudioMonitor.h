// -*- C++ -*-

#ifndef AUDIOMONITOR_H
#define AUDIOMONITOR_H

/// \brief Delegate class for an audio monitor.
class AudioMonitorHandler {
protected:
  AudioMonitorHandler();

public:
  virtual ~AudioMonitorHandler();

  virtual void HandleSample(double time, double left, double right) = 0;
};

class AudioMonitor {
protected:
  AudioMonitor();

public:
  virtual ~AudioMonitor();

  virtual void Start() = 0;
  virtual void Stop() = 0;
};

AudioMonitor *CreateOSXAudioMonitor(AudioMonitorHandler *handler);

#endif // AUDIOMONITOR_H
