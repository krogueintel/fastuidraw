/*!
 * \file painter_vao_pool.hpp
 * \brief file painter_vao_pool.hpp
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

#pragma once

#include <vector>
#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>

#include <private/gl_backend/tex_buffer.hpp>
#include <private/gl_backend/opengl_trait.hpp>
#include <private/util_private.hpp>

namespace fastuidraw { namespace gl { namespace detail {
class painter_vao_pool;

class painter_vao_buffers:public reference_counted<painter_vao_buffers>::non_concurrent
{
public: 
  c_array<PainterIndex>
  index_buffer(void)
  {
    return make_c_array(m_index_buffer);
  }

  c_array<PainterAttribute>
  attribute_buffer(void)
  {
    return make_c_array(m_attribute_buffer);
  }
 
  c_array<uvec4>
  data_buffer(void)
  {
    return make_c_array(m_data_buffer);
  }
 
  c_array<uint32_t>
  header_buffer(void)
  {
    return make_c_array(m_header_buffer);
  }

private:
  friend class painter_vao_pool;

  painter_vao_buffers(void){}

  std::vector<uvec4> m_data_buffer;
  std::vector<uint32_t>  m_header_buffer;
  std::vector<PainterIndex> m_index_buffer;
  std::vector<PainterAttribute> m_attribute_buffer;
};

class painter_vao
{
public:
  painter_vao(void):
    m_vao(0),
    m_attribute_bo(0),
    m_header_bo(0),
    m_index_bo(0),
    m_data_bo(0),
    m_data_tbo(0)
  {}

  GLuint m_vao;
  GLuint m_attribute_bo, m_header_bo, m_index_bo, m_data_bo;
  GLuint m_data_tbo;
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  unsigned int m_data_store_binding_point;
  unsigned int m_pool;
};

class painter_vao_pool:public reference_counted<painter_vao_pool>::non_concurrent
{
public:
  explicit
  painter_vao_pool(const PainterEngineGL::ConfigurationGL &params,
                   enum tex_buffer_support_t tex_buffer_support,
                   unsigned int data_store_binding);

  ~painter_vao_pool();
  unsigned int
  attribute_buffer_size(void) const
  {
    return m_attribute_buffer_size;
  }

  unsigned int
  header_buffer_size(void) const
  {
    return m_header_buffer_size;
  }

  unsigned int
  index_buffer_size(void) const
  {
    return m_index_buffer_size;
  }

  unsigned int
  data_buffer_size(void) const
  {
    return m_data_buffer_size;
  }

  painter_vao
  request_vao(painter_vao_buffers &buffer,
	      unsigned int, unsigned int, unsigned int);

  void
  next_pool(void);

  /*
   * returns the UBO used to hold the values filled
   * by PainterShaderRegistrarGLSL::fill_uniform_buffer().
   * There is only one such UBO per VAO. It is assumed
   * that the ubo_size NEVER changes once this is
   * called once.
   */
  GLuint
  uniform_ubo(GLenum target);

  void
  release_vao(painter_vao &V);

  void
  release_vao_buffer(reference_counted_ptr<painter_vao_buffers> buffer);

  reference_counted_ptr<painter_vao_buffers>
  request_vao_buffers(void);

private:
  GLuint
  generate_tbo(GLuint src_buffer, GLenum fmt, unsigned int unit);

  GLuint
  generate_bo(GLenum bind_target, GLsizei psize, const void *pdata);

  template<typename T>
  GLuint
  generate_bo(GLenum bind_target, c_array<T> pdata)
  {
    return generate_bo(bind_target, sizeof(T) * pdata.size(), pdata.c_ptr());
  }

  template<typename T>
  void
  ready_bo(GLuint bo, GLenum bind_target, c_array<T> pdata)
  {
    fastuidraw_glBindBuffer(bind_target, bo);
    fastuidraw_glBufferData(bind_target, sizeof(T) * pdata.size(), pdata.c_ptr(), GL_STREAM_DRAW);
  }

  void
  create_vao(painter_vao &V);

  void
  release_vao_resources(const painter_vao &V);

  unsigned int m_num_indices, m_num_attributes, m_num_data;
  unsigned int m_attribute_buffer_size, m_header_buffer_size;
  unsigned int m_index_buffer_size;
  int m_blocks_per_data_buffer;
  unsigned int m_data_buffer_size;
  enum glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
  enum tex_buffer_support_t m_tex_buffer_support;
  unsigned int m_data_store_binding;

  unsigned int m_current_pool;
  std::vector<std::vector<painter_vao> > m_free_vaos;
  std::vector<reference_counted_ptr<painter_vao_buffers> > m_free_buffers;
  std::vector<GLuint> m_ubos;
};

}}}
