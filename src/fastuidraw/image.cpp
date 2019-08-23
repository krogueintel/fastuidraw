/*!
 * \file image.cpp
 * \brief file image.cpp
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


#include <list>
#include <map>
#include <mutex>
#include <fastuidraw/image.hpp>
#include <fastuidraw/image_atlas.hpp>
#include <private/array3d.hpp>
#include <private/util_private.hpp>

namespace
{
  /*
   * Copies from src the rectangle:
   *  [source_x, source_x + dest_dim) x [source_y, source_y + dest_dim)
   * to dest.
   *
   * If source_x is negative, then pads each line with the value
   * from src at x=0.
   *
   * If source_y is negative, the source for those lines is
   * taken from src at y=0.
   *
   * For pixel (x,y) with x < 0, takes the value
   * from source at (0, y).
   *
   * For pixel (x,y) with y < 0, takes the value
   * from source at (x, 0).
   *
   * For pixels (x,y) with x >= src_dims.x(), takes the value
   * from source at (src_dims.x() - 1, y).
   *
   * For pixels (x,y) with y >= src_dims.y(), takes the value
   * from source at (x, src_dims.y() - 1).
   *
   * \param dest_dim width and height of destination
   * \param src_dims width and height of source
   * \param source_x, source_y location within src from which to copy
   * \param src: src pixels
   * \param dest: destination pixels
   *
   * If all texel are the same value, returns true.
   */
  template<typename T, typename S>
  void
  copy_sub_data(fastuidraw::c_array<T> dest,
                int w, int h,
                fastuidraw::c_array<const S> src,
                int source_x, int source_y,
                fastuidraw::ivec2 src_dims)
  {
    using namespace fastuidraw;

    FASTUIDRAWassert(w > 0);
    FASTUIDRAWassert(h > 0);
    FASTUIDRAWassert(src_dims.x() > 0);
    FASTUIDRAWassert(src_dims.y() > 0);

    for(int src_y = source_y, dst_y = 0; dst_y < h; ++src_y, ++dst_y)
      {
        c_array<const S> line_src;
        c_array<T> line_dest;
        int dst_x, src_x, src_start;

        line_dest = dest.sub_array(dst_y * w, w);
        if (src_y < 0)
          {
            src_start = 0;
          }
        else if (src_y >= src_dims.y())
          {
            src_start = (src_dims.y() - 1) * src_dims.x();
          }
        else
          {
            src_start = src_y * src_dims.x();
          }
        line_src = src.sub_array(src_start, src_dims.x());

        for(src_x = source_x, dst_x = 0; src_x < 0; ++src_x, ++dst_x)
          {
            line_dest[dst_x] = line_src[0];
          }

        for(src_x = t_max(0, source_x);
            src_x < src_dims.x() && dst_x < w;
            ++src_x, ++dst_x)
          {
            line_dest[dst_x] = line_src[src_x];
          }

        for(;dst_x < w; ++dst_x)
          {
            line_dest[dst_x] = line_src[src_dims.x() - 1];
          }
      }
  }

  fastuidraw::ivec2
  divide_up(fastuidraw::ivec2 numerator, int denominator)
  {
    fastuidraw::ivec2 R;
    R = numerator / denominator;
    if (numerator.x() % denominator != 0)
      {
        ++R.x();
      }
    if (numerator.y() % denominator != 0)
      {
        ++R.y();
      }
    return R;
  }

  int
  number_index_tiles_needed(fastuidraw::ivec2 number_color_tiles,
                            int index_tile_size)
  {
    int return_value = 1;
    fastuidraw::ivec2 tile_count = divide_up(number_color_tiles, index_tile_size);

    while(tile_count.x() > 1 || tile_count.y() > 1)
      {
        return_value += tile_count.x() * tile_count.y();
        tile_count = divide_up(tile_count, index_tile_size);
      }

    return return_value;
  }

  class BackingStorePrivate
  {
  public:
    BackingStorePrivate(fastuidraw::ivec3 whl):
      m_dimensions(whl)
    {}

    BackingStorePrivate(int w, int h, int num_layers):
      m_dimensions(w, h, num_layers)
    {}

    fastuidraw::ivec3 m_dimensions;
  };

  class inited_bool
  {
  public:
    bool m_value;
    inited_bool(bool b = false):m_value(b) {}
  };

  class tile_allocator
  {
  public:
    tile_allocator(int tile_size, fastuidraw::ivec3 store_dimensions);

    ~tile_allocator();

    fastuidraw::ivec3
    allocate_tile(void);

    void
    delete_tile(fastuidraw::ivec3 v);

    int
    number_free(void) const;

    bool
    resize_to_fit(int num_tiles);

    void
    lock_resources(void);

    void
    unlock_resources(void);

    int
    tile_size(void) const
    {
      return m_tile_size;
    }

    const fastuidraw::ivec3&
    num_tiles(void) const
    {
      return m_num_tiles;
    }

  private:
    void
    delete_tile_implement(fastuidraw::ivec3 v);

    int m_tile_size;
    fastuidraw::ivec3 m_next_tile;
    fastuidraw::ivec3 m_num_tiles;
    std::vector<fastuidraw::ivec3> m_free_tiles;
    int m_tile_count;

    int m_lock_resources_counter;
    std::vector<fastuidraw::ivec3> m_delayed_free_tiles;

    #ifdef FASTUIDRAW_DEBUG
    fastuidraw::array3d<inited_bool> m_tile_allocated;
    #endif
  };

  class ResourceReleaseActionList
  {
  public:
    ResourceReleaseActionList(void):
      m_lock_resources_counter(0)
    {}

    ~ResourceReleaseActionList()
    {
      FASTUIDRAWassert(m_lock_resources_counter == 0);
      FASTUIDRAWassert(m_delete_actions.empty());
    }

    void
    add_action(const fastuidraw::reference_counted_ptr<fastuidraw::Image::ResourceReleaseAction> &action)
    {
      if (m_lock_resources_counter != 0u)
        {
          m_delete_actions.push_back(action);
        }
      else
        {
          action->action();
        }
    }

    void
    lock_resources(void)
    {
      ++m_lock_resources_counter;
    }

    void
    unlock_resources(void)
    {
      FASTUIDRAWassert(m_lock_resources_counter >= 1);
      --m_lock_resources_counter;
      if (m_lock_resources_counter == 0)
        {
          for (const auto &v : m_delete_actions)
            {
              v->action();
            }
          m_delete_actions.clear();
        }
    }

  private:
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::Image::ResourceReleaseAction> > m_delete_actions;
    unsigned int m_lock_resources_counter;
  };

  template<typename T>
  fastuidraw::ivec3
  dimensions_of_store(const fastuidraw::reference_counted_ptr<T> &store)
  {
    return (store) ?
      store->dimensions() :
      fastuidraw::ivec3(0, 0, 0);
  }

  class ImageAtlasPrivate
  {
  public:
    ImageAtlasPrivate(int pcolor_tile_size, int pindex_tile_size,
                      const fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase> &pcolor_store,
                      const fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase> &pindex_store):
      m_color_store(pcolor_store),
      m_color_store_constant(m_color_store),
      m_color_tiles(pcolor_tile_size, dimensions_of_store(pcolor_store)),
      m_index_store(pindex_store),
      m_index_store_constant(m_index_store),
      m_index_tiles(pindex_tile_size, dimensions_of_store(pindex_store))
    {}

    int
    number_free_index_tiles(void);

    fastuidraw::ivec3
    add_index_tile(fastuidraw::c_array<const fastuidraw::ivec3> data);

    fastuidraw::ivec3
    add_index_tile_index_data(fastuidraw::c_array<const fastuidraw::ivec3> data);

    void
    delete_index_tile(fastuidraw::ivec3 tile);

    fastuidraw::ivec3
    add_color_tile(fastuidraw::ivec2 src_xy,
                   const fastuidraw::ImageSourceBase &image_data);

    fastuidraw::ivec3
    add_color_tile(fastuidraw::u8vec4 color_data);

    void
    delete_color_tile(fastuidraw::ivec3 tile);

    int
    number_free_color_tiles(void);

    void
    resize_to_fit(int num_color_tiles, int num_index_tiles);

    int
    index_tile_size(void)
    {
      return m_index_tiles.tile_size();
    }

    int
    color_tile_size(void)
    {
      return m_color_tiles.tile_size();
    }

    std::mutex m_mutex;
    ResourceReleaseActionList m_delete_actions;

    fastuidraw::reference_counted_ptr<fastuidraw::AtlasColorBackingStoreBase> m_color_store;
    fastuidraw::reference_counted_ptr<const fastuidraw::AtlasColorBackingStoreBase> m_color_store_constant;
    tile_allocator m_color_tiles;

    fastuidraw::reference_counted_ptr<fastuidraw::AtlasIndexBackingStoreBase> m_index_store;
    fastuidraw::reference_counted_ptr<const fastuidraw::AtlasIndexBackingStoreBase> m_index_store_constant;
    tile_allocator m_index_tiles;
  };

  /* TODO: take into account for repeated tile colors. */
  bool
  enough_room_in_atlas(fastuidraw::ivec2 number_color_tiles,
                       ImageAtlasPrivate *C, int &total_index)
  {
    int total_color;

    total_color = number_color_tiles.x() * number_color_tiles.y();
    total_index = number_index_tiles_needed(number_color_tiles, C->index_tile_size());

    //std::cout << "Need " << total_color << " have: " << C->number_free_color_tiles() << "\n"
    //        << "Need " << total_index << " have: " << C->number_free_index_tiles() << "\n";

    return total_color <= C->number_free_color_tiles()
      && total_index <= C->number_free_index_tiles();
  }

  class per_color_tile
  {
  public:
    per_color_tile(const fastuidraw::ivec3 t, bool b):
      m_tile(t), m_non_repeat_color(b)
    {}

    operator fastuidraw::ivec3() const
    {
      return m_tile;
    }

    fastuidraw::ivec3 m_tile;
    bool m_non_repeat_color;
  };

  class ImagePrivate
  {
  public:
    ImagePrivate(fastuidraw::ImageAtlas &patlas,
                 ImageAtlasPrivate *atlas_private,
                 int w, int h,
                 const fastuidraw::ImageSourceBase &image_data);

    ImagePrivate(fastuidraw::ImageAtlas &patlas,
                 ImageAtlasPrivate *atlas_private, int w, int h,
                 unsigned int m, fastuidraw::Image::type_t t, uint64_t handle,
                 enum fastuidraw::Image::format_t fmt,
                 const fastuidraw::reference_counted_ptr<fastuidraw::Image::ResourceReleaseAction> &action):
      m_atlas(&patlas),
      m_atlas_private(atlas_private),
      m_action(action),
      m_dimensions(w, h),
      m_number_levels(m),
      m_type(t),
      m_format(fmt),
      m_num_color_tiles(-1, -1),
      m_master_index_tile(-1, -1, -1),
      m_master_index_tile_dims(-1.0f, -1.0f),
      m_number_index_lookups(0),
      m_dimensions_index_divisor(-1.0f),
      m_bindless_handle(handle)
    {
    }

    ~ImagePrivate();

    void
    create_color_tiles(const fastuidraw::ImageSourceBase &image_data);

    void
    create_index_tiles(void);

    template<typename T>
    fastuidraw::ivec2
    create_index_layer(fastuidraw::c_array<const T> src_tiles,
                       fastuidraw::ivec2 src_dims,
                       std::list<std::vector<fastuidraw::ivec3> > &destination);

    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_atlas;
    ImageAtlasPrivate *m_atlas_private;
    fastuidraw::reference_counted_ptr<fastuidraw::Image::ResourceReleaseAction> m_action;
    fastuidraw::ivec2 m_dimensions;
    int m_number_levels;

    enum fastuidraw::Image::type_t m_type;
    enum fastuidraw::Image::format_t m_format;

    /* Data for when the image has type on_atlas */
    fastuidraw::ivec2 m_num_color_tiles;
    std::map<fastuidraw::u8vec4, fastuidraw::ivec3> m_repeated_tiles;
    std::vector<per_color_tile> m_color_tiles;
    std::list<std::vector<fastuidraw::ivec3> > m_index_tiles;
    fastuidraw::ivec3 m_master_index_tile;
    fastuidraw::vec2 m_master_index_tile_dims;
    unsigned int m_number_index_lookups;
    float m_dimensions_index_divisor;

    /* data for when image has different type than on_atlas */
    uint64_t m_bindless_handle;
  };
}

