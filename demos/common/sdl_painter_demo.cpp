#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>
#include "sdl_painter_demo.hpp"
#include "text_helper.hpp"

namespace
{
  template<typename T>
  class enum_wrapper
  {
  public:
    explicit
    enum_wrapper(T v):
      m_v(v)
    {}

    T m_v;
  };

  template<typename T>
  enum_wrapper<T>
  make_enum_wrapper(T v)
  {
    return enum_wrapper<T>(v);
  }

  std::ostream&
  operator<<(std::ostream &str, enum_wrapper<bool> b)
  {
    if (b.m_v)
      {
        str << "true";
      }
    else
      {
        str << "false";
      }
    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::gl::PainterEngineGL::data_store_backing_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::gl::PainterEngineGL::data_store_tbo:
        str << "tbo";
        break;

      case fastuidraw::gl::PainterEngineGL::data_store_ubo:
        str << "ubo";
        break;

      case fastuidraw::gl::PainterEngineGL::data_store_ssbo:
        str << "ssbo";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::gl::PainterEngineGL::clipping_type_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::gl::PainterEngineGL::clipping_via_gl_clip_distance:
        str << "on";
        break;

      case fastuidraw::gl::PainterEngineGL::clipping_via_discard:
        str << "off";
        break;

      case fastuidraw::gl::PainterEngineGL::clipping_via_skip_color_write:
        str << "emulate_skip_color_write";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_framebuffer_fetch:
        str << "framebuffer_fetch";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_interlock:
        str << "interlock";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported:
        str << "none";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::PainterBlendShader::shader_type> v)
  {
    switch (v.m_v)
      {
      case fastuidraw::PainterBlendShader::single_src:
        str << "single_src";
        break;

      case fastuidraw::PainterBlendShader::dual_src:
        str << "dual_src";
        break;

      case fastuidraw::PainterBlendShader::framebuffer_fetch:
        str << "framebuffer_fetch";
        break;

      default:
        str << "invalid value";
      }
    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::gl::PainterEngineGL::buffer_streaming_type_t> v)
  {
    switch (v.m_v)
      {
      case fastuidraw::gl::PainterEngineGL::buffer_streaming_use_mapping:
        str << "buffer_streaming_use_mapping";
        break;

      case fastuidraw::gl::PainterEngineGL::buffer_streaming_orphaning:
        str << "buffer_streaming_orphaning";
        break;

      case fastuidraw::gl::PainterEngineGL::buffer_streaming_buffer_subdata:
        str << "buffer_streaming_buffer_subdata";
        break;
      default:
        str << "invalid value";
      }
    return str;
  }

  std::ostream&
  operator<<(std::ostream &ostr, const fastuidraw::PainterShader::Tag &tag)
  {
    ostr << "(ID=" << tag.m_ID << ", group=" << tag.m_group << ")";
    return ostr;
  }

  void
  print_glyph_shader_ids(const fastuidraw::PainterShaderRegistrar &rp,
                         const fastuidraw::PainterGlyphShader &sh)
  {
    for(unsigned int i = 0; i < sh.shader_count(); ++i)
      {
        enum fastuidraw::glyph_type tp;
        tp = static_cast<enum fastuidraw::glyph_type>(i);
        std::cout << "\t\t#" << i << ": " << sh.shader(tp)->tag(rp) << "\n";
      }
  }

  void
  print_stroke_shader_ids(const fastuidraw::PainterShaderRegistrar &rp,
                          const fastuidraw::PainterStrokeShader &shader,
                          const std::string &prefix = "\t\t")
  {
    using namespace fastuidraw;
    vecN<c_string, PainterStrokeShader::number_shader_types> shader_type_labels;

    shader_type_labels[PainterStrokeShader::non_aa_shader] = "non_aa_shader";
    shader_type_labels[PainterStrokeShader::aa_shader] = "aa_shader";

    for (unsigned int tp = 0; tp < PainterEnums::stroking_method_number_precise_choices; ++tp)
      {
        enum PainterEnums::stroking_method_t e_tp;

        e_tp = static_cast<enum PainterEnums::stroking_method_t>(tp);
        for (unsigned int sh = 0; sh < PainterStrokeShader::number_shader_types; ++sh)
          {
            enum PainterStrokeShader::shader_type_t e_sh;

            e_sh = static_cast<enum PainterStrokeShader::shader_type_t>(sh);
            std::cout << prefix << "(" << PainterEnums::label(e_tp)
                      << ", " << shader_type_labels[e_sh] << "): ";

            if (shader.shader(e_tp, e_sh))
              {
                std::cout << shader.shader(e_tp, e_sh)->tag(rp);
              }
            else
              {
                std::cout << "null-shader";
              }
            std::cout << "\n";
          }
      }
  }

