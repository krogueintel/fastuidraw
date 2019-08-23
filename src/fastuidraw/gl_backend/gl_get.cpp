/*!
 * \file gl_get.cpp
 * \brief file gl_get.cpp
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

#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

namespace fastuidraw {
namespace gl {

void
context_get(GLenum v, GLint *ptr)
{
  fastuidraw_glGetIntegerv(v, ptr);
}

void
context_get(GLenum v, GLboolean *ptr)
{
  fastuidraw_glGetBooleanv(v, ptr);
}

void
context_get(GLenum v, bool *ptr)
{
  GLboolean bptr(*ptr ? GL_TRUE : GL_FALSE);
  fastuidraw_glGetBooleanv(v, &bptr);
  *ptr = (bptr == GL_FALSE)?
    false:
    true;
}

void
context_get(GLenum v, GLfloat *ptr)
{
  fastuidraw_glGetFloatv(v, ptr);
}


} //namespace gl
} //namespace fastuidraw
