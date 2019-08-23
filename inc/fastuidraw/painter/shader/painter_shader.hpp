/*!
 * \file painter_shader.hpp
 * \brief file painter_shader.hpp
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


#ifndef FASTUIDRAW_PAINTER_SHADER_HPP
#define FASTUIDRAW_PAINTER_SHADER_HPP
#include <fastuidraw/util/reference_counted.hpp>

namespace fastuidraw
{
  class PainterShaderRegistrar;

/*!\addtogroup PainterShaders
 * @{
 */

  /*!
   * \brief
   * A PainterShader encapsulates how to draw or blend.
   *
   * The real meat of a PainterShader is dependent
   * on the backend. Typically it is a shader source
   * code -fragment- that is placed into a large uber-shader.
   */
  class PainterShader:
    public reference_counted<PainterShader>::concurrent
  {
  public:
    /*!
     * \brief
     * A Tag is how a PainterShader is described for
     * and by a PainterShaderRegistrar.
     */
    class Tag
    {
    public:
      /*!
       * Ctor, initializes \ref m_ID and \ref m_group
       * to 0.
       */
      Tag(void):
        m_ID(0),
        m_group(0)
      {}

      /*!
       * The ID of a PainterShader is unique.
       * Typically, \ref m_ID is used in a switch
       * statement of an uber-shader.
       */
      uint32_t m_ID;

      /*!
       * The group of a PainterShader is
       * used to classify PainterShader objects
       * into groups for the cases when draw call breaks
       * are needed either to improve performance (to
       * prevent divergent branching in shaders) or to
       * insert API state changes. The value 0 is used
       * to indicate "default" shader group. The nullptr
       * shader belongs to group 0.
       */
      uint32_t m_group;
    };

    /*!
     * Ctor for creating a PainterShader which has multiple
     * sub-shaders. The purpose of sub-shaders is for the
     * case where multiple shaders almost same code and those
     * code differences can be realized by examining a sub-shader
     * ID.
     * \param num_sub_shaders number of sub-shaders
     */
    explicit
    PainterShader(unsigned int num_sub_shaders = 1);

    /*!
     * Ctor to create a PainterShader realized as a sub-shader
     * of an existing PainterShader.
     * \param parent parent PainterShader that has sub-shaders.
     * \param sub_shader which sub-shader of the parent PainterShader
     *               The parent PainterShader MUST already be registered
     *               to a PainterShaderRegistrar.
     */
    PainterShader(reference_counted_ptr<PainterShader> parent,
                  unsigned int sub_shader);

    virtual
    ~PainterShader();

    /*!
     * Returns the number of sub-shaders the PainterShader
     * supports.
     */
    unsigned int
    number_sub_shaders(void) const;

    /*!
     * If the PainterShader is a sub-shader returns the parent
     * shader, otherwise returns nullptr.
     */
    const reference_counted_ptr<PainterShader>&
    parent(void) const;

    /*!
     * Returns the sub-shader value as passed in ctor if
     * a sub-shader, otherwise returns 0.
     */
    uint32_t
    sub_shader(void) const;

    /*!
     * Returns the ID of the shader, the shader
     * must be registered to the passed \ref
     * PainterShaderRegistrar to have an ID
     * value for the passed registrar.
     */
    uint32_t
    ID(const PainterShaderRegistrar&) const;

    /*!
     * Returns the shader group to which the shader belongs.
     * A different value in group() triggers a call to
     * PainterDraw:draw_break() to note that the shader
     * group changed. The shader must be registered to the
     * passed \ref PainterShaderRegistrar to have a value
     * from the registrar.
     */
    uint32_t
    group(const PainterShaderRegistrar&) const;

    /*!
     * Returns the Tag of the shader which holds
     * the value for ID() in Tag::m_ID and group()
     * in Tag::m_group. The shader must be registered
     * to the passed \ref PainterShaderRegistrar to
     * have a Tag value from the registrar.
     */
    Tag
    tag(const PainterShaderRegistrar&) const;

    /*!
     * Returns true if this shader has been registered
     * the the passed \ref PainterShaderRegistrar.
     */
    bool
    registered_to(const PainterShaderRegistrar&) const;

  private:
    friend class PainterShaderRegistrar;

    /*!
     * Called by a PainterShaderRegistrar to register the shader to it.
     */
    void
    register_shader(Tag tg, const PainterShaderRegistrar &p);

    /*!
     * Called by PainterShaderRegistrar to set the group for a shader.
     */
    void
    set_group_of_sub_shader(const PainterShaderRegistrar &p, uint32_t group);

    void *m_d;
  };

/*! @} */
}

#endif