  void
  print_dashed_stroke_shader_ids(const fastuidraw::PainterShaderRegistrar &rp,
                                 const fastuidraw::PainterDashedStrokeShaderSet &sh)
  {
    std::cout << "\t\tflat_caps:\n";
    print_stroke_shader_ids(rp, sh.shader(fastuidraw::Painter::flat_caps), "\t\t\t");

    std::cout << "\t\trounded_caps:\n";
    print_stroke_shader_ids(rp, sh.shader(fastuidraw::Painter::rounded_caps), "\t\t\t");

    std::cout << "\t\tsquare_caps:\n";
    print_stroke_shader_ids(rp, sh.shader(fastuidraw::Painter::square_caps), "\t\t\t");
  }

  GLuint
  ready_pixel_counter_ssbo(unsigned int binding_index)
  {
    GLuint return_value(0);
    uint32_t zero[2] = {0, 0};

    fastuidraw_glGenBuffers(1, &return_value);
    fastuidraw_glBindBuffer(GL_SHADER_STORAGE_BUFFER, return_value);
    fastuidraw_glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), zero, GL_STREAM_READ);
    fastuidraw_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    fastuidraw_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, return_value);

    return return_value;
  }

  void
  update_pixel_counts(GLuint bo, fastuidraw::vecN<uint64_t, 4> &dst)
  {
    const uint32_t *p;

    fastuidraw_glBindBuffer(GL_SHADER_STORAGE_BUFFER, bo);
    p = (const uint32_t*)fastuidraw_glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                                     0, 2 * sizeof(uint32_t), GL_MAP_READ_BIT);
    dst[sdl_painter_demo::frame_number_pixels] = p[0];
    dst[sdl_painter_demo::frame_number_pixels_that_neighbor_helper] = p[1];
    fastuidraw_glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    fastuidraw_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    fastuidraw_glDeleteBuffers(1, &bo);

    dst[sdl_painter_demo::total_number_pixels] += dst[sdl_painter_demo::frame_number_pixels];
    dst[sdl_painter_demo::total_number_pixels_that_neighbor_helper] += dst[sdl_painter_demo::frame_number_pixels_that_neighbor_helper];
  }
}

