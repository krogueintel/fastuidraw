/*!
 * \file painter_shader_registrar_glsl.cpp
 * \brief file painter_shader_registrar_glsl.cpp
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

#include <sstream>
#include <vector>
#include <algorithm>

#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/shader_data/painter_shader_data.hpp>
#include <fastuidraw/painter/shader_data/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/shader_data/painter_stroke_params.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <fastuidraw/painter/backend/painter_header.hpp>
#include <fastuidraw/painter/backend/painter_item_matrix.hpp>
#include <fastuidraw/painter/backend/painter_clip_equations.hpp>
#include <fastuidraw/painter/backend/painter_brush_adjust.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_brush_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/unpack_source_generator.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include <fastuidraw/text/glyph_render_data_banded_rays.hpp>

#include <private/glsl/uber_shader_builder.hpp>
#include <private/glsl/backend_shaders.hpp>
#include <private/util_private.hpp>

namespace
{
  enum uniform_ubo_layout
    {
      uniform_ubo_resolution_x_offset,
      uniform_ubo_resolution_y_offset,
      uniform_ubo_recip_resolution_x_offset,
      uniform_ubo_recip_resolution_y_offset,
      uniform_ubo_recip_magnitude_offset,

      uniform_ubo_number_entries
    };

  class BackendConstantsPrivate
  {
  public:
    BackendConstantsPrivate(void):
      m_image_atlas_color_store_width(0),
      m_image_atlas_color_store_height(0),
      m_image_atlas_index_tile_size(0),
      m_image_atlas_color_tile_size(0),
      m_colorstop_atlas_store_width(0)
    {}

    int m_image_atlas_color_store_width;
    int m_image_atlas_color_store_height;
    int m_image_atlas_index_tile_size;
    int m_image_atlas_color_tile_size;
    int m_colorstop_atlas_store_width;
  };

  class UberShaderParamsPrivate
  {
  public:
    UberShaderParamsPrivate(void):
      m_preferred_blend_type(fastuidraw::PainterBlendShader::dual_src),
      m_fbf_blending_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported),
      m_supports_bindless_texturing(false),
      m_clipping_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance),
      m_z_coordinate_convention(fastuidraw::glsl::PainterShaderRegistrarGLSL::z_minus_1_to_1),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(true),
      m_assign_binding_points(true),
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_data_store_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_tbo),
      m_data_blocks_per_store_buffer(-1),
      m_glyph_data_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_tbo),
      m_glyph_data_backing_log2_dims(-1, -1),
      m_have_float_glyph_texture_atlas(true),
      m_colorstop_atlas_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_texture_1d_array),
      m_use_ubo_for_uniforms(true),
      m_use_uvec2_for_bindless_handle(true),
      m_number_context_textures(1),
      m_use_glsl_unpack_fp16(true),
      m_add_default_precision_qualifiers(false),

      m_recompute_binding_points(true),
      m_colorstop_atlas_binding(-1),
      m_image_atlas_color_tiles_nearest_binding(-1),
      m_image_atlas_color_tiles_linear_binding(-1),
      m_image_atlas_index_tiles_binding(-1),
      m_glyph_atlas_store_binding(-1),
      m_data_store_buffer_binding(-1),
      m_context_texture_binding(-1),
      m_coverage_buffer_texture_binding(-1),
      m_uniforms_ubo_binding(-1),
      m_color_interlock_image_buffer_binding(-1)
    {}

    void
    recompute_binding_points(void);

    enum fastuidraw::PainterBlendShader::shader_type m_preferred_blend_type;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t m_fbf_blending_type;
    bool m_supports_bindless_texturing;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_type_t m_clipping_type;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::z_coordinate_convention_t m_z_coordinate_convention;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
    int m_data_blocks_per_store_buffer;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_backing_t m_glyph_data_backing;
    fastuidraw::ivec2 m_glyph_data_backing_log2_dims;
    bool m_have_float_glyph_texture_atlas;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_backing_t m_colorstop_atlas_backing;
    bool m_use_ubo_for_uniforms;
    bool m_use_uvec2_for_bindless_handle;
    unsigned int m_number_context_textures;
    bool m_use_glsl_unpack_fp16;
    bool m_add_default_precision_qualifiers;

    bool m_recompute_binding_points;
    int m_colorstop_atlas_binding;
    int m_image_atlas_color_tiles_nearest_binding;
    int m_image_atlas_color_tiles_linear_binding;
    int m_image_atlas_index_tiles_binding;
    int m_glyph_atlas_store_binding;
    int m_data_store_buffer_binding;
    int m_context_texture_binding;
    int m_coverage_buffer_texture_binding;
    int m_uniforms_ubo_binding;
    int m_color_interlock_image_buffer_binding;

    unsigned int m_num_texture_units;
    unsigned int m_num_ubo_units;
    unsigned int m_num_ssbo_units;
    unsigned int m_num_image_units;
  };

  class VaryingCounts:fastuidraw::noncopyable
  {
  public:
    VaryingCounts(void):
      m_number_varyings(0)
    {}

    void
    update_varying_size(const fastuidraw::glsl::varying_list &plist)
    {
      for(unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolator_number_types; ++i)
        {
          enum fastuidraw::glsl::varying_list::interpolator_type_t q;
          q = static_cast<enum fastuidraw::glsl::varying_list::interpolator_type_t>(i);
          m_number_varyings[q] = std::max(m_number_varyings[q], plist.varyings(q).size());
        }
    }

    fastuidraw::vecN<size_t, fastuidraw::glsl::varying_list::interpolator_number_types> m_number_varyings;
  };

  class BlendShaderGroup
  {
  public:
    typedef fastuidraw::glsl::PainterBlendShaderGLSL Shader;
    typedef fastuidraw::reference_counted_ptr<Shader> Ref;
    std::vector<Ref> m_shaders;
  };

  class CustomBrushShaderGroup:public VaryingCounts
  {
  public:
    typedef fastuidraw::glsl::PainterBrushShaderGLSL Shader;
    typedef fastuidraw::reference_counted_ptr<Shader> Ref;
    std::vector<Ref> m_shaders;
  };

  template<typename T>
  class PerItemShaderRenderType:public VaryingCounts
  {
  public:
    PerItemShaderRenderType(void):
      m_next_item_shader_ID(1)
    {}

    unsigned int
    absorb_item_shader(const fastuidraw::reference_counted_ptr<T> &h)
    {
      unsigned int return_value(m_next_item_shader_ID);

      m_shaders.push_back(h);
      update_varying_size(h->varyings());
      while (m_shaders_keyed_by_id.size() < m_next_item_shader_ID)
        {
          m_shaders_keyed_by_id.push_back(nullptr);
        }
      for (unsigned int i = 0, endi = h->number_sub_shaders(); i < endi; ++i)
        {
          m_shaders_keyed_by_id.push_back(h);
        }
      m_next_item_shader_ID += h->number_sub_shaders();

      return return_value;
    }

    std::vector<fastuidraw::reference_counted_ptr<T> > m_shaders;
    std::vector<fastuidraw::reference_counted_ptr<T> > m_shaders_keyed_by_id;
    unsigned int m_next_item_shader_ID;

    fastuidraw::glsl::varying_list m_main_varyings_shaders_and_shader_datas;
  };

  class PainterShaderRegistrarGLSLPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    PainterShaderRegistrarGLSLPrivate(fastuidraw::glsl::PainterShaderRegistrarGLSL *p);
    ~PainterShaderRegistrarGLSLPrivate();

    template<typename T>
    void
    construct_shader(enum fastuidraw::PainterBlendShader::shader_type tp,
                     enum fastuidraw::PainterSurface::render_type_t render_type,
                     const PerItemShaderRenderType<T> &shaders,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                     fastuidraw::glsl::ShaderSource &out_vertex,
                     fastuidraw::glsl::ShaderSource &out_fragment,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &construct_params,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSL::ShaderFilter<T> *shader_filter,
                     fastuidraw::c_string discard_macro_value);

    template<typename T>
    void
    construct_shader(enum fastuidraw::PainterBlendShader::shader_type tp,
                     enum fastuidraw::PainterSurface::render_type_t render_type,
                     const PerItemShaderRenderType<T> &shaders,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                     fastuidraw::glsl::ShaderSource &out_vertex,
                     fastuidraw::glsl::ShaderSource &out_fragment,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &construct_params,
                     unsigned int shader_id,
                     fastuidraw::c_string discard_macro_value);

    template<typename T>
    void
    construct_shader_common(enum fastuidraw::PainterBlendShader::shader_type tp,
                            enum fastuidraw::PainterSurface::render_type_t render_type,
                            const PerItemShaderRenderType<T> &shaders,
                            const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                            fastuidraw::glsl::ShaderSource &out_vertex,
                            fastuidraw::glsl::ShaderSource &out_fragment,
                            fastuidraw::glsl::detail::UberShaderVaryings &uber_shader_varyings,
                            const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &construct_params,
                            fastuidraw::c_string discard_macro_value);

    void
    update_varying_size(const fastuidraw::glsl::varying_list &plist);

    std::string
    declare_shader_uniforms(const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params);

    void
    ready_main_varyings(void);

    void
    ready_constants(void);

    void
    add_backend_constants(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                          fastuidraw::glsl::ShaderSource &src);

    void
    stream_unpack_code(fastuidraw::glsl::ShaderSource &src);

    enum fastuidraw::PainterBlendShader::shader_type m_blend_type;
    PerItemShaderRenderType<fastuidraw::glsl::PainterItemShaderGLSL> m_item_shaders;
    PerItemShaderRenderType<fastuidraw::glsl::PainterItemCoverageShaderGLSL> m_item_coverage_shaders;

    fastuidraw::vecN<BlendShaderGroup, fastuidraw::PainterBlendShader::number_types> m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
    CustomBrushShaderGroup m_custom_brush_shaders;
    unsigned int m_next_custom_brush_shader_ID;
    fastuidraw::glsl::ShaderSource m_constant_code;
    fastuidraw::glsl::ShaderSource m_vert_shader_utils;
    fastuidraw::glsl::ShaderSource m_frag_shader_utils;
    fastuidraw::glsl::ShaderSource::MacroSet m_banded_rays_macros, m_restricted_rays_macros;;

    fastuidraw::glsl::varying_list m_clip_varyings;
    fastuidraw::glsl::PainterShaderRegistrarGLSL *m_p;
  };
}

/////////////////////////////////////
// PainterShaderRegistrarGLSLPrivate methods
PainterShaderRegistrarGLSLPrivate::
PainterShaderRegistrarGLSLPrivate(fastuidraw::glsl::PainterShaderRegistrarGLSL *p):
  m_next_blend_shader_ID(1),
  m_next_custom_brush_shader_ID(1),
  m_p(p)
{
  /* add varyings needed by fastuidraw_painter_main
   */
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  ready_main_varyings();
  ready_constants();

  m_vert_shader_utils
    .add_source("fastuidraw_bit_utils.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_spread.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_gradient.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_compute_interval.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_unpack_unit_vector.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string",
                ShaderSource::from_resource)
    .add_source("fastuidraw_align.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_read_texels_from_data.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_texture_fetch.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_atlas_image_fetch.glsl.resource_string", ShaderSource::from_resource);

  m_frag_shader_utils
    .add_source("fastuidraw_bit_utils.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_spread.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_gradient.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_compute_interval.glsl.resource_string", ShaderSource::from_resource)
    .add_macros(m_banded_rays_macros)
    .add_source("fastuidraw_banded_rays.glsl.resource_string", ShaderSource::from_resource)
    .remove_macros(m_banded_rays_macros)
    .add_macros(m_restricted_rays_macros)
    .add_source("fastuidraw_restricted_rays.glsl.resource_string", ShaderSource::from_resource)
    .remove_macros(m_restricted_rays_macros)
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_blend_util.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_read_texels_from_data.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_texture_fetch.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_atlas_image_fetch.glsl.resource_string", ShaderSource::from_resource);
}

