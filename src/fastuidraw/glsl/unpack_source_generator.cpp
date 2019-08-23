/*!
 * \file unpack_source_generator.cpp
 * \brief file unpack_source_generator.cpp
 *
 * Copyright 2019 by Intel.
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

#include <vector>
#include <string>
#include <sstream>

#include <fastuidraw/glsl/unpack_source_generator.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <private/util_private.hpp>

namespace
{
  class UnpackElement
  {
  public:
    UnpackElement(void):
      m_type(fastuidraw::glsl::UnpackSourceGenerator::padding_type)
    {}

    UnpackElement(fastuidraw::c_string name,
                  fastuidraw::glsl::UnpackSourceGenerator::type_t tp,
                  enum fastuidraw::glsl::UnpackSourceGenerator::cast_t cast,
                  unsigned int idx):
      m_name(name),
      m_type(tp),
      m_cast(cast),
      m_idx(idx),
      m_bit0(-1),
      m_num_bits(-1)
    {}

    UnpackElement(fastuidraw::c_string name,
                  unsigned int bit0, unsigned int num_bits,
                  fastuidraw::glsl::UnpackSourceGenerator::type_t tp,
                  enum fastuidraw::glsl::UnpackSourceGenerator::cast_t cast,
                  unsigned int idx):
      m_name(name),
      m_type(tp),
      m_cast(cast),
      m_idx(idx),
      m_bit0(bit0),
      m_num_bits(num_bits)
    {}

    bool
    is_bitfield(void) const
    {
      return m_bit0 >= 0;
    }

    std::string m_name;
    fastuidraw::glsl::UnpackSourceGenerator::type_t m_type;
    enum fastuidraw::glsl::UnpackSourceGenerator::cast_t m_cast;
    unsigned int m_idx;
    int m_bit0, m_num_bits;
  };

  class UnpackSourceGeneratorPrivate
  {
  public:
    explicit
    UnpackSourceGeneratorPrivate(fastuidraw::c_string nm):
      m_structs(1, nm)
    {}

    explicit
    UnpackSourceGeneratorPrivate(fastuidraw::c_array<const fastuidraw::c_string> nms):
      m_structs(nms.begin(), nms.end())
    {}

    std::vector<std::string> m_structs;

    /* m_elements[A] is all elements that are unpacked
     * from data at offset A.
     */
    std::vector<std::vector<UnpackElement> > m_elements;
  };
}

////////////////////////////////////////
// UnpackSourceGenerator methods
fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(c_string name)
{
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(name);
}

fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(c_array<const c_string> names)
{
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(names);
}

fastuidraw::glsl::UnpackSourceGenerator::
UnpackSourceGenerator(const UnpackSourceGenerator &rhs)
{
  UnpackSourceGeneratorPrivate *d;
  d = static_cast<UnpackSourceGeneratorPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew UnpackSourceGeneratorPrivate(*d);
}

fastuidraw::glsl::UnpackSourceGenerator::
~UnpackSourceGenerator()
{
  UnpackSourceGeneratorPrivate *d;
  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::UnpackSourceGenerator)

fastuidraw::glsl::UnpackSourceGenerator&
fastuidraw::glsl::UnpackSourceGenerator::
set(unsigned int offset, c_string field_name, type_t type, enum cast_t cast, unsigned int idx)
{
  UnpackSourceGeneratorPrivate *d;

  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  if (offset >= d->m_elements.size())
    {
      d->m_elements.resize(offset + 1);
    }
  d->m_elements[offset].push_back(UnpackElement(field_name, type, cast, idx));
  return *this;
}

fastuidraw::glsl::UnpackSourceGenerator&
fastuidraw::glsl::UnpackSourceGenerator::
set(unsigned int offset,
    unsigned int bit0, unsigned int num_bits,
    c_string field_name, type_t type, enum cast_t cast,
    unsigned int idx)
{
  UnpackSourceGeneratorPrivate *d;

  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  if (offset >= d->m_elements.size())
    {
      d->m_elements.resize(offset + 1);
    }
  d->m_elements[offset].push_back(UnpackElement(field_name, bit0, num_bits, type, cast, idx));
  return *this;
}

