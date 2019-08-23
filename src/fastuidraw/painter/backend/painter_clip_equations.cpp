/*!
 * \file painter_clip_equations.cpp
 * \brief file painter_clip_equations.cpp
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

#include <fastuidraw/painter/backend/painter_clip_equations.hpp>

//////////////////////////////////////////////
// fastuidraw::PainterClipEquations methods
void
fastuidraw::PainterClipEquations::
pack_data(c_array<uvec4> pdst) const
{
  c_array<uint32_t> dst(pdst.flatten_array());

  dst[clip0_coeff_x] = pack_float(m_clip_equations[0].x());
  dst[clip0_coeff_y] = pack_float(m_clip_equations[0].y());
  dst[clip0_coeff_w] = pack_float(m_clip_equations[0].z());

  dst[clip1_coeff_x] = pack_float(m_clip_equations[1].x());
  dst[clip1_coeff_y] = pack_float(m_clip_equations[1].y());
  dst[clip1_coeff_w] = pack_float(m_clip_equations[1].z());

  dst[clip2_coeff_x] = pack_float(m_clip_equations[2].x());
  dst[clip2_coeff_y] = pack_float(m_clip_equations[2].y());
  dst[clip2_coeff_w] = pack_float(m_clip_equations[2].z());

  dst[clip3_coeff_x] = pack_float(m_clip_equations[3].x());
  dst[clip3_coeff_y] = pack_float(m_clip_equations[3].y());
  dst[clip3_coeff_w] = pack_float(m_clip_equations[3].z());
}
