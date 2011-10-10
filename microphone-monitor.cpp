#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <phidget21.h>
#include <sys/time.h>
#include <aubio/aubio.h>

///

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

class MusicMonitor {
public:
  enum BeatKind {
    kBeatLow = 0,
    kBeatHi
  };
  int which_light;
  double last_change_time;
  CPhidgetInterfaceKitHandle ifKit;
  bool switch_lights;

  /* Aubio Objects */
  fvec_t *od_ibuf, *od_onset, *od_onset2;
  cvec_t *od_fftgrain;
  aubio_onsetdetection_t *od_o, *od_o2;
  aubio_pickpeak_t *od_parms;
  aubio_onsetdetection_type od_type_onset, od_type_onset2;
  uint_t od_overlap_size, od_buffer_size, od_samplerate;
  smpl_t od_threshold;
  smpl_t od_silence;
  unsigned od_bufferpos, od_nframes;
  aubio_pvoc_t *od_pv;
  
  MusicMonitor(bool switch_lights_) : switch_lights(switch_lights_) {
    /* Create the Aubio objects. */
    int channels = 1;
    od_threshold = .7;
    od_silence = -70.0;
    od_samplerate = 44100;
    od_overlap_size = 256;
    od_buffer_size = 512;
    od_type_onset = aubio_onset_kl;
    od_type_onset2 = aubio_onset_complex;
    od_overlap_size = 256;
    od_ibuf = new_fvec(od_overlap_size, channels);
    od_onset = new_fvec(1, channels);
    od_onset2 = new_fvec(1, channels);
    od_fftgrain = new_cvec(od_buffer_size, channels);
    od_pv = new_aubio_pvoc(od_buffer_size, od_overlap_size, channels);
    od_parms = new_aubio_peakpicker(od_threshold);
    od_o = new_aubio_onsetdetection(od_type_onset, od_buffer_size, channels);
    od_o2 = new_aubio_onsetdetection(od_type_onset2, od_buffer_size, channels);
    od_bufferpos = 0;

    ///

    which_light = 0;
    last_change_time = -1;
    if (!switch_lights)
      return;

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

  void HandleSample(double time, double left, double right) {
    fvec_write_sample(od_ibuf, (left + right)*.5, 0, od_bufferpos++);
    ++od_nframes;

    double frame_time = (double) od_nframes / od_samplerate;
    if (od_bufferpos == od_overlap_size) {
      aubio_pvoc_do(od_pv, od_ibuf, od_fftgrain);
      aubio_onsetdetection(od_o, od_fftgrain, od_onset);
      if (true) {
        aubio_onsetdetection(od_o2, od_fftgrain, od_onset2);
        od_onset->data[0][0] *= od_onset2->data[0][0];
      }
      if (aubio_peakpick_pimrt(od_onset, od_parms)) {
        fprintf(stderr, "od_onset: %.4fs\n", (float) od_onset->data[0][0]);
        if (aubio_silence_detection(od_ibuf, od_silence) == 1) {
          ;
        } else {
          HandleBeat(MusicMonitor::kBeatLow, frame_time);
        }
      }

      od_bufferpos = 0;
    }
  }

  void HandleBeat(BeatKind kind, double time) {
    fprintf(stderr, "BeatKind: %d, %.4fs\n", kind, time);

    if (!switch_lights)
      return;

    if (time - last_change_time > 0.01) {
      last_change_time = time;
      which_light = !which_light;

      fprintf(stderr, "set light %d\n", which_light);
      CPhidgetInterfaceKit_setOutputState(ifKit, 0, which_light == 0);
      CPhidgetInterfaceKit_setOutputState(ifKit, 2, which_light == 1);
    }
  }
};

///

class DCAudioFileRecorder
{
  MusicMonitor &monitor;

public:
  DCAudioFileRecorder(MusicMonitor &monitor_);

  AudioBufferList *AllocateAudioBufferList(UInt32 numChannels, UInt32 size);
  void  DestroyAudioBufferList(AudioBufferList* list);
  OSStatus Configure();
  OSStatus Start();
  OSStatus Stop();

  AudioBufferList       *fAudioBuffer;
  AudioUnit     fAudioUnit;
protected:
  static OSStatus AudioInputProc(
                                 void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                 UInt32 inNumberFrames, AudioBufferList* ioData);

