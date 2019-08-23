/*!
 * \file stroking_style.hpp
 * \brief file stroking_style.hpp
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


#ifndef FASTUIDRAW_STROKING_STYLE_HPP
#define FASTUIDRAW_STROKING_STYLE_HPP

#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
 * @{
 */

  /*!
   * Simple class to wrap how to stroke: closed or open,
   * join style and cap style. Does NOT include stroking
   * parameters (i.e. \ref PainterStrokeParams).
   */
  class StrokingStyle
  {
  public:
    StrokingStyle(void):
      m_cap_style(PainterEnums::square_caps),
      m_join_style(PainterEnums::miter_clip_joins)
    {}

    /*!
     * Set \ref m_cap_style to the specified value.
     * Default value is \ref PainterEnums::square_caps.
     */
    StrokingStyle&
    cap_style(enum PainterEnums::cap_style c)
    {
      m_cap_style = c;
      return *this;
    }

    /*!
     * Set \ref m_join_style to the specified value.
     */
    StrokingStyle&
    join_style(enum PainterEnums::join_style j)
    {
      m_join_style = j;
      return *this;
    }

    /*!
     * Specifies the what cap-style to use when stroking
     * the path. Default value is PainterEnums::square_caps.
     */
    enum PainterEnums::cap_style m_cap_style;

    /*!
     * Specifies what join style to use when stroking the
     * path. Default value is PainterEnums::miter_clip_joins
     */
    enum PainterEnums::join_style m_join_style;
  };
/*! @} */
}

#endif
