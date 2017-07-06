/*
 * MediaRecorder.cpp
 *
 *  Created on: 29.06.2016
 *      Author: sven
 *
 *      bei .avi stimmt wird die gesamtlaenge nicht richtig geschrieben...
 */
//
#include "MediaRecorder.h"

namespace tav
{

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC

MediaRecorder::MediaRecorder(const char* _fname, PAudio* _pa, int _inWidth, int _inHeight,
		int _monRefRate, const char* _forceCodec):
		inWidth(_inWidth), inHeight(_inHeight), monRefRate(_monRefRate), fileName(_fname),
		nrBufferFrames(40), bufferReadPtr(-1), pa(_pa), forceCodec(_forceCodec),
		num_pbos(4), glDownloadFmt(GL_BGRA), flipH(false)
{

	// init Pixel Buffer Objectss
	switch(glDownloadFmt)
	{
	case GL_RGB:
		glNrBytesPerPixel = 3;
		glFormat = AV_PIX_FMT_RGB24; // convert GL Format to FFmpeg format
		break;
	case GL_BGR:
		glNrBytesPerPixel = 3;
		glFormat = AV_PIX_FMT_BGR24;
		break;
	case GL_RGBA:
		glNrBytesPerPixel = 4;
		glFormat = AV_PIX_FMT_RGBA;
		break;
	case GL_BGRA:
		glFormat = AV_PIX_FMT_BGRA;
		glNrBytesPerPixel = 4;
		break;
	default :
		glFormat = AV_PIX_FMT_BGR24;
		glNrBytesPerPixel = 3;
		std::cerr << "MediaRecorder Unhandled pixel format, use GL_BGRA, GL_RGBA, GL_RGB or GL_BGR." << std::endl;
		break;
	}

	// kinect rgb frames seem to have 4 bytes per pixel
	nbytes = _inWidth * _inHeight * glNrBytesPerPixel;

	// we have to convert 4 byte / pixel to 3 byte / pixel
	//useFiltering = true;

    // create buffers for downloading images from opengl
	buffer = new uint8_t*[nrBufferFrames];
	for (int i=0; i<nrBufferFrames; i++)
		buffer[i] = new uint8_t[ nbytes ];

	/*
	pbos = new GLuint[num_pbos];
	glGenBuffers(num_pbos, pbos);

	for (int i=0; i<num_pbos; ++i)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, nbytes, NULL, GL_STREAM_READ);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	*/
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::record()
{
	AVCodec* cdc;

    n_video_st = { 0 };
    n_audio_st = { 0 };

    // Initialize libavcodec, and register all codecs and formats.
    av_register_all();
//	avcodec_register_all(); // av_register_all ruft dass auch auf...
//	avfilter_register_all();

	std::string fn(fileName);
	fileType = fn.substr(fn.find_last_of(".") + 1).c_str();

	printf("MediaRecorder recording to %s \n", fileName);
	printf("ending: %s \n", fileType);

	if ( forceCodec )
	{
		cdc = avcodec_find_encoder_by_name(forceCodec);
		if (!cdc) std::cerr << "MediaRecorder error: couldn't find encoder: " << forceCodec << std::endl;
	}

	avformat_alloc_output_context2(&oc, NULL, NULL, fileName);

    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", fileName);
    }

    if (!oc) std::cerr << "MediaRecorder error: Could create context." << std::endl;

    fmt = oc->oformat;

    // Add the audio and video streams using the default format codecs
    // and initialize the codecs.
    if (fmt->video_codec != AV_CODEC_ID_NONE)
    {
    	if (forceCodec != 0)
    	{
        	std::cout << " using codec: " << cdc->name << std::endl;
    		add_stream(&n_video_st, oc, &cdc, fmt->video_codec);
    	} else
    	{
    		add_stream(&n_video_st, oc, &video_codec, fmt->video_codec);
        	std::cout << " using codec: " << video_codec->name << std::endl;
    	}

        have_video = 1;
        encode_video = 1;
    } else {
    	printf("no video codec found\n");
    }

    if (fmt->audio_codec != AV_CODEC_ID_NONE){
        add_stream(&n_audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    } else {
    	printf("no audio codec found\n");
    }

    // Now that all the parameters are set, we can open the audio and
    // video codecs and allocate the necessary encode buffers.
//    if (video_st) open_video(oc, video_codec, video_st);
//    if (audio_st) open_audio(oc, audio_codec, audio_st);

    if (have_video) open_video(oc, video_codec, &n_video_st, opt);
	if (have_audio) open_audio(oc, audio_codec, &n_audio_st, opt);
    av_dump_format(oc, 0, fileName, 1);

    // open the output file, if needed
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, fileName, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", fileName,
                    av_err2str(ret));
        }
    }

    // Write the stream header, if any.
    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
    }

	mixDownMap = std::vector< std::vector<unsigned int> >(n_audio_st.st->codecpar->channels);
	unsigned int nrMixChans = static_cast<unsigned int>( (float(pa->maxInputChannels) / float(n_audio_st.st->codecpar->channels)) + 0.5f);

	// make mixDownMap
	for (unsigned int chan=0; chan<n_audio_st.st->codecpar->channels; chan++)
		for (unsigned int i=0;i<nrMixChans;i++)
			if ( chan + i * nrMixChans < pa->maxInputChannels )
				mixDownMap[chan].push_back(chan + i * nrMixChans);

	// start downloading audio frames
	pa->setExtCallback(&MediaRecorder::getMediaRecAudioDataCallback, &audioQueue, n_audio_st.st->codecpar->frame_size,
			n_audio_st.st->codecpar->channels, &mixDownMap);