sdl_painter_demo::
sdl_painter_demo(const std::string &about_text,
                 bool default_value_for_print_painter):
  sdl_demo(about_text),

  m_image_atlas_options("Image Atlas Options", *this),
  m_log2_color_tile_size(m_image_atlas_params.log2_color_tile_size(), "log2_color_tile_size",
                         "Specifies the log2 of the width and height of each color tile.",
                         *this),
  m_log2_num_color_tiles_per_row_per_col(m_image_atlas_params.log2_num_color_tiles_per_row_per_col(),
                                         "log2_num_color_tiles_per_row_per_col",
                                         "Specifies the log2 of the number of color tiles "
                                         "in each row and column of each layer. Note that "
                                         "then the total number of color tiles available "
                                         "is given as num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)",
                                         *this),
  m_num_color_layers(m_image_atlas_params.num_color_layers(), "num_color_layers",
                     "Specifies the number of layers in the color texture. Note that "
                     "then the total number of color tiles available is given as "
                     "num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)"
                     "The number of layers grows to accomodate more images at the cost "
                     "of needing to move color data to new GL textures",
                     *this),
  m_log2_index_tile_size(m_image_atlas_params.log2_index_tile_size(), "log2_index_tile_size",
                         "Specifies the log2 of the width and height of each index tile. "
                         "A negative value disables image atlasing",
                         *this),
  m_log2_num_index_tiles_per_row_per_col(m_image_atlas_params.log2_num_index_tiles_per_row_per_col(),
                                         "log2_num_index_tiles_per_row_per_col",
                                         "Specifies the log2 of the number of index tiles "
                                         "in each row and column of each layer; note that "
                                         "then the total number of index tiles available "
                                         "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col)",
                                         *this),
  m_num_index_layers(m_image_atlas_params.num_index_layers(), "num_index_layers",
                     "Specifies the intial number of layers in the index texture; "
                     "note that then the total number of index tiles initially available "
                     "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col) "
                     "The number of layers grows to accomodate more images at the cost "
                     "of needing to move index data to new GL textures",
                     *this),
  m_support_image_on_atlas(m_image_atlas_params.support_image_on_atlas(),
                           "enabled_image_atlas",
                           "Specifies if image atlasing is enabled. When atlasing is disabled, "
                           "then a draw-call break is made on each different image used unless "
                           "bindless texturing is supported",
                           *this),

  m_glyph_atlas_options("Glyph Atlas options", *this),
  m_glyph_atlas_size(m_glyph_atlas_params.number_floats(),
                     "glyph_atlas_size", "size of glyph store in floats", *this),
  m_glyph_backing_store_type(glyph_backing_store_auto,
                             enumerated_string_type<enum glyph_backing_store_t>()
                             .add_entry("texture_buffer",
                                        glyph_backing_store_texture_buffer,
                                        "use a texture buffer, feature is core in GL but for GLES requires version 3.2, "
                                        "for GLES version pre-3.2, requires the extension GL_OES_texture_buffer or the "
                                        "extension GL_EXT_texture_buffer")
                             .add_entry("texture_array",
                                        glyph_backing_store_texture_array,
                                        "use a 2D texture array to store the glyph data, "
                                        "GL and GLES have feature in core")
                             .add_entry("storage_buffer",
                                        glyph_backing_store_ssbo,
                                        "use a shader storage buffer, feature is core starting in GLES 3.1 and available "
                                        "in GL starting at version 4.2 or via the extension GL_ARB_shader_storage_buffer")
                             .add_entry("auto",
                                        glyph_backing_store_auto,
                                        "query context and decide optimal value"),
                             "geometry_backing_store_type",
                             "Determines how the glyph store is backed.",
                             *this),
  m_glyph_backing_texture_log2_w(10, "glyph_backing_texture_log2_w",
                                 "If glyph_backing_store_type is set to texture_array, then "
                                 "this gives the log2 of the width of the texture array", *this),
  m_glyph_backing_texture_log2_h(10, "glyph_backing_texture_log2_h",
                                 "If glyph_backing_store_type is set to texture_array, then "
                                 "this gives the log2 of the height of the texture array", *this),

  m_colorstop_atlas_options("ColorStop Atlas options", *this),
  m_color_stop_atlas_width(m_colorstop_atlas_params.width(),
                           "colorstop_atlas_width",
                           "width for color stop atlas", *this),
  m_color_stop_atlas_layers(m_colorstop_atlas_params.num_layers(),
                            "colorstop_atlas_layers",
                            "number of layers for the color stop atlas",
                            *this),

  m_painter_options("PainterBackendGL Options", *this),
  m_painter_attributes_per_buffer(m_painter_params.attributes_per_buffer(),
                                  "painter_verts_per_buffer",
                                  "Number of vertices a single API draw can hold",
                                  *this),
  m_painter_indices_per_buffer(m_painter_params.indices_per_buffer(),
                               "painter_indices_per_buffer",
                               "Number of indices a single API draw can hold",
                               *this),
  m_painter_number_pools(m_painter_params.number_pools(), "painter_number_pools",
                         "Number of GL object pools used by the painter", *this),
  m_painter_break_on_shader_change(m_painter_params.break_on_shader_change(),
                                   "painter_break_on_shader_change",
                                   "If true, different shadings are placed into different "
                                   "entries of a call to glMultiDrawElements", *this),
  m_uber_vert_use_switch(m_painter_params.vert_shader_use_switch(),
                         "painter_uber_vert_use_switch",
                         "If true, use a switch statement in uber vertex shader dispatch",
                         *this),
  m_uber_frag_use_switch(m_painter_params.frag_shader_use_switch(),
                         "painter_uber_frag_use_switch",
                         "If true, use a switch statement in uber fragment shader dispatch",
                         *this),
  m_use_uber_item_shader(m_painter_params.use_uber_item_shader(),
                         "painter_use_uber_item_shader",
                         "If true, use an uber-shader for all item shaders",
                         *this),
  m_uber_blend_use_switch(m_painter_params.blend_shader_use_switch(),
                          "painter_uber_blend_use_switch",
                          "If true, use a switch statement in uber blend shader dispatch",
                          *this),
  m_separate_program_for_discard(m_painter_params.separate_program_for_discard(),
                                 "separate_program_for_discard",
                                 "if true, there are two GLSL programs active when drawing: "
                                 "one for those item shaders that have discard and one for "
                                 "those that do not",
                                 *this),
  m_allow_bindless_texture_from_surface(m_painter_params.allow_bindless_texture_from_surface(),
                                        "allow_bindless_texture_from_surface",
                                        "if both this is true and the GL/GLES driver supports "
                                        "bindless texturing, the the textures of the surfaces "
                                        "rendered to will be textured with bindless texturing",
                                        *this),
  m_buffer_streaming_type(m_painter_params.buffer_streaming_type(),
			  enumerated_string_type<enum fastuidraw::gl::PainterEngineGL::buffer_streaming_type_t>()
			  .add_entry("buffer_streaming_use_mapping",
				     fastuidraw::gl::PainterEngineGL::buffer_streaming_use_mapping,
				     "Use glMapBufferRange and glFlushMappedBufferRange recycling BO's across frames")
			  .add_entry("buffer_streaming_orphaning",
				     fastuidraw::gl::PainterEngineGL::buffer_streaming_orphaning,
				     "Call glBufferData each frame to orphan the previous buffer contents but reuse BO names across frames")
			  .add_entry("buffer_streaming_buffer_subdata",
				     fastuidraw::gl::PainterEngineGL::buffer_streaming_buffer_subdata,
				     "Call glBufferSubData thus reusing BO's across frames"),
			  "painter_buffer_streaming",
			  "",
			  *this),
  m_painter_options_affected_by_context("PainterBackendGL Options that can be overridden "
                                        "by version and extension supported by GL/GLES context",
                                        *this),
  m_use_hw_clip_planes(m_painter_params.clipping_type(),
                       enumerated_string_type<clipping_type_t>()
                       .add_entry("on", fastuidraw::gl::PainterEngineGL::clipping_via_gl_clip_distance,
                                  "Use HW clip planes via gl_ClipDistance for clipping")
                       .add_entry_alias("true", fastuidraw::gl::PainterEngineGL::clipping_via_gl_clip_distance)
                       .add_entry("off", fastuidraw::gl::PainterEngineGL::clipping_via_discard,
                                  "Use discard in fragment shader for clipping")
                       .add_entry_alias("false", fastuidraw::gl::PainterEngineGL::clipping_via_discard)
                       .add_entry("emulate_skip_color_write",
                                  fastuidraw::gl::PainterEngineGL::clipping_via_skip_color_write,
                                  "Emulate by (virtually) skipping color writes, painter_blend_type "
                                  "must be framebuffer_fetch"),
                       "painter_use_hw_clip_planes",
                       "",
                       *this),
  m_painter_data_blocks_per_buffer(m_painter_params.data_blocks_per_store_buffer(),
                                   "painter_blocks_per_buffer",
                                   "Number of data blocks a single API draw can hold",
                                   *this),
  m_data_store_backing(m_painter_params.data_store_backing(),
                       enumerated_string_type<data_store_backing_t>()
                       .add_entry("tbo",
                                  fastuidraw::gl::PainterEngineGL::data_store_tbo,
                                  "use a texture buffer (if available) to back the data store. "
                                  "A texture buffer can have a very large maximum size")
                       .add_entry("ubo",
                                  fastuidraw::gl::PainterEngineGL::data_store_ubo,
                                  "use a uniform buffer object to back the data store. "
                                  "A uniform buffer object's maximum size is much smaller than that "
                                  "of a texture buffer object usually")
                       .add_entry("ssbo",
                                  fastuidraw::gl::PainterEngineGL::data_store_ssbo,
                                  "use a shader storage buffer object to back the data store. "
                                  "A shader storage buffer can have a very large maximum size"),
                       "painter_data_store_backing_type",
                       "specifies how the data store buffer is backed",
                       *this),
  m_assign_layout_to_vertex_shader_inputs(m_painter_params.assign_layout_to_vertex_shader_inputs(),
                                          "painter_assign_layout_to_vertex_shader_inputs",
                                          "If true, use layout(location=) in GLSL shader for vertex shader inputs",
                                          *this),
  m_assign_layout_to_varyings(m_painter_params.assign_layout_to_varyings(),
                              "painter_assign_layout_to_varyings",
                              "If true, use layout(location=) in GLSL shader for varyings", *this),
  m_assign_binding_points(m_painter_params.assign_binding_points(),
                          "painter_assign_binding_points",
                          "If true, use layout(binding=) in GLSL shader on samplers and buffers", *this),
  m_support_dual_src_blend_shaders(m_painter_params.support_dual_src_blend_shaders(),
                                   "painter_support_dual_src_blending",
                                   "If true allow the painter to support dual src blend shaders", *this),
  m_preferred_blend_type(m_painter_params.preferred_blend_type(),
                         enumerated_string_type<shader_blend_type>()
                         .add_entry("single_src",
                                    fastuidraw::PainterBlendShader::single_src,
                                    "Use single-source blending")
                         .add_entry("dual_src",
                                    fastuidraw::PainterBlendShader::dual_src,
                                    "Use dual-source blending")
                         .add_entry("framebuffer_fetch",
                                    fastuidraw::PainterBlendShader::framebuffer_fetch,
                                    "Use framebuffer-fetch or interlock, depending on the value of painter_fbf_blending_type"),
                         "painter_preferred_blend_type",
                         "Specifies how to implement all blend shader mode for all those except those "
                         "that cannot be performed with 3D API blending",
                         *this),
  m_fbf_blending_type(m_painter_params.fbf_blending_type(),
                      enumerated_string_type<fbf_blending_type_t>()
                      .add_entry("framebuffer_fetch",
                                 fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_framebuffer_fetch,
                                 "use a framebuffer fetch (if available) to perform blending, "
                                 "that cannot be performed with 3D API blending")
                      .add_entry("interlock",
                                 fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_interlock,
                                 "use image-load store together with interlock (if both available) "
                                 "to perform blending that cannot be performed with 3D API blending")
                      .add_entry("none",
                                 fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported,
                                 "Do not support the blend shaders that cannot be performed with 3D API blending"),
                      "painter_fbf_blending_type",
                      "specifies if/how the painter will perform blending for those blend shaders "
                      "that cannot be performed with 3D API blending",
                      *this),
  m_painter_optimal(painter_optimal_rendering,
                    enumerated_string_type<enum painter_optimal_t>()
                    .add_entry("painter_no_optimal",
                               painter_no_optimal,
                               "Do not query GL/GLES context to configure options and rely "
                               "on the values passed to the command line. Values not possible "
                               "to do by the GL/GLES context will be overriden")
                    .add_entry("painter_optimal_performance",
                               painter_optimal_performance,
                               "Query the GL/GLES context to configure options for optimal "
                               "performance. Additional options set by command line will "
                               "override the values")
                    .add_entry("painter_optimal_rendering",
                               painter_optimal_rendering,
                               "Query the GL/GLES context to configure options for optimal "
                               "rendering quality. Additional options set by command line will "
                               "override the values"),
                    "painter_optimal_auto",
                    "Decide how to initially configure the Painter",
                    *this),
  m_demo_options("Demo Options", *this),
  m_print_painter_config(default_value_for_print_painter,
                         "print_painter_config",
                         "Print PainterBackendGL config", *this),
  m_print_painter_shader_ids(default_value_for_print_painter,
                             "print_painter_shader_ids",
                             "Print PainterBackendGL shader IDs", *this),
  m_pixel_counter_stack(-1, "pixel_counter_latency",
                        "If non-negative, will add code to the painter ubder- shader "
                        "to count number of helper and non-helper pixels. The value "
                        "is how many frames to wait before reading the values from the "
                        "atomic buffers that are updated", *this),
  m_distance_field_pixel_size(fastuidraw::GlyphGenerateParams::distance_field_pixel_size(),
                              "glyph_distance_field_pixel_size",
                              "Pixel size at which to generate distance field glyphs",
                              *this),
  m_distance_field_max_distance(fastuidraw::GlyphGenerateParams::distance_field_max_distance(),
                                "glyph_distance_field_max_distance",
                                "Max distance value in pixels to use when generating "
                                "distance field glyphs; the texels of a distance field "
                                "glyph are always stored in fixed point 8-bits normalized "
                                "to [0,1]. This field gives the clamping and conversion "
                                "to [0,1]", *this),
  m_restricted_rays_max_recursion(fastuidraw::GlyphGenerateParams::restricted_rays_max_recursion(),
                                  "glyph_restricted_rays_max_recursion",
                                  "Maximum level of recursion used when creating restricted rays glyphs",
                                  *this),
  m_restricted_rays_split_thresh(fastuidraw::GlyphGenerateParams::restricted_rays_split_thresh(),
                                 "glyph_restricted_rays_split_thresh",
                                 "Splitting threshhold used when creating restricted rays glyphs",
                                 *this),
  m_restricted_rays_expected_min_render_size(fastuidraw::GlyphGenerateParams::restricted_rays_minimum_render_size(),
                                             "glyph_restricted_rays_expected_min_render_size",
                                             "",
                                             *this),
  m_banded_rays_max_recursion(fastuidraw::GlyphGenerateParams::banded_rays_max_recursion(),
                              "glyph_banded_rays_max_recursion",
                              "Maximum level of recursion to use when generating banded-ray glyphs",
                              *this),
  m_banded_rays_average_number_curves_thresh(fastuidraw::GlyphGenerateParams::banded_rays_average_number_curves_thresh(),
                                             "glyph_banded_rays_average_number_curves_thresh",
                                             "Threshhold to aim for number of curves per band when generating "
                                             "banded-ray glyphs",
                                             *this),
  m_num_pixel_counter_buffers(0),
  m_pixel_counts(0, 0, 0, 0),
  m_painter_stats(fastuidraw::Painter::number_stats(), 0)
{}

