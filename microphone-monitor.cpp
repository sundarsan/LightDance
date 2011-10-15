#include <stdio.h>
#include <sys/time.h>
#include <phidget21.h>
#include <unistd.h>
#include <string.h>

#include "AudioMonitor.h"
#include "MusicMonitor.h"

///

static double get_time_in_seconds(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double) t.tv_sec + t.tv_usec * 1.e-6;
}

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

class SimpleLightSwitcher : public MusicMonitorHandler {
  int which_light;
  double last_change_time;
  CPhidgetInterfaceKitHandle ifKit;

  static int attach_handler(CPhidgetHandle IFK, void *data) {
    int serialNo;
    const char *name;

    CPhidget_getDeviceName(IFK, &name);
    CPhidget_getSerialNumber(IFK, &serialNo);

    fprintf(stderr, "%s %10d attached!\n", name, serialNo);

    return 0;
  }

  static int detach_handler(CPhidgetHandle IFK, void *data) {
    int serialNo;
    const char *name;

    CPhidget_getDeviceName (IFK, &name);
    CPhidget_getSerialNumber(IFK, &serialNo);

    fprintf(stderr, "%s %10d detached!\n", name, serialNo);

    return 0;
  }

  static int error_handler(CPhidgetHandle IFK, void *userptr, int ErrorCode,
                           const char *unknown) {
    fprintf(stderr, "error handled. %d - %s", ErrorCode, unknown);
    return 0;
  }

public:
  SimpleLightSwitcher() {
    which_light = 0;
    last_change_time = -1;

    // Initialize the phidget interfaces.
    // Create the InterfaceKit object.
    CPhidgetInterfaceKit_create(&ifKit);

    // Register device handlers.
    CPhidget_set_OnAttach_Handler((CPhidgetHandle)ifKit, attach_handler, NULL);
    CPhidget_set_OnDetach_Handler((CPhidgetHandle)ifKit, detach_handler, NULL);
    CPhidget_set_OnError_Handler((CPhidgetHandle)ifKit, error_handler, NULL);

    // Open the interfacekit for device connections.
    CPhidget_open((CPhidgetHandle)ifKit, -1);

    // Wait for a device attachment.
    fprintf(stderr, "waiting for interface kit to be attached....\n");
    int result;
    const char *err;
    if ((result = CPhidget_waitForAttachment((CPhidgetHandle)ifKit, 10000))) {
      CPhidget_getErrorDescription(result, &err);
      fprintf(stderr, "problem waiting for attachment: %s\n", err);
      return;
    }

    // Check some properties of the device.
    const char *device_type;
    int num_outputs;
    CPhidget_getDeviceType((CPhidgetHandle)ifKit, &device_type);

    CPhidgetInterfaceKit_getOutputCount(ifKit, &num_outputs);
    if ((strcmp(device_type, "PhidgetInterfaceKit") != 0)) {
      fprintf(stderr, "unexpected device type: %s\n", device_type);
      return;
    }
    if (num_outputs != 4) {
      fprintf(stderr, "unexpected number of device outputs: %d\n", num_outputs);
      return;
    }
  }

  virtual void HandleBeat(BeatKind kind, double time) {
    fprintf(stderr, "BeatKind: %d, %.4fs\n", kind, time);
    if (time - last_change_time > 0.01) {
      last_change_time = time;
      which_light = !which_light;

      fprintf(stderr, "set light %d\n", which_light);
      CPhidgetInterfaceKit_setOutputState(ifKit, 0, which_light == 0);
      CPhidgetInterfaceKit_setOutputState(ifKit, 2, which_light == 1);
    }
  }
};

int main() {
  //  MusicMonitorHandler *MMH = new SimpleLightSwitch;
  MusicMonitorHandler *MMH = new LoggingMusicHandler;
  MusicMonitor *MM = CreateAubioMusicMonitor(MMH);
  AudioMonitor *AM = CreateOSXAudioMonitor(MM);

  AM->Start();
  sleep(2 * 60 * 60);
  AM->Stop();

  delete AM;

  return 0;
}

