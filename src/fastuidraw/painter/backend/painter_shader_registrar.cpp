/*!
 * \file painter_shader_registrar.cpp
 * \brief file painter_shader_registrar.cpp
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

#include <fastuidraw/painter/backend/painter_shader_registrar.hpp>
#include <private/util_private.hpp>
#include <algorithm>
#include <mutex>

namespace
{
  class IDGenerator:fastuidraw::noncopyable
  {
  public:
    static
    unsigned int
    new_value(void)
    {
      static IDGenerator D;

      std::lock_guard<std::mutex> M(D.m_mutex);
      return D.m_current++;
    }

  private:
    IDGenerator(void):
      m_current(0)
    {}

    std::mutex m_mutex;
    unsigned int m_current;
  };

  class PainterShaderRegistrarPrivate
  {
  public:
    PainterShaderRegistrarPrivate(void):
      m_unique_id(IDGenerator::new_value())
    {}

    fastuidraw::Mutex m_mutex;
    unsigned int m_unique_id;
  };
}

////////////////////////////////////
// fastuidraw::PainterShaderRegistrar methods
fastuidraw::PainterShaderRegistrar::
PainterShaderRegistrar(void)
{
  m_d = FASTUIDRAWnew PainterShaderRegistrarPrivate();
}

fastuidraw::PainterShaderRegistrar::
~PainterShaderRegistrar()
{
  PainterShaderRegistrarPrivate *d;
  d = static_cast<PainterShaderRegistrarPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::Mutex&
fastuidraw::PainterShaderRegistrar::
mutex(void)
{
  PainterShaderRegistrarPrivate *d;
  d = static_cast<PainterShaderRegistrarPrivate*>(m_d);
  return d->m_mutex;
}

unsigned int
fastuidraw::PainterShaderRegistrar::
unique_id(void) const
{
  PainterShaderRegistrarPrivate *d;
  d = static_cast<PainterShaderRegistrarPrivate*>(m_d);
  return d->m_unique_id;
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(PainterItemShader *shader)
{
  if (!shader || shader->registered_to(*this))
    {
      return;
    }

  /* register the coverage shader if it has one. */
  register_shader(shader->coverage_shader().get());

  if (shader->parent())
    {
      register_shader(static_cast<PainterItemShader*>(shader->parent().get()));

      /* activate the guard AFTER calling register_shader(),
       * otherwise we would attempt to double-lock the mutex
       */
      Mutex::Guard m(mutex());
      shader->set_group_of_sub_shader(*this, compute_item_sub_shader_group(shader));
    }
  else
    {
      Mutex::Guard m(mutex());
      PainterShader::Tag tag;

      tag = absorb_item_shader(shader);
      shader->register_shader(tag, *this);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(PainterItemCoverageShader *shader)
{
  if (!shader || shader->registered_to(*this))
    {
      return;
    }

  if (shader->parent())
    {
      register_shader(static_cast<PainterItemCoverageShader*>(shader->parent().get()));

      /* activate the guard AFTER calling register_shader(),
       * otherwise we would attempt to double-lock the mutex
       */
      Mutex::Guard m(mutex());
      shader->set_group_of_sub_shader(*this, compute_item_coverage_sub_shader_group(shader));
    }
  else
    {
      Mutex::Guard m(mutex());
      PainterShader::Tag tag;

      tag = absorb_item_coverage_shader(shader);
      shader->register_shader(tag, *this);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(PainterBlendShader *shader)
{
  if (!shader || shader->registered_to(*this))
    {
      return;
    }

  if (!blend_type_supported(shader->type()))
    {
      FASTUIDRAWwarning(!"Attempted to register blend shader of unsupported type");
      return;
    }

  if (shader->parent())
    {
      register_shader(static_cast<PainterBlendShader*>(shader->parent().get()));

      /* activate the guard AFTER calling register_shader(),
       * otherwise we would attempt to double-lock the mutex
       */
      Mutex::Guard m(mutex());
      shader->set_group_of_sub_shader(*this, compute_blend_sub_shader_group(shader));
    }
  else
    {
      Mutex::Guard m(mutex());
      PainterShader::Tag tag;

      tag = absorb_blend_shader(shader);
      shader->register_shader(tag, *this);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(PainterBrushShader *shader)
{
  if (!shader || shader->registered_to(*this))
    {
      return;
    }

  if (shader->parent())
    {
      register_shader(static_cast<PainterBrushShader*>(shader->parent().get()));

      /* activate the guard AFTER calling register_shader(),
       * otherwise we would attempt to double-lock the mutex
       */
      Mutex::Guard m(mutex());
      shader->set_group_of_sub_shader(*this, compute_custom_brush_sub_shader_group(shader));
    }
  else
    {
      Mutex::Guard m(mutex());
      PainterShader::Tag tag;

      tag = absorb_custom_brush_shader(shader);
      shader->register_shader(tag, *this);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterImageBrushShader *shader)
{
  if (!shader)
    {
      return;
    }

  c_array<const reference_counted_ptr<PainterBrushShader> > sub_shaders(shader->sub_shaders());
  for (const reference_counted_ptr<PainterBrushShader> &sub : sub_shaders)
    {
      register_shader(sub);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterGradientBrushShader *shader)
{
  if (!shader)
    {
      return;
    }

  register_shader(shader->white_shader());
  for (unsigned int i = 0; i < PainterBrushEnums::number_spread_types; ++i)
    {
      enum PainterBrushEnums::spread_type_t sp;

      sp = static_cast<enum PainterBrushEnums::spread_type_t>(i);
      register_shader(shader->linear_sub_shader(sp));
      register_shader(shader->radial_sub_shader(sp));
      register_shader(shader->sweep_sub_shader(sp));
      for (int j = 0; j < PainterBrushEnums::number_gradient_types; ++j)
        {
          enum PainterBrushEnums::gradient_type_t gt;

          gt = static_cast<enum PainterBrushEnums::gradient_type_t>(j);
          register_shader(shader->sub_shader(sp, gt));
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterGlyphShader &shader)
{
  for(unsigned int i = 0, endi = shader.shader_count(); i < endi; ++i)
    {
      register_shader(shader.shader(static_cast<enum glyph_type>(i)));
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterBlendShaderSet &p)
{
  for(unsigned int i = 0, endi = p.shader_count(); i < endi; ++i)
    {
      enum PainterEnums::blend_mode_t tp;
      tp = static_cast<enum PainterEnums::blend_mode_t>(i);

      const reference_counted_ptr<PainterBlendShader> &sh(p.shader(tp));
      register_shader(sh);
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterBrushShaderSet &shaders)
{
  register_shader(shaders.standard_brush());
  register_shader(shaders.image_brush());
  register_shader(shaders.gradient_brush());
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterShaderSet &shaders)
{
  register_shader(shaders.stroke_shader());
  register_shader(shaders.dashed_stroke_shader());
  register_shader(shaders.fill_shader());
  register_shader(shaders.glyph_shader());
  register_shader(shaders.blend_shaders());
  register_shader(shaders.brush_shaders());
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterStrokeShader &p)
{
  for (unsigned int tp = 0; tp < PainterEnums::stroking_method_number_precise_choices; ++tp)
    {
      enum PainterEnums::stroking_method_t e_tp;
      e_tp = static_cast<enum PainterEnums::stroking_method_t>(tp);
      for (unsigned int sh = 0; sh < PainterStrokeShader::number_shader_types; ++sh)
        {
          enum PainterStrokeShader::shader_type_t e_sh;
          e_sh = static_cast<enum PainterStrokeShader::shader_type_t>(sh);
          register_shader(p.shader(e_tp, e_sh));
        }
    }
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterFillShader &p)
{
  register_shader(p.item_shader());
  register_shader(p.aa_fuzz_shader());
}

void
fastuidraw::PainterShaderRegistrar::
register_shader(const PainterDashedStrokeShaderSet &p)
{
  for(int i = 0; i < PainterEnums::number_cap_styles; ++i)
    {
      enum PainterEnums::cap_style c;
      c = static_cast<enum PainterEnums::cap_style>(i);
      register_shader(p.shader(c));
    }
}