sdl_painter_demo::
~sdl_painter_demo()
{
  for (GLuint bo : m_pixel_counter_buffers)
    {
      fastuidraw_glDeleteBuffers(1, &bo);
    }
}

void
sdl_painter_demo::
init_gl(int w, int h)
{
  int max_layers(0);

  max_layers = fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS);
  if (max_layers < m_num_color_layers.value())
    {
      std::cout << "num_color_layers exceeds max number texture layers (" << max_layers
                << "), num_color_layers set to that value.\n";
      m_num_color_layers.value() = max_layers;
    }

  if (max_layers < m_color_stop_atlas_layers.value())
    {
      std::cout << "atlas_layers exceeds max number texture layers (" << max_layers
                << "), atlas_layers set to that value.\n";
      m_color_stop_atlas_layers.value() = max_layers;
    }

  if (m_painter_optimal.value() != painter_no_optimal)
    {
      m_painter_params.configure_from_context(m_painter_optimal.value() == painter_optimal_rendering);
    }

#define APPLY_PARAM(X, Y) do {                            \
    if (Y.set_by_command_line())                          \
      {                                                   \
        std::cout << "Apply: "#X": " << Y.value()         \
                  << "\n"; m_painter_params.X(Y.value()); \
      }                                                   \
  } while (0)

  APPLY_PARAM(attributes_per_buffer, m_painter_attributes_per_buffer);
  APPLY_PARAM(indices_per_buffer, m_painter_indices_per_buffer);
  APPLY_PARAM(data_blocks_per_store_buffer, m_painter_data_blocks_per_buffer);
  APPLY_PARAM(number_pools, m_painter_number_pools);
  APPLY_PARAM(break_on_shader_change, m_painter_break_on_shader_change);
  APPLY_PARAM(clipping_type, m_use_hw_clip_planes);
  APPLY_PARAM(buffer_streaming_type, m_buffer_streaming_type);
  APPLY_PARAM(vert_shader_use_switch, m_uber_vert_use_switch);
  APPLY_PARAM(frag_shader_use_switch, m_uber_frag_use_switch);
  APPLY_PARAM(blend_shader_use_switch, m_uber_blend_use_switch);
  APPLY_PARAM(data_store_backing, m_data_store_backing);
  APPLY_PARAM(assign_layout_to_vertex_shader_inputs, m_assign_layout_to_vertex_shader_inputs);
  APPLY_PARAM(assign_layout_to_varyings, m_assign_layout_to_varyings);
  APPLY_PARAM(assign_binding_points, m_assign_binding_points);
  APPLY_PARAM(separate_program_for_discard, m_separate_program_for_discard);
  APPLY_PARAM(allow_bindless_texture_from_surface, m_allow_bindless_texture_from_surface);
  APPLY_PARAM(preferred_blend_type, m_preferred_blend_type);
  APPLY_PARAM(fbf_blending_type, m_fbf_blending_type);
  APPLY_PARAM(support_dual_src_blend_shaders, m_support_dual_src_blend_shaders);
  APPLY_PARAM(use_uber_item_shader, m_use_uber_item_shader);

