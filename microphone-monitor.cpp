#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include "AudioMonitor.h"
#include "MusicMonitor.h"
#include "SimLightController.h"
#include "Util.h"

///

class LoggingAudioHandler : public AudioMonitorHandler {
public:
  virtual void HandleSample(double time, double left, double right) {
    static double start_time = get_time_in_seconds();
    static int count;
    if ((++count % 10000) == 0)
      fprintf(stderr, "%.2fs, %.2fs: (%.2f, %.2f)\n",
              get_time_in_seconds() - start_time, time, left, right);
  }
};

class LoggingMusicHandler : public MusicMonitorHandler {
public:
  virtual void HandleBeat(BeatKind kind, double time) {
    static double start_time = get_time_in_seconds();
    fprintf(stderr, "%.2fs, %.2fs: beat: %d\n",
            get_time_in_seconds() - start_time, time, (int) kind);
  }
};

class SimpleLightSwitcher : public MusicMonitorHandler {;
  LightController *controller;
  int which_light;
  double last_change_time;

public:
  SimpleLightSwitcher(LightController *controller_)
    : controller(controller_)
  {
    which_light = 0;
    last_change_time = -1;
  }
  ~SimpleLightSwitcher() {
    delete controller;
  }

  virtual void HandleBeat(BeatKind kind, double time) {
    controller->BeatNotification(kind, time);

    fprintf(stderr, "BeatKind: %d, %.4fs\n", kind, time);
    if (time - last_change_time > 0.01) {
      last_change_time = time;
      which_light = !which_light;

      fprintf(stderr, "set light %d\n", which_light);
      controller->SetLight(0, which_light == 0);
      controller->SetLight(2, which_light == 1);
    }
  }
};

int main() {
  SimLightController *SLC = CreateSimLightController();
  LightController *controller = SLC;

  if (0)
    controller = CreateChainedLightController(SLC,
                                              CreatePhidgetLightController());

  MusicMonitorHandler *MMH = new SimpleLightSwitcher(controller);
  MusicMonitor *MM = CreateAubioMusicMonitor(MMH);
  AudioMonitor *AM = CreateOSXAudioMonitor(MM);

  AM->Start();

  //  sleep(2 * 60 * 60);
  SLC->MainLoop();

  AM->Stop();

  delete AM;

  return 0;
}