/////////////////////////////////////////////
//ImagePrivate methods
ImagePrivate::
ImagePrivate(fastuidraw::ImageAtlas &patlas,
             ImageAtlasPrivate *atlas_private, int w, int h,
             const fastuidraw::ImageSourceBase &image_data):
  m_atlas(&patlas),
  m_atlas_private(atlas_private),
  m_dimensions(w, h),
  m_number_levels(image_data.number_levels()),
  m_type(fastuidraw::Image::on_atlas),
  m_format(image_data.format()),
  m_bindless_handle(-1)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_dimensions.x() > 0);
  FASTUIDRAWassert(m_dimensions.y() > 0);
  FASTUIDRAWassert(m_atlas);

  create_color_tiles(image_data);
  create_index_tiles();

  /* Mipmap filtering cannot go beyond the tile size or the
   * size of the image.
   */
  int max_levels;
  max_levels = 1u + uint32_log2(t_min(m_atlas_private->color_tile_size(), t_min(w, h)));
  m_number_levels = t_min(max_levels, m_number_levels);
}

ImagePrivate::
~ImagePrivate()
{
  for(const per_color_tile &C : m_color_tiles)
    {
      if (C.m_non_repeat_color)
        {
          m_atlas_private->delete_color_tile(C.m_tile);
        }
    }

  for(const auto &C : m_repeated_tiles)
    {
      m_atlas_private->delete_color_tile(C.second);
    }

  for(const auto &tile_array: m_index_tiles)
    {
      for(const fastuidraw::ivec3 &index_tile : tile_array)
        {
          m_atlas_private->delete_index_tile(index_tile);
        }
    }

  if (m_action)
    {
      m_atlas->queue_resource_release_action(m_action);
    }
}

