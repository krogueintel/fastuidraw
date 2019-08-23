/*!
 * \file gl_binding.hpp
 * \brief file gl_binding.hpp
 *
 * Adapted from: ngl_backend.hpp of WRATH:
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

#ifndef FASTUIDRAW_GL_BINDING_HPP
#define FASTUIDRAW_GL_BINDING_HPP

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/api_callback.hpp>

namespace fastuidraw {


/*!\addtogroup GLUtility
 * @{
 */

/*!
 * \brief
 * Provides interface for application to use GL where function pointers
 * are auto-resolved transparently and under debug provides error checking.
 * Built as a part of a seperate library; for GL it is libNGL; for GLES
 * it is NGLES. The header defines for each GL/GLES function, glFoo, the
 * macro fastuidraw_glFoo.
 *
 * Short version:
 *  - application should call fastuidraw::gl_binding::get_proc_function()
 *    to set the function which will be used to fetch GL function pointers.
 *  - If an application wishes, it can include <fastuidraw/gl_backend/ngl_header.hpp>.
 *    The header will add the GL function-macros and an application can
 *    issue GL calls without needing to fetch the GL functions via
 *    fastuidraw_glFoo where glFoo is the GL funcion to all. Under release,
 *    the macros are defined to function pointers that automatically set
 *    themselves up correcty. For debug, the macros preceed and postceed each
 *    GL function call with error checking call backs so an application writer
 *    can quickly know what line/file triggered an GL error. If an application does
 *    not wish to use the macro system (and will also need to fetch function pointers
 *    itself) it can just include <fastuidraw/gl_backend/gl_header.hpp> which
 *    will include the correct system GL/GLES headers.
 *
 * Long Version:
 *
 * The namespace gl_binding provides an interface for an application
 * to specify how to fetch GL function pointers (see
 * fastuidraw::gl_binding::get_proc_function()) and additional
 * functionality of where to write/store GL error messages. An application
 * can also use this functionality by including <fastuidraw/gl_backend/ngl_header.hpp>.
 * The header will create a macro fastuidraw_glFoo for each GL function
 * glFoo. If FASTUIDRAW_DEBUG is defined, each GL call will be preceded
 * by a callback and postceeded by another call back. The preceed callback
 * to the GL call will call the implementation of CallbackGL::pre_call() of
 * each active CallbackGL object. The post-process callback will repeatedly
 * call glGetError (until it returns no error) to build an error-string. If
 * the error string is non-empty, it is printed to stderr. In addition,
 * regardless if the error-string is non-empty, CallbackGL::post_call() of
 * each active CallbackGL is called.
 *
 * The gl_binding system requires that an application provides
 * a function which the binding system uses to fetch function
 * pointers for the GL API, this is set via
 * gl_binding::get_proc_function().
 *
 * Lastly, before using a GL (or GLES) function, an application should
 * check the GL implementation supports the named function by examining
 * the GL version and/or extensions itself before using the function.
 */
namespace gl_binding {

/*!
 * \brief
 * A CallbackGL defines the interface (via its base class)
 * for callbacks before and after each GL call.
 */
class CallbackGL:public APICallbackSet::CallBack
{
public:
  CallbackGL(void);
};

/*!
 * Sets the function that the system uses
 * to fetch the function pointers for GL or GLES.
 * \param get_proc value to use, default is nullptr.
 * \param fetch_functions if true, fetch all GL functions
 *                        immediately instead of fetching on
 *                        first call.
 */
void
get_proc_function(void* (*get_proc)(c_string),
                  bool fetch_functions = true);
/*!
 * Sets the function that the system uses
 * to fetch the function pointers for GL or GLES.
 * \param datum data pointer passed to get_proc when invoked
 * \param get_proc value to use, default is nullptr.
 * \param fetch_functions if true, fetch all GL functions
 *                        immediately instead of fetching on
 *                        first call.
 */
void
get_proc_function(void *datum,
                  void* (*get_proc)(void*, c_string),
                  bool fetch_functions = true);

/*!
 * Fetches a GL function using the function fetcher
 * passed to get_proc_function().
 * \param function name of function to fetch
 */
void*
get_proc(c_string function);

/*!
 * Function that implements \ref FASTUIDRAW_GL_MESSAGE
 */
void
message(c_string message, c_string src_file, int src_line);
}

/*!\def FASTUIDRAW_GL_MESSAGE
 * Use this macro to emit a string to each of the
 * gl_binding::CallbackGL objects that are active.
 */
#define FASTUIDRAW_GL_MESSAGE(X) \
  gl_binding::message(X, __FILE__, __LINE__)

/*! @} */
}

#endif