#undef APPLY_PARAM

#define APPLY_IMAGE_PARAM(X, Y) do {                      \
    if (Y.set_by_command_line())                          \
      {                                                   \
        std::cout << "Apply: "#X": " << Y.value()         \
                  << "\n"; m_image_atlas_params.X(Y.value()); \
      }                                                   \
  } while (0)

  m_image_atlas_params = m_painter_params.image_atlas_params();
  APPLY_IMAGE_PARAM(log2_color_tile_size, m_log2_color_tile_size);
  APPLY_IMAGE_PARAM(log2_num_color_tiles_per_row_per_col, m_log2_num_color_tiles_per_row_per_col);
  APPLY_IMAGE_PARAM(num_color_layers, m_num_color_layers);
  APPLY_IMAGE_PARAM(log2_index_tile_size, m_log2_index_tile_size);
  APPLY_IMAGE_PARAM(log2_num_index_tiles_per_row_per_col, m_log2_num_index_tiles_per_row_per_col);
  APPLY_IMAGE_PARAM(num_index_layers, m_num_index_layers);
  APPLY_IMAGE_PARAM(support_image_on_atlas, m_support_image_on_atlas);

#undef APPLY_IMAGE_PARAM

  m_glyph_atlas_params = m_painter_params.glyph_atlas_params();
  m_glyph_atlas_params.number_floats(m_glyph_atlas_size.value());
  switch(m_glyph_backing_store_type.value())
    {
    case glyph_backing_store_texture_buffer:
      m_glyph_atlas_params.use_texture_buffer_store();
      break;

    case glyph_backing_store_texture_array:
      m_glyph_atlas_params.use_texture_2d_array_store(m_glyph_backing_texture_log2_w.value(),
                                                      m_glyph_backing_texture_log2_h.value());
      break;

    case glyph_backing_store_ssbo:
      m_glyph_atlas_params.use_storage_buffer_store();
      break;

    default:
      m_glyph_atlas_params.use_optimal_store_backing();
      switch(m_glyph_atlas_params.glyph_data_backing_store_type())
        {
        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_tbo:
          {
            std::cout << "Glyph Store: auto selected texture buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_ssbo:
          {
            std::cout << "Glyph Store: auto selected storage buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_data_texture_array:
          {
            fastuidraw::ivec2 log2_dims(m_glyph_atlas_params.texture_2d_array_store_log2_dims());
            std::cout << "Glyph Store: auto selected texture with dimensions: (2^"
                      << log2_dims.x() << ", 2^" << log2_dims.y() << ") = "
                      << fastuidraw::ivec2(1 << log2_dims.x(), 1 << log2_dims.y())
                      << "\n";
          }
          break;
        }
    }

  m_colorstop_atlas_params = m_painter_params.colorstop_atlas_params();
  m_colorstop_atlas_params
    .width(m_color_stop_atlas_width.value())
    .num_layers(m_color_stop_atlas_layers.value());

  if (!m_color_stop_atlas_width.set_by_command_line())
    {
      m_colorstop_atlas_params.optimal_width();
      std::cout << "Colorstop Atlas optimal width selected to be "
                << m_colorstop_atlas_params.width() << "\n";
    }

  m_painter_params
    .image_atlas_params(m_image_atlas_params)
    .glyph_atlas_params(m_glyph_atlas_params)
    .colorstop_atlas_params(m_colorstop_atlas_params);

  if (m_pixel_counter_stack.value() >= 0)
    {
      fastuidraw::c_string version;
      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          version = "310 es";
        }
      #else
        {
          version = "450";
        }
      #endif
      m_painter_params.glsl_version_override(version);
    }

  m_backend = fastuidraw::gl::PainterEngineGL::create(m_painter_params);

  fastuidraw::GlyphGenerateParams::distance_field_max_distance(m_distance_field_max_distance.value());
  fastuidraw::GlyphGenerateParams::distance_field_pixel_size(m_distance_field_pixel_size.value());
  fastuidraw::GlyphGenerateParams::restricted_rays_max_recursion(m_restricted_rays_max_recursion.value());
  fastuidraw::GlyphGenerateParams::restricted_rays_split_thresh(m_restricted_rays_split_thresh.value());
  fastuidraw::GlyphGenerateParams::restricted_rays_minimum_render_size(m_restricted_rays_expected_min_render_size.value());
  fastuidraw::GlyphGenerateParams::banded_rays_max_recursion(m_banded_rays_max_recursion.value());
  fastuidraw::GlyphGenerateParams::banded_rays_average_number_curves_thresh(m_banded_rays_average_number_curves_thresh.value());

  m_painter = FASTUIDRAWnew fastuidraw::Painter(m_backend);
  m_font_database = FASTUIDRAWnew fastuidraw::FontDatabase();
  m_ft_lib = FASTUIDRAWnew fastuidraw::FreeTypeLib();

  if (m_pixel_counter_stack.value() >= 0)
    {
      fastuidraw::c_string code;
      fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterShaderRegistrarGLSL> R;

      m_pixel_counter_buffer_binding_index = fastuidraw::gl::context_get<GLint>(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS) - 1;
      code =
        "layout(binding = PIXEL_COUNTER_BINDING) buffer pixel_counter_buffer\n"
        "{\n"
        "\tuint num_pixels;\n"
        "\tuint num_neighbor_helper_pixels;\n"
        "};\n"
        "void real_main(void);\n"
        "void main(void)\n"
        "{\n"
        "\tfloat f;\n"
        "\tf = float(gl_HelperInvocation);\n"
        "\tatomicAdd(num_pixels, 1u);\n"
        "\tif(abs(dFdxFine(f)) > 0.0 || abs(dFdyFine(f)) > 0.0)\n"
        "\t\tatomicAdd(num_neighbor_helper_pixels, 1u);\n"
        "\treal_main();\n"
        "}\n";

      R = static_cast<fastuidraw::glsl::PainterShaderRegistrarGLSL*>(&m_painter->painter_shader_registrar());
      R->add_fragment_shader_util(fastuidraw::glsl::ShaderSource()
                                  .add_macro("PIXEL_COUNTER_BINDING", m_pixel_counter_buffer_binding_index)
                                  .add_source(code, fastuidraw::glsl::ShaderSource::from_string)
                                  .add_macro("main", "real_main"));
    }

  if (m_print_painter_config.value())
    {
      std::cout << "\nPainterBackendGL configuration:\n";

      #define LAZY_PARAM(X, Y) do {                                     \
        std::cout << std::setw(40) << Y.name() << ": " << std::setw(8)  \
                  << m_backend->configuration_gl().X()                  \
                  << "  (requested " << m_painter_params.X() << ")\n";  \
      } while(0)

      #define LAZY_IMAGE_PARAM(X, Y) do {                               \
        std::cout << std::setw(40) << Y.name() << ": " << std::setw(8)  \
                  << m_backend->configuration_gl().image_atlas_params().X() \
                  << "  (requested " << m_painter_params.image_atlas_params().X() << ")\n";  \
      } while(0)

      #define LAZY_PARAM_ENUM(X, Y) do {                                \
        std::cout << std::setw(40) << Y.name() <<": " << std::setw(8)   \
                  << make_enum_wrapper(m_backend->configuration_gl().X()) \
                  << "  (requested " << make_enum_wrapper(m_painter_params.X()) \
                  << ")\n";                                             \
      } while(0)

      #define LAZY_IMAGE_PARAM_ENUM(X, Y) do {                          \
        std::cout << std::setw(40) << Y.name() <<": " << std::setw(8)   \
                  << make_enum_wrapper(m_backend->configuration_gl().image_atlas_params().X()) \
                  << "  (requested " << make_enum_wrapper(m_painter_params.image_atlas_params().X()) \
                  << ")\n";                                             \
      } while(0)

      LAZY_PARAM(attributes_per_buffer, m_painter_attributes_per_buffer);
      LAZY_PARAM(indices_per_buffer, m_painter_indices_per_buffer);
      LAZY_PARAM(data_blocks_per_store_buffer, m_painter_data_blocks_per_buffer);
      LAZY_PARAM(number_pools, m_painter_number_pools);
      LAZY_PARAM_ENUM(buffer_streaming_type, m_buffer_streaming_type);
      LAZY_PARAM_ENUM(break_on_shader_change, m_painter_break_on_shader_change);
      LAZY_PARAM_ENUM(clipping_type, m_use_hw_clip_planes);
      LAZY_PARAM_ENUM(vert_shader_use_switch, m_uber_vert_use_switch);
      LAZY_PARAM_ENUM(frag_shader_use_switch, m_uber_frag_use_switch);
      LAZY_PARAM(use_uber_item_shader, m_use_uber_item_shader);
      LAZY_PARAM_ENUM(blend_shader_use_switch, m_uber_blend_use_switch);
      LAZY_PARAM_ENUM(data_store_backing, m_data_store_backing);
      LAZY_PARAM_ENUM(assign_layout_to_vertex_shader_inputs, m_assign_layout_to_vertex_shader_inputs);
      LAZY_PARAM_ENUM(assign_layout_to_varyings, m_assign_layout_to_varyings);
      LAZY_PARAM_ENUM(assign_binding_points, m_assign_binding_points);
      LAZY_PARAM_ENUM(separate_program_for_discard, m_separate_program_for_discard);
      LAZY_PARAM_ENUM(allow_bindless_texture_from_surface, m_allow_bindless_texture_from_surface);
      LAZY_PARAM_ENUM(preferred_blend_type, m_preferred_blend_type);
      LAZY_PARAM_ENUM(fbf_blending_type, m_fbf_blending_type);
      LAZY_PARAM_ENUM(support_dual_src_blend_shaders, m_support_dual_src_blend_shaders);
      std::cout << std::setw(40) << "geometry_backing_store_type:"
                << std::setw(8) << m_painter_params.glyph_atlas_params().glyph_data_backing_store_type()
                << "\n";
      LAZY_IMAGE_PARAM_ENUM(support_image_on_atlas, m_support_image_on_atlas);
      if (m_backend->configuration_gl().image_atlas_params().support_image_on_atlas())
        {
          LAZY_IMAGE_PARAM(log2_color_tile_size, m_log2_color_tile_size);
          LAZY_IMAGE_PARAM(log2_num_color_tiles_per_row_per_col, m_log2_num_color_tiles_per_row_per_col);
          LAZY_IMAGE_PARAM(num_color_layers, m_num_color_layers);
          LAZY_IMAGE_PARAM(log2_index_tile_size, m_log2_index_tile_size);
          LAZY_IMAGE_PARAM(log2_num_index_tiles_per_row_per_col, m_log2_num_index_tiles_per_row_per_col);
          LAZY_IMAGE_PARAM(num_index_layers, m_num_index_layers);
        }

      #undef LAZY_PARAM
      #undef LAZY_IMAGE_PARAM
      #undef LAZY_ENUM_PARAM
    }

  if (m_print_painter_shader_ids.value())
    {
      const fastuidraw::PainterShaderSet &sh(m_painter->default_shaders());
      const fastuidraw::PainterShaderRegistrar &rp(m_backend->painter_shader_registrar());
      std::cout << "Default shader IDs:\n";

      std::cout << "\tGlyph Shaders:\n";
      print_glyph_shader_ids(rp, sh.glyph_shader());

      std::cout << "\tSolid StrokeShaders:\n";
      print_stroke_shader_ids(rp, sh.stroke_shader());

      std::cout << "\tDashed Stroke Shader:\n";
      print_dashed_stroke_shader_ids(rp, sh.dashed_stroke_shader());

      std::cout << "\tFill Shader:"
                << sh.fill_shader().item_shader()->tag(rp) << "\n";
    }

  m_painter_params = m_backend->configuration_gl();
  on_resize(w, h);
  derived_init(w, h);
}

