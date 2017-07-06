/*
 * Freenect2In.cpp
 *
 *  Created on: 12.01.2016
 *      Author: sven
 */


#include "Freenect2In.h"

using namespace std;

namespace tav
{
// intit static variables
bool Freenect2In::protonect_paused = false;
bool Freenect2In::protonect_shutdown = false;
libfreenect2::Freenect2Device* Freenect2In::devtopause = 0;

Freenect2In::Freenect2In(const char* _file, unsigned int _sampleRate, unsigned int _recWidth, unsigned int _recHeight,
		const char* _selSerial) :
		file(_file), sampleRate(_sampleRate), useOpenGl(false), recWidth(_recWidth), recHeight(_recHeight),
		useCuda(true), debug(false), generateMips(false), noDepth(true)
{
	paudioIsReady = false;
	std::cout << "sampleRate " << sampleRate << std::endl;

	pa = new PAudio(&paMutex, &paudioIsReady, 512, 2, 1024, sampleRate, 60, 2);
	pa->start(0);

	while (!paudioIsReady)
		myNanoSleep(1000000);

	testData = new unsigned char[1920 * 1080 * 4];

	selSerial = std::string(_selSerial);

		// create a console logger with debug level (default is console logger with info level)
	libfreenect2::setGlobalLogger(
				libfreenect2::createConsoleLogger(libfreenect2::Logger::Debug));

	if (freenect2.enumerateDevices() == 0)
	{
		std::cout << "Freenect2In no device connected!" << std::endl;
		initOk = false;
	}
	else
	{
		short serialId = -1;

		for (int i=0;i<freenect2.enumerateDevices();i++)
		{
			std::string t_serial = freenect2.getDeviceSerialNumber(i);
			if(std::strcmp(t_serial.c_str(), selSerial.c_str()) == 0)
			{
				std::cout << "found requested serial: " << t_serial << std::endl;
				serial.push_back(t_serial);
			}
		}

		std::cout << "start iterator nr enumerated devices: " << int(freenect2.enumerateDevices()) << std::endl;

		unsigned short ind=0;
		for (vector<string>::iterator it=serial.begin(); it!=serial.end(); ++it)
		{
			bool thisDevOk = true;

			std::cout << "it ind " << ind << std::endl;

			if(useOpenGl)
			{
				std::cout << "use opengl pipeline" << std::endl;
				pipeline.push_back( new libfreenect2::OpenGLPacketPipeline() );
			} else if ( useCuda )
			{
				std::cout << "use cuda pipeline" << std::endl;
				pipeline.push_back( new libfreenect2::CudaPacketPipeline() );

			} else {
				//pipeline.push_back( new libfreenect2::OpenCLPacketPipeline() );
				pipeline.push_back( new libfreenect2::CpuPacketPipeline() );
			}

			std::cout << "use last added pipeline" << std::endl;

			if (pipeline.back())
			{
				dev.push_back(0);

				std::cout << "try to open freenect device: " << (*it) << std::endl;

				dev.back() = freenect2.openDevice( (*it), pipeline.back() );
			} else
			{
				std::cout << "could't open pipeline " << ind << std::endl;
				thisDevOk = false;
			}

			if (dev.back() == 0)
			{
				std::cout << "failure opening device!" << std::endl;
				thisDevOk = false;
			}

			if (thisDevOk)
			{
				serialIndMap[(*it)] = ind;

				devtopause = dev.back();

				signal(SIGINT, sigint_handler);
#ifdef SIGUSR1
				signal(SIGUSR1, sigusr1_handler);
#endif

				// [listeners]
				if(!noDepth) {
					listener.push_back(new libfreenect2::SyncMultiFrameListener (
						libfreenect2::Frame::Color
							| libfreenect2::Frame::Ir
							| libfreenect2::Frame::Depth));
				} else {
					listener.push_back(new libfreenect2::SyncMultiFrameListener ( libfreenect2::Frame::Color ));
				}

				registration.push_back(new libfreenect2::Registration(
						dev.back()->getIrCameraParams(),
						dev.back()->getColorCameraParams()));
				undistorted.push_back(new libfreenect2::Frame(grayWidth, grayHeight, 4));
				registered.push_back(new libfreenect2::Frame(grayWidth, grayHeight, 4));

				dev.back()->setColorFrameListener(listener.back());
				if(!noDepth) dev.back()->setIrAndDepthFrameListener(listener.back());
				dev.back()->start();

				std::cout << "device serial[" << ind << "]: " << dev.back()->getSerialNumber() << std::endl;
				std::cout << "device firmware" << ind << "]: " << dev.back()->getFirmwareVersion()
						<< std::endl;

				streamPar.push_back( std::map<fnc_ImgType, fn_StreamPar*>() );
				streamPar.back()[FNC_RGB] = new fn_StreamPar();
				streamPar.back()[FNC_IR] = new fn_StreamPar();
				streamPar.back()[FNC_DEPTH] = new fn_StreamPar();
				streamPar.back()[FNC_REGIS] = new fn_StreamPar();

				framesIt.push_back( std::map<fnc_ImgType, libfreenect2::Frame*>() );
				frames.push_back( libfreenect2::FrameMap() );

			    framecount.push_back(-1);
			    uploadFrameCnt.push_back(-1);

			    rgb.push_back(0);
				ir.push_back(0);
				depth.push_back(0);

				nrDevices++;
			}

			ind++;
		}

		// init MediaRecorder
		recorder = new MediaRecorder(file, pa, recWidth, recHeight, 30);
		recorder->setOutputSize(1280, 768);
		recorder->setOutputFramerate( 30 );
		recorder->setVideoBitRate(recorder->getOutputWidth() * recorder->getOutputHeight() * 30 * 3 / 20); // kinect has 30 fps
//		recorder->setVideoBitRate(recorder->getOutputWidth() * recorder->getOutputHeight() * sl->getSceneData(0)->monRefRate * 3 / 10);
		recorder->setAudioBitRate(128000);

		if(initOk) start();
	}

	printf("Freenect2 got %d devices \n", nrDevices);
}

//---------------------------------------------------------------

void Freenect2In::start()
{
	unsigned short ind = 0;

	for (vector<libfreenect2::Freenect2Device*>::iterator it=dev.begin(); it!=dev.end(); ++it)
	{
		m_mtx.push_back(new boost::mutex());
		m_Thread.push_back( new boost::thread(&Freenect2In::processQueue, this, ind) );
		ind++;
	}
}

//---------------------------------------------------------------

void Freenect2In::join()
{
	pa->join();
	for (unsigned short i= static_cast<unsigned short>(dev.size()); i<0; i++)
		m_Thread[i]->join();
}

//---------------------------------------------------------------

void Freenect2In::processQueue(unsigned short ind)
{
	while (!protonect_shutdown)
	{
		listener[ind]->waitForNewFrame(frames[ind]);

		// sort pointers
		rgb[ind] = frames[ind][libfreenect2::Frame::Color];

		if(!noDepth)
		{
			ir[ind] = frames[ind][libfreenect2::Frame::Ir];
			depth[ind] = frames[ind][libfreenect2::Frame::Depth];
			registration[ind]->apply(rgb[ind], depth[ind], undistorted[ind], registered[ind]);

			framesIt[ind][FNC_IR] = ir[ind];
			framesIt[ind][FNC_DEPTH] = depth[ind];
			framesIt[ind][FNC_REGIS] = registered[ind];
		}

		framesIt[ind][FNC_RGB] = rgb[ind];

		m_mtx[ind]->lock();
		downloadToBuffer(ind);	// make a copy
		m_mtx[ind]->unlock();

		listener[ind]->release(frames[ind]);	 // frames werden hier losgelassen
													// d.h es muss vorher runtergeladen werden
		framecount[ind]++;
	}

	/** libfreenect2::this_thread::sleep_for(libfreenect2::chrono::milliseconds(100)); */
}

//---------------------------------------------------------------

void Freenect2In::stop()
{
	protonect_shutdown = true;
	join();
	unsigned short ind = 0;
	for (vector<libfreenect2::Freenect2Device*>::iterator it=dev.begin(); it!=dev.end(); ++it)
	{
		(*it)->stop();
		(*it)->close();
		delete registration[ind];
		ind++;
	}
	//delete registration;
}

//---------------------------------------------------------------

void Freenect2In::allocate(fnc_ImgType type, int deviceNr)
{
//	printf("allocate width %d height %d extForm %d pixType: %d\n",streamPar[deviceNr][type]->width,    // width and height
//            streamPar[deviceNr][type]->height,
//			streamPar[deviceNr][type]->extFormat,
//			streamPar[deviceNr][type]->pixType);

	int mimapLevels=0;

	//std::cout << "gen texture" << std::endl;

    // generate color texture
	glGenTextures(1, (GLuint *) &streamPar[deviceNr][type]->texId);
	if(streamPar[deviceNr][type]->texId == 0)
		std::cout << "error generating texture " << std::endl;

	//std::cout << "bind texture : " <<  streamPar[deviceNr][type]->texId << std::endl;
	glBindTexture(GL_TEXTURE_2D, streamPar[deviceNr][type]->texId);

	//std::cout << "texture bound: " << std::endl;

    if (generateMips) mimapLevels = 5; else mimapLevels = 1;

    printf("allocating type %d width %d height %d bytes_per %d \n", type, streamPar[deviceNr][type]->width,
            streamPar[deviceNr][type]->height, streamPar[deviceNr][type]->bytes_per_pixel);


    // set linear filtering
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glTexImage2D(GL_TEXTURE_2D,              // target
                    0,                          // First mipmap level
                    streamPar[deviceNr][type]->intFormat,
                    streamPar[deviceNr][type]->width,    // width and height
                    streamPar[deviceNr][type]->height,
					0,									// border
                    streamPar[deviceNr][type]->extFormat,
                    streamPar[deviceNr][type]->pixType,
					0);

    // unbind
    glBindTexture(GL_TEXTURE_2D, 0);
}

//---------------------------------------------------------------

void Freenect2In::getGlFormat(fnc_ImgType type, fn_StreamPar* strPar)
{
    switch (type)
    {
    	// 1920x1080 32-bit BGRX.
        case FNC_RGB :
        	printf("type init color \n");
        	strPar->width = colorWidth;
        	strPar->height = colorHeight;
            strPar->nrChans = 4;
            strPar->intFormat = GL_RGBA8;
            strPar->extFormat = GL_BGRA;
            strPar->pixType = GL_UNSIGNED_BYTE;
            break;
        case FNC_REGIS :
        	printf("type init regis \n");
        	strPar->width = colorWidth;
        	strPar->height = colorHeight;
            strPar->nrChans = 4;
            strPar->intFormat = GL_RGBA8;
            strPar->extFormat = GL_BGRA;
            strPar->pixType = GL_UNSIGNED_BYTE;
            break;
        // 512x424 float. Range is [0.0, 65535.0].
        case FNC_IR :
        	printf("type init Ir \n");
        	strPar->nrChans = 1;
        	strPar->width = grayWidth;
        	strPar->height = grayHeight;
            strPar->intFormat = GL_R32F;
            strPar->extFormat = GL_RED;
            strPar->pixType = GL_FLOAT;
            break;
        // 512x424 float, unit: millimeter. Non-positive, NaN, and infinity are invalid or missing data.
        case FNC_DEPTH :
        	printf("type init Depth \n");
        	strPar->nrChans = 1;
        	strPar->width = grayWidth;
        	strPar->height = grayHeight;
           	strPar->intFormat = GL_R32F;
            strPar->extFormat = GL_RED;
            strPar->pixType = GL_FLOAT;
            break;
        default:
            printf("Freenect2In: texture allocate: pixel format not found \n");
            break;
    }
}

//---------------------------------------------------------------

void Freenect2In::uploadImg(fnc_ImgType type, int deviceNr)
{
	streamPar[deviceNr][type]->use = true;

//	std::cout << "upload Img" << std::endl;
//
//	std::cout << "framecount[deviceNr] " <<  framecount[deviceNr] << " uploadFrameCnt[deviceNr]:" << uploadFrameCnt[deviceNr] << std::endl;
//	std::cout << "deviceNr " <<  deviceNr << std::endl;
//	std::cout << "streamPar[deviceNr][type]->rawDataInited " <<  streamPar[deviceNr][type]->rawDataInited << std::endl;


	if (nrDevices > deviceNr && framecount[deviceNr] > 0
			&& framecount[deviceNr] != uploadFrameCnt[deviceNr]
			&& streamPar[deviceNr][type]->rawDataInited)
	{
		if (!streamPar[deviceNr][type]->inited)
		{
			getGlFormat(type, streamPar[deviceNr][type]);
			allocate(type, deviceNr);
			streamPar[deviceNr][type]->inited = true;
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, streamPar[deviceNr][type]->texId);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // necessary ?
			glTexSubImage2D(
					GL_TEXTURE_2D,             // target
					0,                          // First mipmap level
					0,
					0,                       // x and y offset
					streamPar[deviceNr][type]->width,
					streamPar[deviceNr][type]->height,
					streamPar[deviceNr][type]->extFormat,
					streamPar[deviceNr][type]->pixType,
					streamPar[deviceNr][type]->rawData);
		}

		uploadFrameCnt[deviceNr] = framecount[deviceNr];
	}
}

