#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#define PA_FRAMES_PER_BUFFER 1024

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <portaudio.h>
#include <jack/jack.h>

extern "C" {
#include <libavcodec/avcodec.h>
  #include <libavresample/avresample.h>
  #include <libavutil/opt.h>
}
  
#define sampletime_t double

class AudioDecoder {
 public:
  AudioDecoder(int rate, int chan);
  ~AudioDecoder();
  void playback(float **buffers, int frame, sampletime_t now, float** input);
  void resample();
    
  jack_client_t *client;
  jack_port_t **output_ports;

  AVAudioResampleContext* resampling_context;
  std::vector<float**> resampled_buffer;
  int samplerate;
  jack_nframes_t jack_buffer_size;
  int channels;
  double epoch_offset;
  std::vector<std::pair<AVFrame*,int>> buffer; 
};

#endif