void
sdl_painter_demo::
on_resize(int w, int h)
{
  fastuidraw::ivec2 wh(w, h);
  if (!m_surface || wh != m_surface->dimensions())
    {
      fastuidraw::PainterSurface::Viewport vwp(0, 0, w, h);

      m_surface = FASTUIDRAWnew fastuidraw::gl::PainterSurfaceGL(fastuidraw::ivec2(w, h), *m_backend);
      m_surface->viewport(vwp);
    }
}

void
sdl_painter_demo::
draw_text(const std::string &text, float pixel_size,
          const fastuidraw::FontBase *font,
          fastuidraw::GlyphRenderer renderer,
          const fastuidraw::PainterData &draw,
          enum fastuidraw::Painter::screen_orientation orientation)
{
  std::istringstream str(text);
  fastuidraw::GlyphRun run(pixel_size, orientation, m_painter->glyph_cache());

  create_formatted_text(run, orientation, str, font, m_font_database);
  m_painter->draw_glyphs(draw, run, 0, run.number_glyphs(), renderer);
}

void
sdl_painter_demo::
pre_draw_frame(void)
{
  if (m_pixel_counter_stack.value() >= 0)
    {
      GLuint bo;

      bo = ready_pixel_counter_ssbo(m_pixel_counter_buffer_binding_index);
      m_pixel_counter_buffers.push_back(bo);
      ++m_num_pixel_counter_buffers;
    }
}

void
sdl_painter_demo::
post_draw_frame(void)
{
  if (m_pixel_counter_stack.value() >= 0
      && m_num_pixel_counter_buffers > m_pixel_counter_stack.value())
    {
      GLuint bo;

      bo = m_pixel_counter_buffers.front();
      update_pixel_counts(bo, m_pixel_counts);

      m_pixel_counter_buffers.pop_front();
      --m_num_pixel_counter_buffers;
    }
  m_painter->query_stats(cast_c_array(m_painter_stats));
}
