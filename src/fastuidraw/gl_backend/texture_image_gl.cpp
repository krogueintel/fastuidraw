/*!
 * \file texture_image_gl.cpp
 * \brief file texture_image_gl.cpp
 *
 * Copyright 2019 by Intel.
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

#include <vector>

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/texture_image_gl.hpp>

#include <private/gl_backend/texture_gl.hpp>
#include <private/gl_backend/bindless.hpp>
#include <private/util_private.hpp>

namespace
{
  class TextureImagePrivate
  {
  public:
    TextureImagePrivate(GLuint tex, bool owns):
      m_texture(tex),
      m_owns_texture(owns)
    {}

    GLuint m_texture;
    bool m_owns_texture;
  };

  class ReleaseTexture:public fastuidraw::Image::ResourceReleaseAction
  {
  public:
    void
    action(void)
    {
      fastuidraw_glDeleteTextures(1, &m_texture);
    }

    static
    fastuidraw::reference_counted_ptr<ReleaseTexture>
    create(unsigned int texture, bool create)
    {
      fastuidraw::reference_counted_ptr<ReleaseTexture> return_value;
      if (create && texture != 0u)
        {
          return_value = FASTUIDRAWnew ReleaseTexture(texture);
        }
      return return_value;
    }

  protected:
    ReleaseTexture(unsigned int tex):
      m_texture(tex)
    {}

  private:
    unsigned int m_texture;
  };

  class BindlessReleaseTexture:public ReleaseTexture
  {
  public:
    void
    action(void)
    {
      fastuidraw::gl::detail::bindless().make_texture_handle_non_resident(m_handle);
      ReleaseTexture::action();
    }

    static
    fastuidraw::reference_counted_ptr<BindlessReleaseTexture>
    create(unsigned int texture, bool create, GLuint64 handle)
    {
      fastuidraw::reference_counted_ptr<BindlessReleaseTexture> return_value;
      if (create && texture != 0u)
        {
          return_value = FASTUIDRAWnew BindlessReleaseTexture(texture, handle);
        }
      return return_value;
    }

  private:
    BindlessReleaseTexture(GLuint texture, GLuint64 handle):
      ReleaseTexture(texture),
      m_handle(handle)
    {}

    GLuint64 m_handle;
  };
}

//////////////////////////////////////////////////////
// fastuidraw::gl::TextureImage methods
fastuidraw::reference_counted_ptr<fastuidraw::gl::TextureImage>
fastuidraw::gl::TextureImage::
create(ImageAtlas &patlas, int w, int h, unsigned int m,
       GLuint texture, bool object_owns_texture,
       enum format_t fmt, bool allow_bindless)
{
  if (w <= 0 || h <= 0 || m <= 0 || texture == 0)
    {
      return nullptr;
    }

  if (!allow_bindless || detail::bindless().not_supported())
    {
      return FASTUIDRAWnew TextureImage(patlas, w, h, m, object_owns_texture, texture, fmt);
    }
  else
    {
      GLuint64 handle;

      handle = detail::bindless().get_texture_handle(texture);
      detail::bindless().make_texture_handle_resident(handle);
      return FASTUIDRAWnew TextureImage(patlas, w, h, m, object_owns_texture, texture, handle, fmt);
    }
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::TextureImage>
fastuidraw::gl::TextureImage::
create(ImageAtlas &patlas, int w, int h, unsigned int m,
       GLenum tex_magnification, GLenum tex_minification,
       enum format_t fmt, bool allow_bindless)
{
  GLuint tex(0);
  static detail::UseTexStorage use_tex_storage;

  if (w <= 0 || h <= 0 || m <= 0)
    {
      return nullptr;
    }

  fastuidraw_glGenTextures(1, &tex);
  FASTUIDRAWassert(tex != 0u);
  fastuidraw_glBindTexture(GL_TEXTURE_2D, tex);
  detail::tex_storage<GL_TEXTURE_2D>(use_tex_storage, GL_RGBA8, ivec2(w, h), m);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_magnification);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_minification);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m - 1);
  fastuidraw_glBindTexture(GL_TEXTURE_2D, 0);

  return create(patlas, w, h, m, tex, true, fmt, allow_bindless);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::TextureImage>
fastuidraw::gl::TextureImage::
create(ImageAtlas &patlas, int pw, int ph,
       const ImageSourceBase &image_data,
       GLenum tex_magnification, GLenum tex_minification,
       bool allow_bindless)
{
  static detail::UseTexStorage use_tex_storage;
  GLuint tex(0);
  int m(image_data.number_levels()), w(pw), h(ph);

  if (w <= 0 || h <= 0 || m <= 0)
    {
      FASTUIDRAWassert(false);
      return nullptr;
    }

  std::vector<fastuidraw::u8vec4> data_storage(w * h);
  fastuidraw::c_array<fastuidraw::u8vec4> data(make_c_array(data_storage));

  /* SIGHS. We first upload the texture data then allow for the potential
   * creation of the bindless handle. We do this because some GL drivers
   * do not correctly handle updating texture contents after generating
   * the bindless handle for it. The likely cause is that uploading texel
   * data triggers the GL implementation to an attach auxiliary surface
   * to the texture but the information for the bindless handle does not
   * get that auxiliary attachment.
   */
  fastuidraw_glGenTextures(1, &tex);
  FASTUIDRAWassert(tex != 0u);
  fastuidraw_glBindTexture(GL_TEXTURE_2D, tex);
  detail::tex_storage<GL_TEXTURE_2D>(use_tex_storage, GL_RGBA8, ivec2(w, h), m);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_magnification);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_minification);
  fastuidraw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m - 1);

  fastuidraw_glBindTexture(GL_TEXTURE_2D, tex);
  for (int l = 0; l < m && w > 0 && h > 0; ++l, w /= 2, h /= 2)
    {
      image_data.fetch_texels(l,
                              fastuidraw::ivec2(0, 0),
                              fastuidraw::t_max(w, 1),
                              fastuidraw::t_max(h, 1),
                              data);

      fastuidraw_glTexSubImage2D(GL_TEXTURE_2D, l,
                                 0, 0,
                                 fastuidraw::t_max(w, 1),
                                 fastuidraw::t_max(h, 1),
                                 GL_RGBA, GL_UNSIGNED_BYTE,
                                 data.c_ptr());
    }
  fastuidraw_glBindTexture(GL_TEXTURE_2D, 0);

  return create(patlas, pw, ph, m, tex, true, image_data.format(), allow_bindless);
}