/*
	if ( n_video_st.enc->pix_fmt == PIX_FMT_YUV420P )
	{
		// gamma(0.1:1:10), contrast(1), brightness(0), saturation(1), gamma_r(1), gamma_grün(1), gamma_blau(1)
		// korrektur für rgb -> yuv wird sonst ein wenig matt...
		const char *filter_descr = "mp=eq2=2.0:2.0:0.0:2.0";

		if ((ret = init_filters(filter_descr, recP)) < 0)
			fprintf(stderr, "Error occurred initing filters\n");

		recP->useFiltering = true;
		std::cout << "use filtering " << std::endl;
	}
*/

	doRec = true;

    m_Thread = new boost::thread(&MediaRecorder::recThread, this);
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::stop()
{
	doRec = false;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::recThread()
{
    while (doRec)
    {
        // select the stream to encode
    	// if video is before audio, write video, otherwise audio
    	if (av_compare_ts(n_video_st.next_pts, n_video_st.enc->time_base,
    					n_audio_st.next_pts, n_audio_st.enc->time_base) <= 0)
    		encode_video = write_video_frame(oc, &n_video_st);
    	else
    		encode_audio = write_audio_frame(oc, &n_audio_st, false);
    }

	std::cout << "MediaRecorder stop, remaining video frames: " << nrReadVideoFrames << std::endl;

	// save rest of pics buffer
	while (nrReadVideoFrames > 0)
		write_video_frame(oc, &n_video_st);

	// save rest of audio buffer
	while (audioQueue.size() > 0)
		write_audio_frame(oc, &n_audio_st, true);


    // Write the trailer, if any. The trailer must be written before you
    // close the CodecContexts open when you wrote the header; otherwise
    // av_codec_close().
    av_write_trailer(oc);

    if (have_video) close_stream(oc, &n_video_st);
    if (have_audio) close_stream(oc, &n_audio_st);

    // Close the output file.
    if (!(fmt->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);

    // free the stream
    avformat_free_context(oc);

    pa->removeExtCallback();
}

//-------------------------------------------------------------------------------------------
// ---------- neu vom muxing example

void MediaRecorder::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

//    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
//           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
//           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
//           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
//           pkt->stream_index);
}

//-------------------------------------------------------------------------------------------

int MediaRecorder::write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

//-------------------------------------------------------------------------------------------

// Add an output stream.
void MediaRecorder::add_stream(MediaRecorder::OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec, enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

    // find the encoder
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;

    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = audioBitRate;
        c->sample_rate = pa->sample_rate;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == pa->sample_rate)
                    c->sample_rate = pa->sample_rate;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
		av_opt_set(c->priv_data, "preset", "fastest", 0);

        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        std::cout << "videobitrate: " << videoBitRate << std::endl;
        c->bit_rate = videoBitRate;

        // Resolution must be a multiple of two.
        c->width    = inWidth;
        c->height   = inHeight;

        // timebase: This is the fundamental unit of time (in seconds) in terms
        // of which frame timestamps are represented. For fixed-fps content,
        // timebase should be 1/framerate and timestamp increments should be
        // identical to 1.
        ost->st->time_base = (AVRational){ 1, monRefRate };
        c->time_base       = ost->st->time_base;
        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
		c->thread_count = 6;

		if (forceCodec == 0 && c->codec_id == AV_CODEC_ID_H264 || c->codec_id == AV_CODEC_ID_MPEG2TS )
		{
			std::cout << "codec is AV_CODEC_ID_H264" << std::endl;

			//film, animation, grain, stillimage, psnr, ssim, fastdecode, zerolatency
			av_opt_set(c->priv_data, "tune", "animation", 0);

			// ultrafast,superfast, veryfast, faster, fast, medium, slow, slower, veryslow
			// je schneller, desto groesser die dateien
			av_opt_set(c->priv_data, "preset", "faster", 0);

			// for compatibility
			c->profile = FF_PROFILE_H264_MAIN;
			c->me_cmp = FF_CMP_CHROMA;
			c->me_method = ME_EPZS;
		}

        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            // jjust for testing, we also add B-frames
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            // Needed to avoid using macroblocks in which some coeffs overflow.
            // This does not happen with normal video, it just happens here as
            // the motion of the chroma plane does not match the luma plane.
            c->mb_decision = 2;
        }
    break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

