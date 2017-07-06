//
//  gl_header.h
//  tav_gl4
//
//  Created by user on 05.07.14.
//  Copyright (c) 2014 user. All rights reserved.
//

#pragma once

#include <GL/glew.h>

#ifdef __linux__
	#ifndef __EMSCRIPTEN__
		#define GLFW_INCLUDE_GL_3
		#include <GL/gl.h>     // core profile
		#include <GL/glext.h>     // core profile
		//#include <GL/glx.h>
	#endif
#elif __APPLE__
	#include <OpenGL/OpenGL.h>     // core profile
	#include <OpenGL/gl3.h>     // core profile
	#include <OpenGL/gl3ext.h>  // core profile
#endif

void getGlError();