//---------------------------------------------------------------

void Freenect2In::uploadColor(int deviceNr)
{
    uploadImg(FNC_RGB, deviceNr);
}

//---------------------------------------------------------------

void Freenect2In::uploadDepth(int deviceNr)
{
    uploadImg(FNC_DEPTH, deviceNr);
}

//---------------------------------------------------------------

void Freenect2In::uploadIr(int deviceNr)
{
    uploadImg(FNC_IR, deviceNr);
}

//---------------------------------------------------------------

void Freenect2In::uploadColor(std::string* serial)
{
    uploadImg(FNC_RGB, serialIndMap[*serial]);
}

//---------------------------------------------------------------

void Freenect2In::uploadDepth(std::string* serial)
{
    uploadImg(FNC_DEPTH, serialIndMap[*serial]);
}

//---------------------------------------------------------------

void Freenect2In::uploadIr(std::string* serial)
{
    uploadImg(FNC_IR, serialIndMap[*serial]);
}

//---------------------------------------------------------------

GLuint Freenect2In::getColorTexId(int deviceNr)
{
	GLuint out = 0;
	if (nrDevices> deviceNr)
		out = streamPar[deviceNr][FNC_RGB]->texId;
	return out;
}

//---------------------------------------------------------------

GLuint Freenect2In::getColorTexId(std::string* serial)
{
	GLuint out = 0;
	if (nrDevices> serialIndMap[*serial])
		out = streamPar[serialIndMap[*serial]][FNC_RGB]->texId;
	return out;
}