PainterShaderRegistrarGLSLPrivate::
~PainterShaderRegistrarGLSLPrivate()
{
}

void
PainterShaderRegistrarGLSLPrivate::
ready_main_varyings(void)
{
  using namespace fastuidraw;

  m_item_shaders.m_main_varyings_shaders_and_shader_datas
    .add_uint("fastuidraw_frag_shader")
    .add_uint("fastuidraw_frag_shader_data_location")
    .add_uint("fastuidraw_blend_shader")
    .add_uint("fastuidraw_blend_shader_data_location")
    .add_uint("fastuidraw_brush_shader")
    .add_uint("fastuidraw_brush_shader_data_location")
    .add_int("fastuidraw_deferred_buffer_offset_x")
    .add_int("fastuidraw_deferred_buffer_offset_y")
    .add_int("fastuidraw_deferred_buffer_min_x")
    .add_int("fastuidraw_deferred_buffer_min_y")
    .add_int("fastuidraw_deferred_buffer_max_x")
    .add_int("fastuidraw_deferred_buffer_max_y");

  m_item_coverage_shaders.m_main_varyings_shaders_and_shader_datas
    .add_uint("fastuidraw_frag_shader")
    .add_uint("fastuidraw_frag_shader_data_location");

  m_clip_varyings
    .add_float("fastuidraw_clip_plane0")
    .add_float("fastuidraw_clip_plane1")
    .add_float("fastuidraw_clip_plane2")
    .add_float("fastuidraw_clip_plane3");
}

void
PainterShaderRegistrarGLSLPrivate::
add_backend_constants(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                      fastuidraw::glsl::ShaderSource &src)
{
  using namespace fastuidraw;

  if (backend.image_atlas_index_tile_size() == 0
      || backend.image_atlas_color_tile_size() == 0
      || backend.image_atlas_color_store_width() == 0
      || backend.image_atlas_color_store_height() == 0)
    {
      src.add_macro("FASTUIDRAW_IMAGE_ATLAS_DISABLED");
    }
  else
    {
      src
        .add_macro_u32("FASTUIDRAW_IMAGE_ATLAS_INDEX_TILE_SIZE", backend.image_atlas_index_tile_size())
        .add_macro_u32("FASTUIDRAW_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(backend.image_atlas_index_tile_size()))
        .add_macro_float("FASTUIDRAW_IMAGE_ATLAS_INDEX_RECIP_TILE_SIZE", 1.0f / static_cast<float>(backend.image_atlas_index_tile_size()))
        .add_macro_u32("FASTUIDRAW_IMAGE_ATLAS_COLOR_TILE_SIZE", backend.image_atlas_color_tile_size())
        .add_macro_u32("FASTUIDRAW_IMAGE_ATLAS_COLOR_TILE_LOG2_SIZE", uint32_log2(backend.image_atlas_color_tile_size()))
        .add_macro_float("FASTUIDRAW_IMAGE_ATLAS_COLOR_RECIP_TILE_SIZE", 1.0f / static_cast<float>(backend.image_atlas_color_tile_size()))
        .add_macro_u32("fastuidraw_imageAtlasLinear_size_x", backend.image_atlas_color_store_width())
        .add_macro_u32("fastuidraw_imageAtlasLinear_size_y", backend.image_atlas_color_store_height())
        .add_macro("fastuidraw_imageAtlasLinear_size", "ivec2(fastuidraw_imageAtlasLinear_size_x, fastuidraw_imageAtlasLinear_size_y)")
        .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal_x", "(1.0 / float(fastuidraw_imageAtlasLinear_size_x) )")
        .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal_y", "(1.0 / float(fastuidraw_imageAtlasLinear_size_y) )")
        .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal",
                   "vec2(fastuidraw_imageAtlasLinear_size_reciprocal_x, fastuidraw_imageAtlasLinear_size_reciprocal_y)");
    }

  src
    .add_macro_u32("fastuidraw_colorStopAtlas_size", backend.colorstop_atlas_store_width())
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )")

    .add_macro_u32("fastuidraw_brush_transformation_matrix_num_blocks",
                   FASTUIDRAW_NUMBER_BLOCK4_NEEDED(PainterBrush::transformation_matrix_data_size))
    .add_macro_u32("fastuidraw_brush_transformation_translation_num_blocks",
                   FASTUIDRAW_NUMBER_BLOCK4_NEEDED(PainterBrush::transformation_translation_data_size))
    .add_macro_u32("fastuidraw_stroke_dashed_stroking_params_header_num_blocks",
                   FASTUIDRAW_NUMBER_BLOCK4_NEEDED(PainterDashedStrokeParams::stroke_static_data_size));
}

