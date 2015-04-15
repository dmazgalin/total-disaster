#include "AudioDecoder.h"


static void playback(float **buffers, int frame, sampletime_t now, float **input) {
  for(int i =0; i < 2; ++i) {
    buffers[i][frame] = input[i][frame];
  }
}

static int process(jack_nframes_t nframes, void *arg) {
  AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(arg);

  if (!decoder->resampled_buffer.empty()) {

    sampletime_t now;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    decoder->epoch_offset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0)) 
      - ((double) jack_get_time() / 1000000.0);
    //printf("jack time: %d tv_sec %d epochOffset: %f\n", jack_get_time(), tv.tv_sec, epochOffset);

    now = jack_last_frame_time(decoder->client);
  
    float** in = decoder->resampled_buffer.front();

    decoder->resampled_buffer.erase(decoder->resampled_buffer.begin());

    jack_default_audio_sample_t *out[decoder->channels+1];

    int i;

    for (i = 0; i < decoder->channels; ++i) {
      out[i] = (float*)(jack_port_get_buffer(decoder->output_ports[i], nframes));
    }

    for (i = 0; i < nframes; ++i) {
      playback(out, i, now + i, in);
    }


  }
  else {
    //    printf("Audio Buffer depleted: missing %d frames\n", nframes);
  }
  return 0;      

}

static void jack_shutdown(void *arg) {
  AudioDecoder* decoder = reinterpret_cast<AudioDecoder*>(arg);

  printf("jack shutdown\n");
  if (decoder->output_ports) free(decoder->output_ports);
  //exit(1);
}

AudioDecoder::AudioDecoder(int rate, int chan)
  : samplerate(rate)
  , channels(chan)
  , epoch_offset(0.0)
  , resampling_context(NULL)
  , jack_buffer_size(0)
{
  bool autoconnect = true;

  const char *client_name = "total-disaster";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  int i;
  char portname[16];

  resampling_context = avresample_alloc_context();
  av_opt_set_int(resampling_context, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
  av_opt_set_int(resampling_context, "out_channel_layout", AV_CH_LAYOUT_STEREO,0);
  av_opt_set_int(resampling_context, "in_samplerate", 44100,0);
  av_opt_set_int(resampling_context, "out_samplerate", 44100,0);
  av_opt_set_int(resampling_context, "in_sample_fmt", AV_SAMPLE_FMT_S16P,0);
  av_opt_set_int(resampling_context, "out_sample_fmt", AV_SAMPLE_FMT_FLTP,0);
  
  
  /* open a client connection to the JACK server */
  
  client = jack_client_open(client_name, options, &status, server_name);
  if (client == NULL) {
    fprintf(stderr, "jack_client_open() failed, "
	    "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf(stderr, "Unable to connect to JACK server\n");
    }
    exit(1);
  }
  if (status & JackServerStarted) {
    fprintf(stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf(stderr, "unique name `%s' assigned\n", client_name);
  }
  
  jack_set_process_callback(client, process, (void *) this);
  
  jack_on_shutdown(client, jack_shutdown, 0);

  printf("engine sample rate: %" PRIu32 "\n",
	 jack_get_sample_rate(client));


  output_ports = (jack_port_t**)(malloc((channels + 1) * sizeof(jack_port_t*)));
  if (!output_ports) {
    fprintf(stderr, "no memory to allocate `output_ports'\n");
    exit(1);
  }

  for (i = 0; i < channels; ++i) {
    sprintf(portname, "output_%d", i);
    output_ports[i] = jack_port_register(client, portname,
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);
    if (output_ports[i] == NULL) {
      fprintf(stderr, "no more JACK ports available\n");
      if (output_ports) free(output_ports);
      exit(1);
    }
  }
  
  output_ports[channels] = NULL;
  
  if (jack_activate(client)) {
    fprintf(stderr, "cannot activate client");
    if (output_ports) free(output_ports);
    exit(1);
  }

  if (autoconnect) {
    const char **ports;
    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsInput);
    for (i = 0; i < channels; ++i) {
      if (ports[i] == NULL) {
        break;
      }
      //sprintf(portname, "output_%d", i);
      if (jack_connect(client, jack_port_name(output_ports[i]), ports[i])) {
        fprintf(stderr, "cannot connect output ports\n");
      }
    }
    free(ports);
  }

  jack_buffer_size = jack_get_buffer_size(client);
}

AudioDecoder::~AudioDecoder() {
}


void AudioDecoder::resample() {
  if (!buffer.empty()) {
    std::pair<AVFrame*, int> pair = buffer.front();

    buffer.erase(buffer.begin());

    int res = avresample_convert_frame(resampling_context, NULL, pair.first);

    if (res == 0) {
      int rem = avresample_available(resampling_context);

      while (rem >= jack_buffer_size) {
	uint8_t** frag = (uint8_t**)calloc(channels, sizeof(uint8_t));

	int alloc_res = av_samples_alloc(frag, NULL, 2, jack_buffer_size, AV_SAMPLE_FMT_FLTP, 0);
	if (alloc_res < 0) {
	  printf("Error allocating sample buffer %d", alloc_res);
	  rem = 0; // stop resampling this frame
	}
	else {
	  rem = avresample_read(resampling_context, frag, jack_buffer_size);

	  resampled_buffer.push_back((float**)frag);
	}
      }
    }
    else {
      printf("Error: resampling frame %d\n", pair.second);
    }
  }
}