//---------------------------------------------------------------

GLuint Freenect2In::getDepthTexId(int deviceNr)
{
	GLuint out = 0;
	if (nrDevices > deviceNr)
		out = streamPar[deviceNr][FNC_DEPTH]->texId;
	return out;
}


//---------------------------------------------------------------

GLuint Freenect2In::getDepthTexId(std::string* serial)
{
	GLuint out = 0;
	if (nrDevices > serialIndMap[*serial] )
		out = streamPar[serialIndMap[*serial] ][FNC_DEPTH]->texId;
	return out;
}

//---------------------------------------------------------------

GLuint Freenect2In::getIrTexId(int deviceNr)
{
	GLuint out = 0;
	if (nrDevices > deviceNr )
		out = streamPar[ deviceNr ][FNC_IR]->texId;
	return out;
}

//---------------------------------------------------------------

GLuint Freenect2In::getIrTexId(std::string* serial)
{
	GLuint out = 0;
	if (nrDevices> serialIndMap[*serial] )
		out = streamPar[serialIndMap[*serial] ][FNC_IR]->texId;
	return out;
}

//---------------------------------------------------------------

unsigned int Freenect2In::getDepthWidth(int deviceNr)
{
	return grayWidth;
}

//---------------------------------------------------------------

unsigned int Freenect2In::getDepthWidth(std::string* serial)
{
	return grayWidth;
}