fastuidraw::gl::TextureImage::
TextureImage(ImageAtlas &patlas, int w, int h, unsigned int m,
             bool object_owns_texture, GLuint texture,
             enum format_t fmt):
  Image(patlas, w, h, m, fastuidraw::Image::context_texture2d, -1, fmt,
        ReleaseTexture::create(texture, object_owns_texture))
{
  m_d = FASTUIDRAWnew TextureImagePrivate(texture, object_owns_texture);
}

fastuidraw::gl::TextureImage::
TextureImage(ImageAtlas &patlas, int w, int h, unsigned int m,
             bool object_owns_texture, GLuint texture, GLuint64 handle,
             enum format_t fmt):
  Image(patlas, w, h, m, fastuidraw::Image::bindless_texture2d, handle, fmt,
        BindlessReleaseTexture::create(texture, object_owns_texture, handle))
{
  m_d = FASTUIDRAWnew TextureImagePrivate(texture, object_owns_texture);
}

fastuidraw::gl::TextureImage::
~TextureImage()
{
  TextureImagePrivate *d;
  d = static_cast<TextureImagePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

GLuint
fastuidraw::gl::TextureImage::
texture(void) const
{
  TextureImagePrivate *d;
  d = static_cast<TextureImagePrivate*>(m_d);
  return d->m_texture;
}
