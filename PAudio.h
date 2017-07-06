/*
 *  PAudio.h
 *  tav
 *
 *  Created by user on 25.05.11.
 *  Copyright 2011 Sven Hahne. All rights reserved.
 *
 */

#pragma once

#ifndef _PAUDIO_
#define _PAUDIO_

#include <sstream>
#include <iostream>
#include <string>
#include <portaudio.h>

#ifdef __APPLE__
#include <pa_mac_core.h>
#elif __linux__
//#include <pa_linux_alsa.h>
#include <pa_jack.h>
#endif

#include "BoostThread.h"
//#include "PaPhaseLockLoop.h"
#ifdef __APPLE__
#include "AiffWriteThread.h"
#endif
//#include "AubioAnalyzer.h"
//#include "MultiChanFFT.h"

#include <stdio.h>
#include <stdlib.h>


class PAudio : public BoostThread
{
	public :

		typedef float SAMPLE;
		typedef struct
		{
			int							numChannels;
			int							rec_buf_size;
			int							frames_per_buffer;
			int							codecFrameSize; // fuer ffmpeg
			unsigned int				codecNrChans; // fuer ffmpeg
			std::vector< std::vector<unsigned int> >* mixDownMap; // fuer ffmpeg
			int							fft_size;
			int							fftUpdtFreq;
			int							frameIndex;
			int							maxFrameIndex;
			int                         nrSepBands;
			SAMPLE**					sampData;
			SAMPLE**					dataToFft;
			float*                      medVol;
			float                       volumeMed;
			boost::mutex*				inMutex;
			bool*						writeToDisk;
	#ifdef __APPLE__
			AiffWriteThread*			writeThread;
			boost::mutex*				writeThreadMutex;
	#endif
			int							blockCounter;
			bool						doUpdate;
			bool*						isWriting;
			bool*						isReady;
			bool						buffersFilled;
			void						(*cbType)(unsigned int numChans, int frames, int offset, int codecFrameSize, unsigned int codecNrChans,
										std::vector< std::vector<unsigned int> >* mixDownMap, float** sampData,
										std::vector<float>* inSampQ);
			std::vector<float>*	inSampQ;
		}
		paAudioInData;
		paAudioInData waveData;


		PAudio(boost::mutex *_mutex, bool* _isReady, int _frameSize, int _rec_buf_size,
			   int _fft_size, int _sample_rate, int _monRefreshRate, int _maxNrChans);
		~PAudio();

		void    processQueue(unsigned N);
		void    initAudio();
		void    pause();
		void    resume();
		void    recordToFile(std::string path);
		void    stopRecordToFile();

		float   getSampDataAtPos(int chanNr, float pos);

        // analysis output
        float   getMedAmp(int chanNr);
    
        int     getMaxNrInChannels();
        int     getNrSepBands();
        unsigned int getFrameNr();

		void    setExtCallback(void (*cbType)(unsigned int numChans, int frames, int offset, int codecFrameSize, unsigned int codecNrChans,
				std::vector< std::vector<unsigned int> >* mixDownMap, float** sampData, std::vector<float>* inSampQ),
       			std::vector<float>* sampQ, int _codecFrameSize, unsigned int _codecNrChans, std::vector< std::vector<unsigned int> >* mixDownMap);
		void    removeExtCallback();
	
		
		bool    isPlaying;

		int         sample_rate;
		int         frames_per_buffer;
		int         rec_buf_size; // multiples of frames_per_buffer
		int         fft_size;
		int         monRefreshRate;

		boost::mutex *mutex;
		bool*       isReady;
		bool        isWriting;
        bool        hasInputDevice;
		float**     pllData;
		int         sampPerPllLoop;
		int         fftUpdtFreq;
         int maxInputChannels;
	private:
		int memberCallback(const void *input, void *output,
						   unsigned long frameCount,
						   const PaStreamCallbackTimeInfo* timeInfo,
						   PaStreamCallbackFlags statusFlags,
						   void *userData);
		static int recordCallback(
								  const void *input, void *output,
								  unsigned long frameCount,
								  const PaStreamCallbackTimeInfo* timeInfo,
								  PaStreamCallbackFlags statusFlags,
								  void *userData);
		constexpr static const float SAMPLE_SILENCE = 0.0f;
		static const PaSampleFormat pa_sample_type = paFloat32;		
		PaStream*           stream;
        const PaDeviceInfo* devInfo;
		bool				writeToDisk;
#ifdef __APPLE__
		AiffWriteThread*	writeThread;
		boost::mutex		writeThreadMutex;
#endif
		int                 counter;
};

#endif
