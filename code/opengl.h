#ifndef OPENGL_H
#define OPENGL_H

#include "utils.h"

#define GLboolean bool
#define GLbyte i8
#define GLshort i16
#define GLint i32

#define GLubyte u8
#define GLushort u16
#define GLuint u32
#define GLbitfield u32
#define GLenum u32
#define GLsizei u32

#define GLfloat f32
#define GLdouble f64
#define GLclampf f32
#define GLclampd f64

#include "KHR/glext.h"
#include "KHR/wglext.h"

#undef GLboolean
#undef GLbyte
#undef GLshort
#undef GLint

#undef GLubyte
#undef GLushort
#undef GLuint
#undef GLbitfield
#undef GLenum
#undef GLsizei

#undef GLfloat
#undef GLdouble
#undef GLclampf
#undef GLclampd


#include <Windows.h>
#include <GL/gl.h>


PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLUSESHADERPROGRAMEXTPROC glUseShaderProgramEXT;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
PFNGLENABLEIPROC glEnablei;
PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM4FPROC glUniform4f;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

void gl_load_functions()
{
#define load_opengl_proc(type, name) name = (type)wglGetProcAddress(#name)
  load_opengl_proc(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT);
  
  load_opengl_proc(PFNGLBINDBUFFERPROC, glBindBuffer);
  load_opengl_proc(PFNGLGENBUFFERSPROC, glGenBuffers);
  load_opengl_proc(PFNGLBUFFERDATAPROC, glBufferData);
  load_opengl_proc(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
  load_opengl_proc(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
  load_opengl_proc(PFNGLCREATESHADERPROC, glCreateShader);
  load_opengl_proc(PFNGLSHADERSOURCEPROC, glShaderSource);
  load_opengl_proc(PFNGLCOMPILESHADERPROC, glCompileShader);
  load_opengl_proc(PFNGLGETSHADERIVPROC, glGetShaderiv);
  load_opengl_proc(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
  load_opengl_proc(PFNGLCREATEPROGRAMPROC, glCreateProgram);
  load_opengl_proc(PFNGLATTACHSHADERPROC, glAttachShader);
  load_opengl_proc(PFNGLLINKPROGRAMPROC, glLinkProgram);
  load_opengl_proc(PFNGLVALIDATEPROGRAMPROC, glValidateProgram);
  load_opengl_proc(PFNGLDELETESHADERPROC, glDeleteShader);
  load_opengl_proc(PFNGLUSEPROGRAMPROC, glUseProgram);
  load_opengl_proc(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);
  load_opengl_proc(PFNGLENABLEIPROC, glEnablei);
  load_opengl_proc(PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl);
  load_opengl_proc(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
  load_opengl_proc(PFNGLUNIFORM4FPROC, glUniform4f);
  load_opengl_proc(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
  load_opengl_proc(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
  load_opengl_proc(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
  load_opengl_proc(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
}


struct gl_ParseResult
{
  String vertex;
  String fragment;
};

gl_ParseResult gl_parse_glsl(String src)
{
  String vertex_search_string = const_string("#shader vertex");
  i32 vertex_index = find_index(src, vertex_search_string);
  String fragment_search_string = const_string("#shader fragment");
  i32 fragment_index = find_index(src, fragment_search_string);
  
  gl_ParseResult result;
  result.vertex = make_string(src.data + vertex_index +
                              vertex_search_string.count, 
                              fragment_index - vertex_index - 
                              vertex_search_string.count);
  result.fragment = make_string(src.data + fragment_index + 
                                fragment_search_string.count,
                                src.count - fragment_index - 
                                fragment_search_string.count);
  
  return result;
}



u32 gl_compile_shader(u32 type, String src)
{
  u32 id = glCreateShader(type);
  glShaderSource(id, 1, &src.data, (i32 *)&src.count);
  glCompileShader(id);
  
  i32 compile_status;
  glGetShaderiv(id, GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE)
  {
    i32 length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    char *message = (char *)temp_alloc(length);
    glGetShaderInfoLog(id, length, &length, message);
    assert(false);
  }
  
  return id;
}

u32 gl_create_shader(String vertex_src, String fragment_src)
{
  u32 program = glCreateProgram();
  u32 vertex_shader = gl_compile_shader(GL_VERTEX_SHADER, vertex_src);
  u32 fragment_shader = gl_compile_shader(GL_FRAGMENT_SHADER, fragment_src);
  
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glValidateProgram(program);
  
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  return program;
}

#endif