//-------------------------------------------------------------------------
// audio output

AVFrame* MediaRecorder::alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

//-------------------------------------------------------------------------

void MediaRecorder::open_audio(AVFormatContext *oc, AVCodec *codec, MediaRecorder::OutputStream *ost,
		AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    // open it
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }


    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    	src_nb_samples = 10000;
    else
    	src_nb_samples = c->frame_size;

    nb_samples = src_nb_samples;


	ret = av_samples_alloc_array_and_samples(&src_samples_data, &src_samples_linesize, c->channels,
                                         src_nb_samples, c->sample_fmt, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate source samples\n");
		exit(1);
	}


    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    // copy the stream parameters to the muxer
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

    // create resampler context
     ost->swr_ctx = swr_alloc();
     if (!ost->swr_ctx) {
    	 fprintf(stderr, "Could not allocate resampler context\n");
         exit(1);
     }

     // set options
     av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
     av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
     av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
     av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
     av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
     av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

     // initialize the resampling context
     if ((ret = swr_init(ost->swr_ctx)) < 0) {
    	 fprintf(stderr, "Failed to initialize the resampling context\n");
         exit(1);
     }

     // compute the number of converted samples: buffering is avoided
     // ensuring that the output buffer will contain at least all the
     // converted input samples
     max_dst_nb_samples = src_nb_samples;
     ret = av_samples_alloc_array_and_samples(&dst_samples_data, &dst_samples_linesize, c->channels,
                                              max_dst_nb_samples, c->sample_fmt, 0);
     if (ret < 0) {
         fprintf(stderr, "Could not allocate destination samples\n");
         exit(1);
     }

     dst_samples_size = av_samples_get_buffer_size(NULL, c->channels, max_dst_nb_samples,
                                                   c->sample_fmt, 0);
}

//-------------------------------------------------------------------------------------------

