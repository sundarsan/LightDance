// -*- C++ -*-

#ifndef LIGHTCONTROLLER_H
#define LIGHTCONTROLLER_H

class LightController {
public:
  LightController();
  virtual ~LightController();

  virtual void BeatNotification(unsigned Index, double Time) = 0;

  virtual void SetLight(unsigned Index, bool Enable) = 0;
};

LightController *CreatePhidgetLightController();

LightController *CreateChainedLightController(LightController *a,
                                              LightController *b);

#endif // LIGHTCONTROLLER_H
