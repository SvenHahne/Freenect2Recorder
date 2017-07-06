/*
 * Freenect2In.h
 *
 *  Created on: 12.01.2016
 *      Author: sven
 */

#ifndef FREENECT2_FREENECT2IN_H_
#define FREENECT2_FREENECT2IN_H_

#pragma once

#include <signal.h>
#include <vector>
#include <stdio.h>

#include <libfreenect2/config.h>
#include <libfreenect2/frame_listener.hpp>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "MediaRecorder.h"
#include "gl_header.h"

/*
#include <Quad.h>
#include <TextureManager.h>
*/

namespace tav
{

typedef struct
{
    int                     	nrChans = 0;
    bool                    	inited = false;
    bool                    	rawDataInited = false;
    bool                    	use = false;
    GLuint                  	width = 0;
    GLuint                  	height = 0;
    GLuint                  	texId = 0;
    GLenum                  	intFormat = 0;
    GLenum                  	extFormat = 0;
    GLenum                  	pixType = GL_UNSIGNED_BYTE;
    size_t						bytes_per_pixel;
    unsigned char*				rawData;
} fn_StreamPar;

enum fnc_ImgType { FNC_RGB=0, FNC_DEPTH=1, FNC_IR=2, FNC_REGIS=3 };

class Freenect2In
{
public:
	Freenect2In(const char* _file, unsigned int _sampleRate, unsigned int _recWidth, unsigned int _recHeight,
			const char* _selSerial);
	~Freenect2In();

    void draw();

	void start();
	void join();
	virtual void processQueue(unsigned short ind);
	void stop();

	void getGlFormat(fnc_ImgType type, fn_StreamPar* strPar);
	void allocate(fnc_ImgType type, int deviceNr);
	void uploadImg(fnc_ImgType type, int deviceNr = 0);

	void uploadColor(int deviceNr = 0);
	void uploadDepth(int deviceNr = 0);
	void uploadIr(int deviceNr = 0);

	void uploadColor(std::string* serial);
	void uploadDepth(std::string* serial);
	void uploadIr(std::string* serial);

	GLuint getColorTexId(int deviceNr = 0);
	GLuint getColorTexId(std::string* serial);
	GLuint getDepthTexId(int deviceNr = 0);
	GLuint getDepthTexId(std::string* serial);
	GLuint getIrTexId(int deviceNr = 0);
	GLuint getIrTexId(std::string* serial);

	unsigned int getDepthWidth(int deviceNr = 0);
	unsigned int getDepthWidth(std::string* serial);
	unsigned int getDepthHeight(int deviceNr = 0);
	unsigned int getDepthHeight(std::string* serial);
	GLenum getDepthFormat(int deviceNr = 0);
	GLenum getDepthFormat(std::string* serial);

	int getFrameNr(int deviceNr = 0);
	int getFrameNr(std::string* serial);

	unsigned int getNrDevices();

	void downloadToBuffer(int deviceNr = 0);


    static void sigint_handler(int s);
    static void sigusr1_handler(int s);
    void myNanoSleep(uint32_t ns);
    void startRec();
    void stopRec();

    static libfreenect2::Freenect2Device*		devtopause;
    static bool 								protonect_shutdown; ///< Whether the running application should shut down.
    static bool 								protonect_paused;
    bool										recording=false;
private:
   // ShaderCollector* 									shCol;

   // TextureManager*										rgbTex;	// rgba 8 bit
   // TextureManager*										irTex;	// red 32f
    MediaRecorder*										recorder;
    PAudio*												pa;

    libfreenect2::Freenect2 							freenect2;
    std::vector<libfreenect2::FrameMap>					frames;
    std::vector<libfreenect2::Freenect2Device*> 		dev;
    std::vector<libfreenect2::PacketPipeline*>			pipeline;
    std::vector<libfreenect2::Registration*>			registration;
    std::vector<libfreenect2::SyncMultiFrameListener*>	listener;
    std::vector<libfreenect2::Frame*>					undistorted;
    std::vector<libfreenect2::Frame*>					registered;

    std::vector<libfreenect2::Frame*>					rgb;
    std::vector<libfreenect2::Frame*>					ir;
    std::vector<libfreenect2::Frame*>					depth;

    std::vector< std::map<fnc_ImgType, fn_StreamPar*> > streamPar;
    std::vector< std::map<fnc_ImgType, libfreenect2::Frame*> > framesIt;

    std::vector<std::string>							serial;

    bool 												debug;
    bool 												initOk = true;
    bool 												texInited = false;
    bool												useOpenGl;
    bool												useCuda;
    bool												noDepth;
    bool												generateMips = false;
    bool												paudioIsReady = false;

    std::vector<int>									framecount;
    std::vector<int>									uploadFrameCnt;

    unsigned short										nrDevices = 0;

    std::vector<boost::thread*>							m_Thread;
    std::vector<boost::mutex*>							m_mtx;

    std::map<std::string, unsigned short>				serialIndMap;

    unsigned int 										recHeight;
    unsigned int 										recWidth;
    unsigned int 										sampleRate;

    unsigned int 										colorWidth = 1920;
    unsigned int 										colorHeight = 1080;
    unsigned int										grayWidth = 512;
    unsigned int										grayHeight = 424;
  //  GLenum												grayFormat = GL_R32F;
    const char* 										file;

    unsigned char*										testData;

    std::string 										selSerial;
    boost::mutex 										paMutex;

};

}

#endif /* FREENECT2_FREENECT2IN_H_ */
