/*
 * main.cpp
 *
 *  Created on: 13.02.2017
 *      Author: sven
 */


#include <iosfwd>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include "gl_header.h"
#include <GLFW/glfw3.h>
#include "Freenect2In.h"
#include "Shaders.h"

int scrWidth = 800;
int scrHeight = 450;

GLuint vao;
GLuint pos_buf;
GLuint tex_buf;
GLFWwindow* window;
GLint vpos_location, vcol_location;

const char* savePath;
tav::Shaders* texShader;
tav::Freenect2In* fnc;
const PAudio* pa;

static const struct
{
    float x, y;
    float u, v;
} vertices[6] =
{
    { -1.f, -1.f, 0.f, 1.f },
    {  1.f, -1.f, 1.f, 1.f },
    {  1.f,  1.f, 1.f, 0.f },
    {  1.f,  1.f, 1.f, 0.f },
    { -1.f,  1.f, 0.f, 0.f },
    { -1.f, -1.f, 0.f, 1.f }
};

static const char* vertex_shader_text =
"attribute vec2 vTex;\n"
"attribute vec2 vPos;\n"
"varying vec2 tex_coord;\n"
"void main()\n"
"{\n"
"    gl_Position = vec4(vPos, 0.0, 1.0);\n"
"    tex_coord = vTex;\n"
"}\n";

static const char* fragment_shader_text =
"varying vec2 tex_coord;\n"
"uniform sampler2D tex;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D( tex, tex_coord);\n"
"}\n";


static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        fnc->stopRec();
        fnc->join();
    }

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
    	if (!fnc->recording)
    	{
    		fnc->startRec();

    		std::cout <<  "save to " << savePath  << std::endl;
    	} else
    	{
    		fnc->stopRec();
    	}
    }
}

void initQuad()
{
    glGenBuffers(1, &pos_buf);
    glBindBuffer(GL_ARRAY_BUFFER, pos_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    vpos_location = glGetAttribLocation(texShader->getProgram(), "vPos");
    vcol_location = glGetAttribLocation(texShader->getProgram(), "vTex");

    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (void*) 0);
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (void*) (sizeof(float) * 2));
}

void initGlfw()
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
//    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    window = glfwCreateWindow(scrWidth, scrHeight, "Simple example", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

	// init glew
	glewExperimental = GL_TRUE;
	if( GLEW_OK != glewInit() )  exit(EXIT_FAILURE);
	glGetError();    // delete glew standard error (bug in glew)
	printf("using GLEW %s\n", glewGetString(GLEW_VERSION));

}

int main(int argc, char *argv[])
{
	double startTime, endTime;
	unsigned int disp=0;
	unsigned int sampleRate=44100;

	initGlfw();

	if (argc > 0)
	{
		savePath = static_cast<const char*>(argv[1]);
		sampleRate = std::atoi(argv[2]);

		const char* ser = "144002333747";
		fnc = new tav::Freenect2In(savePath, sampleRate, 1920, 1080, ser); // Protonect starten und lesen
		//myNanoSleep(200000000);

		texShader = new tav::Shaders(vertex_shader_text, fragment_shader_text, false);
		texShader->link();
		initQuad();

		// start loop
		while (!glfwWindowShouldClose(window))
		{
			endTime = glfwGetTime();

			if(disp == 60){
				disp = 0;
				std::cout << "fps: " << 1.0 / (endTime - startTime) << std::endl;
			}
			startTime = glfwGetTime();
			disp++;

			//--------------------------------------------

			glViewport(0, 0, scrWidth, scrHeight);

			fnc->uploadColor(0);	// get a new image from the kinect if there is one

			texShader->begin();
			texShader->setUniform1i("tex", 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, fnc->getColorTexId(0));

			glDrawArrays(GL_TRIANGLES, 0, 6);

			glfwSwapBuffers(window);
			glfwPollEvents();
		}
	} else
	{
		printf("Please use this Application with the parameters [0]: absolute file path to save to, [1]: input sample rate\n");
	}

	glfwDestroyWindow(window);

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
