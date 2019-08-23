/*!
 * \file varying_list.cpp
 * \brief file varying_list.cpp
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


#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/glsl/varying_list.hpp>
#include <private/util_private.hpp>

namespace
{
  class VaryingListPrivate
  {
  public:
    enum
      {
        interpolation_number_types = fastuidraw::glsl::varying_list::interpolator_number_types
      };

    fastuidraw::vecN<fastuidraw::string_array, interpolation_number_types> m_varyings;
    fastuidraw::string_array m_alias_varying_names, m_alias_varying_source_names;
  };
}

/////////////////////////////////////////////
// fastuidraw::glsl::varying_list methods
fastuidraw::glsl::varying_list::
varying_list(void)
{
  m_d = FASTUIDRAWnew VaryingListPrivate();
}

fastuidraw::glsl::varying_list::
varying_list(const varying_list &rhs)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew VaryingListPrivate(*d);
}

fastuidraw::glsl::varying_list::
~varying_list()
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::varying_list)

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
varyings(enum interpolator_type_t q) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWmessaged_assert(q < interpolator_number_types,
                            "varying_list::varyings() requested invalid "
                            "interpolator_type_t value");
  return (q < interpolator_number_types) ?
    d->m_varyings[q].get() :
    c_array<const c_string>();
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_varying(c_string pname, enum interpolator_type_t q)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWmessaged_assert(q < interpolator_number_types,
                            "varying_list::add_varying() requested invalid "
                            "interpolator_type_t value");
  if (q < interpolator_number_types)
    {
      d->m_varyings[q].push_back(pname);
    }
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_varying_alias(c_string name, c_string src_name)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_alias_varying_names.push_back(name);
  d->m_alias_varying_source_names.push_back(src_name);
  return *this;
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
alias_varying_names(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_alias_varying_names.get();
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
alias_varying_source_names(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_alias_varying_source_names.get();
}