void
PainterShaderRegistrarGLSLPrivate::
ready_constants(void)
{
  using namespace fastuidraw;

  /* fp32 can store a 24-bit integer exactly,
   * however, the operation of converting from
   * uint to normalized fp32 may lose a bit,
   * so 23-bits it is.
   * TODO: go through the requirements of IEEE754,
   * what a compiler of a driver might do and
   * what a GPU does to see how many bits we
   * really have.
   */
  uint32_t z_bits_supported;
  z_bits_supported = 23u;

  m_constant_code
    .add_macro_u32("fastuidraw_half_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported - 1))
    .add_macro_u32("fastuidraw_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported))

    ////////////////////////////////////
    // constants for PainterHeader
    .add_macro_u32("FASTUIDRAW_HEADER_DRAWING_OCCLUDER", PainterHeader::drawing_occluder);

    ////////////////////////////////////////
    // constants for GlyphRenderDataRestrictedRays
  m_restricted_rays_macros
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_node_bit", GlyphRenderDataRestrictedRays::hierarchy_is_node_bit)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_node_mask", FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::hierarchy_is_node_bit, 1u))
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_split_coord_bit", GlyphRenderDataRestrictedRays::hierarchy_splitting_coordinate_bit)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_child0_bit", GlyphRenderDataRestrictedRays::hierarchy_child0_offset_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_child1_bit", GlyphRenderDataRestrictedRays::hierarchy_child1_offset_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_child_num_bits", GlyphRenderDataRestrictedRays::hierarchy_child_offset_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_curve_list_bit0", GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_curve_list_num_bits", GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_curve_list_size_bit0", GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_hierarchy_curve_list_size_num_bits", GlyphRenderDataRestrictedRays::hierarchy_leaf_curve_list_size_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_winding_value_bias", GlyphRenderDataRestrictedRays::winding_bias)
    .add_macro_u32("fastuidraw_restricted_rays_winding_value_bit0", GlyphRenderDataRestrictedRays::winding_value_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_winding_value_num_bits", GlyphRenderDataRestrictedRays::winding_value_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_position_delta_divide", GlyphRenderDataRestrictedRays::delta_div_factor)
    .add_macro_u32("fastuidraw_restricted_rays_position_delta_x_bit0", GlyphRenderDataRestrictedRays::delta_x_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_position_delta_y_bit0", GlyphRenderDataRestrictedRays::delta_y_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_position_delta_num_bits", GlyphRenderDataRestrictedRays::delta_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_curve_entry_num_bits", GlyphRenderDataRestrictedRays::curve_numbits)
    .add_macro_u32("fastuidraw_restricted_rays_curve_entry0_bit0", GlyphRenderDataRestrictedRays::curve_entry0_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_curve_entry1_bit0", GlyphRenderDataRestrictedRays::curve_entry1_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_curve_is_quadratic_bit", GlyphRenderDataRestrictedRays::curve_is_quadratic_bit)
    .add_macro_u32("fastuidraw_restricted_rays_curve_is_quadratic_mask", FASTUIDRAW_MASK(GlyphRenderDataRestrictedRays::curve_is_quadratic_bit, 1u))
    .add_macro_u32("fastuidraw_restricted_rays_curve_bit0", GlyphRenderDataRestrictedRays::curve_location_bit0)
    .add_macro_u32("fastuidraw_restricted_rays_curve_num_bits", GlyphRenderDataRestrictedRays::curve_location_numbits)
    .add_macro_float("fastuidraw_restricted_rays_glyph_coord_value", GlyphRenderDataRestrictedRays::glyph_coord_value);

    ////////////////////////////////////////
    // constants for GlyphRenderDataBandedRays
  m_banded_rays_macros
    .add_macro_u32("fastuidraw_banded_rays_numcurves_numbits", GlyphRenderDataBandedRays::band_numcurves_numbits)
    .add_macro_u32("fastuidraw_banded_rays_numcurves_bit0", GlyphRenderDataBandedRays::band_numcurves_bit0)
    .add_macro_u32("fastuidraw_banded_rays_curveoffset_numbits", GlyphRenderDataBandedRays::band_curveoffset_numbits)
    .add_macro_u32("fastuidraw_banded_rays_curveoffset_bit0", GlyphRenderDataBandedRays::band_curveoffset_bit0)
    .add_macro_float("fastuidraw_banded_rays_glyph_coord", GlyphRenderDataBandedRays::glyph_coord_value)
    .add_macro_float("fastuidraw_banded_rays_glyph_coord_half_recip", 0.5f / static_cast<float>(GlyphRenderDataBandedRays::glyph_coord_value))
    .add_macro_float("fastuidraw_banded_rays_glyph_coord_doubled", 2 * GlyphRenderDataBandedRays::glyph_coord_value)
    ;
}

void
PainterShaderRegistrarGLSLPrivate::
stream_unpack_code(fastuidraw::glsl::ShaderSource &str)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  UnpackSourceGenerator("fastuidraw_header")
    .set_uint(PainterHeader::clip_equations_location_offset, ".clipping_location")
    .set_uint(PainterHeader::item_matrix_location_offset, ".item_matrix_location")
    .set_uint(PainterHeader::brush_shader_data_location_offset, ".brush_shader_data_location")
    .set_uint(PainterHeader::item_shader_data_location_offset, ".item_shader_data_location")
    .set_uint(PainterHeader::blend_shader_data_location_offset, ".blend_shader_data_location")
    .set_uint(PainterHeader::brush_shader_offset, ".brush_shader")
    .set_int(PainterHeader::z_offset, ".z")
    .set_uint(PainterHeader::item_shader_offset, ".item_shader")
    .set_uint(PainterHeader::blend_shader_offset, ".blend_shader")
    .set_uint(PainterHeader::brush_adjust_location_offset, ".brush_adjust_location")
    .set_int(PainterHeader::offset_to_deferred_coverage_x_offset, ".offset_to_deferred_coverage.x")
    .set_int(PainterHeader::offset_to_deferred_coverage_y_offset, ".offset_to_deferred_coverage.y")
    .set_int(PainterHeader::deferred_coverage_min_x_offset, ".deferred_coverage_min.x")
    .set_int(PainterHeader::deferred_coverage_min_y_offset, ".deferred_coverage_min.y")
    .set_int(PainterHeader::deferred_coverage_max_x_offset, ".deferred_coverage_max.x")
    .set_int(PainterHeader::deferred_coverage_max_y_offset, ".deferred_coverage_max.y")
    .stream_unpack_function(str, "fastuidraw_read_header");

  UnpackSourceGenerator("fastuidraw_clipping_data")
    .set_float(PainterClipEquations::clip0_coeff_x, ".clip0.x")
    .set_float(PainterClipEquations::clip0_coeff_y, ".clip0.y")
    .set_float(PainterClipEquations::clip0_coeff_w, ".clip0.z")
    .set_float(PainterClipEquations::clip1_coeff_x, ".clip1.x")
    .set_float(PainterClipEquations::clip1_coeff_y, ".clip1.y")
    .set_float(PainterClipEquations::clip1_coeff_w, ".clip1.z")
    .set_float(PainterClipEquations::clip2_coeff_x, ".clip2.x")
    .set_float(PainterClipEquations::clip2_coeff_y, ".clip2.y")
    .set_float(PainterClipEquations::clip2_coeff_w, ".clip2.z")
    .set_float(PainterClipEquations::clip3_coeff_x, ".clip3.x")
    .set_float(PainterClipEquations::clip3_coeff_y, ".clip3.y")
    .set_float(PainterClipEquations::clip3_coeff_w, ".clip3.z")
    .stream_unpack_function(str, "fastuidraw_read_clipping");

  UnpackSourceGenerator("fastuidraw_brush_adjust_data")
    .set_float(PainterBrushAdjust::shear_x_offset, ".shear.x")
    .set_float(PainterBrushAdjust::shear_y_offset, ".shear.y")
    .set_float(PainterBrushAdjust::translation_x_offset, ".translate.x")
    .set_float(PainterBrushAdjust::translation_y_offset, ".translate.y")
    .stream_unpack_function(str, "fastuidraw_read_brush_adjust");

  /* Matrics in GLSL are [column][row], that is why we use the
   * matrix_colX_rowY_offset enums
   */
  vecN<c_string, 2> painter_item_matrix_structs("mat3", "vec2");
  UnpackSourceGenerator(painter_item_matrix_structs)
    .set_float(PainterItemMatrix::matrix_col0_row0_offset, "[0][0]", 0)
    .set_float(PainterItemMatrix::matrix_col0_row1_offset, "[0][1]", 0)
    .set_float(PainterItemMatrix::matrix_col0_row2_offset, "[0][2]", 0)
    .set_float(PainterItemMatrix::matrix_col1_row0_offset, "[1][0]", 0)
    .set_float(PainterItemMatrix::matrix_col1_row1_offset, "[1][1]", 0)
    .set_float(PainterItemMatrix::matrix_col1_row2_offset, "[1][2]", 0)
    .set_float(PainterItemMatrix::matrix_col2_row0_offset, "[2][0]", 0)
    .set_float(PainterItemMatrix::matrix_col2_row1_offset, "[2][1]", 0)
    .set_float(PainterItemMatrix::matrix_col2_row2_offset, "[2][2]", 0)
    .set_float(PainterItemMatrix::normalized_translate_x, ".x", 1)
    .set_float(PainterItemMatrix::normalized_translate_y, ".y", 1)
    .stream_unpack_function(str, "fastuidraw_read_item_matrix");

  UnpackSourceGenerator("fastuidraw_stroking_params")
    .set_float(PainterStrokeParams::stroke_radius_offset, ".radius")
    .set_float(PainterStrokeParams::stroke_miter_limit_offset, ".miter_limit")
    .set_uint(PainterStrokeParams::stroking_units_offset, ".stroking_units")
    .stream_unpack_function(str, "fastuidraw_read_stroking_params")
    .stream_unpack_size_function(str, "fastuidraw_read_stroking_params_size");

  UnpackSourceGenerator("fastuidraw_dashed_stroking_params_header")
    .set_float(PainterDashedStrokeParams::stroke_radius_offset, ".radius")
    .set_float(PainterDashedStrokeParams::stroke_miter_limit_offset, ".miter_limit")
    .set_uint(PainterDashedStrokeParams::stroking_units_offset, ".stroking_units")
    .set_float(PainterDashedStrokeParams::stroke_dash_offset_offset, ".dash_offset")
    .set_float(PainterDashedStrokeParams::stroke_total_length_offset, ".total_length")
    .set_float(PainterDashedStrokeParams::stroke_first_interval_start_offset, ".first_interval_start")
    .set_float(PainterDashedStrokeParams::stroke_first_interval_start_on_looping_offset, ".first_interval_start_on_looping")
    .set_uint(PainterDashedStrokeParams::stroke_number_intervals_offset, ".number_intervals")
    .stream_unpack_function(str, "fastuidraw_read_dashed_stroking_params_header")
    .stream_unpack_size_function(str, "fastuidraw_read_dashed_stroking_params_header_size");
}

