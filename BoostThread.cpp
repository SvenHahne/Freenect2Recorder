/*
 *  BoostThread.cpp
 *  ta_visualizer
 *
 *  Created by user on 11.01.11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include <iostream>

#include "BoostThread.h"

BoostThread::BoostThread()
{
	// the thread is not-a-thread until we call start()
}

BoostThread::~BoostThread() 
{

}
void BoostThread::start(int N)
{
	m_Thread = boost::thread(&BoostThread::processQueue, this, N);
}

void BoostThread::join()
{
	m_Thread.join();
}

void BoostThread::processQueue(unsigned N)
{
//	std::cout << "processing"  << std::endl;	
	//boost::this_thread::sleep(workTime);
}