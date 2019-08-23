/*!
 * \file font.hpp
 * \brief file font.hpp
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


#ifndef FASTUIDRAW_FONT_HPP
#define FASTUIDRAW_FONT_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/text/character_encoding.hpp>
#include <fastuidraw/text/font_properties.hpp>
#include <fastuidraw/text/font_metrics.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/text/glyph_metrics.hpp>
#include <fastuidraw/text/glyph_metrics_value.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */
  class Glyph;
  class GlyphCache;

  /*!
   * \brief
   * FontBase provides an interface for a font
   * to generate glyph rendering data.
   */
  class FontBase:
    public reference_counted<FontBase>::concurrent
  {
  public:
    /*!
     * Ctor.
     * \param pprops font properties describing the font
     * \param pmetrics metrics common to all glyphs of the font
     */
    FontBase(const FontProperties &pprops,
             const FontMetrics &pmetrics);

    virtual
    ~FontBase();

    /*!
     * Returns the properties of the font.
     */
    const FontProperties&
    properties(void) const;

    /*!
     * Returns the metrics of the font.
     */
    const FontMetrics&
    metrics(void) const;

    /*!
     * Returns the unique ID of the \ref FontBase object.
     * The value is gauranteed to be unique and different
     * from any previously created fonts (even those that
     * have been destroyed). The value is assigned by the
     * first \ref FontBase created gets the value 0 and
     * each subsequence \ref FontBase created increments
     * the global value by 1. Thus it is reasonable, to
     * use arrays instead of associative keys for font
     * choosing.
     */
    unsigned int
    unique_id(void) const;

    /*!
     * To be implemented by a derived class to return the
     * index values (glyph codes) for a sequence of character
     * code. A glyph code of 0 indicates that a character
     * code is not present in the font.
     * \param encoding character encoding of character codes
     * \param in_character_codes character codes from which to
     *                           fetch glyph codes
     * \param[out] out_glyph_codes location to which to write
     *                             glyph codes.
     */
    virtual
    void
    glyph_codes(enum CharacterEncoding::encoding_value_t encoding,
                c_array<const uint32_t> in_character_codes,
                c_array<uint32_t> out_glyph_codes) const = 0;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * glyph_codes(CharacterEncoding::unicode,
     *             in_character_codes,
     *             out_glyph_codes);
     * \endcode
     * \param in_character_codes character codes from which to
     *                           fetch glyph codes
     * \param[out] out_glyph_codes location to which to write
     *                             glyph codes.
     */
    void
    glyph_codes(c_array<const uint32_t> in_character_codes,
                c_array<uint32_t> out_glyph_codes) const
    {
      glyph_codes(CharacterEncoding::unicode, in_character_codes, out_glyph_codes);
    }

    /*!
     * Provided as a conveniance to fetch a single
     * glyph_code.
     * \param pcharacter_code Unicode character code from which to
     *                        fetch a glyph code.
     */
    uint32_t
    glyph_code(uint32_t pcharacter_code) const
    {
      uint32_t return_value(0);
      c_array<const uint32_t> p(&pcharacter_code, 1);
      c_array<uint32_t> g(&return_value, 1);
      glyph_codes(p, g);
      return return_value;
    }

    /*!
     * To be implemented by a derived class to return
     * the number of glyph codes the instance has.
     * In particular the return value of glyph_code()
     * is always less than number_glyphs() and the
     * input to compute_metrics() will also have value
     * no more than number_glyphs().
     */
    virtual
    unsigned int
    number_glyphs(void) const = 0;

    /*!
     * To be implemented by a derived class to indicate
     * that it will return non-nullptr in
     * compute_rendering_data() when passed a
     * GlyphRenderer whose GlyphRenderer::m_type
     * is a specified value.
     */
    virtual
    bool
    can_create_rendering_data(enum glyph_type tp) const = 0;

    /*!
     * To be implemented by a derived class to provide the metrics
     * data for the named glyph.
     * \param glyph_code glyph code of glyph to compute the metric values
     * \param[out] metrics location to which to place the metric values for the glyph
     */
    virtual
    void
    compute_metrics(uint32_t glyph_code, GlyphMetricsValue &metrics) const = 0;

    /*!
     * To be implemented by a derived class to generate glyph
     * rendering data given a glyph code and GlyphRenderer.
     * \param render specifies object to return via GlyphRenderer::type(),
     *               it is guaranteed by the caller that can_create_rendering_data()
     *               returns true on render.type()
     * \param glyph_metrics GlyphMetrics values as computed by compute_metrics()
     * \param[out] path location to which to write the Path of the glyph
     * \param[out] render_size location to which to write the render size
     *                         of the glyph
     */
    virtual
    GlyphRenderData*
    compute_rendering_data(GlyphRenderer render, GlyphMetrics glyph_metrics,
                           Path &path, vec2 &render_size) const = 0;

  private:
    void *m_d;
  };
/*! @} */
}

#endif
