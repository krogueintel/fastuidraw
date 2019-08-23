/*!
 * \file glyph_attribute.hpp
 * \brief file glyph_attribute.hpp
 *
 * Copyright 2018 by Intel.
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


#ifndef FASTUIDRAW_GLYPH_ATTRIBUTE_HPP
#define FASTUIDRAW_GLYPH_ATTRIBUTE_HPP

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/text/glyph_atlas.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */
  /*!
   * \brief
   * A GlyphAttribute represents one \ref PainterAttribute per
   * glyph corner.
   */
  class GlyphAttribute
  {
  public:
    /*!
     * Enumeration to define bit-masks of if an index
     * is on the right or left side and on the bottom
     * or top side of a glyph.
     */
    enum corner_masks
      {
        right_corner_mask = 1,
        top_corner_mask   = 2,
      };

    /*!
     * Enumeration naming the corners of a glyph.
     */
    enum corner_t
      {
        bottom_left_corner = 0,
        bottom_right_corner = right_corner_mask,
        top_left_corner = top_corner_mask,
        top_right_corner = right_corner_mask | top_corner_mask
      };

    /*!
     * When packing 8-bit texel data into the store, each
     * 32-bit value of the store holds a 2x2 block of 8-bit
     * texels. This enumeration describes the packing an
     * an attribute to get the texel data.
     */
    enum rect_glyph_layout
      {
        rect_width_num_bits = 8,
        rect_height_num_bits = 8,
        rect_x_num_bits = 8,
        rect_y_num_bits = 8,

        rect_width_bit0 = 0,
        rect_height_bit0 = rect_width_bit0 + rect_width_num_bits,
        rect_x_bit0 = rect_height_bit0 + rect_height_num_bits,
        rect_y_bit0 = rect_x_bit0 + rect_x_num_bits,
      };

    /*!
     * Pack into this GlyphAttribute via \ref rect_glyph_layout
     * to access texel data from the  store.
     */
    void
    pack_texel_rect(unsigned int width, unsigned int height);

    /*!
     * \brief
     * Represents an opaque array of \ref GlyphAttribute
     * values.
     */
    class Array:noncopyable
    {
    public:
      /*!
       * Return the size of Array
       */
      unsigned int
      size(void) const;

      /*!
       * change the size of Array
       */
      void
      resize(unsigned int);

      /*!
       * Equivalent to resize(0).
       */
      void
      clear(void)
      {
        resize(0);
      }

      /*!
       * Returns a reference to an element of the Array
       */
      GlyphAttribute&
      operator[](unsigned int);

      /*!
       * Returns a reference to an element of the Array
       */
      const GlyphAttribute&
      operator[](unsigned int) const;

      /*!
       * Return the backing store of the Array; valid
       * until resize() is called.
       */
      c_array<const GlyphAttribute>
      data(void) const;

      /*!
       * Return the backing store of the Array; valid
       * until resize() is called.
       */
      c_array<GlyphAttribute>
      data(void);

    private:
      friend class GlyphCache;
      friend class Glyph;

      Array(void *d):
        m_d(d)
      {}

      void *m_d;
    };

    /*!
     * The data of this single per-corner attribute,
     * enumerated by \ref corner_t.
     */
    vecN<uint32_t, 4> m_data;
  };
/*! @} */
}

#endif