AVFrame* MediaRecorder::get_audio_frame(MediaRecorder::OutputStream *ost, bool clear)
{
	AVFrame *frame = ost->tmp_frame;
    int16_t *q = (int16_t*)frame->data[0];
	int newSize = 0;

	if ( int(audioQueue.size()) >= frame->nb_samples * ost->enc->channels )
	{
		for ( int j=0;j<frame->nb_samples;j++ )
			for ( int chanNr=0; chanNr<ost->enc->channels; chanNr++ )
				*q++ = static_cast<int>(audioQueue[j*ost->enc->channels + chanNr] * 32767);

		newSize = audioQueue.size() - frame->nb_samples * ost->enc->channels;
		audioQueue.erase(audioQueue.begin(), audioQueue.begin() + frame->nb_samples * ost->enc->channels);
		audioQueue.resize(newSize);

	} else if(clear)
	{
		for ( unsigned int j=0; j<(audioQueue.size() / ost->enc->channels); j++ )
			for ( int chanNr=0; chanNr<ost->enc->channels; chanNr++ )
				*q++ = static_cast<int>(audioQueue[j*ost->enc->channels + chanNr] * 32767);

		// add zeros to complete the frame_size
		for ( unsigned int j=0; j<frame->nb_samples - (audioQueue.size() / ost->enc->channels); j++ )
			for ( int chanNr=0; chanNr<ost->enc->channels; chanNr++ )
				*q++ = 0;

		audioQueue.clear();
		audioQueue.resize(0);
	}

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

//-------------------------------------------------------------------------------------------

// encode one audio frame and send it to the muxer return 1 when encoding is finished, 0 otherwise
int MediaRecorder::write_audio_frame(AVFormatContext *oc, MediaRecorder::OutputStream *ost,  bool clear)
{
    AVCodecContext* c = ost->enc;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame* frame=0;
    int ret;
    int got_packet=0;
    int dst_nb_samples;

    if(audioQueue.size() > 0)
    {
        av_init_packet(&pkt);

        frame = get_audio_frame(ost, clear);

    	if (frame)
    	{
    		// convert samples from native format to destination codec format, using the resampler
    		// compute destination number of samples
    		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
    				c->sample_rate, c->sample_rate, AV_ROUND_UP);
    		av_assert0(dst_nb_samples == frame->nb_samples);

    		// when we pass a frame to the encoder, it may keep a reference to it internally;
    		// make sure we do not overwrite it here
    		ret = av_frame_make_writable(ost->frame);
    		if (ret < 0)
    			exit(1);

    		// convert to destination format
    		ret = swr_convert(ost->swr_ctx,
    				ost->frame->data, dst_nb_samples,
					(const uint8_t **)frame->data, frame->nb_samples);
    		if (ret < 0) {
    			fprintf(stderr, "Error while converting\n");
    			exit(1);
    		}

    		frame = ost->frame;
    		frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);

    		ost->samples_count += dst_nb_samples;
    	} else {
			fprintf(stderr, "Couldnt get audio frame\n");
    	}

    	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    	//ret = avcodec_send_frame(c, frame);

    	if (ret < 0) {
    		fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
    		exit(1);
    	}

    	if (got_packet) {
    		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    		if (ret < 0) {
    			fprintf(stderr, "Error while writing audio frame: %s\n",
    					av_err2str(ret));
    			exit(1);
    		}
    	}
    }

    return (frame || got_packet) ? 0 : 1;
}

//-------------------------------------------------------------------------------------------
// video output