void
ImagePrivate::
create_color_tiles(const fastuidraw::ImageSourceBase &image_data)
{
  int tile_interior_size;
  int color_tile_size;

  color_tile_size = m_atlas_private->color_tile_size();
  tile_interior_size = color_tile_size;
  m_num_color_tiles = divide_up(m_dimensions, tile_interior_size);
  m_master_index_tile_dims = fastuidraw::vec2(m_dimensions) / static_cast<float>(tile_interior_size);
  m_dimensions_index_divisor = static_cast<float>(tile_interior_size);

  unsigned int savings(0);
  for(int ty = 0, source_y = 0;
      ty < m_num_color_tiles.y();
      ++ty, source_y += tile_interior_size)
    {
      for(int tx = 0, source_x = 0;
          tx < m_num_color_tiles.x();
          ++tx, source_x += tile_interior_size)
        {
          fastuidraw::ivec3 new_tile;
          fastuidraw::ivec2 src_xy(source_x, source_y);
          bool all_same_color;
          fastuidraw::u8vec4 same_color_value;

          all_same_color = image_data.all_same_color(src_xy, color_tile_size, &same_color_value);

          if (all_same_color)
            {
              std::map<fastuidraw::u8vec4, fastuidraw::ivec3>::iterator iter;

              iter = m_repeated_tiles.find(same_color_value);
              if (iter != m_repeated_tiles.end())
                {
                  new_tile = iter->second;
                  ++savings;
                }
              else
                {
                  new_tile = m_atlas_private->add_color_tile(same_color_value);
                  m_repeated_tiles[same_color_value] = new_tile;
                }
            }
          else
            {
              new_tile = m_atlas_private->add_color_tile(src_xy, image_data);
            }

          m_color_tiles.push_back(per_color_tile(new_tile, !all_same_color) );
        }
    }

  FASTUIDRAWunused(savings);
  //std::cout << "Saved " << savings << " out of "
  //        << m_num_color_tiles.x() * m_num_color_tiles.y()
  //        << " tiles from repeat color magicks\n";
}


