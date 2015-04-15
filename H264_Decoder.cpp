#include "H264_Decoder.h"
#include <vector>

H264_Decoder::H264_Decoder(h264_decoder_callback frameCallback, void* user)
  :vcodec(NULL)
  ,acodec(NULL)
  ,vcodec_context(NULL)
  ,acodec_context(NULL)
  ,format_context(NULL)
  ,video_stream(NULL)
  ,parser(NULL)
  ,fp(NULL)
  ,frame(0)
  ,audio_frame(0)
  ,cb_frame(frameCallback)
  ,cb_user(user)
  ,frame_timeout(0)
  ,frame_delay(0)
  ,audio_decoder()
{
  av_register_all();
  avcodec_register_all();
}

H264_Decoder::~H264_Decoder() {

  if(parser) {
    av_parser_close(parser);
    parser = NULL;
  }

  if(vcodec_context) {
    avcodec_close(vcodec_context);
    av_free(vcodec_context);
    vcodec_context = NULL;
  }

  if(format_context) {
    avformat_free_context(format_context);
    format_context = NULL;
  }

  if(picture) {
    av_free(picture);
    picture = NULL;
  }

  if(fp) {
    fclose(fp);
    fp = NULL;
  }

  cb_frame = NULL;
  cb_user = NULL;
  frame = 0;
  frame_timeout = 0;
}

bool H264_Decoder::load(std::string filepath, float fps) {
  format_context = avformat_alloc_context();

  if (avformat_open_input(&format_context, filepath.c_str(), nullptr, nullptr) != 0) {
    printf("Error: could not open input file.\n");
    return false;
  }

  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    printf("Error: could not find stream info.\n");
    return false;
  }

  for (auto i = 0; i < format_context->nb_streams; ++i) {
    if (format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream = format_context->streams[i];
    }
    if (format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream = format_context->streams[i];
    }
  }

  if (!video_stream) {
    printf("Error: no video stream found. Maybe this is an audio only file?\n");
    return false;
  }

  if (!audio_stream) {
    printf("Error: no audio stream found. Maybe this is an video only file?\n");
    return false;
  }

  vcodec = avcodec_find_decoder(video_stream->codec->codec_id);
  if(!vcodec) {
    printf("Error: cannot find the h264 codec: %s\n", filepath.c_str());
    return false;
  }

  vcodec_context = avcodec_alloc_context3(vcodec);

  if (!video_stream->codec->extradata_size && vcodec_context->extradata_size) {
       video_stream->codec->extradata = (uint8_t *)av_mallocz(vcodec_context->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
       if (video_stream->codec->extradata) {
           memcpy(video_stream->codec->extradata, vcodec_context->extradata, vcodec_context->extradata_size);
           video_stream->codec->extradata_size = vcodec_context->extradata_size;
       }
   }

  acodec = avcodec_find_decoder(audio_stream->codec->codec_id);

  if (!acodec) {
    printf("Error: cannot find audio codec: %s\n", filepath.c_str());
    return false;
  }



  // this does not work well with mpeg2 movies
  // if(vcodec->capabilities & CODEC_CAP_TRUNCATED) {
  //   vcodec_context->flags |= CODEC_FLAG_TRUNCATED;
  // }

  if(avcodec_open2(vcodec_context, vcodec, NULL) < 0) {
    printf("Error: could not open codec.\n");
    return false;
  }


  acodec_context = avcodec_alloc_context3(acodec);

  if(avcodec_open2(acodec_context, acodec, NULL) < 0) {
    printf("Error: could not open codec. \n");
    return false;
  }

  const char* sample_fmt = av_get_sample_fmt_name(acodec_context->sample_fmt);

  printf("Sample Format: %s\n", sample_fmt);
  
  audio_decoder = new AudioDecoder(44100, 2);
  
  fp = fopen(filepath.c_str(), "rb");

  if(!fp) {
    printf("Error: cannot open: %s\n", filepath.c_str());
    return false;
  }

  picture = av_frame_alloc();
  audio_decoded_frame = av_frame_alloc();
  parser = av_parser_init(video_stream->codec->codec_id);

  if(!parser) {
    printf("Erorr: cannot create H264 parser.\n");
    return false;
  }


  if(fps > 0.0001f) {
    frame_delay = (1.0f/fps) * 1000ull * 1000ull * 1000ull;
    frame_timeout = rx_hrtime() + frame_delay;
  }

  return true;
}

