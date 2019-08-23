/*!
 * \file sdl_demo.cpp
 * \brief file sdl_demo.cpp
 *
 * Adapted from: sdl_demo.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#include <typeinfo>
#include <fstream>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_surface.h>

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/gl_backend/gl_binding.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

#include "generic_command_line.hpp"
#include "simple_time.hpp"
#include "stream_holder.hpp"
#include "sdl_demo.hpp"

namespace
{
  int
  GetSDLGLValue(SDL_GLattr arg)
  {
    int R(0);
    SDL_GL_GetAttribute(arg, &R);
    return R;
  }

  void
  print_gl_extensions(std::ostream &dst)
  {
    int cnt;

    cnt = fastuidraw::gl::context_get<GLint>(GL_NUM_EXTENSIONS);
    dst << "\nGL_EXTENSIONS(" << cnt << "):";
    for(int i = 0; i < cnt; ++i)
      {
        dst << "\n\t" << fastuidraw_glGetStringi(GL_EXTENSIONS, i);
      }
  }

  bool
  is_help_request(const std::string &v)
  {
    return v==std::string("-help")
      or v==std::string("--help")
      or v==std::string("-h");
  }

  void
  reverse_y_of_sdl_event(int h, SDL_Event &ev)
  {
    switch(ev.type)
      {
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
          ev.button.y = h - ev.button.y;
          break;

      case SDL_MOUSEMOTION:
        ev.motion.y = h - ev.motion.y;
        ev.motion.yrel = -ev.motion.yrel;
        break;
      }
  }

  void*
  get_proc(fastuidraw::c_string proc_name)
  {
    return SDL_GL_GetProcAddress(proc_name);
  }

  class OstreamLogger:public fastuidraw::gl_binding::CallbackGL
  {
  public:
    explicit
    OstreamLogger(const fastuidraw::reference_counted_ptr<StreamHolder> &str):
      m_str(str)
    {}

    virtual
    void
    pre_call(fastuidraw::c_string call_string_values,
             fastuidraw::c_string call_string_src,
             fastuidraw::c_string function_name,
             void *function_ptr,
             fastuidraw::c_string src_file, int src_line);

    virtual
    void
    post_call(fastuidraw::c_string call_string_values,
              fastuidraw::c_string call_string_src,
              fastuidraw::c_string function_name,
              fastuidraw::c_string error_string,
              void *function_ptr,
              fastuidraw::c_string src_file, int src_line);

    virtual
    void
    message(fastuidraw::c_string message,
            fastuidraw::c_string src_file, int src_line);

  private:
    fastuidraw::reference_counted_ptr<StreamHolder> m_str;
  };
}

//////////////////////////
// OstreamLogger methods
void
OstreamLogger::
pre_call(fastuidraw::c_string call_string_values,
         fastuidraw::c_string call_string_src,
         fastuidraw::c_string function_name,
         void *function_ptr,
         fastuidraw::c_string src_file, int src_line)
{
  FASTUIDRAWunused(call_string_src);
  FASTUIDRAWunused(function_name);
  FASTUIDRAWunused(function_ptr);
  m_str->stream() << "Pre: [" << src_file << "," << src_line << "] "
                  << call_string_values << "\n";
}

void
OstreamLogger::
post_call(fastuidraw::c_string call_string_values,
          fastuidraw::c_string call_string_src,
          fastuidraw::c_string function_name,
          fastuidraw::c_string error_string,
          void *function_ptr,
          fastuidraw::c_string src_file, int src_line)
{
  FASTUIDRAWunused(call_string_src);
  FASTUIDRAWunused(function_name);
  FASTUIDRAWunused(function_ptr);
  FASTUIDRAWunused(error_string);
  m_str->stream() << "Post: [" << src_file << "," << src_line << "] "
                  << call_string_values;

  if (error_string && *error_string)
    {
      m_str->stream() << "{" << error_string << "}";
    }
  m_str->stream() << "\n";
}

void
OstreamLogger::
message(fastuidraw::c_string message,
        fastuidraw::c_string src_file,
        int src_line)
{
  m_str->stream() << "Message: [" << src_file << "," << src_line << "] "
                  << message << "\n";
}

////////////////////////////
// sdl_demo methods
sdl_demo::
sdl_demo(const std::string &about_text, bool dimensions_must_match_default_value):
  m_handle_events(true),
  m_about(command_line_argument::tabs_to_spaces(command_line_argument::format_description_string("", about_text))),
  m_common_label("Screen and Context Option", *this),
  m_red_bits(8, "red_bits",
             "Bpp of red channel, non-positive values mean use SDL defaults",
             *this),
  m_green_bits(8, "green_bits",
               "Bpp of green channel, non-positive values mean use SDL defaults",
               *this),
  m_blue_bits(8, "blue_bits",
              "Bpp of blue channel, non-positive values mean use SDL defaults",
              *this),
  m_alpha_bits(8, "alpha_bits",
               "Bpp of alpha channel, non-positive values mean use SDL defaults",
               *this),
  m_depth_bits(24, "depth_bits",
               "Bpp of depth buffer, non-positive values mean use SDL defaults",
               *this),
  m_stencil_bits(8, "stencil_bits",
                 "Bpp of stencil buffer, non-positive values mean use SDL defaults",
                 *this),
  m_fullscreen(false, "fullscreen", "fullscreen mode", *this),
  m_hide_cursor(false, "hide_cursor", "If true, hide the mouse cursor with a SDL call", *this),
  m_use_msaa(false, "enable_msaa", "If true enables MSAA", *this),
  m_msaa(4, "msaa_samples",
         "If greater than 0, specifies the number of samples "
         "to request for MSAA. If not, SDL will choose the "
         "sample count as the highest available value",
         *this),
  m_width(800, "width", "window width", *this),
  m_height(480, "height", "window height", *this),
  m_dimensions_must_match(dimensions_must_match_default_value, "dimensions_must_match",
                          "If true, then will abort if the created window dimensions do not "
                          "match precisely the width and height parameters", *this),
  m_bpp(32, "bpp", "bits per pixel", *this),
  m_log_gl_commands("", "log_gl", "if non-empty, GL commands are logged to the named file. "
                    "If value is stderr then logged to stderr, if value is stdout logged to stdout", *this),
  m_print_gl_info(false, "print_gl_info", "If true print to stdout GL information", *this),
  m_swap_interval(-1, "swap_interval",
                  "If set, pass the specified value to SDL_GL_SetSwapInterval, "
                  "a value of 0 means no vsync, a value of 1 means vsync and "
                  "a value of -1, if the platform supports, late swap tearing "
                  "as found in extensions GLX_EXT_swap_control_tear and "
                  "WGL_EXT_swap_control_tear. STRONG REMINDER: the value is "
                  "only passed to SDL_GL_SetSwapInterval if the value is set "
                  "at command line", *this),
  #ifdef FASTUIDRAW_GL_USE_GLES
  m_gl_major(3, "gles_major", "GLES major version", *this),
  m_gl_minor(0, "gles_minor", "GLES minor version", *this),
  #else
  m_gl_major(3, "gl_major", "GL major version", *this),
  m_gl_minor(3, "gl_minor", "GL minor version", *this),
  m_gl_forward_compatible_context(false, "foward_context", "if true request forward compatible context", *this),
  m_gl_debug_context(false, "debug_context", "if true request a context with debug", *this),
  m_gl_core_profile(true, "core_context", "if true request a context which is core profile", *this),
  m_try_to_get_latest_gl_version(true, "try_to_get_latest_gl_version",
                                 "If true, first create a GL context the old fashioned way "
                                 "and query its context version and then max that value with "
                                 "the requested version before making the context used by the application",
                                 *this),
  #endif

  m_show_framerate(false, "show_framerate", "if true show the cumulative framerate at end", *this),
  m_num_warm_up_frames(10, "num_warm_up_frames",
		       "Number of warm-up frames to ignore in timing the average framerate", *this),
  m_reverse_event_y(false),
  m_window(nullptr),
  m_ctx(nullptr)
{

}

sdl_demo::
~sdl_demo()
{
  if (m_window)
    {
      if (m_ctx)
        {
          SDL_GL_MakeCurrent(m_window, nullptr);
          SDL_GL_DeleteContext(m_ctx);
        }

      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
    }
}

void
sdl_demo::
set_sdl_gl_context_attributes(void)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_gl_major.value());
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_gl_minor.value());
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }
  #else
    {
      if (m_gl_major.value() >= 3)
        {
          int context_flags(0);
          int profile_mask(0);

          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_gl_major.value());
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_gl_minor.value());

          if (m_gl_forward_compatible_context.value())
            {
              context_flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
            }

          if (m_gl_debug_context.value())
            {
              context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
            }

          if (m_gl_core_profile.value())
            {
              profile_mask = SDL_GL_CONTEXT_PROFILE_CORE;
            }
          else
            {
              profile_mask = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
            }
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_mask);
        }
    }
  #endif
}

void
sdl_demo::
create_sdl_gl_context(void)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      set_sdl_gl_context_attributes();
      m_ctx = SDL_GL_CreateContext(m_window);
    }
  #else
    {
      if (!m_try_to_get_latest_gl_version.value())
        {
          set_sdl_gl_context_attributes();
          m_ctx = SDL_GL_CreateContext(m_window);
          return;
        }

      /* Some WGL/GLX implementations will only give the exact GL
       * version requested, but for our purposes we really want the
       * latest version we can get. Very often, by having SDL create
       * a context the old-fashioned way, we can get a context of
       * the greatest version for compatibility profiles. We get SDL to
       * make a GL context the old-fashioned way by NOT setting any
       * of the SDL-GL attributes related to context versions/profiles
       */
      m_ctx = SDL_GL_CreateContext(m_window);
      if (m_ctx == nullptr)
        {
          std::cerr << "Unable to create vanilla GL context: " << SDL_GetError() << "\n";
          return;
        }
      SDL_GL_MakeCurrent(m_window, m_ctx);

      /* Query the version of a context made the old-way and max
       * that value with the requested version; note that we
       * CANNOT use ngl_ system because (1) the get_proc function
       * is not yet assigned and (2) some GL implementation on
       * MS-Windows have that the functions returned by
       * wglGetProcAddress are only good for the context that made
       * them (shudders).
       */
      PFNGLGETINTEGERVPROC get_integer;
      get_integer = (PFNGLGETINTEGERVPROC)get_proc("glGetIntegerv");
      if (get_integer)
        {
          fastuidraw::ivec2 ver(0, 0);
          fastuidraw::ivec2 req(m_gl_major.value(), m_gl_minor.value());

          get_integer(GL_MAJOR_VERSION, &ver.x());
          get_integer(GL_MINOR_VERSION, &ver.y());
          req = fastuidraw::t_max(ver, req);
          m_gl_major.value() = req.x();
          m_gl_minor.value() = req.y();
        }

      SDL_GL_MakeCurrent(m_window, nullptr);
      SDL_GL_DeleteContext(m_ctx);
      set_sdl_gl_context_attributes();
      m_ctx = SDL_GL_CreateContext(m_window);
    }
  #endif
}

