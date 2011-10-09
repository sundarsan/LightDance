#include <stdio.h>
#include <phidget21.h>
#include <unistd.h>
#include <string.h>

int attach_handler(CPhidgetHandle IFK, void *data) {
  int serialNo;
  const char *name;

  CPhidget_getDeviceName(IFK, &name);
  CPhidget_getSerialNumber(IFK, &serialNo);

  fprintf(stderr, "%s %10d attached!\n", name, serialNo);

  return 0;
}

int detach_handler(CPhidgetHandle IFK, void *data) {
  int serialNo;
  const char *name;

  CPhidget_getDeviceName (IFK, &name);
  CPhidget_getSerialNumber(IFK, &serialNo);

  fprintf(stderr, "%s %10d detached!\n", name, serialNo);

  return 0;
}

int error_handler(CPhidgetHandle IFK, void *userptr, int ErrorCode,
                 const char *unknown) {
  fprintf(stderr, "error handled. %d - %s", ErrorCode, unknown);
  return 0;
}

int main(int argc, char* argv[]) {
  // Create the InterfaceKit object.
  CPhidgetInterfaceKitHandle ifKit = 0;
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
    return 0;
  }

  // Check some properties of the device.
  const char *device_type;
  int num_outputs;
  CPhidget_getDeviceType((CPhidgetHandle)ifKit, &device_type);

  CPhidgetInterfaceKit_getOutputCount(ifKit, &num_outputs);
  if ((strcmp(device_type, "PhidgetInterfaceKit") != 0)) {
    fprintf(stderr, "unexpected device type: %s\n", device_type);
    return 1;
  }
  if (num_outputs != 4) {
    fprintf(stderr, "unexpected number of device outputs: %d\n", num_outputs);
    return 1;
  }

  for (unsigned i = 0; i != 10000000; ++i) {
    CPhidgetInterfaceKit_setOutputState(ifKit, 0, (i % 200) < 100);
    CPhidgetInterfaceKit_setOutputState(ifKit, 2, (i % 200) >= 100);
    usleep(10000);
  }

  CPhidget_close((CPhidgetHandle)ifKit);
  CPhidget_delete((CPhidgetHandle)ifKit);

  return 0;
}

