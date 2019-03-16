#include "platform.h"
#include "asteroids.cpp"
#include "opengl.h"
#include <xaudio2.h>
#include <Windows.h>
#include "KHR/wglext.h"



void gl_load_functions()
{
#define load_opengl_proc(name) *(u64 *)&name = (u64)wglGetProcAddress(#name)
  load_opengl_proc(glBindBuffer);
  load_opengl_proc(glGenBuffers);
  load_opengl_proc(glBufferData);
  load_opengl_proc(glVertexAttribPointer);
  load_opengl_proc(glEnableVertexAttribArray);
  load_opengl_proc(glCreateShader);
  load_opengl_proc(glShaderSource);
  load_opengl_proc(glCompileShader);
  load_opengl_proc(glGetShaderiv);
  load_opengl_proc(glGetShaderInfoLog);
  load_opengl_proc(glCreateProgram);
  load_opengl_proc(glAttachShader);
  load_opengl_proc(glLinkProgram);
  load_opengl_proc(glValidateProgram);
  load_opengl_proc(glDeleteShader);
  load_opengl_proc(glUseProgram);
  load_opengl_proc(glDebugMessageCallback);
  load_opengl_proc(glEnablei);
  load_opengl_proc(glDebugMessageControl);
  load_opengl_proc(glGetUniformLocation);
  load_opengl_proc(glUniform4f);
  load_opengl_proc(glGenVertexArrays);
  load_opengl_proc(glBindVertexArray);
  load_opengl_proc(glDeleteBuffers);
  load_opengl_proc(glDeleteVertexArrays);
}


PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

struct Win32AppState
{
  b32 running;
};


u64 global_counts_per_second;
Win32AppState global_app_state;


ALLOCATOR(heap_allocator)
{
  byte *result = 0;
  switch (mode)
  {
    case AllocatorMode_ALLOCATE:
    {
      result = (byte *)VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE,
                                    PAGE_READWRITE);
    } break;
    
    case AllocatorMode_FREE:
    {
      b32 success = VirtualFree(old_memory_ptr, 0, MEM_RELEASE);
      assert(success);
    } break;
    
    invalid_default_case();
  }
  return result;
}


LRESULT CALLBACK WindowProc(HWND window,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam)
{
  switch (message)
  {
    case WM_DESTROY:
    case WM_CLOSE:
    case WM_QUIT: 
    {
      global_app_state.running = false;
    } break;
    
    default: {
      return DefWindowProcA(window, message, wParam, lParam);
    }
  }
  return 0;
}


String win32_get_work_dir()
{
  String full_path;
  full_path.data = (char *)temp_alloc(MAX_PATH);
  full_path.count = GetModuleFileNameA(0, full_path.data, MAX_PATH);
  assert(full_path.count);
  
  u32 last_slash_index = find_last_index(full_path, const_string("\\"));
  String result = substring(full_path, 0, last_slash_index + 1);
  
#ifdef ASTEROIDS_PROD
  String path_end = const_string("../data/");
#else
  String path_end = const_string("../data/");
#endif
  
  result = concat(result, path_end);
  return result;
}

String platform_read_entire_file(String file_name)
{
  String path = win32_get_work_dir();
  String full_name = concat(path, file_name);
  char *c_file_name = temp_c_string(full_name);
  HANDLE file = CreateFileA(c_file_name,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            0,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            0);
  
  assert(file != INVALID_HANDLE_VALUE);
  
  LARGE_INTEGER file_size_li;
  GetFileSizeEx(file, &file_size_li);
  u64 file_size = file_size_li.QuadPart;
  
  char *buffer = (char *)alloc(file_size);
  u32 bytes_read;
  ReadFile(file, buffer, (DWORD)file_size, (LPDWORD)&bytes_read, 0);
  assert(bytes_read == file_size);
  
  String result = make_string(buffer, (u32)file_size);
  return result;
}

void APIENTRY opengl_debug_callback(GLenum source,
                                    GLenum type,
                                    GLuint id,
                                    GLenum severity,
                                    GLsizei length,
                                    const GLchar* message,
                                    const void* userParam)
{
  OutputDebugStringA(message);
  __debugbreak();
}

void win32_handle_button(Button *b, b32 new_is_down)
{
  if (b->is_down && !new_is_down)
  {
    b->went_up = true;
  } 
  else if (!b->is_down && new_is_down)
  {
    b->went_down = true;
  }
  b->is_down = new_is_down;
}

f32 win32_get_time()
{
  LARGE_INTEGER time_li;
  QueryPerformanceCounter(&time_li);
  f32 result = (f32)time_li.QuadPart/(f32)global_counts_per_second;
  return result;
}

IXAudio2 *win32_init_xaudio()
{
  IXAudio2 *xaudio;
  HRESULT ok = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(ok == S_OK);
  
  IXAudio2MasteringVoice* mastering_voice;
  ok = xaudio->CreateMasteringVoice(&mastering_voice);
  assert(ok == S_OK);
  
  return xaudio;
}

#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'

HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD &dwChunkSize, DWORD & dwChunkDataPosition)
{
  HRESULT hr = S_OK;
  if( INVALID_SET_FILE_POINTER == SetFilePointer( hFile, 0, NULL, FILE_BEGIN ) )
    return HRESULT_FROM_WIN32( GetLastError() );
  
  DWORD dwChunkType;
  DWORD dwChunkDataSize;
  DWORD dwRIFFDataSize = 0;
  DWORD dwFileType;
  DWORD bytesRead = 0;
  DWORD dwOffset = 0;
  
  while (hr == S_OK)
  {
    DWORD dwRead;
    if( 0 == ReadFile( hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL ) )
      hr = HRESULT_FROM_WIN32( GetLastError() );
    
    if( 0 == ReadFile( hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL ) )
      hr = HRESULT_FROM_WIN32( GetLastError() );
    
    switch (dwChunkType)
    {
      case fourccRIFF:
      dwRIFFDataSize = dwChunkDataSize;
      dwChunkDataSize = 4;
      if( 0 == ReadFile( hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL ) )
        hr = HRESULT_FROM_WIN32( GetLastError() );
      break;
      
      default:
      if( INVALID_SET_FILE_POINTER == SetFilePointer( hFile, dwChunkDataSize, NULL, FILE_CURRENT ) )
        return HRESULT_FROM_WIN32( GetLastError() );            
    }
    
    dwOffset += sizeof(DWORD) * 2;
    
    if (dwChunkType == fourcc)
    {
      dwChunkSize = dwChunkDataSize;
      dwChunkDataPosition = dwOffset;
      return S_OK;
    }
    
    dwOffset += dwChunkDataSize;
    
    if (bytesRead >= dwRIFFDataSize) return S_FALSE;
    
  }
  
  return S_OK;
}

HRESULT ReadChunkData(HANDLE hFile, void *buffer, DWORD buffersize, DWORD bufferoffset)
{
  HRESULT hr = S_OK;
  if( INVALID_SET_FILE_POINTER == SetFilePointer( hFile, bufferoffset, NULL, FILE_BEGIN ) )
    return HRESULT_FROM_WIN32( GetLastError() );
  DWORD dwRead;
  if( 0 == ReadFile( hFile, buffer, buffersize, &dwRead, NULL ) )
    hr = HRESULT_FROM_WIN32( GetLastError() );
  return hr;
}

struct win32_AudioBuffer
{
  byte *data;
  u32 size;
};
win32_AudioBuffer win32_load_audio_file(IXAudio2 *xaudio, WAVEFORMATEXTENSIBLE *wfx, String file_name)
{
  char *c_file_name = temp_c_string(file_name);
  
  HANDLE file = CreateFile(
    c_file_name,
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    0,
    NULL);
  assert(file != INVALID_HANDLE_VALUE);
  
  SetFilePointer(file, 0, NULL, FILE_BEGIN);
  
  DWORD dwChunkSize;
  DWORD dwChunkPosition;
  //check the file type, should be fourccWAVE or 'XWMA'
  FindChunk(file, fourccRIFF, dwChunkSize, dwChunkPosition);
  DWORD filetype;
  ReadChunkData(file, &filetype, sizeof(DWORD), dwChunkPosition);
  assert(filetype == fourccWAVE);
  
  FindChunk(file,fourccFMT, dwChunkSize, dwChunkPosition );
  ReadChunkData(file, wfx, dwChunkSize, dwChunkPosition );
  
  //fill out the audio data buffer with the contents of the fourccDATA chunk
  FindChunk(file,fourccDATA,dwChunkSize, dwChunkPosition );
  byte* pDataBuffer = (byte *)alloc(dwChunkSize);
  ReadChunkData(file, pDataBuffer, dwChunkSize, dwChunkPosition);
  
  win32_AudioBuffer result;
  result.data = pDataBuffer;
  result.size = dwChunkSize;
  
  return result;
}