enum fastuidraw::return_code
sdl_demo::
init_sdl(void)
{
  #ifdef _WIN32
    {
      SetProcessDPIAware();
    }
  #endif
  
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
      std::cerr << "\nFailed on SDL_Init\n";
      return fastuidraw::routine_fail;
    }

  int video_flags;
  video_flags = SDL_WINDOW_RESIZABLE;

  if (m_fullscreen.value())
    {
      video_flags = video_flags | SDL_WINDOW_FULLSCREEN;
    }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, m_stencil_bits.value());
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, m_depth_bits.value());
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, m_red_bits.value());
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, m_green_bits.value());
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, m_blue_bits.value());
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, m_alpha_bits.value());
  if (m_use_msaa.value())
    {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, m_msaa.value());
    }

  video_flags |= SDL_WINDOW_OPENGL;
  m_window = SDL_CreateWindow("",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              m_width.value(),
                              m_height.value(),
                              video_flags);

  if (m_window == nullptr)
    {
      std::cerr << "\nFailed on SDL_SetVideoMode\n";
      return fastuidraw::routine_fail;
    }

  if (m_dimensions_must_match.value())
    {
      int w, h;
      bool is_fullscreen;
      is_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
      SDL_GetWindowSize(m_window, &w, &h);
      if (w != m_width.value() || h != m_height.value() || is_fullscreen != m_fullscreen.value())
        {
          std::cerr << "\nDimensions did not match and required to match\n";
          return fastuidraw::routine_fail;
        }
    }

  fastuidraw::reference_counted_ptr<StreamHolder> str;
  if (!m_log_gl_commands.value().empty())
    {
      str = FASTUIDRAWnew StreamHolder(m_log_gl_commands.value());
    }

  create_sdl_gl_context();
  if (m_ctx == nullptr)
    {
      std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
      return fastuidraw::routine_fail;
    }
  SDL_GL_MakeCurrent(m_window, m_ctx);

  if (m_swap_interval.set_by_command_line())
    {
      if (SDL_GL_SetSwapInterval(m_swap_interval.value()) != 0)
        {
          std::cerr << "Warning unable to set swap interval: "
                    << SDL_GetError() << "\n";
        }
    }
  fastuidraw::gl_binding::get_proc_function(get_proc);

  if (m_hide_cursor.value())
    {
      SDL_ShowCursor(SDL_DISABLE);
    }

  if (str)
    {
      m_gl_logger = FASTUIDRAWnew OstreamLogger(str);
    }

  if (m_print_gl_info.value())
    {
      std::cout << "\nSwapInterval: " << SDL_GL_GetSwapInterval()
                << "\ndepth bits: " << GetSDLGLValue(SDL_GL_DEPTH_SIZE)
                << "\nstencil bits: " << GetSDLGLValue(SDL_GL_STENCIL_SIZE)
                << "\nred bits: " << GetSDLGLValue(SDL_GL_RED_SIZE)
                << "\ngreen bits: " << GetSDLGLValue(SDL_GL_GREEN_SIZE)
                << "\nblue bits: " << GetSDLGLValue(SDL_GL_BLUE_SIZE)
                << "\nalpha bits: " << GetSDLGLValue(SDL_GL_ALPHA_SIZE)
                << "\ndouble buffered: " << GetSDLGLValue(SDL_GL_DOUBLEBUFFER)
                << "\nGL_MAJOR_VERSION: " << fastuidraw::gl::context_get<GLint>(GL_MAJOR_VERSION)
                << "\nGL_MINOR_VERSION: " << fastuidraw::gl::context_get<GLint>(GL_MINOR_VERSION)
                << "\nGL_VERSION string:" << fastuidraw_glGetString(GL_VERSION)
                << "\nGL_VENDOR:" << fastuidraw_glGetString(GL_VENDOR)
                << "\nGL_RENDERER:" << fastuidraw_glGetString(GL_RENDERER)
                << "\nGL_SHADING_LANGUAGE_VERSION:" << fastuidraw_glGetString(GL_SHADING_LANGUAGE_VERSION)
                << "\nGL_MAX_VARYING_COMPONENTS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_VARYING_COMPONENTS)
                << "\nGL_MAX_VERTEX_ATTRIBS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_VERTEX_ATTRIBS)
                << "\nGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
                << "\nGL_MAX_VERTEX_UNIFORM_BLOCKS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_VERTEX_UNIFORM_BLOCKS)
                << "\nGL_MAX_FRAGMENT_UNIFORM_BLOCKS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
                << "\nGL_MAX_COMBINED_UNIFORM_BLOCKS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_COMBINED_UNIFORM_BLOCKS)
                << "\nGL_MAX_UNIFORM_BLOCK_SIZE:" << fastuidraw::gl::context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE)
                << "\nGL_MAX_TEXTURE_SIZE: " << fastuidraw::gl::context_get<GLint>(GL_MAX_TEXTURE_SIZE)
                << "\nGL_MAX_ARRAY_TEXTURE_LAYERS: " << fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS)
                << "\nGL_MAX_TEXTURE_BUFFER_SIZE: " << fastuidraw::gl::context_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE);


      #ifndef FASTUIDRAW_GL_USE_GLES
        {
          std::cout << "\nGL_MAX_GEOMETRY_UNIFORM_BLOCKS:" << fastuidraw::gl::context_get<GLint>(GL_MAX_GEOMETRY_UNIFORM_BLOCKS)
                    << "\nGL_MAX_CLIP_DISTANCES:" << fastuidraw::gl::context_get<GLint>(GL_MAX_CLIP_DISTANCES);
        }
      #endif

      print_gl_extensions(std::cout);
      std::cout << "\n";
    }

  return fastuidraw::routine_success;
}

