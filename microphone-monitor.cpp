#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

class DCAudioFileRecorder
{
public:
  DCAudioFileRecorder();

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

DCAudioFileRecorder::DCAudioFileRecorder() {
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

  fprintf(stderr, "got data!\n");

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
  DCAudioFileRecorder Recorder;
  Recorder.Configure();

  Recorder.Start();
  sleep(1);
  Recorder.Stop();

  return 0;
}