std::string
PainterShaderRegistrarGLSLPrivate::
declare_shader_uniforms(const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  std::ostringstream ostr;

  if (params.use_ubo_for_uniforms())
    {
      c_string ext="xyzw";
      /* Mesa packs UBO data float[N] as really vec4[N],
       * so instead realize the data directly as vec4[K]
       */
      ostr << "FASTUIDRAW_LAYOUT_BINDING("
           << params.uniforms_ubo_binding()
           << ") " << " uniform fastuidraw_uniform_block {\n"
           << "vec4 fastuidraw_uniforms["
           << PainterShaderRegistrarGLSL::ubo_size() / 4 << "];\n"
           << "};\n"
           << "#define fastuidraw_viewport_pixels vec2(fastuidraw_uniforms["
           << uniform_ubo_resolution_x_offset / 4 << "]."
           << ext[uniform_ubo_resolution_x_offset % 4]
           << ", fastuidraw_uniforms[" << uniform_ubo_resolution_y_offset / 4 << "]."
           << ext[uniform_ubo_resolution_y_offset % 4] << ")\n"
           << "#define fastuidraw_viewport_recip_pixels vec2(fastuidraw_uniforms["
           << uniform_ubo_recip_resolution_x_offset / 4
           << "]."<< ext[uniform_ubo_recip_resolution_x_offset % 4]
           << ", fastuidraw_uniforms["
           << uniform_ubo_recip_resolution_y_offset / 4 << "]."
           << ext[uniform_ubo_recip_resolution_y_offset % 4] << ")\n"
           << "#define fastuidraw_viewport_recip_pixels_magnitude fastuidraw_uniforms["
           << uniform_ubo_recip_magnitude_offset / 4 << "]."
           << ext[uniform_ubo_recip_magnitude_offset % 4] << "\n";
    }
  else
    {
      ostr << "uniform float fastuidraw_uniforms["
           << PainterShaderRegistrarGLSL::ubo_size() << "];\n"
           << "#define fastuidraw_viewport_pixels vec2(fastuidraw_uniforms["
           << uniform_ubo_resolution_x_offset << "], fastuidraw_uniforms["
           << uniform_ubo_resolution_y_offset << "])\n"
           << "#define fastuidraw_viewport_recip_pixels vec2(fastuidraw_uniforms["
           << uniform_ubo_recip_resolution_x_offset
           << "], fastuidraw_uniforms["
           << uniform_ubo_recip_resolution_y_offset << "])\n"
           << "#define fastuidraw_viewport_recip_pixels_magnitude fastuidraw_uniforms["
           << uniform_ubo_recip_magnitude_offset << "]\n";
    }

  return ostr.str();
}

