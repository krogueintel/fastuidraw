/*!
 * \file painter_surface_gl_private.hpp
 * \brief file painter_surface_gl_private.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_PAINTER_SURFACE_GL_PRIVATE_HPP
#define FASTUIDRAW_PAINTER_SURFACE_GL_PRIVATE_HPP

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>
#include <private/gl_backend/scratch_renderer.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class PainterSurfaceGLPrivate:noncopyable
{
public:
  explicit
  PainterSurfaceGLPrivate(reference_counted_ptr<ScratchRenderer> scratch_renderer,
                          enum PainterSurface::render_type_t type,
                          GLuint texture, ivec2 dimensions,
                          bool allow_bindless);

  ~PainterSurfaceGLPrivate();

  static
  PainterSurfaceGL*
  surface_gl(const reference_counted_ptr<PainterSurface> &surface);

  GLuint
  color_buffer(void)
  {
    return buffer(buffer_color);
  }

  c_array<const GLenum>
  draw_buffers(bool with_color_buffer);

  GLuint
  fbo(bool with_color_buffer);

  reference_counted_ptr<const Image>
  image(ImageAtlas &atlas);

  enum PainterSurface::render_type_t m_render_type;
  PainterSurface::Viewport m_viewport;
  vec4 m_clear_color;
  ivec2 m_dimensions;

private:
  enum buffer_t
    {
      buffer_color,
      buffer_depth,

      number_buffer_t
    };

  GLuint
  buffer(enum buffer_t);

  vecN<GLuint, number_buffer_t> m_buffers;
  vecN<GLuint, 2> m_fbo;
  vecN<vecN<GLenum, 1>, 2> m_draw_buffer_values;
  vecN<c_array<const GLenum>, 2> m_draw_buffers;
  reference_counted_ptr<ScratchRenderer> m_scratch_renderer;
  reference_counted_ptr<const Image> m_image;
  bool m_own_texture, m_allow_bindless;
};

}}}

#endif