/*
 * returns the number of index tiles needed to
 * store the created index data.
 */
template<typename T>
fastuidraw::ivec2
ImagePrivate::
create_index_layer(fastuidraw::c_array<const T> src_tiles,
                   fastuidraw::ivec2 src_dims,
                   std::list<std::vector<fastuidraw::ivec3> > &destination)
{
  int index_tile_size;
  fastuidraw::ivec2 num_index_tiles;

  index_tile_size = m_atlas_private->index_tile_size();
  num_index_tiles = divide_up(src_dims, index_tile_size);

  destination.push_back(std::vector<fastuidraw::ivec3>());

  std::vector<fastuidraw::ivec3> vtile_data(index_tile_size * index_tile_size);
  fastuidraw::c_array<fastuidraw::ivec3> tile_data;
  tile_data = fastuidraw::make_c_array(vtile_data);

  for(int source_y = 0; source_y < src_dims.y(); source_y += index_tile_size)
    {
      for(int source_x = 0; source_x < src_dims.x(); source_x += index_tile_size)
        {
          fastuidraw::ivec3 new_tile;
          copy_sub_data<fastuidraw::ivec3, T>(tile_data, index_tile_size, index_tile_size,
                                              src_tiles, source_x, source_y,
                                              src_dims);
          new_tile = m_atlas_private->add_index_tile(tile_data);
          destination.back().push_back(new_tile);
        }
    }
  return num_index_tiles;
}

void
ImagePrivate::
create_index_tiles(void)
{
  fastuidraw::ivec2 num_index_tiles;
  int level(2);
  float findex_tile_size;

  findex_tile_size = static_cast<float>(m_atlas_private->index_tile_size());
  num_index_tiles = create_index_layer<per_color_tile>(fastuidraw::make_c_array(m_color_tiles),
                                                       m_num_color_tiles, m_index_tiles);

  for(level = 2; num_index_tiles.x() > 1 || num_index_tiles.y() > 1; ++level)
    {
      num_index_tiles = create_index_layer<fastuidraw::ivec3>(fastuidraw::make_c_array(m_index_tiles.back()),
                                                              num_index_tiles, m_index_tiles);
      m_dimensions_index_divisor *= findex_tile_size;
      m_master_index_tile_dims /= findex_tile_size;
    }

  FASTUIDRAWassert(m_index_tiles.back().size() == 1);
  m_master_index_tile = m_index_tiles.back()[0];
  m_number_index_lookups = m_index_tiles.size();
}

///////////////////////////////////////////
// tile_allocator methods
tile_allocator::
tile_allocator(int tile_size, fastuidraw::ivec3 store_dimensions):
  m_tile_size(tile_size),
  m_next_tile(0, 0, 0),
  m_num_tiles(tile_size > 0 ? store_dimensions.x() / m_tile_size : 0,
              tile_size > 0 ? store_dimensions.y() / m_tile_size : 0,
              tile_size > 0 ? store_dimensions.z() : 0),
  m_tile_count(0),
  m_lock_resources_counter(0)