template<typename T>
void
PainterShaderRegistrarGLSLPrivate::
construct_shader_common(enum fastuidraw::PainterBlendShader::shader_type blend_type,
                        enum fastuidraw::PainterSurface::render_type_t render_type,
                        const PerItemShaderRenderType<T> &shaders,
                        const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                        fastuidraw::glsl::ShaderSource &vert,
                        fastuidraw::glsl::ShaderSource &frag,
                        fastuidraw::glsl::detail::UberShaderVaryings &uber_shader_varyings,
                        const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params,
                        fastuidraw::c_string discard_macro_value)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  std::string varying_layout_macro, binding_layout_macro;
  std::string declare_varyings;
  AliasVaryingLocation main_varying_datum, brush_varying_datum;
  AliasVaryingLocation clip_varying_datum;
  c_string shader_blend_macro(nullptr);
  std::string declare_vertex_shader_ins;
  std::string declare_uniforms;
  const varying_list *main_varyings;

  if (params.assign_layout_to_vertex_shader_inputs())
    {
      std::ostringstream ostr;
      ostr << "layout(location = " << PainterShaderRegistrarGLSL::attribute0_slot
           << ") in uvec4 fastuidraw_attribute0;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::attribute1_slot
           << ") in uvec4 fastuidraw_attribute1;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::attribute2_slot
           << ") in uvec4 fastuidraw_attribute2;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::header_attrib_slot
           << ") in uint fastuidraw_header_attribute;\n";
      declare_vertex_shader_ins = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "in uvec4 fastuidraw_attribute0;\n"
           << "in uvec4 fastuidraw_attribute1;\n"
           << "in uvec4 fastuidraw_attribute2;\n"
           << "in uint fastuidraw_header_attribute;\n";
      declare_vertex_shader_ins = ostr.str();
    }

  if (params.assign_layout_to_varyings())
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_VARYING(X) layout(location = X)\n";
      varying_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_VARYING(X)\n";
      varying_layout_macro = ostr.str();
    }

  if (params.assign_binding_points())
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_BINDING(X) layout(binding = X)\n"
           << "#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(binding = X, Y)\n";
      binding_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_BINDING(X)\n"
           << "#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(Y)\n";
      binding_layout_macro = ostr.str();
    }

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.add_varyings("clip", m_clip_varyings, &clip_varying_datum);
    }

  main_varyings = &shaders.m_main_varyings_shaders_and_shader_datas;
  if (render_type == PainterSurface::color_buffer_type)
    {
      uber_shader_varyings.add_varyings("brush",
                                        m_custom_brush_shaders.m_number_varyings,
                                        &brush_varying_datum);
    }

  uber_shader_varyings.add_varyings("main", *main_varyings, &main_varying_datum);

  declare_uniforms = declare_shader_uniforms(params);
  declare_varyings = uber_shader_varyings.declare_varyings("fastuidraw_varying");

  if (params.z_coordinate_convention() == PainterShaderRegistrarGLSL::z_minus_1_to_1)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
    }
  else
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
    }

  if (params.use_glsl_unpack_fp16())
    {
      vert.add_macro("FASTUIDRAW_GL_HAS_UNPACKFP16");
      frag.add_macro("FASTUIDRAW_GL_HAS_UNPACKFP16");
    }

  if (render_type == PainterSurface::color_buffer_type)
    {
      vert.add_macro("FASTUIDRAW_RENDER_TO_COLOR_BUFFER");
      frag.add_macro("FASTUIDRAW_RENDER_TO_COLOR_BUFFER");
      switch(params.clipping_type())
        {
        case PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance:
          vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_GL_CLIP_DISTACE");
          frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_GL_CLIP_DISTACE");
          break;

        case PainterShaderRegistrarGLSL::clipping_via_discard:
          vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_DISCARD");
          frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_DISCARD");
          break;

        case PainterShaderRegistrarGLSL::clipping_via_skip_color_write:
          if (blend_type == PainterBlendShader::framebuffer_fetch)
            {
              vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_SKIP_COLOR_WRITE");
              frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_SKIP_COLOR_WRITE");
            }
          else
            {
              /* if the blend_type is not framebuffer_fetch, then skipping
               * color write is not possible and so we need to fallback
               * to discard
               */
              vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_DISCARD");
              frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_DISCARD");
            }
          break;
        }

      switch(blend_type)
        {
        case PainterBlendShader::framebuffer_fetch:
          if (params.fbf_blending_type() == PainterShaderRegistrarGLSL::fbf_blending_framebuffer_fetch)
            {
              shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH";
            }
          else
            {
              FASTUIDRAWassert(params.fbf_blending_type() == PainterShaderRegistrarGLSL::fbf_blending_interlock);
              shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_INTERLOCK";
            }
          break;

        case PainterBlendShader::dual_src:
          shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND";
          break;

        case PainterBlendShader::single_src:
          shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND";
          break;

        default:
          shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_INVALID_BLEND";
          FASTUIDRAWassert(!"Invalid blend_type");
        }
    }
  else
    {
      vert.add_macro("FASTUIDRAW_RENDER_TO_DEFERRED_COVERAGE_BUFFER");
      frag.add_macro("FASTUIDRAW_RENDER_TO_DEFERRED_COVERAGE_BUFFER");
      if (params.clipping_type() == PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
        {
          vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_GL_CLIP_DISTACE");
          frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_GL_CLIP_DISTACE");
        }
    }

  if (params.supports_bindless_texturing())
    {
      vert.add_macro("FASTUIDRAW_SUPPORT_BINDLESS_TEXTURE");
      frag.add_macro("FASTUIDRAW_SUPPORT_BINDLESS_TEXTURE");
      if (params.use_uvec2_for_bindless_handle())
        {
          vert.add_macro("FASTUIDRAW_BINDLESS_HANDLE_UVEC2");
          frag.add_macro("FASTUIDRAW_BINDLESS_HANDLE_UVEC2");
        }
      else
        {
          vert.add_macro("FASTUIDRAW_BINDLESS_HANDLE_128U");
          frag.add_macro("FASTUIDRAW_BINDLESS_HANDLE_128U");
        }
    }

  switch(params.colorstop_atlas_backing())
    {
    case PainterShaderRegistrarGLSL::colorstop_texture_1d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
      }
    break;

    case PainterShaderRegistrarGLSL::colorstop_texture_2d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
      }
    break;

    default:
      FASTUIDRAWassert(!"Invalid colorstop_atlas_backing() value");
    }

  switch(params.data_store_backing())
    {
    case PainterShaderRegistrarGLSL::data_store_ubo:
      {
        vert
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());

        frag
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());
      }
      break;

    case PainterShaderRegistrarGLSL::data_store_tbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
      }
      break;

    case PainterShaderRegistrarGLSL::data_store_ssbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_SSBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_SSBO");
      }
      break;

    default:
      FASTUIDRAWassert(!"Invalid data_store_backing() value");
    }

  switch(params.glyph_data_backing())
    {
    case PainterShaderRegistrarGLSL::glyph_data_texture_array:
      {
        vert
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_DATA_WIDTH_LOG2", params.glyph_data_backing_log2_dims().x())
          .add_macro("FASTUIDRAW_GLYPH_DATA_HEIGHT_LOG2", params.glyph_data_backing_log2_dims().y());

        frag
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_DATA_WIDTH_LOG2", params.glyph_data_backing_log2_dims().x())
          .add_macro("FASTUIDRAW_GLYPH_DATA_HEIGHT_LOG2", params.glyph_data_backing_log2_dims().y());
      }
      break;

    case PainterShaderRegistrarGLSL::glyph_data_tbo:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
      }
      break;

    case PainterShaderRegistrarGLSL::glyph_data_ssbo:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_SSBO");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_SSBO");
      }
      break;
    }

  if (params.add_default_precision_qualifiers())
    {
      vert.add_source("fastuidraw_default_precision.glsl.resource_string", ShaderSource::from_resource);
      frag.add_source("fastuidraw_default_precision.glsl.resource_string", ShaderSource::from_resource);
    }

  add_backend_constants(backend, vert);
  vert
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", params.colorstop_atlas_binding())
    .add_macro("FASTUIDRAW_COLOR_TILE_LINEAR_BINDING", params.image_atlas_color_tiles_linear_binding())
    .add_macro("FASTUIDRAW_COLOR_TILE_NEAREST_BINDING", params.image_atlas_color_tiles_nearest_binding())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", params.image_atlas_index_tiles_binding())
    .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_BINDING", params.glyph_atlas_store_binding())
    .add_macro("FASTUIDRAW_PAINTER_STORE_BINDING", params.data_store_buffer_binding())
    .add_macro("FASTUIDRAW_PAINTER_BLEND_INTERLOCK_BINDING", params.color_interlock_image_buffer_binding())
    .add_macro("FASTUIDRAW_PAINTER_CONTEXT_TEXTURE_BINDING", params.context_texture_binding())
    .add_macro("FASTUIDRAW_PAINTER_NUMBER_CONTEXT_TEXTURES", params.number_context_textures())
    .add_macro("FASTUIDRAW_PAINTER_DEFERRED_COVERAGE_TEXTURE_BINDING", params.coverage_buffer_texture_binding())
    .add_macro("fastuidraw_varying", "out")
    .add_source("fastuidraw_unpackHalf2x16.glsl.resource_string", ShaderSource::from_resource)
    .add_source(declare_vertex_shader_ins.c_str(), ShaderSource::from_string)
    .add_source(declare_varyings.c_str(), ShaderSource::from_string);

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.stream_alias_varyings(false, vert, m_clip_varyings, true, clip_varying_datum);
    }
  uber_shader_varyings.stream_alias_varyings(false, vert, *main_varyings, true, main_varying_datum);

  vert
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_atlases.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_globals.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_vert_shader_utils);

  const char *vert_main_src;
  if (render_type == PainterSurface::color_buffer_type)
    {
      vert_main_src = "fastuidraw_painter_main.vert.glsl.resource_string";
    }
  else
    {
      vert_main_src = "fastuidraw_painter_main_deferred_coverage.vert.glsl.resource_string";
    }

  vert
    .add_source("fastuidraw_painter_clipping.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(vert_main_src, ShaderSource::from_resource);

  stream_unpack_code(vert);

  add_backend_constants(backend, frag);
  frag
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_DISCARD", discard_macro_value)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", params.colorstop_atlas_binding())
    .add_macro("FASTUIDRAW_COLOR_TILE_LINEAR_BINDING", params.image_atlas_color_tiles_linear_binding())
    .add_macro("FASTUIDRAW_COLOR_TILE_NEAREST_BINDING", params.image_atlas_color_tiles_nearest_binding())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", params.image_atlas_index_tiles_binding())
    .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_BINDING", params.glyph_atlas_store_binding())
    .add_macro("FASTUIDRAW_PAINTER_STORE_BINDING", params.data_store_buffer_binding())
    .add_macro("FASTUIDRAW_PAINTER_BLEND_INTERLOCK_BINDING", params.color_interlock_image_buffer_binding())
    .add_macro("FASTUIDRAW_PAINTER_CONTEXT_TEXTURE_BINDING", params.context_texture_binding())
    .add_macro("FASTUIDRAW_PAINTER_NUMBER_CONTEXT_TEXTURES", params.number_context_textures())
    .add_macro("FASTUIDRAW_PAINTER_DEFERRED_COVERAGE_TEXTURE_BINDING", params.coverage_buffer_texture_binding())
    .add_macro("fastuidraw_varying", "in")
    .add_source("fastuidraw_unpackHalf2x16.glsl.resource_string", ShaderSource::from_resource)
    .add_source(declare_varyings.c_str(), ShaderSource::from_string);

  uber_shader_varyings.stream_varying_rw_copies(frag);

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.stream_alias_varyings(false, frag, m_clip_varyings, true, clip_varying_datum);
    }
  uber_shader_varyings.stream_alias_varyings(false, frag, *main_varyings, true, main_varying_datum);

  if (render_type == PainterSurface::color_buffer_type)
    {
      frag.add_macro(shader_blend_macro);
    }

  frag
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_atlases.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_frag_shader_utils)
    .add_source("fastuidraw_painter_clipping.frag.glsl.resource_string", ShaderSource::from_resource);

  const char *frag_main_src;
  if (render_type == PainterSurface::color_buffer_type)
    {
      frag_main_src = "fastuidraw_painter_main.frag.glsl.resource_string";
      frag.add_source("fastuidraw_painter_deferred_coverage_buffer.frag.glsl.resource_string",
                      ShaderSource::from_resource);
    }
  else
    {
      frag_main_src = "fastuidraw_painter_main_deferred_coverage.frag.glsl.resource_string";
    }

  frag
    .add_source(frag_main_src, ShaderSource::from_resource);

  stream_unpack_code(frag);

  if (render_type == PainterSurface::color_buffer_type)
    {
      stream_uber_blend_shader(params.blend_shader_use_switch(), frag,
                               make_c_array(m_blend_shaders[blend_type].m_shaders),
                               blend_type, *m_p);

      stream_uber_brush_vert_shader(params.vert_shader_use_switch(), vert,
                                    make_c_array(m_custom_brush_shaders.m_shaders),
                                    uber_shader_varyings, brush_varying_datum, *m_p);

      stream_uber_brush_frag_shader(params.frag_shader_use_switch(), frag,
                                    make_c_array(m_custom_brush_shaders.m_shaders),
                                    uber_shader_varyings, brush_varying_datum, *m_p);
    }
}