void win32_play_audio(IXAudio2SourceVoice *source_voice, XAUDIO2_BUFFER buffer,  win32_AudioBuffer buf)
{
  buffer.AudioBytes = buf.size;  //buffer containing audio data
  buffer.pAudioData = buf.data;  //size of the audio buffer in bytes
  buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer
  
  HRESULT ok = source_voice->SubmitSourceBuffer(&buffer);
  assert(ok == S_OK);
  ok = source_voice->Start(0);
  assert(ok == S_OK);
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prevInstance,
                     LPSTR commandLine,
                     int showCommandLine)
{
  // NOTE(lvl5): init default context
  u64 temp_storage_size = kilobytes(40);
  void *memory = heap_allocator(AllocatorMode_ALLOCATE, 
                                temp_storage_size, 0, 0, 0, 32);
  __default_temp_storage = {};
  init(&__default_temp_storage, memory, temp_storage_size);
  
  LocalContext heap_ctx = make_context(0);
  heap_ctx.allocator = heap_allocator;
  push_context(heap_ctx);
  // end of init
  
  
  // NOTE(lvl5): windows init
  WNDCLASSA window_class = {};
  window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursor((HINSTANCE)0, IDC_ARROW);
  window_class.lpszClassName = "testThingWindowClass";
  
  if (!RegisterClassA(&window_class))
  {
    return 0;
  }
  
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
  HWND window = CreateWindowA(window_class.lpszClassName,
                              "keks",
                              WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                              CW_USEDEFAULT, // x
                              CW_USEDEFAULT, // y
                              WINDOW_WIDTH,
                              WINDOW_HEIGHT,
                              0,
                              0,
                              instance,
                              0);
  if (!window) 
  {
    return 0;
  }
  
#ifdef ASTEROIDS_PROD
  //global_window = window;
#endif
  
#if 0 
  WAVEFORMATEXTENSIBLE wfx = {};
  XAUDIO2_BUFFER buffer = {};
  
  IXAudio2 *xaudio = win32_init_xaudio();
  win32_AudioBuffer cough = win32_load_audio_file(xaudio, &wfx, const_string("../data/test.wav"));
  
  IXAudio2SourceVoice* source_voice;
  HRESULT ok = xaudio->CreateSourceVoice(&source_voice, (WAVEFORMATEX*)&wfx);
  assert(ok == S_OK);
#endif
  
  //win32_play_audio(source_voice, buffer, cough);
  
  // NOTE(lvl5): init openGL
  HDC device_context = GetDC(window);
  
  PIXELFORMATDESCRIPTOR pixel_format_descriptor = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,    // Flags
    PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
    32,                   // Colordepth of the framebuffer.
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0, 0, 0, 0,
    24,                   // Number of bits for the depthbuffer
    8,                    // Number of bits for the stencilbuffer
    0,                    // Number of Aux buffers in the framebuffer.
    PFD_MAIN_PLANE,
    0,
    0, 0, 0
  };
  
  int pixel_format = ChoosePixelFormat(device_context, &pixel_format_descriptor);
  if (!pixel_format)
  {
    return 0;
  }
  b32 pixel_format_is_set = SetPixelFormat(device_context, pixel_format, 
                                           &pixel_format_descriptor);
  if (!pixel_format_is_set)
  {
    return 0;
  }
  HGLRC opengl_context = wglCreateContext(device_context);
  if (!opengl_context)
  {
    return 0;
  }
  b32 context_was_made_current = wglMakeCurrent(device_context, opengl_context);
  if (!context_was_made_current)
  {
    return 0;
  }
  
  gl_load_functions();
  load_opengl_proc(wglSwapIntervalEXT);
  
  wglSwapIntervalEXT(1);
  
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(opengl_debug_callback, 0);
  GLuint unusedIds = 0;
  glDebugMessageControl(GL_DONT_CARE,
                        GL_DONT_CARE,
                        GL_DONT_CARE,
                        0,
                        &unusedIds,
                        true);
  
  
  // NOTE(lvl5): message loop
  
  LARGE_INTEGER counts_per_second_li;
  QueryPerformanceFrequency(&counts_per_second_li);
  global_counts_per_second = counts_per_second_li.QuadPart;
  
  MSG message;
  global_app_state.running = true;
  
  
  GameMemory game_memory = {};
  game_memory.size = megabytes(128);
  game_memory.data = alloc(game_memory.size);
  
  GameInput game_input = {};
  
  GameScreen game_screen;
  game_screen.size.x = WINDOW_WIDTH;
  game_screen.size.y = WINDOW_HEIGHT;
  
  f32 last_time = win32_get_time();
  
  f32 timer = 0;
  
  while (global_app_state.running)
  {
    for (u32 button_index = 0; 
         button_index < array_count(game_input.buttons);
         button_index++)
    {
      Button *button = game_input.buttons + button_index;
      button->went_up = false;
      button->went_down = false;
    }
    
    while (PeekMessage(&message, window, 0, 0, PM_REMOVE)) 
    {
      switch (message.message)
      {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
#define KEY_WAS_DOWN_BIT 1 << 30
#define KEY_IS_UP_BIT 1 << 31
          b32 key_is_down = !(message.lParam & KEY_IS_UP_BIT);
          WPARAM key_code = message.wParam;
          
          switch (key_code)
          {
            case VK_LEFT:
            win32_handle_button(&game_input.left, key_is_down);
            break;
            case VK_RIGHT:
            win32_handle_button(&game_input.right, key_is_down);
            break;
            case VK_UP:
            win32_handle_button(&game_input.up, key_is_down);
            break;
            case VK_SPACE:
            win32_handle_button(&game_input.space, key_is_down);
            break;
          }
          
        } break;
        
        default:
        {
          TranslateMessage(&message);
          DispatchMessage(&message);
        }
      }
    }
    
    game_input.delta_time = win32_get_time() - last_time;
    if (game_input.delta_time > 0.1f) 
    {
      game_input.delta_time = 1.0f/60.0f;
    }
    
    if (timer <= 0)
    {
      //win32_play_audio(source_voice, buffer, cough);
      timer = 0.5f;
    }
    timer -= game_input.delta_time;
    
    last_time = win32_get_time();
    
    game_update(&game_memory, &game_input, &game_screen);
    
    reset_temp_storage();
    SwapBuffers(device_context);
  }
  
  pop_context();
  return 0;
}
