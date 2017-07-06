#include "Shaders.h"


namespace tav
{
    Shaders::Shaders()
    {
        //getGlVersions();
        debug = false;
    }

    
    //--------------------------------------------------------------
    Shaders::Shaders(const char* vert, const char* frag, bool readFromFile)
    {
        if (readFromFile)
        {
            if(vert) vs = textFileRead(vert);
            if(frag) fs = textFileRead(frag);
        } else
        {
            if(vert) vs = (char*) vert;
            if(frag) fs = (char*) frag;
        }
        create();
    }

    
    //--------------------------------------------------------------
    Shaders::Shaders(const char* vert, const char* geom, const char* frag, bool readFromFile)
    {
        if (readFromFile)
        {
            vs = textFileRead(vert);
            gs = textFileRead(geom);
            fs = textFileRead(frag);
        } else
        {
            vs = (char*) vert;
            gs = (char*) geom;
            fs = (char*) frag;
        }
        
        create();
    }
    
    
    //--------------------------------------------------------------
    Shaders::Shaders(const char* vert, const char* tessCtr, const char* tessEv, const char* geom,
    		const char* frag, bool readFromFile)
    {
        if (readFromFile)
        {
            vs = textFileRead(vert);
            if(tessCtr) cs = textFileRead(tessCtr);
            if(tessEv) es = textFileRead(tessEv);
            if(geom) gs = textFileRead(geom);
            fs = textFileRead(frag);
        } else
        {
            vs = (char*) vert;
            if(tessCtr) { cs = (char*) tessCtr; }
            if(tessEv) { es = (char*) tessEv; }
            if(geom) { gs = (char*) geom; }
            fs = (char*) frag;
        }

        create();
    }


    //--------------------------------------------------------------
    Shaders::~Shaders()
    {
        remove();
        
        if(vs) delete [] vs;
        if(cs) delete [] cs;
        if(es) delete [] es;
        if(gs) delete [] gs;
        if(fs) delete [] fs;
    }
    
    
    //--------------------------------------------------------------
    // add text sources
    bool Shaders::addVertSrc(const char* vert, bool readFromFile)
    {
        bool ret = true;
        
        pathToResources();
        if (readFromFile)
        {
            vs = textFileRead(vert);
        } else {
            vs = (char*) vert;
        }

        return ret;
    }
    

    //--------------------------------------------------------------
    bool Shaders::addContrSrc(const char* contr, bool readFromFile)
    {
        bool ret = true;
        
        pathToResources();
        if (readFromFile)
        {
            cs = textFileRead(contr);
        } else {
            cs = (char*) contr;
        }
        
        return ret;
    }

    
    //--------------------------------------------------------------
    bool Shaders::addEvalSrc(const char* eval, bool readFromFile)
    {
        bool ret = true;
        
        pathToResources();
        if (readFromFile)
        {
            es = textFileRead(eval);
        } else {
            es = (char*) eval;
        }
        
        return ret;
    }
    

    //--------------------------------------------------------------
    bool Shaders::addGeomSrc(const char* geom, bool readFromFile)
    {
        bool ret = true;
        
        pathToResources();
        if (readFromFile)
        {
            gs = textFileRead(geom);
        } else {
            gs = (char*) geom;
        }
        
        return ret;
    }

    
    //--------------------------------------------------------------
    bool Shaders::addFragSrc(const char* frag, bool readFromFile)
    {
        bool ret = true;
        
        pathToResources();
        if (readFromFile)
        {
            fs = textFileRead(frag);
        } else {
            fs = (char*) frag;
        }
        
        return ret;
    }

    //--------------------------------------------------------------
    // create programm
    GLuint Shaders::create()
    {
        program = glCreateProgram();

        if (vs) attachShader( program, GL_VERTEX_SHADER, vs );
#ifndef __EMSCRIPTEN__
        if (cs) { attachShader( program, GL_TESS_CONTROL_SHADER, cs ); printf("add tess contr \n"); }
        if (es) { attachShader( program, GL_TESS_EVALUATION_SHADER, es ); printf("add tess eval \n");}
        if (gs) attachShader( program, GL_GEOMETRY_SHADER, gs );
#endif
        if (fs) attachShader( program, GL_FRAGMENT_SHADER, fs );

        bLoaded = true;
        
      //  identMat = glm::mat4(1.f);

        return program;
    }