bool H264_Decoder::readFrame() {
  uint64_t now = rx_hrtime();

  AVPacket pkt;
  av_init_packet(&pkt);

  pkt.data = NULL;
  pkt.size = 0;

  int frames_read = av_read_frame(format_context, &pkt);

  if (frames_read >= 0) {
    if (pkt.stream_index == video_stream->index) {
      video_buffer.push_back(std::make_pair(pkt, frame++));
    }
    else {
      audio_buffer.push_back(std::make_pair(pkt, audio_frame++));
    }
  }
  else {
    int res = seekTo(0.1);
    printf("Looping %d, starting audio\n", res);
  }

  if(now >= frame_timeout) {
    // it may take some 'reads' before we can set the fps
    if(frame_timeout == 0 && frame_delay == 0) {
      double fps = av_q2d(vcodec_context->time_base);

      if(fps > 0.0) {
  	frame_delay = fps * 1000ull * 1000ull * 1000ull;
      }
    }

    if ((now - frame_timeout) > frame_delay) {
      uint64_t time_since_last_frame = (now - frame_timeout);
      printf("Late Frame rendering: %llu\n", time_since_last_frame);
    }

    if(frame_delay > 0) {       
      frame_timeout = rx_hrtime() + frame_delay;
    }
    
    decodeFrame();
  }

  decodeAudioFrame();
  
  return true;
}

int H264_Decoder::seekTo(double s) {
  printf("Seeking to %fs\n", s);
  int ts = (s*(video_stream->time_base.den))/(video_stream->time_base.num);
  int res = av_seek_frame(format_context, video_stream->index, ts, AVSEEK_FLAG_ANY);
  if (res < 0) {
    av_strerror(res, errbuf, 128);
    printf("Error: Seek failed %s\n", errbuf);
  }
  avcodec_flush_buffers(vcodec_context);
  return res;
}

void H264_Decoder::decodeAudioFrame() {
  int got_audio_frame = 0;

  int len;
  if (!audio_buffer.empty()) {
    std::pair<AVPacket, int> apair = audio_buffer.front();
    audio_buffer.erase(audio_buffer.begin());
    AVPacket pkt = apair.first;

    len = avcodec_decode_audio4(acodec_context, audio_decoded_frame, &got_audio_frame, &pkt);

    if (len < 0) {
      printf("Error: decoding audio packet failed: %d", len);
    }
    else {
      if (got_audio_frame == 0) {
	printf("Error: decoding audio packet missing frame data");
      }
      else {
	audio_decoder->buffer.push_back(std::make_pair(audio_decoded_frame, audio_frame));
	audio_decoder->resample();
      }
    }
  }
}

void H264_Decoder::decodeFrame() {
  int got_picture = 0;
  int len = 0;
  if (!video_buffer.empty()) {
    std::pair<AVPacket, int> vpair = video_buffer.front();
    AVPacket pkt = vpair.first;
    
    video_buffer.erase(video_buffer.begin());
    printf("Decoding (%d|%lu)\n", vpair.second, video_buffer.size());
  
    len = avcodec_decode_video2(vcodec_context, picture, &got_picture, &pkt);
    if(len < 0) {
      printf("Decoding failed with error. %d %d.\n", len, got_picture);
    }

    if(got_picture != 0) {
      if(cb_frame) {
	cb_frame(picture, &pkt, cb_user);
      }
    }

    av_free_packet(&pkt);
  }
}

int H264_Decoder::readBuffer() {
  return 0;
}

bool H264_Decoder::update(bool& needsMoreBytes) {
  needsMoreBytes = false;

  if(!fp) {
    printf("Cannot update .. file not opened...\n");
    return false;
  }

  if(buffer.size() == 0) {
    needsMoreBytes = true;
    return false;
  }

  uint8_t* data = NULL;
  int size = 0;
  int len = av_parser_parse2(parser, vcodec_context, &data, &size,
                             &buffer[0], buffer.size(), 0, 0, AV_NOPTS_VALUE);

  if(size == 0 && len >= 0) {
    needsMoreBytes = true;
    return false;
  }

  if(len) {
    //decodeFrame(&buffer[0], size);
    buffer.erase(buffer.begin(), buffer.begin() + len);
    return true;
  }

  return false;
}
