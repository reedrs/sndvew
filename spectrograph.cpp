#include <GL/glew.h>
//#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>

#include "stb_image.h"
#include "spectrograph.h"
#include <complex.h>
#include <fftw3.h>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <string.h>

void is_bad_problem() {
    switch(glGetError()) {
	break;
	case GL_NO_ERROR:
           fprintf(stderr, "No error has been recorded. The value of this symbolic constant is guaranteed to be 0.\n");

	   break;
	case GL_INVALID_ENUM:
           fprintf(stderr, "An unacceptable value is specified for an enumerated argument. The offending command is ignored and has no other\n");
           fprintf(stderr, "side effect than to set the error flag.\n");

	   break;
	case        GL_INVALID_VALUE:
           fprintf(stderr, "A numeric argument is out of range. The offending command is ignored and has no other side effect than to set the\n");
           fprintf(stderr, "error flag.\n");

	   break;
	case        GL_INVALID_OPERATION:
           fprintf(stderr, "The specified operation is not allowed in the current state. The offending command is ignored and has no other side\n");
           fprintf(stderr, "effect than to set the error flag.\n");

	   break;
	case        GL_INVALID_FRAMEBUFFER_OPERATION:
           fprintf(stderr, "The framebuffer object is not complete. The offending command is ignored and has no other side effect than to set\n");
           fprintf(stderr, "the error flag.\n");

	   break;
	case        GL_OUT_OF_MEMORY:
           fprintf(stderr, "There is not enough memory left to execute the command. The state of the GL is undefined, except for the state of\n");
           fprintf(stderr, "the error flags, after this error is recorded.\n");

	   break;

	default:
	   fprintf(stderr, "wtf even\n");
    }
}


GLint compile_shader(const char* vertex_shader_source, const char* fragment_shader_source) {
    enum Consts {INFOLOG_LEN = 512};
    GLchar infoLog[INFOLOG_LEN];
    GLint fragment_shader;
    GLint shader_program;
    GLint success;
    GLint vertex_shader;

    /* Vertex shader */
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, INFOLOG_LEN, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
    }

    /* Fragment shader */
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, INFOLOG_LEN, NULL, infoLog);
        printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
    }

    /* Link shaders */
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader_program, INFOLOG_LEN, NULL, infoLog);
        printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return shader_program;
}

Spectrograph::Spectrograph() {
	this->width = 1920/2;
	this->height = 1080/2;

	this->fft_samples = 1024;
	this->fft_input_buffer = new double[fft_samples];
	this->fft_buffer = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * this->fft_samples*2);
}
Spectrograph::Spectrograph(glm::vec3 pos, glm::vec3 size) : Graph(pos, size) {
    this->width = size.x;
    this->height = size.y;
    this->fft_samples = 1024;
    this->fft_input_buffer = new double[fft_samples];
    this->fft_buffer = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * this->fft_samples*2);
}

void Spectrograph::setSize(int x, int y) {
    this->size.x = x;
    this->size.y = y;

    this->vertices[1] = this->size[1];
    this->vertices[16] = this->size[1];
    this->vertices[21] = this->size[1];

    this->vertices[5] = this->size[0];
    this->vertices[20] = this->size[0];
    this->vertices[25] = this->size[0];
}

void Spectrograph::setup(AudioBuffer* audio_buffer, const char* gradient_filename) {

	this->audio_buffer = audio_buffer;
	this->fft_plan = fftw_plan_dft_r2c_1d(this->fft_samples, this->fft_input_buffer, this->fft_buffer, FFTW_ESTIMATE);

	this->shader = compile_shader(this->vsh_source, this->fsh_source);
	GLint pos = glGetAttribLocation(this->shader, "position");
	GLint tex_pos = glGetAttribLocation(this->shader, "a_texCoord");

	GLfloat vertices[] = {
		0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

		0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
	};
	memcpy(this->vertices, vertices, sizeof(GLfloat)*30);

	this->setSize(width, height);

	glGenBuffers(1, &this->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(this->vertices), this->vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (GLvoid*)0);
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glVertexAttribPointer(tex_pos, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(pos);
	glEnableVertexAttribArray(tex_pos);

	
	
	glGenTextures(1, &this->texture);
	glBindTexture(GL_TEXTURE_2D, this->texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	glGenTextures(1, &this->gradient_tex);
	glBindTexture(GL_TEXTURE_2D, this->gradient_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int tex_w, tex_h, tex_channels;
	unsigned char* tex_data = stbi_load("gradient.png", &tex_w, &tex_h, &tex_channels, 0);
	if (!tex_data) {
	    fprintf(stderr, "Failed to load image texture %s\n", gradient_filename);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
	// stbi_image_free(tex_data);


	is_bad_problem();

	glUseProgram(this->shader);

	GLuint tex_u;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->gradient_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, this->texture);

	tex_u = glGetUniformLocation(this->shader, "gradient");
	glUniform1i(tex_u, 0);

	tex_u = glGetUniformLocation(this->shader, "audio_data");
	glUniform1i(tex_u, 1);

	texturebuf = new GLfloat[this->width * this->height];

	// GLfloat buf[this->width][this->height];
	for (int i=0; i<this->width; i++) {
	    for (int j=0; j<this->height; j++) {
			texturebuf[i + j*width] = 0.00f;
		}
	}

	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, this->width, this->height, 0, GL_RED, GL_FLOAT, texturebuf);


	glm::mat4 view          = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
        glm::mat4 projection    = glm::mat4(1.0f);
	projection = glm::ortho(0.0f, 1920.0f, 1080.0f, 0.0f, 0.1f, 100.0f);
        view       = glm::translate(view, glm::vec3(position.x, position.y, -3.0f));

	GLint viewAddr = glGetUniformLocation(this->shader, "view");
	glUniformMatrix4fv(viewAddr, 1, GL_FALSE, glm::value_ptr(view));

	GLint projAddr = glGetUniformLocation(this->shader, "proj");
	glUniformMatrix4fv(projAddr, 1, GL_FALSE, glm::value_ptr(projection));

	GLint xoffsetAddr = glGetUniformLocation(this->shader, "x_offset");
	glUniform1f(xoffsetAddr, 0.0f);

}

void Spectrograph::update() {
    while (audio_buffer->unread() >= fft_samples) {
	audio_buffer->pop(this->fft_input_buffer, fft_samples);
	// memcpy(this->fft_input_buffer, samples, fft_samples);
	fftw_execute(this->fft_plan);

	for (int j=0; j<this->height; j++) {
	    texturebuf[current_col + j*width] = this->fft_buffer[j][1]*.1;
	    // texturebuf[current_col + j*width] = (float)(j)/this->height;
	}

	current_col+=1;
	if (current_col >= this->width)
	    current_col = 0;
    }

    glUseProgram(this->shader);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->texture);

    // glTexSubImage2D(GL_TEXTURE_2D, 0, current_col, 0, 1, this->height, GL_RED, GL_FLOAT, texturebuf);

    // would be nice to use a float internal format like GL_R16F but it doesn't seem to be supported on the pi?
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, this->width, this->height, 0, GL_RED, GL_FLOAT, texturebuf);

    GLint xoffsetAddr = glGetUniformLocation(this->shader, "x_offset");
    //fprintf(stderr, "%f\n", (float)(current_col)/width);
    glUniform1f(xoffsetAddr, (float)(current_col)/width);
    //is_bad_problem();


}

void Spectrograph::draw() {
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->gradient_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, this->texture);

	glUseProgram(this->shader);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