template<typename T>
void
PainterShaderRegistrarGLSLPrivate::
construct_shader(enum fastuidraw::PainterBlendShader::shader_type blend_type,
                 enum fastuidraw::PainterSurface::render_type_t render_type,
                 const PerItemShaderRenderType<T> &shaders,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                 fastuidraw::glsl::ShaderSource &vert,
                 fastuidraw::glsl::ShaderSource &frag,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSL::ShaderFilter<T> *shader_filter,
                 fastuidraw::c_string discard_macro_value)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  std::vector<reference_counted_ptr<T> > work_shaders;
  c_array<const reference_counted_ptr<T> > item_shaders;
  UberShaderVaryings uber_shader_varyings;
  AliasVaryingLocation shader_varying_datum;

  if (shader_filter)
    {
      for(const auto &sh : shaders.m_shaders)
        {
          if (shader_filter->use_shader(sh))
            {
              work_shaders.push_back(sh);
            }
        }
      item_shaders = make_c_array(work_shaders);
    }
  else
    {
      item_shaders = make_c_array(shaders.m_shaders);
    }

  uber_shader_varyings.add_varyings("shader",
                                    shaders.m_number_varyings,
                                    &shader_varying_datum);

  construct_shader_common(blend_type, render_type, shaders, backend, vert, frag,
                          uber_shader_varyings,
                          params, discard_macro_value);

  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, item_shaders,
                          uber_shader_varyings, shader_varying_datum, *m_p);

  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, item_shaders,
                          uber_shader_varyings, shader_varying_datum, *m_p);
}

template<typename T>
void
PainterShaderRegistrarGLSLPrivate::
construct_shader(enum fastuidraw::PainterBlendShader::shader_type blend_type,
                 enum fastuidraw::PainterSurface::render_type_t render_type,
                 const PerItemShaderRenderType<T> &shaders,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                 fastuidraw::glsl::ShaderSource &vert,
                 fastuidraw::glsl::ShaderSource &frag,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params,
                 unsigned int shader_id,
                 fastuidraw::c_string discard_macro_value)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  reference_counted_ptr<T> shader;
  UberShaderVaryings uber_shader_varyings;
  AliasVaryingLocation shader_varying_datum;
  c_array<const reference_counted_ptr<T> > item_shaders;

  FASTUIDRAWassert(shader_id < shaders.m_shaders_keyed_by_id.size());
  FASTUIDRAWassert(shaders.m_shaders_keyed_by_id[shader_id]);

  shader = shaders.m_shaders_keyed_by_id[shader_id];
  FASTUIDRAWassert(shader_id >= shader->ID(*m_p));
  item_shaders = c_array<const reference_counted_ptr<T> >(&shader, 1);

  uber_shader_varyings.add_varyings("shader",
                                    shader->varyings(),
                                    &shader_varying_datum);

  construct_shader_common(blend_type, render_type, shaders, backend, vert, frag,
                          uber_shader_varyings,
                          params, discard_macro_value);

  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, item_shaders,
                          uber_shader_varyings, shader_varying_datum, *m_p);

  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, item_shaders,
                          uber_shader_varyings, shader_varying_datum, *m_p);
}

