/*!
 * \file glyph_atlas.hpp
 * \brief file glyph_atlas.hpp
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


#ifndef FASTUIDRAW_GLYPH_ATLAS_HPP
#define FASTUIDRAW_GLYPH_ATLAS_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */
  /*!
   * \brief
   * GlyphAtlasStoreBase represents an interface to an aray of
   * uint32_t values.
   *
   * An example implementation in GL would be a buffer object that backs
   * a single usamplerBuffer. An implementation of the class does NOT need to be
   * thread safe because the user of the backing store, \ref GlyphAtlas performs
   * calls to the backing store behind its own mutex.
   */
  class GlyphAtlasBackingStoreBase:
    public reference_counted<GlyphAtlasBackingStoreBase>::concurrent
  {
  public:
    virtual
    ~GlyphAtlasBackingStoreBase();

    /*!
     * Returns the number of uint32_t backed by the store.
     */
    unsigned int
    size(void);

    /*!
     * To be implemented by a derived class to load
     * data into the store.
     * \param location to load data
     * \param pdata data to load
     */
    virtual
    void
    set_values(unsigned int location, c_array<const uint32_t> pdata) = 0;

    /*!
     * To be implemented by a derived class to flush contents
     * to the backing store.
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * Resize the object to a larger size.
     * \param new_size new number of uint32_t for the store to back
     */
    void
    resize(unsigned int new_size);

  protected:
    /*!
     * Ctor.
     * \param psize number of uint32_t elements that the
                    GlyphAtlasBackingStoreBase backs
     */
    explicit
    GlyphAtlasBackingStoreBase(unsigned int psize);

    /*!
     * To be implemented by a derived class to resize the
     * object. When called, the return value of size() is
     * the size before the resize completes.
     * \param new_size new number of int32_t for the store to back
     */
    virtual
    void
    resize_implement(unsigned int new_size) = 0;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * A GlyphAtlas is a common location to place glyph data of
   * an application. Ideally, all glyph data is placed into a
   * single GlyphAtlas. Methods of GlyphAtlas are thread safe,
   * protected behind atomics and a mutex within the GlyphAtlas.
   */
  class GlyphAtlas:
    public reference_counted<GlyphAtlas>::concurrent
  {
  public:
    /*!
     * Ctor.
     * \param pstore GlyphAtlasBackingStoreBase to which to store  data
     */
    explicit
    GlyphAtlas(reference_counted_ptr<GlyphAtlasBackingStoreBase> pstore);

    virtual
    ~GlyphAtlas();

    /*!
     * Negative return value indicates failure.
     */
    int
    allocate_data(c_array<const uint32_t> pdata);

    /*!
     * Deallocate data
     */
    void
    deallocate_data(int location, int count);

    /*!
     * Returns how much  data has been allocated
     */
    unsigned int
    data_allocated(void);

    /*!
     * Frees all allocated regions of this GlyphAtlas;
     */
    void
    clear(void);

    /*!
     * Returns the number of times that clear() has been called.
     */
    unsigned int
    number_times_cleared(void) const;

    /*!
     * Calls GlyphAtlasBackingStoreBase::flush() on
     * the  backing store (see store()).
     */
    void
    flush(void) const;

    /*!
     * Returns the store for this GlyphAtlas.
     */
    const reference_counted_ptr<const GlyphAtlasBackingStoreBase>&
    store(void) const;

    /*!
     * Increments an internal counter. If this internal
     * counter is greater than zero, then clear() and
     * deallocate_data() are -delayed- until the counter
     * reaches zero again (see unlock_resources()). The
     * use case is for buffered painting where the GPU
     * calls are delayed for later (to batch commands)
     * and a clear() or deallocate_data() was issued
     * during painting.
     */
    void
    lock_resources(void);

    /*!
     * Decrements an internal counter. If this internal
     * counter reaches zero then any deallocate_data()
     * and clear() calls are issued.
     */
    void
    unlock_resources(void);

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw

#endif
