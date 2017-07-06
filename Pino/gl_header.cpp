#include "gl_header.h"
#include <iostream>


void getGlError()
{
    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        switch (glErr)
        {
            case GL_INVALID_ENUM:
                std::cerr << "GL Error: GL_INVALID_ENUM" << std::endl;
                break;
            case GL_INVALID_VALUE:
            	std::cerr << "GL Error: GL_INVALID_VALUE, A numeric argument is out of range" << std::endl;
                break;
            case GL_INVALID_OPERATION:
            	std::cerr << "GL Error: GL_INVALID_OPERATION, The specified operation is not allowed in the current state." << std::endl;
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
            	std::cerr << "GL Error: GL_INVALID_FRAMEBUFFER_OPERATION, The framebuffer object is not complete." << std::endl;
                break;
            case GL_OUT_OF_MEMORY:
            	std::cerr << "GL Error: GL_OUT_OF_MEMORY, GL_OUT_OF_MEMORY" << std::endl;
                break;
        }
    }
}
