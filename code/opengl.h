#ifndef OPENGL_H
#define OPENGL_H

#include "utils.h"
//#include <Windows.h>
#define APIENTRY __stdcall
#define WINGDIAPI __declspec(dllimport)

#include <GL/gl.h>
#include "KHR/glext.h"

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