#ifdef FASTUIDRAW_DEBUG
  ,
  m_tile_allocated(m_num_tiles.x(), m_num_tiles.y(), m_num_tiles.z())
#endif
{
  FASTUIDRAWassert(m_tile_size == 0 || store_dimensions.x() % m_tile_size == 0);
  FASTUIDRAWassert(m_tile_size == 0 || store_dimensions.y() % m_tile_size == 0);
}

tile_allocator::
~tile_allocator()
{
  FASTUIDRAWassert(m_lock_resources_counter == 0);
  FASTUIDRAWassert(m_tile_count == 0);
}

fastuidraw::ivec3
tile_allocator::
allocate_tile(void)
{
  fastuidraw::ivec3 return_value;
  if (m_free_tiles.empty())
    {
      if (m_next_tile.x() < m_num_tiles.x() || m_next_tile.y() < m_num_tiles.y() || m_next_tile.z() < m_num_tiles.z())
        {
          return_value = m_next_tile;
          ++m_next_tile.x();
          if (m_next_tile.x() == m_num_tiles.x())
            {
              m_next_tile.x() = 0;
              ++m_next_tile.y();
              if (m_next_tile.y() == m_num_tiles.y())
                {
                  m_next_tile.y() = 0;
                  ++m_next_tile.z();
                }
            }
        }
      else
        {
          FASTUIDRAWassert(!"Color tile room exhausted");
          return fastuidraw::ivec3(-1, -1,-1);
        }
    }
  else
    {
      return_value = m_free_tiles.back();
      m_free_tiles.pop_back();
    }

  #ifdef FASTUIDRAW_DEBUG
    {
      FASTUIDRAWassert(!m_tile_allocated(return_value.x(), return_value.y(), return_value.z()).m_value);
      m_tile_allocated(return_value.x(), return_value.y(), return_value.z()) = true;
    }
  #endif

  ++m_tile_count;
  return return_value;
}

void
tile_allocator::
lock_resources(void)
{
  ++m_lock_resources_counter;
}

void
tile_allocator::
unlock_resources(void)
{
  FASTUIDRAWassert(m_lock_resources_counter >= 1);
  --m_lock_resources_counter;
  if (m_lock_resources_counter == 0)
    {
      for(unsigned int i = 0, endi = m_delayed_free_tiles.size(); i < endi; ++i)
        {
          delete_tile_implement(m_delayed_free_tiles[i]);
        }
      m_delayed_free_tiles.clear();
    }
}

void
tile_allocator::
delete_tile(fastuidraw::ivec3 v)
{
  if (m_lock_resources_counter == 0)
    {
      delete_tile_implement(v);
    }
  else
    {
      m_delayed_free_tiles.push_back(v);
    }
}

void
tile_allocator::
delete_tile_implement(fastuidraw::ivec3 v)
{
  FASTUIDRAWassert(m_lock_resources_counter == 0);
  #ifdef FASTUIDRAW_DEBUG
    {
      FASTUIDRAWassert(m_tile_allocated(v.x(), v.y(), v.z()).m_value);
      m_tile_allocated(v.x(), v.y(), v.z()) = false;
    }
  #endif

  --m_tile_count;
  m_free_tiles.push_back(v);
}

int
tile_allocator::
number_free(void) const
{
  return m_num_tiles.x() * m_num_tiles.y() * m_num_tiles.z() - m_tile_count;
}

bool
tile_allocator::
resize_to_fit(int num_tiles)
{
  if (num_tiles > number_free())
    {
      /* resize to fit the number of needed tiles,
       * compute how many more tiles needed and from
       * there compute how many more layers needed.
       */
      int needed_tiles, tiles_per_layer, needed_layers;
      needed_tiles = num_tiles - number_free();
      tiles_per_layer = m_num_tiles.x() * m_num_tiles.y();
      needed_layers = needed_tiles / tiles_per_layer;
      if (needed_tiles > needed_layers * tiles_per_layer)
        {
          ++needed_layers;
        }

      /* TODO:
       *  Should we resize at powers of 2, or just to what is
       *  needed?
       */
      m_num_tiles.z() += needed_layers;
      #ifdef FASTUIDRAW_DEBUG
        {
          m_tile_allocated.resize(m_num_tiles.x(), m_num_tiles.y(), m_num_tiles.z());
        }
      #endif

      return true;
    }
  else
    {
      return false;
    }
}

/////////////////////////////////////////
// ImageAtlasPrivate methods
int
ImageAtlasPrivate::
number_free_index_tiles(void)
{
  std::lock_guard<std::mutex> M(m_mutex);
  return m_index_tiles.number_free();
}