  AudioDeviceID fInputDeviceID;
  UInt32        fAudioChannels, fAudioSamples;
  AudioStreamBasicDescription   fOutputFormat, fDeviceFormat;
  FSRef fOutputDirectory;

};

#include <sys/param.h>

DCAudioFileRecorder::DCAudioFileRecorder(MusicMonitor &monitor_) :
  monitor(monitor_)
{
  fInputDeviceID = 0;
  fAudioChannels = fAudioSamples = 0;
}

// Convenience function to dispose of our audio buffers.
void DCAudioFileRecorder::DestroyAudioBufferList(AudioBufferList* list) {
  UInt32 i;

  if(list) {
    for(i = 0; i < list->mNumberBuffers; i++) {
      if(list->mBuffers[i].mData)
        free(list->mBuffers[i].mData);
    }
    free(list);
  }
}

// Convenience function to allocate our audio buffers.
AudioBufferList *DCAudioFileRecorder::AllocateAudioBufferList(
                                                              UInt32 numChannels, UInt32 size)
{
  AudioBufferList*                      list;
  UInt32                                                i;

  list = (AudioBufferList*)calloc(
                                  1, sizeof(AudioBufferList) + numChannels * sizeof(AudioBuffer));
  if (list == NULL)
    return NULL;

  list->mNumberBuffers = numChannels;
  for (i = 0; i < numChannels; ++i) {
    list->mBuffers[i].mNumberChannels = 1;
    list->mBuffers[i].mDataByteSize = size;
    list->mBuffers[i].mData = malloc(size);
    if(list->mBuffers[i].mData == NULL) {
      DestroyAudioBufferList(list);
      return NULL;
    }
  }
  return list;
}

OSStatus DCAudioFileRecorder::AudioInputProc(
                                             void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                             const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                             UInt32 inNumberFrames, AudioBufferList* ioData)
{
  DCAudioFileRecorder *afr = (DCAudioFileRecorder*)inRefCon;
  OSStatus      err = noErr;

  // Render into audio buffer.
  err = AudioUnitRender(afr->fAudioUnit, ioActionFlags, inTimeStamp,
                        inBusNumber, inNumberFrames, afr->fAudioBuffer);
  if(err)
    fprintf(stderr, "AudioUnitRender() failed with error %i\n", err);

  // Dump the data, for now.
  //  fprintf(stderr, "got data! (%d, %d, %d, %d)\n", afr->fAudioBuffer->mNumberBuffers, afr->fAudioBuffer->mBuffers[0].mNumberChannels, afr->fAudioBuffer->mBuffers[0].mDataByteSize, inNumberFrames);
  assert(afr->fAudioBuffer->mNumberBuffers == 2);
  AudioBuffer &left = afr->fAudioBuffer->mBuffers[0];
  AudioBuffer &right = afr->fAudioBuffer->mBuffers[1];
  assert(left.mDataByteSize == right.mDataByteSize);
  assert(left.mDataByteSize == inNumberFrames * sizeof(float));
  float *left_data = (float*) left.mData;
  float *right_data = (float*) right.mData;
  static double moving_avg = 0.;
  double in_beat = 0;
  for (unsigned i = 0; i != inNumberFrames; ++i) {
    double avg = fabs(left_data[i] + right_data[i]) * .5;
    double time = inTimeStamp->mSampleTime / afr->fOutputFormat.mSampleRate +
      (double) i / afr->fOutputFormat.mSampleRate;
    moving_avg = moving_avg * .1 + avg * .9;
    afr->monitor.HandleSample(time, left_data[i], right_data[i]);
    if (0)
      printf("%.4fs: (%+5.3f, %+5.3f): %5.3f\n", time, left_data[i], right_data[i],
             moving_avg);
    if (0) {
    if (moving_avg > .4) {
      if (!in_beat) {
        afr->monitor.HandleBeat(MusicMonitor::kBeatLow, time);
        in_beat = 1;
      }
    } else if (moving_avg < .1) {
      in_beat = 0;
    }
    }
  }

  return err;
}

OSStatus DCAudioFileRecorder::Configure() {
  Component                                     component;
  ComponentDescription          description;
  OSStatus      err = noErr;
  UInt32        param;
  AURenderCallbackStruct        callback;

  // Open the AudioOutputUnit
  // There are several different types of Audio Units.
  // Some audio units serve as Outputs, Mixers, or DSP
  // units. See AUComponent.h for listing
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_HALOutput;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;
  description.componentFlags = 0;
  description.componentFlagsMask = 0;
  if((component = FindNextComponent(NULL, &description)))
    {
      err = OpenAComponent(component, &fAudioUnit);
      if(err != noErr)
        {
          fAudioUnit = NULL;
          return err;
        }
    }

  // Configure the AudioOutputUnit: You must enable the Audio Unit (AUHAL) for
  // input and output for the same device.  When using AudioUnitSetProperty the
  // 4th parameter in the method refer to an AudioUnitElement.  When using an
  // AudioOutputUnit for input the element will be '1' and the output element
  // will be '0'.

  // Enable input on the AUHAL.
  param = 1;
  err = AudioUnitSetProperty(fAudioUnit, kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Input, 1, &param, sizeof(UInt32));
  if (err == noErr) {
    // Disable Output on the AUHAL
    param = 0;
    err = AudioUnitSetProperty(fAudioUnit, kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output, 0, &param,
                               sizeof(UInt32));
  }

  // Select the default input device
  param = sizeof(AudioDeviceID);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                 &param, &fInputDeviceID);
  if (err != noErr) {
    fprintf(stderr, "failed to get default input device\n");
    return err;
  }