//////////////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants methods
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
BackendConstants(PainterEngine *p)
{
  m_d = FASTUIDRAWnew BackendConstantsPrivate();
  set_from_backend(p);
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
BackendConstants(const BackendConstants &obj)
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew BackendConstantsPrivate(*d);
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
~BackendConstants()
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_backend(PainterEngine *p)
{
  if (p)
    {
      set_from_atlas(p->image_atlas());
      set_from_atlas(p->colorstop_atlas());
    }
  return *this;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_atlas(ImageAtlas &p)
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(m_d);

  d->m_image_atlas_color_store_width = p.color_store() ? p.color_store()->dimensions().x() : 0;
  d->m_image_atlas_color_store_height = p.color_store() ? p.color_store()->dimensions().y() : 0;
  d->m_image_atlas_index_tile_size = p.index_tile_size() > 0 ? p.index_tile_size() : 0;
  d->m_image_atlas_color_tile_size = p.color_tile_size() > 0 ? p.color_tile_size() : 0;

  return *this;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_atlas(ColorStopAtlas &p)
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(m_d);
  d->m_colorstop_atlas_store_width = p.backing_store()->dimensions().x();
  return *this;
}

assign_swap_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants)

setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_store_width)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_store_height)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_index_tile_size)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_tile_size)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, colorstop_atlas_store_width)

/////////////////////////////////////////////////
// UberShaderParamsPrivate methods
void
UberShaderParamsPrivate::
recompute_binding_points(void)
{
  using namespace fastuidraw::glsl;

  FASTUIDRAWassert(m_recompute_binding_points);
  m_recompute_binding_points = false;

  m_num_texture_units = 0;
  m_num_ubo_units = 0;
  m_num_ssbo_units = 0;
  m_num_image_units = 0;

  m_colorstop_atlas_binding = m_num_texture_units++;
  m_image_atlas_color_tiles_nearest_binding = m_num_texture_units++;
  m_image_atlas_color_tiles_linear_binding = m_num_texture_units++;
  m_image_atlas_index_tiles_binding = m_num_texture_units++;
  m_coverage_buffer_texture_binding = m_num_texture_units++;

  switch (m_data_store_backing)
    {
    default:
    case PainterShaderRegistrarGLSL::data_store_tbo:
      m_data_store_buffer_binding = m_num_texture_units++;
      break;
    case PainterShaderRegistrarGLSL::data_store_ubo:
      m_data_store_buffer_binding = m_num_ubo_units++;
      break;
    case PainterShaderRegistrarGLSL::data_store_ssbo:
      m_data_store_buffer_binding = m_num_ssbo_units++;
      break;
    }

  switch (m_glyph_data_backing)
    {
    default:
    case PainterShaderRegistrarGLSL::glyph_data_tbo:
    case PainterShaderRegistrarGLSL::glyph_data_texture_array:
      m_glyph_atlas_store_binding = m_num_texture_units++;
      break;
    case PainterShaderRegistrarGLSL::glyph_data_ssbo:
      m_glyph_atlas_store_binding = m_num_ssbo_units++;
      break;
    }

  if (m_use_ubo_for_uniforms)
    {
      m_uniforms_ubo_binding = m_num_ubo_units++;
    }
  else
    {
      m_uniforms_ubo_binding = -1;
    }

  /* this comes last to make sure that the texture bindings
   * of the external textures are last.
   */
  m_context_texture_binding = m_num_texture_units;
  m_num_texture_units += m_number_context_textures;

  /* This must come last to make sure that the binding points
   * of using or not using fbf-interlock match up.
   */
  if (m_fbf_blending_type == PainterShaderRegistrarGLSL::fbf_blending_interlock)
    {
      m_color_interlock_image_buffer_binding = m_num_image_units++;
    }
  else
    {
      m_color_interlock_image_buffer_binding = -1;
    }
}

////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams methods
fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
UberShaderParams(void)
{
  m_d = FASTUIDRAWnew UberShaderParamsPrivate();
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
UberShaderParams(const UberShaderParams &obj)
{
  UberShaderParamsPrivate *d;
  d = static_cast<UberShaderParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew UberShaderParamsPrivate(*d);
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
~UberShaderParams()
{
  UberShaderParamsPrivate *d;
  d = static_cast<UberShaderParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterShaderSet
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::UberShaderParams::
default_shaders(void) const
{
  detail::ShaderSetCreator S(preferred_blend_type(), fbf_blending_type());
  return S.create_shader_set();
}

assign_swap_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, supports_bindless_texturing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_type_t,
                 clipping_type)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::z_coordinate_convention_t,
                 z_coordinate_convention)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_layout_to_vertex_shader_inputs)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_layout_to_varyings)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_binding_points)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, vert_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, frag_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, blend_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, use_uvec2_for_bindless_handle)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, int, data_blocks_per_store_buffer)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, fastuidraw::ivec2, glyph_data_backing_log2_dims)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_backing_t, colorstop_atlas_backing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, use_glsl_unpack_fp16)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, add_default_precision_qualifiers)

get_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams, UberShaderParamsPrivate, unsigned int, num_ubo_units)
get_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams, UberShaderParamsPrivate, unsigned int, num_ssbo_units)
get_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams, UberShaderParamsPrivate, unsigned int, num_texture_units)
get_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams, UberShaderParamsPrivate, unsigned int, num_image_units)

#define uber_shader_params_set_implement_dirty(type, member)            \
  fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams&       \
  fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::      \
  member(type v)                                                        \
  {                                                                     \
    UberShaderParamsPrivate *d;                                         \
    d = static_cast<UberShaderParamsPrivate*>(m_d);                     \
    d->m_##member = v;                                                  \
    d->m_recompute_binding_points = true;                               \
    return *this;                                                       \
  }

#define uber_shader_params_setget_implement_dirty(type, member)         \
  uber_shader_params_set_implement_dirty(type, member)                  \
  get_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams, UberShaderParamsPrivate, type, member)

uber_shader_params_setget_implement_dirty(enum fastuidraw::PainterBlendShader::shader_type, preferred_blend_type)
uber_shader_params_setget_implement_dirty(enum fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t, fbf_blending_type)
uber_shader_params_setget_implement_dirty(enum fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_backing_t, data_store_backing)
uber_shader_params_setget_implement_dirty(enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_backing_t, glyph_data_backing)
uber_shader_params_setget_implement_dirty(bool, use_ubo_for_uniforms)
uber_shader_params_setget_implement_dirty(unsigned int, number_context_textures)

#define uber_shader_params_get_dirty(member)                            \
  int                                                                   \
  fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::      \
  member(void) const                                                    \
  {                                                                     \
    UberShaderParamsPrivate *d;                                         \
    d = static_cast<UberShaderParamsPrivate*>(m_d);                     \
    d->m_recompute_binding_points = true;                               \
    if (d->m_recompute_binding_points)                                  \
      {                                                                 \
        d->recompute_binding_points();                                  \
      }                                                                 \
    return d->m_##member;                                               \
  }

uber_shader_params_get_dirty(colorstop_atlas_binding)
uber_shader_params_get_dirty(image_atlas_color_tiles_nearest_binding)
uber_shader_params_get_dirty(image_atlas_color_tiles_linear_binding)
uber_shader_params_get_dirty(image_atlas_index_tiles_binding)
uber_shader_params_get_dirty(glyph_atlas_store_binding)
uber_shader_params_get_dirty(data_store_buffer_binding)
uber_shader_params_get_dirty(context_texture_binding)
uber_shader_params_get_dirty(coverage_buffer_texture_binding)
uber_shader_params_get_dirty(color_interlock_image_buffer_binding)
uber_shader_params_get_dirty(uniforms_ubo_binding)

