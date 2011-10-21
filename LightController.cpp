#include <stdio.h>
#include <string.h>
#include <phidget21.h>

#include "LightController.h"

LightController::LightController() {}
LightController::~LightController() {}

namespace {

class PhidgetLightController : public LightController {
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
  PhidgetLightController() {
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
  ~PhidgetLightController() {
  }

  virtual void BeatNotification(unsigned Index, double Time) {
  }

  virtual void SetLight(unsigned Index, bool Enable) {
    CPhidgetInterfaceKit_setOutputState(ifKit, Index, Enable);
  }
};

class ChainedLightController : public LightController {
  LightController *a, *b;

public:
  ChainedLightController(LightController *a_, LightController *b_)
    : a(a_), b(b_) {}
  ~ChainedLightController() {
    delete a;
    delete b;
  }

  virtual void BeatNotification(unsigned Index, double Time) {
    a->BeatNotification(Index, Time);
    b->BeatNotification(Index, Time);
  }

  virtual void SetLight(unsigned Index, bool Enable) {
    a->SetLight(Index, Enable);
    b->SetLight(Index, Enable);
  }
};

}

LightController *CreatePhidgetLightController() {
  return new PhidgetLightController();
}

LightController *CreateChainedLightController(LightController *a,
                                              LightController *b) {
  return new ChainedLightController(a, b);
}

