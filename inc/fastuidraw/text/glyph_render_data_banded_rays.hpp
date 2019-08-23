/*!
 * \file glyph_render_data_banded_rays.hpp
 * \brief file glyph_render_data_banded_rays.hpp
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


#ifndef FASTUIDRAW_GLYPH_RENDER_DATA_BANDED_RAYS_HPP
#define FASTUIDRAW_GLYPH_RENDER_DATA_BANDED_RAYS_HPP

#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * A GlyphRenderDataBandedRays represents the data needed to
   * build a glyph to render it with a modification to the technique
   * of "GPU-Centered Font Rendering Directly from Glyph Outlines"
   * by Eric Lengyel. The class follows the original paper except
   * that rather than using a set of textures to hold the data,
   * we use a single buffer (from \ref GlyphAtlas) to hold the data.
   *
   * The attribute data for a Glyph has:
   *   - the number of bands horizontally H
   *   - the number of bands vertically V
   *   - the offset to the glyph data
   *
   * Each horizontal band is the EXACT same height and each vertical
   * band is the EXACT same width. The data location of a horizontal
   * and vertical band is implicitely given from the location of
   * the glyph data as follows (where O = start of glyph data):
   *
   *   - horizontal_band_plus_infinity(I) is at               I + O
   *   - horizontal_band_negative_infinity(I) is at       H + I + O
   *   - vertical_band_plus_infinity(I) is at         2 * H + I + O
   *   - vertical_band_negative_infinity(I) is at V + 2 * H + I + O
   *
   * The value I for horizontal bands is computed as I = ny * V and
   * the value I for vertical bands is computed as I = nx * H where
   * nx is the glyph x-coordinate normalized to [0, 1] and ny is the
   * glyph y-coordinate normalized to [0, 1].
   *
   * A band is encoded by \ref band_t (which gives the number of
   * curves and offset to the curves).
   *
   * Rather than encoding where in the glyph the horizontal and
   * vertical bands split, we just say that the bands are split
   * in the middle always (TODO: is this wise really?)
   */
  class GlyphRenderDataBandedRays:public GlyphRenderData
  {
  public:
    /*!
     * A band_t represents the header for listing the curves of a band.
     * Each curve of a band is THREE points (see \ref point_packing_t
     * how points are packed). The packing of the curves is done as follows.
     * Let the band have curves c1, c2, ..., cN were the curve cI has points
     * (pI_0, pI_1, pI_2). The packing of points is:
     *
     * p1_0, p1_1, p1_2, p2_0, p2_1, p2_2, .... , pN_0, pN_1, pN_2
     */
    enum band_t
      {
        /*
         * Number of bits used to encode the number
         * of curves; note that the maximum number
         * of allowed curves in a band is 256 curves.
         */
        band_numcurves_numbits = 8,

        /*
         * The number of bits to encode the offset to
         * where the curves are located RELATIVE to
         * the location of the glyph data.
         */
        band_curveoffset_numbits = 32 - band_numcurves_numbits,

        /*!
         * first bit used to encode the number of curves
         * in a band
         */
        band_numcurves_bit0 = 0,

        /*!
         * first bit used to encode the offset to the curves
         */
        band_curveoffset_bit0 = band_numcurves_bit0 + band_numcurves_numbits
      };

    /*!
     * Points are packed as (fp16, fp16) pairs.
     */
    enum point_packing_t
      {
      };

    enum
      {
        /*!
         * The glyph coordinate value in each coordiante varies
         * from -\ref glyph_coord_value to +\ref glyph_coord_value,
         * i.e. the glyph is drawn as rect with min-corner
         * (-\ref glyph_coord_value, -\ref glyph_coord_value)
         * and max-corner (+\ref glyph_coord_value, +\ref glyph_coord_value)
         */
        glyph_coord_value = 32,
      };

    /*!
     * This enumeration describes the meaning of the
     * attributes. The data of the glyph is offset so
     * that a shader can assume that the bottom left
     * corner has glyph-coordinate (0, 0) and the top
     * right corner has glyph-coordinate (width, height)
     * where width and height are the width and height
     * of the glyph in glyph coordinates.
     */
    enum attribute_values_t
      {
        /*!
         * Value is 0 if on min-x side of glyph, value is
         * 1 if on max-x side of glyph; packed as uint.
         */
        glyph_normalized_x,

        /*!
         * Value is 0 if on min-y side of glyph, value is
         * 1 if on max-y side of glyph; packed as uint.
         */
        glyph_normalized_y,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the number of vertical bands in the glyph;
         * packing as uint.
         */
        glyph_num_vertical_bands,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the number of horizontal bands in the glyph;
         * packing as uint.
         */
        glyph_num_horizontal_bands,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the fill rule and the offset into the store for
         * the glyph data. The offset is encoded as follows
         *  - bits0-bits29 encode the offset
         *  - bit30 indicates to complement fill
         *  - bit31 up indicates odd-even fill rule and
         *          down indicates non-zero fill rule.
         */
        glyph_offset,

        /*!
         * Number attribute values needed.
         */
        glyph_num_attributes
      };

    /*!
     * A query_info holds data about a \ref GlyphRenderDataBandedRays
     * value (after its finalized method).
     */
    class query_info
    {
    public:
      /*!
       * Default ctor, initializing the value as empty
       */
      query_info(void):
        m_number_vertical_bands(0),
        m_number_horizontal_bands(0)
      {}

      /*!
       * Set the value of a \ref vecN of \ref GlyphAttribute values
       * derived from this query_info object
       * \param out_attribs location to which to write GlyphAttribute values
       * \param fill_rule fill rule with which to fill the glyphs
       * \param offset location of glyph data
       */
      void
      set_glyph_attributes(vecN<GlyphAttribute, glyph_num_attributes> *out_attribs,
                           enum PainterEnums::fill_rule_t fill_rule,
                           uint32_t offset);

      /*!
       * The GPU data of the queried \ref GlyphRenderDataBandedRays
       * object; the data pointed to by the array is backed
       * internally by the queried \ref GlyphRenderDataBandedRays;
       * thus, if the point becomes invalid once the queried
       * \ref GlyphRenderDataBandedRays goes out of scope.
       */
      c_array<const uint32_t> m_gpu_data;

      /*!
       * The number of vertical bands of the queried
       * \ref GlyphRenderDataBandedRays
       */
      int m_number_vertical_bands;

      /*!
       * The number of horizontal bands of the queried
       * \ref GlyphRenderDataBandedRays
       */
      int m_number_horizontal_bands;
    };

    /*!
     * Ctor.
     */
    GlyphRenderDataBandedRays(void);

    ~GlyphRenderDataBandedRays();

    /*!
     * Start a contour. Before starting a new contour
     * the previous contour must be closed by calling
     * line_to() or quadratic_to() connecting to the
     * start point of the previous contour.
     * \param pt start point of the new contour
     */
    void
    move_to(vec2 pt);

    /*!
     * Add a line segment connecting the end point of the
     * last curve or line segment of the current contour to
     * a given point.
     * \param pt end point of the new line segment
     */
    void
    line_to(vec2 pt);

    /*!
     * Add a quadratic curveconnecting the end point of the
     * last curve or line segment of the current contour
     * \param ct control point of the quadratic curve
     * \param pt end point of the quadratic curve
     */
    void
    quadratic_to(vec2 ct, vec2 pt);

    /*!
     * Finalize the input data after which no more contours or curves may be added;
     * all added contours must be closed before calling finalize(). Once finalize()
     * is called, no further data can be added. All contours added must be closed as
     * well.
     * \param f fill rule to use for rendering, must be one of
     *          PainterEnums::nonzero_fill_rule or \ref
     *          PainterEnums::odd_even_fill_rule.
     * \param glyph_rect the rect of the glpyh
     * \param max_recursion maximum level of recursion to employ to reduce the
     *                      complexity of each band. The number of bands that are
     *                      generated in a dimension is 2^N where N is the number
     *                      of levels of recursion used to generate bands.
     * \param avg_num_curves_thresh when a band is generated, the average number of
     *                              curves needed to perform the coverage computation
     *                              is computed. When that value is lower than this
     *                              parameter, the band is considered that is does
     *                              not need to be reduced to smaller bands.
     */
    void
    finalize(enum PainterEnums::fill_rule_t f, const Rect &glyph_rect,
             int max_recursion, float avg_num_curves_thresh);

    /*!
     * Finalize the input data after which no more contours or curves may be added;
     * all added contours must be closed before calling finalize(). Once finalize()
     * is called, no further data can be added. All contours added must be closed as
     * well. Provided as a conveniance, equivalent to
     * \code
     * finalize(f, glyph_rect, GlyphGenerateParams::banded_rays_max_recursion(),
     *          GlyphGenerateParams::banded_rays_average_number_curves_thresh());
     * \endcode
     * \param f fill rule to use for rendering, must be one of
     *          PainterEnums::nonzero_fill_rule or \ref
     *          PainterEnums::odd_even_fill_rule.
     * \param glyph_rect the rect of the glpyh
     */
    void
    finalize(enum PainterEnums::fill_rule_t f, const Rect &glyph_rect);

    /*!
     * Query the data; may only be called after finalize(). Returns
     * \ref routine_fail if finalize() has not yet been called.
     * \param out_info location to which to write information about
     *                 this object.
     */
    enum return_code
    query(query_info *out_info) const;

    virtual
    c_array<const c_string>
    render_info_labels(void) const;

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes,
                    c_array<float> render_costs) const;

  private:
    void *m_d;
  };
/*! @} */
}

#endif