AVFrame* MediaRecorder::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::open_video(AVFormatContext *oc, AVCodec *codec, MediaRecorder::OutputStream *ost,
		AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    // allocate and init a re-usable frame
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

//	// copy data and linesize picture pointers to frame
//    *((AVPicture *)ost->frame) = dst_picture;

	// allocate a frame for filtering
	filt_frame = av_frame_alloc();
	if (!filt_frame) {
		fprintf(stderr, "Could not allocate video filt_frame\n");
	    exit(1);
	}
	avpicture_alloc((AVPicture *)filt_frame, c->pix_fmt, c->width, c->height);
	filt_frame->width = c->width;
	filt_frame->height = c->height;
	if ( !filt_frame )
	{
		std::cout << "error allocating memory for the swscale conversion img" << std::endl;
	} else {
		filt_frame->pts = 0;
	}

    // If the output format is not YUV420P, then a temporary YUV420P
    // picture is needed too. It is then converted to the required
    // output format.
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }

    // copy the stream parameters to the muxer
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	pFrameRGB->width = inWidth;
	pFrameRGB->height= inHeight;
	if (pFrameRGB == NULL) std::cout << "Codec not allocate frame" << std::endl;

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	av_image_alloc(pFrameRGB->data, pFrameRGB->linesize, inWidth, inHeight, glFormat, 1);
	if( !pFrameRGB ) std::cout << "error allocation av_image_alloc for opengl capturing" << std::endl;

}

//-------------------------------------------------------------------------------------------

AVFrame* MediaRecorder::get_video_frame(MediaRecorder::OutputStream *ost)
{
    AVCodecContext *c = ost->enc;

    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        // as we only generate a YUV420P picture, we must convert it
        // to the codec pixel format if needed
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          AV_PIX_FMT_YUV420P,
                                          inWidth, inHeight,
//                                          c->width, c->height,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr,
                        "Could not initialize the conversion context\n");
                exit(1);
            }
        }

        sws_scale(ost->sws_ctx,
                  (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize,
                  0, c->height, ost->frame->data, ost->frame->linesize);
    }

    return ost->frame;
}

