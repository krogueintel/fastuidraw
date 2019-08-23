/*!
 * \file painter_shader_data.hpp
 * \brief file painter_shader_data.hpp
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


#ifndef FASTUIDRAW_PAINTER_SHADER_DATA_HPP
#define FASTUIDRAW_PAINTER_SHADER_DATA_HPP

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterShaderData
 * @{
 */

  /*!
   * \brief
   * Common base class to \ref PainterItemShaderData,
   * \ref PainterBrushShaderData and \ref
   * PainterBlendShaderData to hold shader data for
   * custom shaders.
   */
  class PainterShaderData:noncopyable
  {
  public:
    virtual
    ~PainterShaderData()
    {}

    /*!
     * To be implemented by a derived class to pack the
     * data.
     * \param dst location to which to pack the data
     */
    virtual
    void
    pack_data(c_array<uvec4> dst) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the number of uvec4 blocks needed
     * to pack the data.
     */
    virtual
    unsigned int
    data_size(void) const = 0;

    /*!
     * To be optionally implemented by a derived class to
     * save references to resources that need to be resident
     * after packing. Default implementation does nothing.
     * \param dst location to which to save resources.
     */
    virtual
    void
    save_resources(c_array<reference_counted_ptr<const resource_base> > dst) const
    {
      FASTUIDRAWunused(dst);
    }

    /*!
     * To be optionally implemented by a derived class to
     * return the number of resources that need to be resident
     * after packing. Default implementation returns 0.
     */
    virtual
    unsigned int
    number_resources(void) const
    {
      return 0;
    };
  };

  /*!
   * \brief
   * PainterItemShaderData holds custom data for item shaders
   */
  class PainterItemShaderData:public PainterShaderData
  {};

  /*!
   * \brief
   * PainterBlendShaderData holds custom data for blend shaders
   */
  class PainterBlendShaderData:public PainterShaderData
  {};

/*! @} */

} //namespace fastuidraw

#endif
