#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <phidget21.h>

static double get_time_in_seconds(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double) t.tv_sec + t.tv_usec * 1.e-6;
}

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

int please_exit = 0;
void catch_sigint(int signal) {
  please_exit = 1;
}

int main(int argc, char* argv[]) {
  // Set up a SIGINT handler.
  signal(SIGINT, catch_sigint);

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

  while (1) {
    double time = get_time_in_seconds();
    double factor = 1;
    int l0_state = fmod(time * factor, 20) < 10;
    int l1_state = fmod(time * factor, 20) >= 10;
    CPhidgetInterfaceKit_setOutputState(ifKit, 0, l0_state);
    CPhidgetInterfaceKit_setOutputState(ifKit, 2, l1_state);
    usleep(100000);
  }

  fprintf(stderr, "shutting down...\n");
  CPhidget_close((CPhidgetHandle)ifKit);
  CPhidget_delete((CPhidgetHandle)ifKit);

  return 0;
}

