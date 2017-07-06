/*
 *  PAudio.cpp
 *  tav
 *
 *  Created by user on 25.05.11.
 *  Copyright 2011 Sven Hahne. All rights reserved.
 *
 */

#include "PAudio.h"



PAudio::PAudio(boost::mutex *_mutex, bool* _isReady, int _frameSize, int _rec_buf_size,
			   int _fft_size, int _sample_rate, int _monRefreshRate, int _maxNrChans)
: mutex(_mutex), isReady(_isReady), frames_per_buffer(_frameSize),
rec_buf_size(_rec_buf_size), fft_size(_fft_size), sample_rate(_sample_rate),
monRefreshRate(_monRefreshRate), hasInputDevice(true), maxInputChannels(_maxNrChans)
{
	isPlaying = false;
	writeToDisk = false;
}

//------------------------------------------------------------------------------------------------------------

PAudio::~PAudio()
{
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
}

//------------------------------------------------------------------------------------------------------------

void PAudio::pause()
{
	Pa_StopStream(stream);
	isPlaying = false;
}

//------------------------------------------------------------------------------------------------------------

void PAudio::resume()
{
	Pa_StartStream(stream);
	isPlaying = true;
}

//------------------------------------------------------------------------------------------------------------

void PAudio::recordToFile(std::string _path)
{
	std::string path = _path.substr( 0, _path.length() - 3 );
	path += "aif";
#ifdef __APPLE__
	waveData.writeThread->initFile(path);
	waveData.writeThread->start(0);
	writeToDisk = true;
#endif
}

//------------------------------------------------------------------------------------------------------------

void PAudio::stopRecordToFile()
{
#ifdef __APPLE__
	writeThread->stop();
	writeToDisk = false;
	std::cout << "close audio file" << std::endl;
#endif
}

//------------------------------------------------------------------------------------------------------------

void PAudio::processQueue(unsigned N)
{
	try {
		initAudio();
	}
	catch (...)
	{
		std::cout << "An unknown exception occured." << std::endl;
	}
}

//------------------------------------------------------------------------------------------------------------

int PAudio::memberCallback(const void *input, void *output,
						   unsigned long frameCount,
						   const PaStreamCallbackTimeInfo* timeInfo,
						   PaStreamCallbackFlags statusFlags,
						   void *userData)
{

	paAudioInData *waveData = (paAudioInData*)(userData);
	int numChannels = (int)(waveData->numChannels);
//	int offset =  ( waveData->frameIndex - frames_per_buffer + waveData->maxFrameIndex )
//					% waveData->maxFrameIndex
//					* numChannels;
#ifdef __APPLE__
	if ( *waveData->writeToDisk == true )
	{
		waveData->writeThreadMutex->lock();
		waveData->writeThread->addBlock( waveData->sampData, offset, frames_per_buffer );
		waveData->writeThreadMutex->unlock();
	}	
#endif
	return paContinue;
}

//------------------------------------------------------------------------------------------------------------

