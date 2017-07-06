/*
 *  BoostThread.h
 *  ta_visualizer
 *
 *  Created by Sven Hahne on 11.01.11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _BOOSTTHREAD_
#define _BOOSTTHREAD_

#include <iostream>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>


class BoostThread
	{
	public:
		BoostThread();		
		~BoostThread();		
		void start(int N);
		void join();
		virtual void processQueue(unsigned N);
	private:		
		boost::thread m_Thread;
	};


#endif