void
sdl_demo::
reverse_event_y(bool v)
{
  m_reverse_event_y = v;
}

void
sdl_demo::
swap_buffers(unsigned int count)
{
  for(unsigned int i = 0; i < count; ++i)
    {
      SDL_GL_SwapWindow(m_window);
    }
}


int
sdl_demo::
main(int argc, char **argv)
{
  simple_time render_time;
  unsigned int num_frames;

  if (argc == 2 && is_help_request(argv[1]))
    {
      std::cout << m_about << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);
      return 0;
    }

  std::cout << "\n\nRunning: \"";
  for(int i = 0; i < argc; ++i)
    {
      std::cout << argv[i] << " ";
    }

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  enum fastuidraw::return_code R;
  R = init_sdl();
  int w, h;

  if (R == fastuidraw::routine_fail)
    {
      return -1;
    }

  m_run_demo = true;
  SDL_GetWindowSize(m_window, &w, &h);
  init_gl(w, h);

  num_frames = 0;
  while(m_run_demo)
    {
      if (num_frames == m_num_warm_up_frames.value())
        {
          render_time.restart();
        }

      pre_draw_frame();
      draw_frame();
      post_draw_frame();
      swap_buffers();
      ++num_frames;

      if (m_run_demo && m_handle_events)
        {
          SDL_Event ev;
          while(m_run_demo && m_handle_events && SDL_PollEvent(&ev))
            {
              if (m_reverse_event_y)
                {
                  int w, h;
                  FASTUIDRAWassert(m_window);
                  SDL_GetWindowSize(m_window, &w, &h);
                  reverse_y_of_sdl_event(h, ev);
                }
              handle_event(ev);
            }
        }
    }

  if (m_show_framerate.value() && num_frames > m_num_warm_up_frames.value())
    {
      int32_t ms;
      float msf, numf;

      num_frames -= m_num_warm_up_frames.value();

      ms = render_time.elapsed();
      numf = static_cast<float>(std::max(1u, num_frames));
      msf = static_cast<float>(std::max(1, ms));
      std::cout << "Rendered " << num_frames << " in " << ms << " ms.\n"
                << "ms/frame = " << msf / numf  << "\n"
                << "FPS = " << 1000.0f * numf / msf << "\n";
    }

  return m_return_value;
}

fastuidraw::ivec2
sdl_demo::
dimensions(void)
{
  fastuidraw::ivec2 return_value;

  FASTUIDRAWassert(m_window);
  SDL_GetWindowSize(m_window, &return_value.x(), &return_value.y());
  return return_value;
}