fastuidraw::ivec3
ImageAtlasPrivate::
add_index_tile(fastuidraw::c_array<const fastuidraw::ivec3> data)
{
  fastuidraw::ivec3 return_value;
  std::lock_guard<std::mutex> M(m_mutex);

  return_value = m_index_tiles.allocate_tile();
  if (return_value != fastuidraw::ivec3(-1, -1, -1))
    {
      m_index_store->set_data(return_value.x() * m_index_tiles.tile_size(),
                              return_value.y() * m_index_tiles.tile_size(),
                              return_value.z(),
                              m_index_tiles.tile_size(),
                              m_index_tiles.tile_size(),
                              data);
    }

  return return_value;
}

void
ImageAtlasPrivate::
delete_index_tile(fastuidraw::ivec3 tile)
{
  std::lock_guard<std::mutex> M(m_mutex);
  m_index_tiles.delete_tile(tile);
}

int
ImageAtlasPrivate::
number_free_color_tiles(void)
{
  std::lock_guard<std::mutex> M(m_mutex);
  return m_color_tiles.number_free();
}

fastuidraw::ivec3
ImageAtlasPrivate::
add_color_tile(fastuidraw::u8vec4 color_data)
{
  fastuidraw::ivec3 return_value;
  fastuidraw::ivec2 dst_xy;
  int sz;

  std::lock_guard<std::mutex> M(m_mutex);
  return_value = m_color_tiles.allocate_tile();
  if (return_value != fastuidraw::ivec3(-1, -1, -1))
    {
      dst_xy.x() = return_value.x() * m_color_tiles.tile_size();
      dst_xy.y() = return_value.y() * m_color_tiles.tile_size();
      sz = m_color_tiles.tile_size();

      for (int level = 0; sz > 0; ++level, sz /= 2, dst_xy /= 2)
        {
          m_color_store->set_data(level, dst_xy, return_value.z(),
                                  sz, color_data);
        }
    }

  return return_value;
}

fastuidraw::ivec3
ImageAtlasPrivate::
add_color_tile(fastuidraw::ivec2 src_xy,
               const fastuidraw::ImageSourceBase &image_data)
{
  fastuidraw::ivec3 return_value;
  fastuidraw::ivec2 dst_xy;
  int sz, level, end_level;

  std::lock_guard<std::mutex> M(m_mutex);
  return_value = m_color_tiles.allocate_tile();
  if (return_value != fastuidraw::ivec3(-1, -1, -1))
    {
      dst_xy.x() = return_value.x() * m_color_tiles.tile_size();
      dst_xy.y() = return_value.y() * m_color_tiles.tile_size();
      sz = m_color_tiles.tile_size();
      end_level = image_data.number_levels();

      for (level = 0; level < end_level && sz > 0; ++level, sz /= 2, dst_xy /= 2, src_xy /= 2)
        {
          m_color_store->set_data(level, dst_xy, return_value.z(), src_xy, sz, image_data);
        }

      for (; sz > 0; ++level, sz /= 2, dst_xy /= 2, src_xy /= 2, sz /= 2)
        {
          m_color_store->set_data(level, dst_xy, return_value.z(), sz,
                                  fastuidraw::u8vec4(255u, 255u, 0u, 255u));
        }
    }

  return return_value;
}

void
ImageAtlasPrivate::
delete_color_tile(fastuidraw::ivec3 tile)
{
  std::lock_guard<std::mutex> M(m_mutex);
  m_color_tiles.delete_tile(tile);
}

void
ImageAtlasPrivate::
resize_to_fit(int num_color_tiles, int num_index_tiles)
{
  std::lock_guard<std::mutex> M(m_mutex);
  if (m_color_tiles.resize_to_fit(num_color_tiles))
    {
      m_color_store->resize(m_color_tiles.num_tiles().z());
    }
  if (m_index_tiles.resize_to_fit(num_index_tiles))
    {
      m_index_store->resize(m_index_tiles.num_tiles().z());
    }
}

////////////////////////////////////////
// fastuidraw::ImageSourceCArray methods
fastuidraw::ImageSourceCArray::
ImageSourceCArray(uvec2 dimensions,
                  c_array<const c_array<const u8vec4> > pdata,
                  enum Image::format_t fmt):
  m_dimensions(dimensions),
  m_data(pdata),
  m_format(fmt)
{}

bool
fastuidraw::ImageSourceCArray::
all_same_color(ivec2 location, int square_size, u8vec4 *dst) const
{
  location.x() = t_max(location.x(), 0);
  location.y() = t_max(location.y(), 0);

  location.x() = t_min(int(m_dimensions.x()) - 1, location.x());
  location.y() = t_min(int(m_dimensions.y()) - 1, location.y());

  square_size = t_min(int(m_dimensions.x()) - location.x(), square_size);
  square_size = t_min(int(m_dimensions.y()) - location.y(), square_size);

  *dst = m_data[0][location.x() + location.y() * m_dimensions.x()];
  for (int y = 0, sy = location.y(); y < square_size; ++y, ++sy)
    {
      for (int x = 0, sx = location.x(); x < square_size; ++x, ++sx)
        {
          if (*dst != m_data[0][sx + sy * m_dimensions.x()])
            {
              return false;
            }
        }
    }
  return true;
}

