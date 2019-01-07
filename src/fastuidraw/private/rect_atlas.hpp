/*!
 * \file rect_atlas.hpp
 * \brief file rect_atlas.hpp
 *
 * Adapted from: WRATHAtlasBase.hpp and WRATHAtlas.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#pragma once

#include <list>
#include <map>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

#include "util_private.hpp"


namespace fastuidraw {
namespace detail {

/*!\class RectAtlas
 * Provides an interface to allocate and free rectangle
 * regions from a large rectangle.
 */
class RectAtlas:public fastuidraw::noncopyable
{
private:
  class tree_base;
  class tree_node_with_children;
  class tree_node_without_children;
  typedef std::pair<tree_base*, enum return_code> add_return_value;

public:
  /*!
   * An rectangle gives the location (i.e size and
   * position) of a rectangle within a RectAtlas.
   * The location of a rectangle does not change for the
   * lifetime of the rectangle after returned by
   * add_rectangle().
   */
  class rectangle:public fastuidraw::noncopyable
  {
  public:
    /*!
     * Returns the minX_minY of the rectangle.
     */
    const ivec2&
    minX_minY(void) const
    {
      return m_minX_minY;
    }

    /*!
     * Returns the size of the rectangle.
     */
    const ivec2&
    size(void) const
    {
      return m_size;
    }

    const ivec2&
    unpadded_minX_minY(void) const
    {
      return m_unpadded_minX_minY;
    }

    const ivec2&
    unpadded_size(void) const
    {
      return m_unpadded_size;
    }

    /*!
     * Returns the owning RectAtlas of this
     * rectangle.
     */
    const RectAtlas*
    atlas(void) const
    {
      return m_atlas;
    }

    virtual
    ~rectangle()
    {}

  private:
    friend class RectAtlas;
    friend class tree_base;

    rectangle(RectAtlas *p, const ivec2 &psize):
      m_atlas(p),
      m_minX_minY(0, 0),
      m_size(psize),
      m_tree(nullptr)
    {}

    void
    finalize(int left, int right,
             int top, int bottom)
    {
      m_unpadded_minX_minY = m_minX_minY - ivec2(left, top);
      m_unpadded_size = m_size - ivec2(left + right, top + bottom);
    }

    RectAtlas *m_atlas;
    ivec2 m_minX_minY, m_size;
    ivec2 m_unpadded_minX_minY, m_unpadded_size;
    tree_base *m_tree;
  };

  /*!
   * Ctor
   * \param dimensions dimension of the atlas, this is then the return value to size().
   */
  explicit
  RectAtlas(const ivec2 &dimensions);

  virtual
  ~RectAtlas();

  /*!
   * Returns a pointer to the a newly created rectangle
   * of the requested size. Returns nullptr on failure.
   * The rectangle is owned by this RectAtlas.
   * An implementation may not change the location
   * (or size) of a rectangle once it has been
   * returned by add_rectangle().
   * \param dimension width and height of the rectangle
   */
  const rectangle*
  add_rectangle(const ivec2 &dimension,
                int left_padding, int right_padding,
                int top_padding, int bottom_padding);

  /*!
   * Clears the RectAtlas, in doing so deleting
   * all recranges allocated by \ref add_rectangle().
   * After clear(), all rectangle objects
   * returned by add_rectangle() are deleted, and as
   * such the pointers are then wild-invalid.
   */
  void
  clear(void);

  /*!
   * Returns the size of the \ref RectAtlas,
   * i.e. the value passed as dimensions
   * in RectAtlas().
   */
  ivec2
  size(void) const;

private:
  class tree_sorter
  {
  public:
    bool
    operator()(tree_base *lhs, tree_base *rhs) const;
  };

  static
  void
  move_rectangle(rectangle *rect, const ivec2 &moveby)
  {
    FASTUIDRAWassert(rect);
    rect->m_minX_minY += moveby;
  }

  static
  void
  set_minX_minY(rectangle *rect, const ivec2 &bl)
  {
    FASTUIDRAWassert(rect);
    rect->m_minX_minY = bl;
  }

  tree_base *m_root;
  ivec2 m_rejected_request_size;
  rectangle m_empty_rect;
};

} //namespace detail

} //namespace fastuidraw
