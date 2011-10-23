#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <sys/time.h>
#include <unistd.h>

#include "AudioMonitor.h"
#include "LightInfo.h"
#include "LightManager.h"
#include "MusicMonitor.h"
#include "SimLightController.h"
#include "Util.h"

///

class LoggingAudioHandler : public AudioMonitorHandler {
public:
  virtual void HandleSample(double time, double left, double right) {
    static int count;
    if ((++count % 10000) == 0)
      fprintf(stderr, "%.2fs, %.2fs: (%.2f, %.2f)\n",
              get_elapsed_time_in_seconds(), time, left, right);
  }
};

class LoggingMusicHandler : public MusicMonitorHandler {
  FILE *fp;
  MusicMonitorHandler *Chain;

public:
  LoggingMusicHandler(const char *OutputPath, MusicMonitorHandler *Chain_)
    : fp(0), Chain(Chain_)
  {
    if (std::string(OutputPath) == "-") {
      fp = stdout;
    } else {
      fp = fopen(OutputPath, "w");
      if (!fp) {
        fprintf(stderr, "unable to open: %s\n", OutputPath);
        exit(1);
      }
    }
  }
  ~LoggingMusicHandler() {
    if (fp != stdout)
      fclose(fp);
    delete Chain;
  }

  virtual void HandleBeat(BeatKind kind, double time) {
    Chain->HandleBeat(kind, time);
    fprintf(fp, "BeatKind: %d, Time: %.4fs, CurrentTime: %.4fs\n", kind, time,
            get_elapsed_time_in_seconds());
    fflush(fp);
  }
};

int main(int argc, char **argv) {
  bool SwitchLights = false;
  const char *LogBeats = 0;

  for (int i = 1; i != argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--switch-lights") {
      SwitchLights = true;
    } else if (arg == "--log-beats") {
      if (++i == argc) {
        fprintf(stderr, "%s: missing argument to: %s\n", argv[0], arg.c_str());
        return 1;
      }
      LogBeats = argv[i];
    } else {
      fprintf(stderr, "%s: unknown argument: %s\n", argv[0], arg.c_str());
      return 1;
    }
  }

  // We just hard code the light configuration for now.
  std::vector<LightInfo> LightSetup;
  LightSetup.push_back(LightInfo::Make(LightInfo::kLightKind_Pinspot,
                                       LightInfo::kLightColor_Red,
                                       /*Index=*/0));
  LightSetup.push_back(LightInfo::Make(LightInfo::kLightKind_Pinspot,
                                       LightInfo::kLightColor_Green,
                                       /*Index=*/2));

  // Create the light controller.
  SimLightController *SLC = CreateSimLightController();
  LightController *controller = SLC;

  if (SwitchLights)
    controller = CreateChainedLightController(SLC,
                                              CreatePhidgetLightController());

  // Create the light manager as our handler.
  MusicMonitorHandler *MMH = CreateLightManager(controller, LightSetup);

  if (LogBeats)
    MMH = new LoggingMusicHandler(LogBeats, MMH);

  MusicMonitor *MM = CreateAubioMusicMonitor(MMH);
  AudioMonitor *AM = CreateOSXAudioMonitor(MM);

  AM->Start();

  //  sleep(2 * 60 * 60);
  SLC->MainLoop();

  AM->Stop();

  delete AM;

  return 0;
}