const fastuidraw::glsl::UnpackSourceGenerator&
fastuidraw::glsl::UnpackSourceGenerator::
stream_unpack_function(ShaderSource &dst, c_string function_name) const
{
  UnpackSourceGeneratorPrivate *d;
  unsigned int number_blocks;
  std::ostringstream str;
  const fastuidraw::c_string swizzles[] =
    {
      ".x",
      ".xy",
      ".xyz",
      ".xyzw"
    };
  const fastuidraw::c_string utemp_components[] =
    {
      "utemp.x",
      "utemp.y",
      "utemp.z",
      "utemp.w"
    };

  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);
  str << "void\n" << function_name << "(in uint location, ";

  for (unsigned int s = 0; s < d->m_structs.size(); ++s)
    {
      if (s != 0)
        {
          str << ", ";
        }
      str << "out " << d->m_structs[s] << " out_value" << s;
    }
  str << ")\n"
      << "{\n"
      << "\tuvec4 utemp;\n"
      << "\tuint tempbits;\n"
      << "\tfloat ftemp;\n";

  number_blocks = FASTUIDRAW_NUMBER_BLOCK4_NEEDED(d->m_elements.size());
  FASTUIDRAWassert(4 * number_blocks >= d->m_elements.size());

  for(unsigned int b = 0, i = 0; b < number_blocks; ++b)
    {
      unsigned int cmp, li;
      fastuidraw::c_string swizzle;

      li = d->m_elements.size() - i;
      cmp = fastuidraw::t_min(4u, li);
      FASTUIDRAWassert(cmp >= 1 && cmp <= 4);

      swizzle = swizzles[cmp - 1];
      str << "\tutemp" << swizzle << " = fastuidraw_fetch_data(int(location) + "
          << b << ")" << swizzle << ";\n";

      /* perform bit cast to correct type. */
      for(unsigned int k = 0; k < 4 && i < d->m_elements.size(); ++k, ++i)
        {
          for (const auto &e : d->m_elements[i])
            {
              fastuidraw::c_string src;

              src = utemp_components[k];
              if (e.is_bitfield())
                {
                  str << "\ttempbits = FASTUIDRAW_EXTRACT_BITS("
                      << e.m_bit0 << ", " << e.m_num_bits
                      << ", " << src << ");\n";
                  src = "tempbits";
                }

              if (e.m_cast == reinterpret_to_float_bits)
                {
                  str << "\tftemp = uintBitsToFloat(" << src << ");\n";
                  src = "ftemp";
                }

              switch(e.m_type)
                {
                case int_type:
                  str << "\tout_value" << e.m_idx
                      << e.m_name << " = "
                      << "int(" << src << ");\n";
                  break;
                case uint_type:
                  str << "\tout_value" << e.m_idx
                      << e.m_name << " = "
                      << "uint(" << src << ");\n";
                  break;
                case float_type:
                  str << "\tout_value" << e.m_idx
                      << e.m_name << " = "
                      << "float(" << src << ");\n";
                  break;

                default:
                case padding_type:
                  str << "\t//Padding at component " << src << "\n";
                }
            }
        }
    }

  str << "}\n\n";
  dst.add_source(str.str().c_str(), glsl::ShaderSource::from_string);

  return *this;
}

const fastuidraw::glsl::UnpackSourceGenerator&
fastuidraw::glsl::UnpackSourceGenerator::
stream_unpack_size_function(ShaderSource &dst,
                            c_string function_name) const
{
  std::ostringstream str;
  UnpackSourceGeneratorPrivate *d;
  d = static_cast<UnpackSourceGeneratorPrivate*>(m_d);

  str << "uint\n"
      << function_name << "(void)\n"
      << "{\n"
      << "\treturn uint("
      << FASTUIDRAW_NUMBER_BLOCK4_NEEDED(d->m_elements.size())
      << ");\n"
      << "}\n";
  dst.add_source(str.str().c_str(), glsl::ShaderSource::from_string);

  return *this;
}
