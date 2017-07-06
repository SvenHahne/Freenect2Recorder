//////////////////////////////////////////////////////////////////////////////
//
//  --- LoadShaders.h ---
//
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string>
//#include <glm/glm.hpp>
#include "gl_header.h"


#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif



namespace tav {

class Shaders {
public:
    Shaders();
    Shaders(const char* vert, const char* frag, bool readFromFile = false);
    Shaders(const char* vert, const char* geom, const char* frag, bool readFromFile = false);
    Shaders(const char* vert, const char* tessCtr, const char* tessEv, const char* geom, const char* frag, bool readFromFile=false);

    ~Shaders();

    bool    addVertSrc(const char* vert, bool readFromFile = false);
    bool    addContrSrc(const char* contr, bool readFromFile = false);
    bool    addEvalSrc(const char* eval, bool readFromFile = false);
    bool    addGeomSrc(const char* geom, bool readFromFile = false);
    bool    addFragSrc(const char* frag, bool readFromFile = false);
    
    GLuint  create();
    void    begin();
    void    end();
    void    link();
    void    remove();

    // GLES 2.0
    void	bindAttribLocation(unsigned int pos, std::string name);

    GLuint& getProgram();
    GLint   getUniformLocation(const std::string & name);
    
    void    setUniform1ui(const std::string & name, unsigned int v1);
    void    setUniform2ui(const std::string & name, unsigned int v1, unsigned int v2);
    void    setUniform3ui(const std::string & name, unsigned int v1, unsigned int v2, unsigned int v3);
    void    setUniform4ui(const std::string & name, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4);
    
    void    setUniform1i(const std::string & name, int v1);
	void    setUniform2i(const std::string & name, int v1, int v2);
	void    setUniform3i(const std::string & name, int v1, int v2, int v3);
	void    setUniform4i(const std::string & name, int v1, int v2, int v3, int v4);
	
	void    setUniform1f(const std::string & name, float v1);
	void    setUniform2f(const std::string & name, float v1, float v2);
	void    setUniform3f(const std::string & name, float v1, float v2, float v3);
	void    setUniform4f(const std::string & name, float v1, float v2, float v3, float v4);
	
	// set an array of uniform values
	void    setUniform1iv(const std::string & name, int* v, int count = 1);
	void    setUniform2iv(const std::string & name, int* v, int count = 1);
	void    setUniform3iv(const std::string & name, int* v, int count = 1);
	void    setUniform4iv(const std::string & name, int* v, int count = 1);
	
	void    setUniform1fv(const std::string & name, float* v, int count = 1);
	void    setUniform2fv(const std::string & name, float* v, int count = 1);
	void    setUniform3fv(const std::string & name, float* v, int count = 1);
	void    setUniform4fv(const std::string & name, float* v, int count = 1);

    void    setUniformMatrix3fv(const std::string & name, float* v, int count = 1);
    void    setUniformMatrix4fv(const std::string & name, float* v, int count = 1);
  //  void    setIdentMatrix4fv(const std::string & name);

    bool    debug = false;
    bool    needsTime = false;
private:
    void    attachShader(GLuint program, GLenum type, const char* src);
    void    checkStatusLink(GLuint obj);
    void    checkStatusCompile(GLuint obj);

    char*   textFileRead(const char *fn);
    void    getGlVersions();
    void    pathToResources();
    
    GLuint  program;
    char*   vs = NULL;
    char*   cs = NULL;
    char*   es = NULL;
    char*   gs = NULL;
    char*   fs = NULL;
    bool    bLoaded = false;
  //  glm::mat4 identMat;
};
    
}