unsigned int
fastuidraw::ImageSourceCArray::
number_levels(void) const
{
  return m_data.size();
}

void
fastuidraw::ImageSourceCArray::
fetch_texels(unsigned int mipmap_level, ivec2 location,
             unsigned int w, unsigned int h,
             c_array<u8vec4> dst) const
{
  if (mipmap_level >= m_data.size())
    {
      std::fill(dst.begin(), dst.end(), u8vec4(255u, 255u, 0u, 255u));
    }
  else
    {
      copy_sub_data(dst, w, h,
                    m_data[mipmap_level],
                    location.x(), location.y(),
                    ivec2(m_dimensions.x() >> mipmap_level,
                          m_dimensions.y() >> mipmap_level));
    }
}

enum fastuidraw::Image::format_t
fastuidraw::ImageSourceCArray::
format(void) const
{
  return m_format;
}

//////////////////////////////////////////////////
// fastuidraw::AtlasColorBackingStoreBase methods
fastuidraw::AtlasColorBackingStoreBase::
AtlasColorBackingStoreBase(ivec3 whl)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(whl);
}

fastuidraw::AtlasColorBackingStoreBase::
AtlasColorBackingStoreBase(int w, int h, int num_layers)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(w, h, num_layers);
}

fastuidraw::AtlasColorBackingStoreBase::
~AtlasColorBackingStoreBase()
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec3
fastuidraw::AtlasColorBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

void
fastuidraw::AtlasColorBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWassert(new_num_layers > d->m_dimensions.z());
  resize_implement(new_num_layers);
  d->m_dimensions.z() = new_num_layers;
}

///////////////////////////////////////////////
// fastuidraw::AtlasIndexBackingStoreBase methods
fastuidraw::AtlasIndexBackingStoreBase::
AtlasIndexBackingStoreBase(ivec3 whl)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(whl);
}

fastuidraw::AtlasIndexBackingStoreBase::
AtlasIndexBackingStoreBase(int w, int h, int l)
{
  m_d = FASTUIDRAWnew BackingStorePrivate(w, h, l);
}

fastuidraw::AtlasIndexBackingStoreBase::
~AtlasIndexBackingStoreBase()
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec3
fastuidraw::AtlasIndexBackingStoreBase::
dimensions(void) const
{
  BackingStorePrivate *d;
  d = static_cast<BackingStorePrivate*>(m_d);
  return d->m_dimensions;
}

void
fastuidraw::AtlasIndexBackingStoreBase::
resize(int new_num_layers)
{
  BackingStorePrivate *d;

  d = static_cast<BackingStorePrivate*>(m_d);
  FASTUIDRAWassert(new_num_layers > d->m_dimensions.z());
  resize_implement(new_num_layers);
  d->m_dimensions.z() = new_num_layers;
}

//////////////////////////////
// fastuidraw::ImageAtlas methods
fastuidraw::ImageAtlas::
ImageAtlas(int pcolor_tile_size, int pindex_tile_size,
           fastuidraw::reference_counted_ptr<AtlasColorBackingStoreBase> pcolor_store,
           fastuidraw::reference_counted_ptr<AtlasIndexBackingStoreBase> pindex_store)
{
  m_d = FASTUIDRAWnew ImageAtlasPrivate(pcolor_tile_size, pindex_tile_size,
                                       pcolor_store, pindex_store);
}

fastuidraw::ImageAtlas::
~ImageAtlas()
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::ImageAtlas::
lock_resources(void)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_color_tiles.lock_resources();
  d->m_index_tiles.lock_resources();
  d->m_delete_actions.lock_resources();
}

void
fastuidraw::ImageAtlas::
unlock_resources(void)
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);

  std::lock_guard<std::mutex> M(d->m_mutex);
  d->m_color_tiles.unlock_resources();
  d->m_index_tiles.unlock_resources();
  d->m_delete_actions.unlock_resources();
}

int
fastuidraw::ImageAtlas::
color_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->color_tile_size();
}

int
fastuidraw::ImageAtlas::
index_tile_size(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->index_tile_size();
}

void
fastuidraw::ImageAtlas::
flush(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  std::lock_guard<std::mutex> M(d->m_mutex);

  if (d->m_index_store)
    {
      d->m_index_store->flush();
    }

  if (d->m_color_store)
    {
      d->m_color_store->flush();
    }
}