//////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSL methods
fastuidraw::glsl::PainterShaderRegistrarGLSL::
PainterShaderRegistrarGLSL(void)
{
  m_d = FASTUIDRAWnew PainterShaderRegistrarGLSLPrivate(this);
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::
~PainterShaderRegistrarGLSL()
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
add_vertex_shader_util(const ShaderSource &src)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->m_vert_shader_utils.add_source(src);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
add_fragment_shader_util(const ShaderSource &src)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->m_frag_shader_utils.add_source(src);
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_coverage_shader_group(PainterShader::Tag tag,
                                   const reference_counted_ptr<PainterItemCoverageShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_custom_brush_shader_group(PainterShader::Tag tag,
                                  const reference_counted_ptr<PainterBrushShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterItemShaderGLSL> h;
  PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWmessaged_assert(shader.dynamic_cast_ptr<PainterItemShaderGLSL>(),
                            "PainterItemShader fails to be dynamic-castable "
                            "to PainterItemShaderGLSL");
  h = shader.static_cast_ptr<PainterItemShaderGLSL>();

  return_value.m_ID = d->m_item_shaders.absorb_item_shader(h);
  return_value.m_group = 0;
  return_value.m_group = compute_item_shader_group(return_value, shader);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag(*this));
  tg.m_ID += shader->sub_shader();
  return compute_item_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_item_coverage_shader(const reference_counted_ptr<PainterItemCoverageShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterItemCoverageShaderGLSL> h;
  PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWmessaged_assert(shader.dynamic_cast_ptr<PainterItemCoverageShaderGLSL>(),
                            "PainterItemCoverageShader fails to be dynamic-castable "
                            "to PainterItemCoverageShaderGLSL");
  h = shader.static_cast_ptr<PainterItemCoverageShaderGLSL>();

  return_value.m_ID = d->m_item_coverage_shaders.absorb_item_shader(h);
  return_value.m_group = 0;
  return_value.m_group = compute_item_coverage_shader_group(return_value, shader);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_coverage_sub_shader_group(const reference_counted_ptr<PainterItemCoverageShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag(*this));
  tg.m_ID += shader->sub_shader();
  return compute_item_coverage_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<PainterBlendShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWmessaged_assert(shader.dynamic_cast_ptr<PainterBlendShaderGLSL>(),
                            "PainterBlendShader fails to be dynamic-castable "
                            "to PainterBlendShaderGLSL");
  h = shader.static_cast_ptr<PainterBlendShaderGLSL>();

  d->m_blend_shaders[h->type()].m_shaders.push_back(h);

  return_value.m_ID = d->m_next_blend_shader_ID;
  return_value.m_group = 0;
  d->m_next_blend_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_blend_shader_group(return_value, h);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag(*this));
  tg.m_ID += shader->sub_shader();
  return compute_blend_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_custom_brush_shader(const reference_counted_ptr<PainterBrushShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<PainterBrushShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWmessaged_assert(shader.dynamic_cast_ptr<PainterBrushShaderGLSL>(),
                            "PainterBrushShader fails to be dynamic-castable "
                            "to PainterBrushShaderGLSL");
  h = shader.static_cast_ptr<PainterBrushShaderGLSL>();

  d->m_custom_brush_shaders.m_shaders.push_back(h);
  d->m_custom_brush_shaders.update_varying_size(h->varyings());

  return_value.m_ID = d->m_next_custom_brush_shader_ID;
  return_value.m_group = 0;
  d->m_next_custom_brush_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_custom_brush_shader_group(return_value, h);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_custom_brush_sub_shader_group(const reference_counted_ptr<PainterBrushShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag(*this));
  tg.m_ID += shader->sub_shader();
  return compute_custom_brush_shader_group(tg, shader);
}

unsigned int
fastuidraw::glsl::PainterShaderRegistrarGLSL::
registered_shader_count(void)
{
  unsigned int return_value;
  PainterShaderRegistrarGLSLPrivate *d;

  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  return_value = d->m_item_shaders.m_shaders.size()
    + d->m_item_coverage_shaders.m_shaders.size()
    + d->m_custom_brush_shaders.m_shaders.size();
  for (unsigned int i = 0; i < PainterBlendShader::number_types; ++i)
    {
      return_value += d->m_blend_shaders[i].m_shaders.size();
    }

  return return_value;
}

unsigned int
fastuidraw::glsl::PainterShaderRegistrarGLSL::
registered_blend_shader_count(enum PainterBlendShader::shader_type tp)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  return d->m_blend_shaders[tp].m_shaders.size();
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
construct_item_uber_shader(enum PainterBlendShader::shader_type tp,
                           const BackendConstants &backend_constants,
                           ShaderSource &out_vertex,
                           ShaderSource &out_fragment,
                           const UberShaderParams &construct_params,
                           const ShaderFilter<PainterItemShaderGLSL> *item_shader_filter,
                           c_string discard_macro_value)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->construct_shader<PainterItemShaderGLSL>(tp, PainterSurface::color_buffer_type,
                                             d->m_item_shaders,
                                             backend_constants, out_vertex, out_fragment,
                                             construct_params, item_shader_filter,
                                             discard_macro_value);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
construct_item_uber_coverage_shader(const BackendConstants &backend_constants,
                                    ShaderSource &out_vertex,
                                    ShaderSource &out_fragment,
                                    const UberShaderParams &construct_params,
                                    const ShaderFilter<PainterItemCoverageShaderGLSL> *item_shader_filter)
{
  PainterShaderRegistrarGLSLPrivate *d;
  enum PainterBlendShader::shader_type v;

  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  /* the PainterBlendShader::shader_type value should be
   * ignored when making coverage shaders, set it to a
   * deliberately bad value.
   */
  v = static_cast<PainterBlendShader::shader_type>(-1);
  d->construct_shader<PainterItemCoverageShaderGLSL>(v, PainterSurface::deferred_coverage_buffer_type,
                                                     d->m_item_coverage_shaders,
                                                     backend_constants, out_vertex, out_fragment,
                                                     construct_params, item_shader_filter,
                                                     "fastuidraw_do_nothing()");
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
construct_item_shader(enum PainterBlendShader::shader_type tp,
                      const BackendConstants &backend_constants,
                      ShaderSource &out_vertex,
                      ShaderSource &out_fragment,
                      const UberShaderParams &construct_params,
                      unsigned int shader_id,
                      c_string discard_macro_value)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->construct_shader<PainterItemShaderGLSL>(tp, PainterSurface::color_buffer_type,
                                             d->m_item_shaders,
                                             backend_constants, out_vertex, out_fragment,
                                             construct_params, shader_id,
                                             discard_macro_value);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
construct_item_coverage_shader(const BackendConstants &backend_constants,
                               ShaderSource &out_vertex,
                               ShaderSource &out_fragment,
                               const UberShaderParams &construct_params,
                               unsigned int shader_id)
{
  PainterShaderRegistrarGLSLPrivate *d;
  enum PainterBlendShader::shader_type v;

  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  /* the PainterBlendShader::shader_type value should be
   * ignored when making coverage shaders, set it to a
   * deliberately bad value.
   */
  v = static_cast<PainterBlendShader::shader_type>(-1);
  d->construct_shader<PainterItemCoverageShaderGLSL>(v, PainterSurface::deferred_coverage_buffer_type,
                                                     d->m_item_coverage_shaders,
                                                     backend_constants, out_vertex, out_fragment,
                                                     construct_params, shader_id,
                                                     "FASTUIDRAW_DISCARD_COLOR_WRITE=true");
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
ubo_size(void)
{
  return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(uniform_ubo_number_entries);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
fill_uniform_buffer(const PainterSurface::Viewport &vwp,
                    c_array<uint32_t> p)
{
  int w, h;
  vec2 wh, recip;

  w = t_max(1, vwp.m_dimensions.x());
  h = t_max(1, vwp.m_dimensions.y());

  wh = vec2(w, h);
  recip = vec2(1.0f, 1.0f) / wh;

  p[uniform_ubo_resolution_x_offset]= pack_float(wh.x());
  p[uniform_ubo_resolution_y_offset]= pack_float(wh.y());
  p[uniform_ubo_recip_resolution_x_offset]= pack_float(recip.x());
  p[uniform_ubo_recip_resolution_y_offset]= pack_float(recip.y());
  p[uniform_ubo_recip_magnitude_offset]= pack_float(recip.magnitude());
}
