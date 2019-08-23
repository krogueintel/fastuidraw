/*!
 * \file image.hpp
 * \brief file image.hpp
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

#ifndef FASTUIDRAW_IMAGE_HPP
#define FASTUIDRAW_IMAGE_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{

///@cond
class ImageSourceBase;
class ImageAtlas;
///@endcond

/*!\addtogroup Imaging
 * @{
 */

  /*!
   * \brief
   * An Image represents an image comprising of RGBA8 values.
   * The texel values themselves are stored in a ImageAtlas.
   */
  class Image:public resource_base
  {
  public:
    /*!
     * Enumeration to describe the format of an image
     */
    enum format_t
      {
        /*!
         * Image is non-premultiplied RGBA format (each
         * color channel of each pixel taking 8-bits).
         */
        rgba_format,

        /*!
         * Image is RGBA format (each color channel of
         * each pixel taking 8-bits) with the RGB channels
         * pre-multiplied by the alpha channel.
         */
        premultipied_rgba_format,
      };

    /*!
     * Gives the image-type of an Image
     */
    enum type_t
      {
        /*!
         * Indicates that the Image is on an ImageAtlas.
         * Such images will not introcude draw-breaks, but
         * are more complicated to sample from.
         */
        on_atlas,

        /*!
         * Indicates that the Image is backed by a gfx API
         * texture via a bindless interface. Such images
         * do not introduce draw-breaks and are simple
         * from. The only draw back, is that not all 3D
         * API implementations support bindless texruing
         * (for example the GL/GLES backends require that
         * the GL/GLES implementation support an extension.
         */
        bindless_texture2d,

        /*!
         * Indicates to source the Image data from a currently
         * bound texture of the 3D API context. These image are
         * simple to sample from but introduce draw breaks. Using
         * images of these types should be avoided at all costs
         * since whenever one uses Image objects of these types
         * the 3D API state needs to change which induces a stage
         * change and draw break, harming performance.
         */
        context_texture2d,
      };

    /*!
     * Class representing an action to execute when a bindless
     * image is deleted. The action is guaranteed to be executed
     * AFTER the 3D API is no longer using the resources that
     * back the image.
     */
    class ResourceReleaseAction:
      public reference_counted<ResourceReleaseAction>::concurrent
    {
    public:
      virtual
      ~ResourceReleaseAction()
      {}

      /*!
       * To be implemented by a derived class to perform resource
       * release actions.
       */
      virtual
      void
      action(void) = 0;
    };

    virtual
    ~Image();

    /*!
     * Returns the number of index look-ups to get to the image data.
     *
     * Only applies when type() returns \ref on_atlas.
     */
    unsigned int
    number_index_lookups(void) const;

    /*!
     * Returns the dimensions of the image, i.e the width and height.
     */
    ivec2
    dimensions(void) const;

    /*!
     * Returns the number of mipmap levels the image supports.
     */
    unsigned int
    number_mipmap_levels(void) const;

    /*!
     * Returns the "head" index tile as returned by
     * ImageAtlas::add_index_tile() or
     * ImageAtlas::add_index_tile_index_data().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    ivec3
    master_index_tile(void) const;

    /*!
     * If number_index_lookups() > 0, returns the number of texels in
     * each dimension of the master index tile this Image lies.
     * If number_index_lookups() is 0, the returns the same value
     * as dimensions().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    vec2
    master_index_tile_dims(void) const;

    /*!
     * Returns the quotient of dimensions() divided
     * by master_index_tile_dims().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    float
    dimensions_index_divisor(void) const;

    /*!
     * Returns the bindless handle for the Image.
     *
     * Only applies when type() does NOT return \ref on_atlas.
     */
    uint64_t
    bindless_handle(void) const;

    /*!
     * Returns the image type.
     */
    type_t
    type(void) const;

    /*!
     * Returns the format of the image.
     */
    enum format_t
    format(void) const;

  protected:
    /*!
     * Protected ctor for creating an Image backed by a bindless texture;
     * Backends should use this ctor for Image deried classes that do
     * cleanup (for example releasing the handle and/or deleting the texture).
     * \param atlas ImageAtlas atlas onto which to place the image.
     * \param w width of underlying texture
     * \param h height of underlying texture
     * \param m number of mipmap levels of the image
     * \param type the type of the bindless texture, must NOT have value
     *             \ref on_atlas.
     * \param fmt the format of the image
     * \param handle the bindless handle value used by the Gfx API in
     *               shaders to reference the texture.
     * \param action action to call to release backing resources of
     *               the created Image.
     */
    Image(ImageAtlas &atlas, int w, int h, unsigned int m,
          enum type_t type, uint64_t handle, enum format_t fmt,
          const reference_counted_ptr<ResourceReleaseAction> &action =
          reference_counted_ptr<ResourceReleaseAction>());

  private:
    friend class ImageAtlas;

    Image(ImageAtlas &atlas, int w, int h,
          const ImageSourceBase &image_data);

    void *m_d;
  };

  /*!
   * \brief
   * ImageSourceBase defines the inteface for copying texel data
   * from a source (CPU memory, a file, etc) to an
   * AtlasColorBackingStoreBase derived object.
   */
  class ImageSourceBase
  {
  public:
    virtual
    ~ImageSourceBase()
    {}

    /*!
     * To be implemented by a derived class to return true
     * if a region (across all mipmap levels) has a constant
     * color.
     * \param location location at LOD 0
     * \param square_size width and height of region to check
     * \param dst if all have texels of the region have the same
     *            color, write that color value to dst.
     */
    virtual
    bool
    all_same_color(ivec2 location, int square_size, u8vec4 *dst) const = 0;

    /*!
     * To be implemented by a derived class to return the number of
     * levels (including the base-image) of image source has, i.e.
     * if the image is to have no mipmapping, return 1.
     */
    virtual
    unsigned int
    number_levels(void) const = 0;

    /*!
     * To be implemented by a derived class to write to
     * a \ref c_array of \ref u8vec4 data a rectangle of
     * texels of a particular mipmap level. NOTE: if pixels
     * are requested to be fetched from outside the sources
     * natural dimensions, those pixels outside are to be
     * duplicates of the boundary values.
     *
     * \param level LOD of data where 0 represents the highest
     *              level of detail and each successive level
     *              is half the resolution in each dimension;
     *              it is gauranteed by the called that
     *              0 <= level() < number_levels()
     * \param location (x, y) location of data from which to copy
     * \param w width of data from which to copy
     * \param h height of data from which to copy
     * \param dst location to which to write data, data is to be packed
     *            dst[x + w * y] holds the texel data
     *            (x + location.x(), y + location.y()) with
     *            0 <= x < w and 0 <= y < h
     */
    virtual
    void
    fetch_texels(unsigned int level, ivec2 location,
                 unsigned int w, unsigned int h,
                 c_array<u8vec4> dst) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the format of the image data.
     */
    virtual
    enum Image::format_t
    format(void) const = 0;
  };

  /*!
   * \brief
   * An implementation of \ref ImageSourceBase where the data is backed
   * by a c_array<const u8vec4> data.
   */
  class ImageSourceCArray:public ImageSourceBase
  {
  public:
    /*!
     * Ctor.
     * \param dimensions width and height of the LOD level 0 mipmap;
     *                   the LOD level n is then assumed to be size
     *                   (dimensions.x() >> n, dimensions.y() >> n)
     * \param pdata the texel data, the data is NOT copied, thus the
     *              contents backing the texel data must not be freed
     *              until the ImageSourceCArray goes out of scope.
     * \param fmt the format of the image data
     */
    ImageSourceCArray(uvec2 dimensions,
                      c_array<const c_array<const u8vec4> > pdata,
                      enum Image::format_t fmt);
    virtual
    bool
    all_same_color(ivec2 location, int square_size, u8vec4 *dst) const;

    virtual
    unsigned int
    number_levels(void) const;

    virtual
    void
    fetch_texels(unsigned int mimpap_level, ivec2 location,
                 unsigned int w, unsigned int h,
                 c_array<u8vec4> dst) const;

    virtual
    enum Image::format_t
    format(void) const;

  private:
    uvec2 m_dimensions;
    c_array<const c_array<const u8vec4> > m_data;
    enum Image::format_t m_format;
  };

/*! @} */

} //namespace

#endif
