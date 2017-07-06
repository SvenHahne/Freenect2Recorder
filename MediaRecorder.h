/*
 * MediaRecorder.h
 *
 *  Created on: 29.06.2016
 *      Author: sven
 */

#ifndef FFMPEG_MEDIARECORDER_H_
#define FFMPEG_MEDIARECORDER_H_

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <math.h>
#include "PAudio.h"
#include <gl_header.h>
#include <GLFW/glfw3.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#ifndef AV_PKT_FLAG_KEY
#define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif
}


#ifdef  __linux__
#ifdef  __cplusplus

static const std::string av_make_error_string(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return (std::string)errbuf;
}

#undef av_err2str
#define av_err2str(errnum) av_make_error_string(errnum).c_str()

#endif // __cplusplus
#endif // __linux__


namespace tav
{

class MediaRecorder
{
public:
	// a wrapper around a single output AVStream
	typedef struct OutputStream {
	    AVStream *st;
	    AVCodecContext *enc;

	    /* pts of the next frame that will be generated */
	    int64_t next_pts;
	    int samples_count;

	    AVFrame *frame;
	    AVFrame *tmp_frame;

	    float t, tincr, tincr2;

	    struct SwsContext *sws_ctx;
	    struct SwrContext *swr_ctx;
	} OutputStream;

	MediaRecorder(const char* _fname, PAudio* _pa, int _inWidth, int _inHeight, int _monRefRate,
			const char* _forceCodec=0);
	virtual ~MediaRecorder();

	void record();
	void stop();
	virtual void recThread();

	void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
	int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);
	void add_stream(MediaRecorder::OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);
	AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate,
			int nb_samples);
	void open_audio(AVFormatContext *oc, AVCodec *codec, MediaRecorder::OutputStream *ost, AVDictionary *opt_arg);
	AVFrame *get_audio_frame(OutputStream *ost, bool clear);
	int write_audio_frame(AVFormatContext *oc, MediaRecorder::OutputStream *ost,  bool clear);
	void open_video(AVFormatContext *oc, AVCodec *codec, MediaRecorder::OutputStream *ost, AVDictionary *opt_arg);
	AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
	AVFrame *get_video_frame(MediaRecorder::OutputStream *ost);
	int write_video_frame(AVFormatContext *oc, MediaRecorder::OutputStream *ost);
	void close_stream(AVFormatContext *oc, MediaRecorder::OutputStream *ost);


/*
	AVStream* add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);
	void open_audio(AVFormatContext *oc, AVCodec *codec, AVStream *st);
	bool get_audio_frame(int16_t *samples, int frame_size, int nb_channels, bool clear);
	//void write_audio_frame(AVFormatContext *oc, AVStream *st, bool clear);
	void close_audio(AVFormatContext *oc, AVStream *st);
	void open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st);
	void write_video_frame(AVFormatContext *oc, AVStream *st);
	void close_video(AVFormatContext *oc, AVStream *st);
	int init_filters(const char *filters_descr);
*/
	void downloadFrame();
	void downloadFrame(unsigned char* bufPtr);

	static void getMediaRecAudioDataCallback(unsigned int numChans, int frames, int offset,
			int codecFrameSize, unsigned int codecNrChans, std::vector< std::vector<unsigned int> >* mixDownMap,
			float** sampData, std::vector<float>* inSampQ);
	void myNanoSleep(uint32_t ns);

	bool isRecording();
	void setOutputSize(unsigned int _width, unsigned int _height);
	void setOutputFramerate(unsigned int _frameRate);
	void setVideoBitRate(unsigned int _bitRate);
	void setAudioBitRate(unsigned int _bitRate);
	int getOutputWidth();
	int getOutputHeight();
private:
	PAudio* 				pa;

    const char*				fileName;
    const char*				fileType;
    const char*				forceCodec;

    AVDictionary*			opt=NULL;
    AVOutputFormat*			fmt;
    AVFormatContext*		oc;
	AVFilterGraph*			filter_graph;
	AVFilterContext*		buffersink_ctx;
	AVFilterContext*		buffersrc_ctx;
	SwsContext*				converter=NULL;	// muss unbedingt auf NULL initialisiert werden!!!
    AVStream*				audio_st=NULL;
    AVStream*				video_st=NULL;
    AVCodec*				audio_codec;
    AVCodec*				video_codec;
    double 					audio_time, video_time;
    int 					ret;

    uint8_t**				src_samples_data;
    int       				src_samples_linesize;
    int       				src_nb_samples;
    int 					max_dst_nb_samples;
    uint8_t**				dst_samples_data;
    int       				dst_samples_linesize;
    int       				dst_samples_size;
    struct SwrContext*		swr_ctx = NULL;
	std::vector<float> 		audioQueue;

    AVFrame*				frame;
	AVFrame*				pFrameRGB;
	AVFrame*				filt_frame;
    AVPicture 				src_picture, dst_picture;

	uint8_t** 				buffer;

    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;

    int						renderFrameNr=0;
    int						recFrameInt = 1;
    int						bufferReadPtr;
    int						skippedFrames=0;
    int						recFrameRate=30;
    int						monRefRate;

    int			 			inWidth;
	int 					inHeight;
	int 					outWidth=640;
	int		 				outHeight=360;

    int						audioBitRate=64000;
    int						videoBitRate=400000;

    unsigned int			nrBufferFrames;

    unsigned int			writeVideoFrame = 0;
    unsigned int			nrReadVideoFrames = 0;
	unsigned int 			glNrBytesPerPixel=3;

    AVPixelFormat			glFormat;

    bool					doRec = false;
    bool					useFiltering = false;
    bool					flipH = false;

    boost::thread*	 		m_Thread;
    boost::mutex	 		writeMutex;

//--------------------------------------------------------
    MediaRecorder::OutputStream n_video_st;
    MediaRecorder::OutputStream n_audio_st;

    std::vector< std::vector<unsigned int> > mixDownMap;
    unsigned int 			nrMixChans;
    unsigned char* 			ptr;

    GLenum 					glDownloadFmt;
    GLuint* 				pbos;
    uint64_t 				num_pbos;
    uint64_t 				dx=0;
    uint64_t 				num_downloads=0;

    int 					nbytes; /* number of bytes in the pbo buffer. */
    int						show =0;
};

} /* namespace tav */

#endif /* FFMPEG_MEDIARECORDER_H_ */