    //--------------------------------------------------------------
    void Shaders::checkStatusLink( GLuint obj )
    {
        GLint status = GL_FALSE, len = 10;
        
        if( glIsProgram(obj) )  glGetProgramiv( obj, GL_LINK_STATUS, &status );
        
        if( status != GL_TRUE )
        {
            std::cerr << "shader didn´t link!!!"<< std::endl;
            
            if( glIsShader(obj) )   glGetShaderiv( obj, GL_INFO_LOG_LENGTH, &len );
            if( glIsProgram(obj) )  glGetProgramiv( obj, GL_INFO_LOG_LENGTH, &len );
            
            GLchar* log = new GLchar[len];
            if( glIsShader(obj) )   glGetShaderInfoLog( obj, len, NULL, &log[0] );
            if( glIsProgram(obj) )  glGetProgramInfoLog( obj, len, NULL, &log[0] );
            std::cout << "shader log: " << log << std::endl << std::endl;
            std::cout << "shader vertex code: log: " << vs << std::endl << std::endl;
            if (gs != 0) std::cout << "shader geometry code: log: " << gs << std::endl << std::endl;
            std::cout << "shader fragment code: " << std::endl << fs << std::endl << std::endl;
        }
    }

    //--------------------------------------------------------------
    void Shaders::checkStatusCompile( GLuint obj )
    {
        GLint status = GL_FALSE, len = 10;
        if( glIsShader(obj) ) glGetShaderiv( obj, GL_COMPILE_STATUS, &status );
        
        if( status != GL_TRUE )
        {
            std::cerr << "shader didn´t compile!!!"<< std::endl;
            
            if( glIsShader(obj) )   glGetShaderiv( obj, GL_INFO_LOG_LENGTH, &len );
            if( glIsProgram(obj) )  glGetProgramiv( obj, GL_INFO_LOG_LENGTH, &len );

            GLchar* log = new GLchar[len];
            if( glIsShader(obj) )   glGetShaderInfoLog( obj, len, NULL, &log[0] );
            if( glIsProgram(obj) )  glGetProgramInfoLog( obj, len, NULL, &log[0] );
            std::cout << "shader log: " << log << std::endl << std::endl;
            std::cout << "shader vertex code: " << std::endl << vs << std::endl << std::endl;
            std::cout << "shader fragment code: " << std::endl << fs << std::endl << std::endl;
        }
    }

    //--------------------------------------------------------------
    void Shaders::attachShader( GLuint program, GLenum type, const char* src )
    {
        if(debug)
        {
            std::cout << "attach ";
            switch (type) {
                case GL_VERTEX_SHADER:
                    std::cout << "GL_VERTEX_SHADER" << std::endl;
                    break;
                case GL_TESS_CONTROL_SHADER:
                    std::cout << "GL_TESS_CONTROL_SHADER" << std::endl;
                    break;
                case GL_TESS_EVALUATION_SHADER:
                    std::cout << "GL_TESS_EVALUATION_SHADER" << std::endl;
                    break;
                case GL_GEOMETRY_SHADER:
                    std::cout << "GL_GEOMETRY_SHADER" << std::endl;
                    break;
                case GL_FRAGMENT_SHADER:
                    std::cout << "GL_FRAGMENT_SHADER" << std::endl;
                    break;
                default:
                    break;
            }
        }
        
        GLuint shader = glCreateShader(type);
        
        glShaderSource( shader, 1, &src, NULL );
        glCompileShader( shader );
        
        checkStatusCompile( shader );
        
        glAttachShader( program, shader );
        glDeleteShader( shader );
    }


    //--------------------------------------------------------------
    void Shaders::begin()
    {
        glUseProgram(program);
    }
    
    //--------------------------------------------------------------
    void Shaders::end()
    {
        glUseProgram(0);
    }

    //--------------------------------------------------------------
    void Shaders::link()
    {
        glLinkProgram(program);
        checkStatusLink(program);
    }

    //--------------------------------------------------------------

    void Shaders::remove()
    {
        glUseProgram(0);
        glDeleteShader(program);
    }
    
    //--------------------------------------------------------------
    void Shaders::bindAttribLocation(unsigned int pos, std::string name)
    {
#ifdef USE_WEBGL
    	glBindAttribLocation ( program, pos, name.c_str() );
#endif
    }