const fastuidraw::reference_counted_ptr<const fastuidraw::AtlasColorBackingStoreBase>&
fastuidraw::ImageAtlas::
color_store(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_color_store_constant;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::AtlasIndexBackingStoreBase>&
fastuidraw::ImageAtlas::
index_store(void) const
{
  ImageAtlasPrivate *d;
  d = static_cast<ImageAtlasPrivate*>(m_d);
  return d->m_index_store_constant;
}

void
fastuidraw::ImageAtlas::
queue_resource_release_action(const reference_counted_ptr<Image::ResourceReleaseAction> &action)
{
  if (action)
    {
      ImageAtlasPrivate *d;
      d = static_cast<ImageAtlasPrivate*>(m_d);

      std::lock_guard<std::mutex> M(d->m_mutex);
      d->m_delete_actions.add_action(action);
    }
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::ImageAtlas::
create_image_on_atlas(int w, int h, const ImageSourceBase &image_data)
{
  int tile_interior_size;
  ivec2 num_color_tiles;
  int index_tiles;
  ImageAtlasPrivate *d;

  d = static_cast<ImageAtlasPrivate*>(m_d);
  if (w <= 0 || h <= 0 || !d->m_color_store || !d->m_index_store)
    {
      return reference_counted_ptr<Image>();
    }

  tile_interior_size = color_tile_size();
  if (tile_interior_size <= 0)
    {
      return reference_counted_ptr<Image>();
    }

  num_color_tiles = divide_up(ivec2(w, h), tile_interior_size);
  if (!enough_room_in_atlas(num_color_tiles, d, index_tiles))
    {
      /* TODO:
       * there actually might be enough room if we take into account
       * the savings from repeated tiles. The correct thing is to
       * delay this until iamge construction, check if it succeeded
       * and if not then delete it and return an invalid handle.
       */
      d->resize_to_fit(num_color_tiles.x() * num_color_tiles.y(), index_tiles);
    }

  return FASTUIDRAWnew Image(*this, w, h, image_data);
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::ImageAtlas::
create_non_atlas(int w, int h, const ImageSourceBase &image_data)
{
  reference_counted_ptr<Image> return_value;

  return_value = create_image_bindless(w, h, image_data);
  if (!return_value)
    {
      return_value = create_image_context_texture2d(w, h, image_data);
    }
  return return_value;
}

fastuidraw::reference_counted_ptr<fastuidraw::Image>
fastuidraw::ImageAtlas::
create(int w, int h, const ImageSourceBase &image_data,
       enum Image::type_t type)
{
  reference_counted_ptr<Image> return_value;
  vecN<enum Image::type_t, 2> try_types;

  try_types[0] = type;
  switch (type)
    {
    case Image::bindless_texture2d:
      try_types[1] = Image::on_atlas;
      break;
    case Image::on_atlas:
      try_types[1] = Image::bindless_texture2d;
      break;
    default:
      try_types[1] = type;
    }

  for (unsigned int i = 0; i < 2 && !return_value; ++i)
    {
      if (try_types[i] == Image::bindless_texture2d)
        {
          return_value = create_image_bindless(w, h, image_data);
        }
      else if (try_types[i] == Image::on_atlas)
        {
          return_value = create_image_on_atlas(w, h, image_data);
        }
    }

  if (return_value)
    {
      return return_value;
    }

  return_value = create_image_context_texture2d(w, h, image_data);
  return return_value;
}

//////////////////////////////////////
// fastuidraw::Image methods
fastuidraw::Image::
Image(ImageAtlas &patlas, int w, int h, unsigned int m,
      enum type_t type, uint64_t handle, enum format_t fmt,
      const reference_counted_ptr<Image::ResourceReleaseAction> &action)
{
  ImageAtlasPrivate *atlas_private;
  atlas_private = static_cast<ImageAtlasPrivate*>(patlas.m_d);
  m_d = FASTUIDRAWnew ImagePrivate(patlas, atlas_private, w, h, m, type, handle, fmt, action);
}

fastuidraw::Image::
Image(ImageAtlas &patlas, int w, int h,
      const ImageSourceBase &image_data)
{
  ImageAtlasPrivate *atlas_private;
  atlas_private = static_cast<ImageAtlasPrivate*>(patlas.m_d);
  m_d = FASTUIDRAWnew ImagePrivate(patlas, atlas_private, w, h, image_data);
}

fastuidraw::Image::
~Image()
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}


unsigned int
fastuidraw::Image::
number_index_lookups(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_number_index_lookups;
}

fastuidraw::ivec2
fastuidraw::Image::
dimensions(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_dimensions;
}

unsigned int
fastuidraw::Image::
number_mipmap_levels(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_number_levels;
}

fastuidraw::ivec3
fastuidraw::Image::
master_index_tile(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile;
}

fastuidraw::vec2
fastuidraw::Image::
master_index_tile_dims(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_master_index_tile_dims;
}

float
fastuidraw::Image::
dimensions_index_divisor(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_dimensions_index_divisor;
}

enum fastuidraw::Image::type_t
fastuidraw::Image::
type(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_type;
}

uint64_t
fastuidraw::Image::
bindless_handle(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_bindless_handle;
}

enum fastuidraw::Image::format_t
fastuidraw::Image::
format(void) const
{
  ImagePrivate *d;
  d = static_cast<ImagePrivate*>(m_d);
  return d->m_format;
}