int PAudio::recordCallback(
						   const void *input, void *output,
						   unsigned long frameCount,
						   const PaStreamCallbackTimeInfo* timeInfo,
						   PaStreamCallbackFlags statusFlags,
						   void *userData )
{
	paAudioInData *waveData         = (paAudioInData*)(userData);
	SAMPLE** sampData               = (SAMPLE**) (waveData->sampData);
	SAMPLE** dataToFft              = (SAMPLE**) (waveData->dataToFft);
	int numChannels                 = (int)(waveData->numChannels);
	int frames_per_buffer           = (int)(waveData->frames_per_buffer);
	int rec_buf_size                = (int)(waveData->rec_buf_size);
	int fft_size                    = (int)(waveData->fft_size);
    int nrSepBands                  = (int)(waveData->nrSepBands);
    bool* pIsReady                  = (bool*)(waveData->isReady);
	bool* isWriting                 = (bool*)(waveData->isWriting);
    float* medVol                   = (float*)(waveData->medVol);
    float volMed                    = (float)(waveData->volumeMed);

	const SAMPLE *rptr = (const SAMPLE*) input;
	
    (void) output; // Prevent unused variable warnings.
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

	if ( waveData->blockCounter >= rec_buf_size ) waveData->buffersFilled = true;
	if ( waveData->blockCounter >= (rec_buf_size+1) ) *pIsReady = true;

	if ( waveData->doUpdate )
	{
		waveData->inMutex->lock();
		*isWriting = true;
		
		if( input != NULL )
		{
			for( unsigned int i=0; i<frameCount; i++ )
			{
				for( int j=0; j<numChannels; j++ ) 
				{
					sampData[j][ waveData->frameIndex ] = *rptr;
					*rptr++;
				}
				waveData->frameIndex = ( waveData->frameIndex +1 ) % waveData->maxFrameIndex;
			}
		}

		int offset = ( waveData->frameIndex - frames_per_buffer + waveData->maxFrameIndex )
					  % waveData->maxFrameIndex;
        
        // sollte als thread laufen
//        for( int j=0; j<numChannels; j++ )
//            abios->process(&sampData[j][offset], j);

//        for( int i=0; i<frameCount; i++ )
//            std::cout << "c0: " << sampData[j][ waveData->frameIndex ] << std::endl;
        
        
		if ( waveData->frameIndex == 0 )
		{
			// kopiere die neuen samples in ein array mit der kompletten verfügbaren länge
			// in der richtigen reihenfolge
			for ( int i=0;i<rec_buf_size;i++ )
				for ( int j=0; j<numChannels; j++ )
					memcpy(&dataToFft[j][i * frames_per_buffer],
						   &sampData[j][ (waveData->frameIndex + i * frames_per_buffer) % waveData->maxFrameIndex ], 
						   frames_per_buffer * sizeof(SAMPLE));

         /*
            // threaded wackelt noch zu sehr von der Performance her
			 // ist zwar schneller, aber die threads muessten hoeher priorität haben...
			 if ( waveData->buffersFilled )
			 {
			 ffts->forwZeroBack(waveData->dataToFft);
			 
			 // mach einen geglätteten std::endlos-loop
			 for(int j=0; j<numChannels; j++) pll->getIfft( ffts->demirrored[j], j, fft_size/2 );
			 
			 // mach eine pseudo phaselock loop
			 //pll->phaseLock( waveDatas->sepChans );
			 }
         */
		}

		*isWriting = false;

#ifdef __APPLE__
		if ( *waveData->writeToDisk == true )
		{
			waveData->writeThreadMutex->lock();
			waveData->writeThread->addBlock( waveData->sampData, offset, frames_per_buffer );
			waveData->writeThreadMutex->unlock();
		}	
#endif
		// extern callback, wird von ffmpeg mediarecorder benutzt
		if (waveData->cbType != NULL) 
			(*waveData->cbType)(numChannels, static_cast<int>(frameCount), offset, waveData->codecFrameSize,
					waveData->codecNrChans, waveData->mixDownMap, sampData, waveData->inSampQ);

		waveData->inMutex->unlock();
		waveData->blockCounter++;
	}

//	tmr->saveCurrentTimeAsEnd();
//	if ( *cntr % 40 == 0 ) std::cout << tmr->getCurrentDt() << std::endl;
//	*cntr += 1;
	
	//return ((PAudio*)userData)->memberCallback(input, output, frameCount, timeInfo, statusFlags, userData);
    
	return paContinue;
}

//------------------------------------------------------------------------------------------------------------