//-------------------------------------------------------------------------------------------
// encode one video frame and send it to the muxer return 1 when encoding is finished, 0 otherwise
int MediaRecorder::write_video_frame(AVFormatContext *oc, MediaRecorder::OutputStream *ost)
{
    int ret;
    int got_packet = 0;
    AVPacket pkt = { 0 };
    AVCodecContext *c = ost->enc;
    AVFrame* inframe = nullptr;
    AVFrame* frame=0;

    if(nrReadVideoFrames > 0)
    {
        frame = get_video_frame(ost);

        // only write if there was a frame downloaded
    	if ( useFiltering )
    	{
    		// allocate a frame with the settings of the context
    		inframe = av_frame_alloc();
    		av_image_alloc(inframe->data, inframe->linesize, c->width, c->height, c->pix_fmt, 1);
    		inframe->width = c->width;
    		inframe->height = c->height;
    		if ( !inframe )  std::cout << "error copying frame to inframe" << std::endl;
    	}

    	// get picture from the buffer
    	// reset picture type for having encoder setting it
    	frame->pict_type = AV_PICTURE_TYPE_NONE;
    	frame->format = c->pix_fmt;

    	ret = av_image_fill_arrays (pFrameRGB->data, pFrameRGB->linesize, buffer[writeVideoFrame], glFormat, inWidth, inHeight, 1);
    	writeVideoFrame = (writeVideoFrame+1) % nrBufferFrames;
    	if (ret < 0) {
            fprintf(stderr, "av_image_fill_arrays error: %s\n", av_err2str(ret));
    	}

    	nrReadVideoFrames--;

    	int sws_flags = SWS_BILINEAR | SWS_ACCURATE_RND ;
    			//SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP |

    	// No more frames to compress. The codec has a latency of a few
    	// frames if using B-frames, so we get the last frames by
    	// passing the same picture again.

    	// setup a sws context
    	// sws_getContext seems to be deprecated
    	// this checks if the context can be reused
    	converter = sws_getCachedContext(converter,
    									 inWidth,	// srcwidth
    									 inHeight,	// srcheight
    									 glFormat,		// src_format
    									 c->width,		// dstWidth
    									 c->height,		// dstHeight
    									 c->pix_fmt,		// dst_format
    									 sws_flags,			// flags
    									 NULL,				// srcFilter
    									 NULL,				// dstFilter
    									 NULL				// param
    									 );

    	// flip image horizontally
    	int stride[4];
    	if (flipH)
    	{
       		*pFrameRGB->data = *pFrameRGB->data + inWidth *glNrBytesPerPixel *(inHeight-1);
    		for (int i=0;i<4;i++) stride[i] = -inWidth*glNrBytesPerPixel;
    	} else {
    		for (int i=0;i<4;i++) stride[i] = inWidth *glNrBytesPerPixel;
    	}

		if ( useFiltering )
		{
			sws_scale(converter, pFrameRGB->data, stride, 1,
					c->height, ost->frame->data, ost->frame->linesize);

			//		sws_scale(converter, pFrameRGB->data, stride, 1,
			//				  outHeight -1,
			//				  ((AVPicture *)frame)->data,
			//				  ((AVPicture *)frame)->linesize);

			// push the frame into the filtergraph
			// killt den frame
			ret = av_buffersrc_add_frame(buffersrc_ctx, frame);
			if ( ret != 0 ) std::cout <<  "Error while feeding the filtergraph. error code " << ret << std::endl;

			if (av_buffersink_get_frame(buffersink_ctx, filt_frame) < 0)
				std::cout << "error filtering frame" << std::endl;

			filt_frame->pts = frame->pts;

		} else
		{
			sws_scale(converter,			// the scaling context previously created with sws_getContext()
					pFrameRGB->data, 		// the array containing the pointers to the planes of the source slice
					stride,					// the array containing the strides for each plane of the source image
					0,						// the position in the source image of the slice to
					// process, that is the number (counted starting from
					// zero) in the image of the first row of the slice
					inHeight,				// the height of the source slice, that is the number of rows in the slice
					ost->frame->data,	// the array containing the pointers to the planes of the destination image
					ost->frame->linesize);	// the array containing the strides for each plane of the destination image
		}

        ost->frame->pts = ost->next_pts++;

    	//---------------------------------------------

        av_init_packet(&pkt);

		ret = av_frame_make_writable(ost->frame);
    	if (ret < 0) {
    		fprintf(stderr, "could not make frame writable: %s\n", av_err2str(ret));
        }

        // encode the image
    	if ( useFiltering )
    		ret = avcodec_encode_video2(c, &pkt, filt_frame, &got_packet);
    	 else
    		ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);

    	if (ret < 0) {
            fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
            exit(1);
        }


        if (got_packet) {
            ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        } else {
            ret = 0;
            fprintf(stderr, "didn't get video packet\n");
        }
    }

//	return (frame ) ? 0 : 1;
	return (frame || got_packet) ? 0 : 1;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::close_stream(AVFormatContext *oc, MediaRecorder::OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------

void MediaRecorder::downloadFrame()
{
	if(doRec)
	{
		// lass alle bilder ausserhalb der framerate aus
		if ( renderFrameNr % recFrameInt == 0 )
		{
			if ( nrReadVideoFrames >= nrBufferFrames )
			{
				std::cout << "buffer overflow!!! quitting" << std::endl;
				doRec = false;
			}

			bufferReadPtr = (bufferReadPtr +1) % nrBufferFrames;

			double startRead = glfwGetTime();

			if (num_downloads < num_pbos)
			{
				// First we need to make sure all our pbos are bound, so glMap/Unmap will
				// read from the oldest bound buffer first.
				glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[dx]);
				glReadPixels(0, 0, inWidth, inHeight, glDownloadFmt, GL_UNSIGNED_BYTE, 0);  // When a GL_PIXEL_PACK_BUFFER is bound,
																							// the last 0 is used as offset into the buffer to read into.
				num_downloads++;
			} else
			{
				// Read from the oldest bound pbo.
				glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[dx]);

				ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
				if (ptr != NULL)
				{
					memcpy(buffer[bufferReadPtr], ptr, nbytes);
					glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
					nrReadVideoFrames++;

				} else
				{
					std::cerr << "Failed to map the buffer" << std::endl;
				}

				// Trigger the next read.
				glReadPixels(0, 0, inWidth, inHeight, glDownloadFmt, GL_UNSIGNED_BYTE, 0);
			}

		    dx = ++dx % num_pbos;
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);


/*
			glReadBuffer( GL_BACK );
			glReadPixels(0, 0, inWidth, inHeight, glDownloadFmt, GL_UNSIGNED_BYTE, buffer[bufferReadPtr]);

			writeMutex.lock();
			picQueue.push_back( &buffer[bufferReadPtr] );
			writeMutex.unlock();

			double endRead = glfwGetTime();
			if ( show > 20) {
				show = 0;
				std::cout << "diff: " << endRead - startRead << std::endl;
			}
			show++;
*/
		}

		renderFrameNr++;
	}
}

