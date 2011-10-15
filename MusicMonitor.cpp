#include <stdio.h>

#include <aubio/aubio.h>

#include "MusicMonitor.h"

MusicMonitorHandler::MusicMonitorHandler() {}
MusicMonitorHandler::~MusicMonitorHandler() {}

MusicMonitor::MusicMonitor() {}
MusicMonitor::~MusicMonitor() {}

namespace {

class AubioMusicMonitor : public MusicMonitor {
  MusicMonitorHandler *handler;

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

public:
  AubioMusicMonitor(MusicMonitorHandler *handler_)
    : handler(handler_)
  {
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
  }

  virtual ~AubioMusicMonitor() {
    delete handler;
  }

  virtual void HandleSample(double time, double left, double right) {
    fvec_write_sample(od_ibuf, (left + right)*.5, 0, od_bufferpos++);
    ++od_nframes;

    if (od_bufferpos != od_overlap_size)
      return;

    double frame_time = (double) od_nframes / od_samplerate;
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
        handler->HandleBeat(MusicMonitorHandler::kBeatLow, frame_time);
      }
    }

    od_bufferpos = 0;
  }
};

}

MusicMonitor *CreateAubioMusicMonitor(MusicMonitorHandler *handler) {
  return new AubioMusicMonitor(handler);
}