void PAudio::initAudio()
{	
	counter = 0;
	
	PaStreamParameters  inputParameters;
    PaError             err = paNoError;
	
    //-------- init audio -----------------
#ifdef __linux__
    PaJack_SetClientName("Freenect2Recorder");
#endif

    err = Pa_Initialize();
    if( err != paNoError ) std::cout << "device initialisation error" << std::endl;

#ifdef __linux__

    bool found = false;
    int foundIndx = 0;
    PaHostApiIndex nrHostApi = Pa_GetHostApiCount();
    printf("nrHostApis: %d \n", nrHostApi);

    for (int i=0;i<nrHostApi;i++)
    {
    	 const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
    	 const PaDeviceInfo* devInfo;

    	 printf("api name %d: %s nrDevices: %d\n", i, api->name, api->deviceCount);

    	 if(strcmp(api->name, "JACK Audio Connection Kit") == 0 || i==2)
    	 {
    		 printf("using jack \n");

        	 for(int j=0;j<Pa_GetDeviceCount();j++)
        	 {
        		 devInfo = Pa_GetDeviceInfo(j);
        		 printf("dev nr %d name: %s maxNrInputChans: %d \n", j, devInfo->name, devInfo->maxInputChannels);

        		 if (std::strcmp(devInfo->name, "ardour") == 0)
        		 {
            		 foundIndx = j;
            		 inputParameters.device = (PaDeviceIndex)foundIndx;

            		 printf("found default device!!!! using dev nr %d name: %s maxNrInputChans: %d \n", j, devInfo->name, devInfo->maxInputChannels);
        		 }
        	 }

        	 //std::cout << "default device: " << api->defaultInputDevice << std::endl;
    		 //inputParameters.device = api->defaultInputDevice;
    		 found = true;
    	 }
    }

    if(!found)	inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.device = 0;

    std::cout << "using device: " << inputParameters.device << std::endl;

#elif __APPLE__
    inputParameters.device = Pa_GetDefaultInputDevice();
#endif

    if (inputParameters.device == paNoDevice)
    {
        waveData.numChannels = 0;
        std::cerr << "PAudio Error: No Default input device." << std::endl;
    } else
    {
        // --- init userData ----
    
        devInfo = Pa_GetDeviceInfo(inputParameters.device);

      //  maxInputChannels = devInfo->maxInputChannels;
      //  maxInputChannels = std::min(devInfo->maxInputChannels, maxInputChannels);

        waveData.numChannels = maxInputChannels;
        waveData.frameIndex = 0;
        waveData.maxFrameIndex = frames_per_buffer * rec_buf_size;
        waveData.frames_per_buffer = frames_per_buffer;
        waveData.rec_buf_size = rec_buf_size;
        waveData.fft_size = fft_size;
        waveData.fftUpdtFreq = fft_size / frames_per_buffer;
        waveData.nrSepBands = 4;
        waveData.medVol = new float[maxInputChannels];
        waveData.volumeMed = 1.f;
        //waveData.volumeMed = 4.f;
        waveData.sampData = new SAMPLE*[maxInputChannels];
        waveData.dataToFft = new SAMPLE*[maxInputChannels];
        
        for ( unsigned int j=0;j<maxInputChannels;j++)
        {
            waveData.sampData[j] = (SAMPLE *) malloc( frames_per_buffer * rec_buf_size * sizeof(SAMPLE) );
            waveData.dataToFft[j] = (SAMPLE *) malloc( fft_size * sizeof(SAMPLE) );
            
            for( int i=0; i<frames_per_buffer * rec_buf_size; i++ ) waveData.sampData[j][i] = 0;
            for( int i=0; i<fft_size; i++ ) waveData.dataToFft[j][i] = 0;
        }
        
        if ( waveData.sampData[0] == NULL )
            std::cerr << "Could not allocate record array." << std::endl;
        
        waveData.inMutex = mutex;
        waveData.blockCounter = 0;
        waveData.doUpdate = true;
        waveData.buffersFilled = false;
        waveData.isReady = isReady;
        waveData.isWriting = &isWriting;
        waveData.cbType = NULL;
        waveData.inSampQ = NULL;
        
#ifdef __APPLE__
        // --- init aiff writer --
        writeThread = new AiffWriteThread(&writeThreadMutex, 2, 16, sample_rate);
        waveData.writeThread = writeThread;
        waveData.writeThreadMutex = &writeThreadMutex;
        waveData.writeToDisk = &writeToDisk;
#endif
        // -----------------------
        
        inputParameters.channelCount = maxInputChannels;
        inputParameters.sampleFormat = pa_sample_type;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = NULL;

        
        // Record some audio. --------------------------------------------
        
        err = Pa_OpenStream(&stream,
                            &inputParameters,
                            NULL,                  // &outputParameters,
                            sample_rate,
                            frames_per_buffer,
                            paClipOff,      // we won't output out of range samples so don't bother clipping them
                            recordCallback,
                            &waveData);
        
        if ( err != paNoError ) std::cerr << "Stream initialisation error" << std::endl;
        
        err = Pa_StartStream( stream );
        isPlaying = true;
        
        if ( err != paNoError ) std::cerr << "Stream start error" << err;
        
        std::cout << "Now recording!!" << std::endl;
    }
}

//--------------------------------------------------------------------------------------------

float PAudio::getMedAmp(int chanNr)
{
    return waveData.medVol[chanNr];
}

//--------------------------------------------------------------------------------------------

int PAudio::getMaxNrInChannels()
{
    return maxInputChannels;
}

//--------------------------------------------------------------------------------------------

int PAudio::getNrSepBands()
{
    return waveData.nrSepBands;
}

//--------------------------------------------------------------------------------------------

unsigned int PAudio::getFrameNr()
{
    return waveData.blockCounter;
}

//--------------------------------------------------------------------------------------------

float PAudio::getSampDataAtPos(int chanNr, float pos)
{
    return waveData.sampData[chanNr][static_cast<int>(pos * static_cast<float>(frames_per_buffer * rec_buf_size))];
}

//--------------------------------------------------------------------------------------------

void PAudio::setExtCallback(void (*cbType)(unsigned int numChans, int frames, int offset, int codecFrameSize,
		unsigned int codecNrChans, std::vector< std::vector<unsigned int> >* mixDownMap, float** sampData,
		std::vector<float>* inSampQ), std::vector<float>* sampQ, int _codecFrameSize, unsigned int _codecNrChans,
		std::vector< std::vector<unsigned int> >* mixDownMap)
{
	waveData.cbType = cbType;
	waveData.inSampQ = sampQ;
	waveData.codecFrameSize = _codecFrameSize;
	waveData.codecNrChans = _codecNrChans;
	waveData.mixDownMap = mixDownMap;
}

//--------------------------------------------------------------------------------------------

void PAudio::removeExtCallback()
{
	waveData.cbType = NULL;
}