//-------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------
// frames kommen von der kinect mit 4 bytes pro pixel (rgba)
void MediaRecorder::downloadFrame(unsigned char* bufPtr)
{
	if(doRec)
	{
		if ( nrReadVideoFrames >= nrBufferFrames )
		{
			std::cout << "buffer overflow!!! quitting" << std::endl;
			doRec = false;
		}

		bufferReadPtr = (bufferReadPtr +1) % nrBufferFrames;
		buffer[bufferReadPtr] = bufPtr;
		nrReadVideoFrames++;
	}
}

//-------------------------------------------------------------------------------------------

// wenn einer neuer buffer von paudio geschrieben wird, schieb ihn in die q
void MediaRecorder::getMediaRecAudioDataCallback(unsigned int numChans, int frames, int offset, int codecFrameSize,
		unsigned int codecNrChans, std::vector< std::vector<unsigned int> >* mixDownMap, float** sampData,
		std::vector<float>* inSampQ)
{
	float mixSamp = 0.f;

	// write interleaved
	// if the number of channels of the codec is the same as the actual number of channels of paudio,
	// just copy
	if (numChans == codecNrChans)
	{
		for (unsigned int samp=0; samp<frames; samp++)
			for (unsigned int chan=0; chan<numChans; chan++)
				inSampQ->push_back( sampData[chan][offset + samp] );
	} else
	{
		// mix down
		for (unsigned int samp=0; samp<frames; samp++)
			for (unsigned int chan=0; chan<codecNrChans; chan++)
			{
				mixSamp = 0.f;
				for ( unsigned int i=0;i< mixDownMap->at(chan).size();i++)
					mixSamp += sampData[ mixDownMap->at(chan)[i] ][offset + samp];

				inSampQ->push_back( mixSamp );
			}
	}
}

//-------------------------------------------------------------------------------------------

bool MediaRecorder::isRecording()
{
	return doRec;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::myNanoSleep(uint32_t ns)
{
  struct timespec tim;
  tim.tv_sec = 0;
  tim.tv_nsec = (long)ns;
  nanosleep(&tim, NULL);
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::setOutputSize(unsigned int _width, unsigned int _height)
{
	outWidth = _width;
	outHeight = _height;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::setOutputFramerate(unsigned int _frameRate)
{
	recFrameRate = _frameRate;
	recFrameInt = monRefRate / _frameRate;
	std::cout << "recFrameInt: " << recFrameInt << " recFrameRate " << recFrameRate << std::endl;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::setVideoBitRate(unsigned int _bitRate)
{
	videoBitRate = _bitRate;
}

//-------------------------------------------------------------------------------------------

void MediaRecorder::setAudioBitRate(unsigned int _bitRate)
{
	audioBitRate = _bitRate;
}

//-------------------------------------------------------------------------------------------

int MediaRecorder::getOutputWidth(){
	return outWidth;
}

//-------------------------------------------------------------------------------------------

int MediaRecorder::getOutputHeight(){
	return outHeight;
}

//-------------------------------------------------------------------------------------------

MediaRecorder::~MediaRecorder()
{
}

} /* namespace tav */
