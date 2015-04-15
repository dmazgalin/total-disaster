/*

  H264_Decoder
  ---------------------------------------

  Example that shows how to use the libav parser system. This class forces a
  H264 parser and codec. You use it by opening a file that is encoded with x264
  using the `load()` function. You can pass the framerate you want to use for playback.
  If you don't pass the framerate, we will detect it as soon as the parser found
  the correct information.

  After calling load(), you can call readFrame() which will read a new frame when
  necessary. It will also make sure that it will read enough data from the buffer/file
  when there is not enough data in the buffer.

  `readFrame()` will trigger calls to the given `h264_decoder_callback` that you pass
  to the constructor.

*/
#ifndef H264_DECODER_H
#define H264_DECODER_H

#define H264_INBUF_SIZE 16384                                                           /* number of bytes we read per chunk */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <tinylib.h>

#include "AudioDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavresample/avresample.h>
}

typedef void(*h264_decoder_callback)(AVFrame* frame, AVPacket* pkt, void* user);         /* the decoder callback, which will be called when we have decoded a frame */

class H264_Decoder {

 public:
  H264_Decoder(h264_decoder_callback frameCallback, void* user);                         /* pass in a callback function that is called whenever we decoded a video frame, make sure to call `readFrame()` repeatedly */
  ~H264_Decoder();                                                                       /* d'tor, cleans up the allocated objects and closes the codec context */
  bool load(std::string filepath, float fps = 0.0f);                                     /* load a video file which is encoded with x264 */
  bool readFrame();                                                                      /* read a frame if necessary */
  int seekTo(double ts);

 private:
  bool update(bool& needsMoreBytes);                                                     /* internally used to update/parse the data we read from the buffer or file */
  int readBuffer();                                                                      /* read a bit more data from the buffer */
  void decodeFrame();                                             /* decode a frame we read from the buffer */
  void decodeAudioFrame();

 public:
  AVCodec* vcodec;                                                                        /* the AVCodec* which represents the H264 decoder */
  AVCodec* acodec;                                                                        /* the AVCodec* which represents the Audio decoder */
  AVCodecContext* vcodec_context;                                                         /* the video context; keeps generic state */
  AVCodecContext* acodec_context;                                                         /* the audio context; keeps generic state */ 
  AVFormatContext* format_context;
  AVCodecParserContext* parser;                                                          /* parser that is used to decode the h264 bitstream */
  AVStream* video_stream;
  AVStream* audio_stream;
  AVFrame* picture;                                                                      /* will contain a decoded picture */
  uint8_t inbuf[H264_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];                         /* used to read chunks from the file */
  FILE* fp;                                                                              /* file pointer to the file from which we read the h264 data */
  int frame;                                                                             /* the number of decoded frames */
  int audio_frame;
  AVFrame* audio_decoded_frame;
  h264_decoder_callback cb_frame;                                                        /* the callback function which will receive the frame/packet data */
  void* cb_user;                                                                         /* the void* with user data that is passed into the set callback */
  uint64_t frame_timeout;                                                                /* timeout when we need to parse a new frame */
  uint64_t frame_delay;                                                                  /* delay between frames (in ns) */
  std::vector<uint8_t> buffer;                                                           /* buffer we use to keep track of read/unused bitstream data */
  std::vector<std::pair<AVPacket, int>> video_buffer;
  std::vector<std::pair<AVPacket, int>> audio_buffer;

  AudioDecoder* audio_decoder;
  AVAudioResampleContext* aresample_context;
  char errbuf[128];

};

#endif