//---------------------------------------------------------------

unsigned int Freenect2In::getDepthHeight(int deviceNr)
{
	return grayHeight;
}

//---------------------------------------------------------------

unsigned int Freenect2In::getDepthHeight(std::string* serial)
{
	return grayHeight;
}

//---------------------------------------------------------------

GLenum Freenect2In::getDepthFormat(int deviceNr)
{
	return GL_R32F;
}

//---------------------------------------------------------------

GLenum Freenect2In::getDepthFormat(std::string* serial)
{
	return GL_R32F;
}

//---------------------------------------------------------------

int Freenect2In::getFrameNr(int deviceNr)
{
	return uploadFrameCnt[deviceNr];
}

//---------------------------------------------------------------

int Freenect2In::getFrameNr(std::string* serial)
{
	return uploadFrameCnt[ serialIndMap[*serial] ];
}

//---------------------------------------------------------------

unsigned int Freenect2In::getNrDevices()
{
	return nrDevices;
}

//---------------------------------------------------------------

void Freenect2In::downloadToBuffer(int deviceNr)
{
	std::map<fnc_ImgType, libfreenect2::Frame*>::iterator iter;
	for (iter = framesIt[deviceNr].begin(); iter != framesIt[deviceNr].end();
			++iter)
	{
		libfreenect2::Frame* frame = iter->second;

		if (streamPar[deviceNr][iter->first]->use)
		{
			if (!streamPar[deviceNr][iter->first]->rawDataInited)
			{
				//printf("make buffer with bytes per pixel %d width %d hieght %d \n",
				//		frame->bytes_per_pixel, frame->width, frame->height);

				switch (iter->first)
				{
				case FNC_RGB:
					streamPar[deviceNr][FNC_RGB]->rawData =
							new unsigned char[colorWidth * colorHeight
									* frame->bytes_per_pixel];
					break;
				case FNC_IR:
					streamPar[deviceNr][FNC_IR]->rawData =
							new unsigned char[grayWidth * grayHeight
									* frame->bytes_per_pixel];
					break;
				case FNC_DEPTH:
					streamPar[deviceNr][FNC_DEPTH]->rawData =
							new unsigned char[grayWidth * grayHeight
									* frame->bytes_per_pixel];
					break;
				case FNC_REGIS:
					streamPar[deviceNr][FNC_REGIS]->rawData =
							new unsigned char[colorWidth * colorHeight
									* frame->bytes_per_pixel];
					break;
				}

				streamPar[deviceNr][iter->first]->bytes_per_pixel =
						frame->bytes_per_pixel;
				streamPar[deviceNr][iter->first]->rawDataInited = true;

			}
			else
			{
				std::copy(frame->data,
						frame->data + frame->width * frame->height * frame->bytes_per_pixel,
						streamPar[deviceNr][iter->first]->rawData);

				switch (iter->first)
				{
				case FNC_RGB:
					if(recording) recorder->downloadFrame( streamPar[deviceNr][FNC_RGB]->rawData );
					break;
				default:
					break;
				}
			}
		}
	}
}