    //--------------------------------------------------------------

    GLuint& Shaders::getProgram()
    {
        return program;
    }

    //--------------------------------------------------------------

    char* Shaders::textFileRead(const char *fn)
    {
        FILE *fp;
        char *content = NULL;
        
        int count=0;
        
        if (fn != NULL) {
            
            fp = fopen(fn,"rt");
            
            if (fp != NULL) {
                
                fseek(fp, 0, SEEK_END);
                count = (int)ftell(fp);
                rewind(fp);
                
                if (count > 0) {
                    content = (char *) malloc(sizeof(char) * (count+1));
                    count = (int)fread(content,sizeof(char),count,fp);
                    content[count] = '\0';
                }
                fclose(fp);
                
            } else {
                std::cout << "tav::Shaders Error: file not found" << std::endl;
            }
        }
        return content;
    }

    //--------------------------------------------------------------
    void Shaders::getGlVersions()
    {
        const char *verstr = (const char*) glGetString(GL_VERSION);
        std::cout << "OpenGL Version: " << verstr << std::endl;
        const char *slVerstr = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
        std::cout << "Glsl Version: " << slVerstr << std::endl;
    }
    
    //--------------------------------------------------------------
    GLint Shaders::getUniformLocation(const std::string & name) {
        GLint loc = -1;

        // Desktop GL seems to be faster fetching the value from the GPU each time
        // than retrieving it from cache.
        loc = glGetUniformLocation(program, name.c_str());
        
        return loc;
    }
    
    
    //--------------------------------------------------------------
    void Shaders::setUniform1ui(const std::string & name, unsigned int v1) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform1ui(loc, v1);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform2ui(const std::string & name, unsigned int v1, unsigned int v2) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform2ui(loc, v1, v2);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform3ui(const std::string & name, unsigned int v1, unsigned int v2, unsigned int v3) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform3ui(loc, v1, v2, v3);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform4ui(const std::string & name, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) 	glUniform4ui(loc, v1, v2, v3, v4);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform1i(const std::string & name, int v1) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform1i(loc, v1);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform2i(const std::string & name, int v1, int v2) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform2i(loc, v1, v2);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform3i(const std::string & name, int v1, int v2, int v3) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform3i(loc, v1, v2, v3);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform4i(const std::string & name, int v1, int v2, int v3, int v4) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) 	glUniform4i(loc, v1, v2, v3, v4);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform1f(const std::string & name, float v1) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform1f(loc, v1);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform2f(const std::string & name, float v1, float v2) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform2f(loc, v1, v2);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform3f(const std::string & name, float v1, float v2, float v3) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform3f(loc, v1, v2, v3);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform4f(const std::string & name, float v1, float v2, float v3, float v4) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform4f(loc, v1, v2, v3, v4);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform1iv(const std::string & name, int* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform1iv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform2iv(const std::string & name, int* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform2iv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform3iv(const std::string & name, int* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform3iv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform4iv(const std::string & name, int* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform4iv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform1fv(const std::string & name, float* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform1fv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform2fv(const std::string & name, float* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform2fv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform3fv(const std::string & name, float* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform3fv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniform4fv(const std::string & name, float* v, int count) {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniform4fv(loc, count, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniformMatrix3fv(const std::string & name, float* v, int count)
    {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniformMatrix3fv(loc, count, GL_FALSE, v);
        }
    }
    
    //--------------------------------------------------------------
    void Shaders::setUniformMatrix4fv(const std::string & name, float* v, int count)
    {
        if(bLoaded) {
            int loc = getUniformLocation(name);
            if (loc != -1) glUniformMatrix4fv(loc, count, GL_FALSE, v);
        }
    }
    
    //--------------------------------------------------------------
//    void Shaders::setIdentMatrix4fv(const std::string & name)
//    {
//      //  setUniformMatrix4fv(name, &identMat[0][0]);
//    }

    //--------------------------------------------------------------
    void Shaders::pathToResources()
    {
#ifdef __APPLE__
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
        char path[PATH_MAX];
        if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)) printf("error setting path to resources \n");
        CFRelease(resourcesURL);
        chdir(path);
#endif
    }

}   // end namespace