  // Set the current device to the default input unit.
  err = AudioUnitSetProperty(fAudioUnit, kAudioOutputUnitProperty_CurrentDevice,
                             kAudioUnitScope_Global, 0, &fInputDeviceID,
                             sizeof(AudioDeviceID));
  if (err != noErr) {
    fprintf(stderr, "failed to set AU input device\n");
    return err;
  }

  // Setup render callback: This will be called when the AUHAL has input data.
  callback.inputProc = DCAudioFileRecorder::AudioInputProc;
  callback.inputProcRefCon = this;
  err = AudioUnitSetProperty(fAudioUnit,
                             kAudioOutputUnitProperty_SetInputCallback,
                             kAudioUnitScope_Global, 0, &callback,
                             sizeof(AURenderCallbackStruct));

  // Get hardware device format.
  param = sizeof(AudioStreamBasicDescription);
  err = AudioUnitGetProperty(fAudioUnit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input, 1, &fDeviceFormat, &param);
  if (err != noErr) {
    fprintf(stderr, "failed to get input device ASBD\n");
    return err;
  }

  // Twiddle the format to our liking.
  fAudioChannels = MAX(fDeviceFormat.mChannelsPerFrame, 2);
  fOutputFormat.mChannelsPerFrame = fAudioChannels;
  fOutputFormat.mSampleRate = fDeviceFormat.mSampleRate;
  fOutputFormat.mFormatID = kAudioFormatLinearPCM;
  fOutputFormat.mFormatFlags = kAudioFormatFlagIsFloat | \
    kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  if (fOutputFormat.mFormatID == kAudioFormatLinearPCM && fAudioChannels == 1)
    fOutputFormat.mFormatFlags &= ~kLinearPCMFormatFlagIsNonInterleaved;
  fOutputFormat.mBitsPerChannel = sizeof(Float32) * 8;
  fOutputFormat.mBytesPerFrame = fOutputFormat.mBitsPerChannel / 8;
  fOutputFormat.mFramesPerPacket = 1;
  fOutputFormat.mBytesPerPacket = fOutputFormat.mBytesPerFrame;

  // Set the AudioOutputUnit output data format.
  err = AudioUnitSetProperty(fAudioUnit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output, 1, &fOutputFormat,
                             sizeof(AudioStreamBasicDescription));
  if(err != noErr) {
    fprintf(stderr, "failed to set input device ASBD\n");
    return err;
  }

  // Get the number of frames in the IO buffer(s).
  param = sizeof(UInt32);
  err = AudioUnitGetProperty(fAudioUnit, kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Global, 0, &fAudioSamples, &param);
  if (err != noErr) {
    fprintf(stderr, "failed to get audio sample size\n");
    return err;
  }

  // Initialize the AU.
  err = AudioUnitInitialize(fAudioUnit);
  if (err != noErr) {
    fprintf(stderr, "failed to initialize AU\n");
    return err;
  }

  // Allocate our audio buffers.
  fAudioBuffer = AllocateAudioBufferList(fOutputFormat.mChannelsPerFrame,
                                         fAudioSamples *
                                         fOutputFormat.mBytesPerFrame);
  if (fAudioBuffer == NULL) {
    fprintf(stderr, "failed to allocate buffers\n");
    return err;
  }

  return noErr;
}

OSStatus DCAudioFileRecorder::Start() {
  // Start pulling for audio data.
  OSStatus err = AudioOutputUnitStart(fAudioUnit);
  if (err != noErr) {
    fprintf(stderr, "failed to start AU\n");
    return err;
  }

  fprintf(stderr, "recording started...\n");
  return err;
}

OSStatus DCAudioFileRecorder::Stop()
{
  // Stop pulling audio data
  OSStatus err = AudioOutputUnitStop(fAudioUnit);
  if (err != noErr) {
    fprintf(stderr, "failed to stop AU\n");
    return err;
  }

  fprintf(stderr, "recording stopped.\n");
  return err;
}

int main() {
  MusicMonitor Monitor(/*switch_lights=*/ true);
  DCAudioFileRecorder Recorder(Monitor);
  Recorder.Configure();

  Recorder.Start();
  sleep(2 * 60 * 60);
  Recorder.Stop();

  return 0;
}