//---------------------------------------------------------------

void Freenect2In::sigint_handler(int s)
{
	protonect_shutdown = true;
}

//---------------------------------------------------------------

//Doing non-trivial things in signal handler is bad. If you want to pause,
//do it in another thread.
//Though libusb operations are generally thread safe, I cannot guarantee
//everything above is thread safe when calling start()/stop() while
//waitForNewFrame().
void Freenect2In::sigusr1_handler(int s)
{
	if (devtopause == 0)
		return;
/// [pause]
	if (protonect_paused)
		devtopause->start();
	else
		devtopause->stop();
	protonect_paused = !protonect_paused;
/// [pause]
}

//---------------------------------------------------------------

void Freenect2In::startRec()
{
	recording = true;
	recorder->record();
}

//---------------------------------------------------------------

void Freenect2In::stopRec()
{
	recording = false;
	pa->join();
	std::cout << "stopping record" << std::endl;
	recorder->stop();
}

//---------------------------------------------------------------

void Freenect2In::myNanoSleep(uint32_t ns) {
	struct timespec tim;
	tim.tv_sec = 0;
	tim.tv_nsec = (long)ns;
	nanosleep(&tim, NULL);
}

//---------------------------------------------------------------

Freenect2In::~Freenect2In()
{
	//if (pipeline) delete pipeline;
	//delete rawData;
}

}

