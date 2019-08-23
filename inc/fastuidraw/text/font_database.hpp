/*!
 * \file font_database.hpp
 * \brief file font_database.hpp
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


#ifndef FASTUIDRAW_FONT_DATABASE_HPP
#define FASTUIDRAW_FONT_DATABASE_HPP

#include <fastuidraw/text/glyph_source.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * \brief
   * A FontDatabase performs the act of font selection and glyph
   * selection. It uses the values of \ref FontProperties (except
   * for FontProperties::source_label()) to select suitable font
   * or fonts.
   */
  class FontDatabase:public reference_counted<FontDatabase>::concurrent
  {
  public:

    /*!
     * \brief
     * A FontGeneratorBase is a means to create a font. Adding a font
     * via a FontGenerator allows one to avoid opening and creating
     * the font until the font is actually needed.
     */
    class FontGeneratorBase:public reference_counted<FontGeneratorBase>::concurrent
    {
    public:
      /*!
       * To be implemented by a derived class to generate a font.
       */
      virtual
      reference_counted_ptr<const FontBase>
      generate_font(void) const = 0;

      /*!
       * To be implemented by a derived class to return the FontProperties
       * of the font that would be generated by generate_font().
       */
      virtual
      const FontProperties&
      font_properties(void) const = 0;
    };

    /*!
     * \brief
     * A FontGroup represents a group of fonts which is selected
     * from a FontProperties value. The accessors for FontGroup
     * are methods of FontDatabase::parent_group(FontGroup),
     * FontDatabase::fetch_font(FontGroup, unsigned int)
     * and FontDatabase::number_fonts(FontGroup).
     */
    class FontGroup
    {
    public:
      FontGroup(void):
        m_d(nullptr)
      {}

    private:
      friend class FontDatabase;
      void *m_d;
    };

    /*!
     * Enumeration to define bits for how fonts and glyphs
     * are selected.
     */
    enum selection_bits_t
      {
        /*!
         * Require an exact match when selecting a font
         */
        exact_match = 1,

        /*!
         * Ignore FontProperties::style() field when selecting
         * a font
         */
        ignore_style = 2,

        /*!
         * Ignore FontProperties::bold() and FontProperties::italic()
         * when selecing font.
         */
        ignore_bold_italic = 4,
      };

    /*!
     * Ctor
     */
    explicit
    FontDatabase(void);

    ~FontDatabase();

    /*!
     * Add a font to this FontDatabase; the value of
     * FontBase::properties().source_label()
     * will be used as a key to uniquely identify the
     * font. If a font is already present with the
     * the same value, will return \ref routine_fail
     * and not add the font.
     * \param h font to add
     */
    enum return_code
    add_font(const reference_counted_ptr<const FontBase> &h);

    /*!
     * Add a font to this FontDatabase; the value of
     * FontGeneratorBase::font_properties().source_label()
     * will be used as a key to uniquely identify the
     * font. If a font is already present with the
     * the same value, will return \ref routine_fail
     * and not add the font.
     * \param h font to add
     */
    enum return_code
    add_font_generator(const reference_counted_ptr<const FontGeneratorBase> &h);

    /*!
     * If the font named by a generator is not yet part of the
     * FontDatabase, add it the FontDatabase, otherwise use
     * the existing generator. From the generator, return the
     * font it generates.
     */
    reference_counted_ptr<const FontBase>
    fetch_or_generate_font(const reference_counted_ptr<const FontGeneratorBase> &h);

    /*!
     * Fetch a font using FontProperties::source_label()
     * as the key to find the font added with add_font()
     * or add_font_generator().
     * \param source_label value of FontProperties::source_label()
     *                     to hunt for a font to have.
     */
    reference_counted_ptr<const FontBase>
    fetch_font(c_string source_label);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * std::ostringstream str;
     * str << filename << ":" << face_index;
     * return fetch_font(str.str().c_str());
     * \endcode
     * \param filename file source of font to hunt for
     * \param face_index face index source of font to hunt for
     */
    reference_counted_ptr<const FontBase>
    fetch_font(c_string filename, int face_index);

    /*!
     * Returns the number of fonts in a FontGroup
     * \param G FontGroup to query
     */
    unsigned int
    number_fonts(FontGroup G);

    /*!
     * Returns a font of a FontGroup
     * \param G FontGroup to query
     * \param N index of font with 0 <= N < number_fonts(G)
     */
    reference_counted_ptr<const FontBase>
    fetch_font(FontGroup G, unsigned int N);

    /*!
     * FontGroup objects are grouped into hierarchies for
     * selection. Returns the parent FontGroup of a FontGroup.
     * \param G FontGroup to query
     */
    FontGroup
    parent_group(FontGroup G);

    /*!
     * Fetch a font from a FontProperties description. The return
     * value will be the closest matched font added with add_font().
     * \param props FontProperties by which to search
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    reference_counted_ptr<const FontBase>
    fetch_font(const FontProperties &props, uint32_t selection_strategy);

    /*!
     * Fetch a FontGroup from a FontProperties value
     * \param props font properties used to generate group.
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    FontGroup
    fetch_group(const FontProperties &props, uint32_t selection_strategy);

    /*!
     * Returns the root FontGroup for all fonts added to this FontDatabase.
     */
    FontGroup
    root_group(void);

    /*!
     * Fetch a GlyphSource with font merging from a glyph rendering type,
     * font properties and character code.
     * \param props font properties used to fetch font
     * \param character_code character code of glyph to fetch
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    GlyphSource
    fetch_glyph(const FontProperties &props, uint32_t character_code,
                uint32_t selection_strategy = 0u);

    /*!
     * Fetch a GlyphSource with font merging from a glyph rendering type, font properties
     * and character code.
     * \param group FontGroup used to fetch font
     * \param character_code character code of glyph to fetch
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    GlyphSource
    fetch_glyph(FontGroup group, uint32_t character_code,
                uint32_t selection_strategy = 0u);

    /*!
     * Fetch a GlyphSource with font merging from a glyph rendering type,
     * font preference and character code.
     * \param h pointer to font from which to fetch the glyph, if the glyph
     *          is not present in the font attempt to get the glyph from
     *          a font of similiar properties
     * \param character_code character code of glyph to fetch
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    GlyphSource
    fetch_glyph(const FontBase *h, uint32_t character_code,
                uint32_t selection_strategy = 0u);

    /*!
     * Fetch a GlyphSource without font merging from a glyph rendering type,
     * font and character code.
     * \param h pointer to font from which to fetch the glyph, if the glyph
     *          is not present in the font, then return an invalid Glyph.
     * \param character_code character code of glyph to fetch
     */
    GlyphSource
    fetch_glyph_no_merging(const FontBase *h, uint32_t character_code);

    /*!
     * Fill Glyph values from an iterator range of character code values.
     * \tparam input_iterator read iterator to type that is castable to uint32_t
     * \tparam output_iterator write iterator to Glyph
     * \param group FontGroup to choose what font
     * \param character_codes_begin iterator to 1st character code
     * \param character_codes_end iterator to one past last character code
     * \param output_begin begin iterator to output
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence(FontGroup group,
                          input_iterator character_codes_begin,
                          input_iterator character_codes_end,
                          output_iterator output_begin,
                          uint32_t selection_strategy = 0u);

    /*!
     * Fill Glyph values from an iterator range of character code values.
     * \tparam input_iterator read iterator to type that is castable to uint32_t
     * \tparam output_iterator write iterator to Glyph
     * \param h pointer to font from which to fetch the glyph, if the glyph
     *          is not present in the font attempt to get the glyph from
     *          a font of similiar properties
     * \param character_codes_begin iterator to 1st character code
     * \param character_codes_end iterator to one past last character code
     * \param output_begin begin iterator to output
     * \param selection_strategy using bit-wise ors of values of
     *                           \ref selection_bits_t to choose the
     *                           matching criteria of selecting a font.
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence(const FontBase *h,
                          input_iterator character_codes_begin,
                          input_iterator character_codes_end,
                          output_iterator output_begin,
                          uint32_t selection_strategy = 0u);

    /*!
     * Fill an array of Glyph values from an array of character code values.
     * \tparam input_iterator read iterator to type that is castable to uint32_t
     * \tparam output_iterator write iterator to Glyph
     * \param h pointer to font from which to fetch the glyph, if the glyph
     *          is not present in the font attempt to get the glyph from
     *          a font of similiar properties
     * \param character_codes_begin iterator to first character code
     * \param character_codes_end iterator to one pash last character code
     * \param output_begin begin iterator to output
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence_no_merging(const FontBase *h,
                                     input_iterator character_codes_begin,
                                     input_iterator character_codes_end,
                                     output_iterator output_begin);

  private:
    void
    lock_mutex(void);

    void
    unlock_mutex(void);

    GlyphSource
    fetch_glyph_no_lock(FontGroup group, uint32_t character_code,
                        uint32_t selection_strategy);

    GlyphSource
    fetch_glyph_no_lock(const FontBase *h,
                        uint32_t character_code,
                        uint32_t selection_strategy);

    GlyphSource
    fetch_glyph_no_merging_no_lock(const FontBase *h, uint32_t character_code);

    void *m_d;
  };

  template<typename input_iterator,
           typename output_iterator>
  void
  FontDatabase::
  create_glyph_sequence(FontGroup group,
                        input_iterator character_codes_begin,
                        input_iterator character_codes_end,
                        output_iterator output_begin,
                        uint32_t selection_strategy)
  {
    lock_mutex();
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph_no_lock(group, v, selection_strategy);
      }
    unlock_mutex();
  }

  template<typename input_iterator,
           typename output_iterator>
  void
  FontDatabase::
  create_glyph_sequence(const FontBase *h,
                        input_iterator character_codes_begin,
                        input_iterator character_codes_end,
                        output_iterator output_begin,
                        uint32_t selection_strategy)
  {
    lock_mutex();
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph_no_lock(h, v, selection_strategy);
      }
    unlock_mutex();
  }

  template<typename input_iterator,
           typename output_iterator>
  void
  FontDatabase::
  create_glyph_sequence_no_merging(const FontBase *h,
                                   input_iterator character_codes_begin,
                                   input_iterator character_codes_end,
                                   output_iterator output_begin)
  {
    lock_mutex();
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph_no_merging_no_lock(h, v);
      }
    unlock_mutex();
  }

/*! @} */
}

#endif
