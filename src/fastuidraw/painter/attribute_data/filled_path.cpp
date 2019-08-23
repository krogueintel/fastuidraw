/*!
 * \file filled_path.cpp
 * \brief file filled_path.cpp
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


#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <ctime>
#include <set>
#include <math.h>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/attribute_data/filled_path.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>

#include <private/util_private.hpp>
#include <private/util_private_ostream.hpp>
#include <private/bounding_box.hpp>
#include <private/clip.hpp>
#include <3rd_party/glu-tess/glu-tess.hpp>

/* Actual triangulation is handled by GLU-tess.
 * The main complexity in creating a FilledPath
 * comes from two elements:
 *  - handling overlapping edges
 *  - creating a hierarchy for creating triangulations
 *    and for culling.
 *
 * The first is needed because GLU-tess will fail
 * if any two edges overlap (we say a pair of edges
 * overlap if they intersect at more than just a single
 * point). We handle this by observing that GLU-tess
 * takes doubles but TessellatedPath is floats. When
 * we feed the coordinates to GLU-tess, we offset the
 * values by an amount that is visible in fp64 but not
 * in fp32. When adding the contours to GLU-tess,
 * the locations of the points are discretized and
 * then offsets are added. In addition, some contour
 * filtering is applied as well. Afterwards, there is
 * no more discretization applied to the points.
 * The details are in CoordinateConverter and PointHoard.
 *
 * The second is needed for primarily to speed up
 * tessellation. If a TessellatedPath has a large
 * number of vertices, then that is likely because
 * it is a high level of detail and likely zoomed in
 * a great deal. To handle that, we need only to
 * have the triangulation of a smaller portion of
 * it ready. Thus we break the original path into
 * a hierarchy of paths. The partitioning is done
 * a single half plane at a time. A contour from
 * the original path is computed by simply removing
 * any points on the wrong side of the half plane
 * and inserting the points where the path crossed
 * the half plane. The sub-path objects are computed
 * via the class SubPath. The class SubsetPrivate
 * is the one that represents an element in the
 * hierarchy that is triangulated on demand.
 */

/* Values to define how to create Subset objects.
 */
namespace SubsetConstants
{
  enum
    {
      recursion_depth = 12,
      points_per_subset = 64
    };

  /* if negative, aspect ratio is not
   * enfored.
   */
  const double size_max_ratio = 4.0;
}

/* Constants for CoordinateConverter.
 * CoordinateConverter's purpose is to remap
 * the bounding box of a fastuidraw::TessellatedPath
 * to [1, 1 + 2 ^ N] x [1,  1 + 2 ^ N]
 * and then apply a fudge offset to the point
 * that an fp64 sees but an fp32 does not.
 *
 * We do this to allow for the input TessellatedPath
 * to have overlapping edges. The value for the
 * fudge offset is to be incremented on each point.
 *
 * An fp32 has a 23-bit significand that allows it
 * to represent any integer in the range [-2^24, 2^24]
 * exactly. An fp64 has a 52 bit significand.
 *
 * We set N to be 24 and the fudginess to be 2^-20
 * (leaving 9-bits for GLU to use for intersections).
 */
namespace CoordinateConverterConstants
{
  enum
    {
      log2_box_dim = 24,
      negative_log2_fudge = 20,
      box_dim = (1 << log2_box_dim),
    };

  /* essentially the height of one pixel
   * from coordinate conversions. We are
   * targetting a resolution of no more
   * thant 2^13. We also can have that
   * a subset is zoomed in by up to a
   * factor of 2^4. This leaves us with
   * 7 = 24 - 13 - 4 bits.
   */
  const double min_height = double(1u << 7u);
}

namespace
{
  class PointHoard;

  unsigned int
  signed_to_unsigned(int w)
  {
    int v, s, r;

    // outputs the ordering
    // 0, -1, +1, -2, +2
    v = fastuidraw::t_abs(w);
    s = (w < 0) ? -1 : 0;
    r = 2 * v + s;

    FASTUIDRAWassert(r >= 0);
    return r;
  }

  class AAEdge
  {
  public:
    unsigned int m_start, m_end, m_next;
    bool m_is_closing_edge;
    bool m_draw_edge, m_draw_join_to_next;
    bool m_draw_join_as_miter;
  };

  class AAFuzzCounts
  {
  public:
    AAFuzzCounts(void):
      m_attribute_count(0),
      m_index_count(0),
      m_depth_count(0)
    {}

    unsigned int m_attribute_count;
    unsigned int m_index_count;
    unsigned int m_depth_count;
  };

  class AAFuzz:fastuidraw::noncopyable
  {
  public:
    typedef std::vector<AAEdge> Contour;

    AAFuzz(void)
    {}

    void
    begin_boundary(void);

    void
    end_boundary(void);

    void
    add_edge(unsigned int p0, unsigned int p1,
             bool edge_drawn, bool p1_is_path_join);

    const std::list<Contour>&
    contours(void) const
    {
      return m_contours;
    }

    const AAFuzzCounts&
    edge_counts(void) const
    {
      return m_edge_counts;
    }

  private:
    std::list<Contour> m_contours;
    AAFuzzCounts m_edge_counts;

    Contour m_current;
  };

  class TriangleList:fastuidraw::noncopyable
  {
  public:
    TriangleList(void):
      m_count(0)
    {}

    void
    add_index(unsigned int idx)
    {
      m_indices.push_back(idx);
      ++m_count;
    }

    unsigned int
    count(void) const
    {
      return m_count;
    }

    void
    fill_at(unsigned int &offset,
            fastuidraw::c_array<unsigned int> dest,
            fastuidraw::c_array<const unsigned int> &sub_range)
    {
      FASTUIDRAWassert(count() + offset <= dest.size());
      std::copy(m_indices.begin(), m_indices.end(), &dest[offset]);
      sub_range = dest.sub_array(offset, count());
      offset += count();
    }

    const std::list<unsigned int>&
    indices(void) const
    {
      return m_indices;
    }

    bool
    empty(void) const
    {
      return m_indices.empty();
    }

  private:
    std::list<unsigned int> m_indices;
    unsigned int m_count;
  };

  class WindingComponentData:
    public fastuidraw::reference_counted<WindingComponentData>::non_concurrent
  {
  public:
    TriangleList m_triangles;
    AAFuzz m_aa_fuzz;
  };

  typedef std::map<int, fastuidraw::reference_counted_ptr<WindingComponentData> > PerWindingComponentData;

  bool
  is_even(int v)
  {
    return (v % 2) == 0;
  }

  class CoordinateConverter
  {
  public:
    explicit
    CoordinateConverter(const fastuidraw::dvec2 &pmin, const fastuidraw::dvec2 &pmax):
      m_bounds(pmin, pmax)
    {
      fastuidraw::dvec2 delta;

      delta = pmax - pmin;
      m_scale = fastuidraw::vecN<double, 2>(1.0, 1.0) / delta;
      m_scale *= static_cast<double>(CoordinateConverterConstants::box_dim);
      m_translate = pmin;
      m_delta_fudge = ::exp2(static_cast<double>(-CoordinateConverterConstants::negative_log2_fudge));
    }

    fastuidraw::ivec2
    iapply(const fastuidraw::dvec2 &pt) const
    {
      fastuidraw::dvec2 r;
      fastuidraw::ivec2 return_value;

      r = m_scale * (pt - m_translate);
      return_value.x() = 1 + clamp_int(r.x());
      return_value.y() = 1 + clamp_int(r.y());
      return return_value;
    }

    fastuidraw::dvec2
    unapply(const fastuidraw::ivec2 &ipt) const
    {
      fastuidraw::dvec2 p(ipt.x() - 1, ipt.y() - 1);
      p /= m_scale;
      p += m_translate;
      return p;
    }

    fastuidraw::dvec2
    unapply(const fastuidraw::dvec2 &ipt) const
    {
      fastuidraw::dvec2 p(ipt.x() - 1.0, ipt.y() - 1.0);
      p /= m_scale;
      p += m_translate;
      return p;
    }

    double
    fudge_delta(void) const
    {
      return m_delta_fudge;
    }

    const fastuidraw::BoundingBox<double>&
    bounds(void) const
    {
      return m_bounds;
    }

  private:
    static
    int
    clamp_int(int v)
    {
      v = fastuidraw::t_max(v, 0);
      v = fastuidraw::t_min(v, static_cast<int>(CoordinateConverterConstants::box_dim));
      return v;
    }

    fastuidraw::BoundingBox<double> m_bounds;
    double m_delta_fudge;
    fastuidraw::dvec2 m_scale, m_translate;
  };

  class SubContourPoint:public fastuidraw::dvec2
  {
  public:
    enum
      {
        on_min_x_boundary = 1,
        on_max_x_boundary = 2,
        on_x_boundary = on_min_x_boundary | on_max_x_boundary,

        on_min_y_boundary = 4,
        on_max_y_boundary = 8,
        on_y_boundary = on_min_y_boundary | on_max_y_boundary,

        on_min_boundary = on_min_y_boundary | on_min_x_boundary,
        on_max_boundary = on_max_y_boundary | on_max_x_boundary,

        is_path_join = 16,
      };

    enum corner_t
      {
        /* NOTE: ordering of corners in enum is in order of making progress
         * around the square to increment the winding number.
         */
        corner_minx_miny = 0,
        corner_minx_maxy,
        corner_maxx_maxy,
        corner_maxx_miny,

        not_corner
      };

    SubContourPoint(const fastuidraw::dvec2 &pt, uint32_t f):
      fastuidraw::dvec2(pt),
      m_flags(f)
    {
      FASTUIDRAWassert(good_boundary_bits(m_flags));
    }

    SubContourPoint(const fastuidraw::vec2 &pt, uint32_t f):
      fastuidraw::dvec2(pt),
      m_flags(f)
    {
      FASTUIDRAWassert(good_boundary_bits(m_flags));
    }

    uint32_t
    flags(void) const
    {
      return m_flags;
    }

    static
    enum corner_t
    corner(uint32_t b)
    {
      FASTUIDRAWassert(good_boundary_bits(b));
      switch (b & 15)
        {
        case on_min_x_boundary | on_min_y_boundary: return corner_minx_miny;
        case on_min_x_boundary | on_max_y_boundary: return corner_minx_maxy;
        case on_max_x_boundary | on_min_y_boundary: return corner_maxx_miny;
        case on_max_x_boundary | on_max_y_boundary: return corner_maxx_maxy;
        default:  return not_corner;
        }
    }

    static
    enum corner_t
    next_corner(enum corner_t c)
    {
      FASTUIDRAWassert(c < not_corner);
      return static_cast<enum corner_t>((c + 1) % not_corner);
    }

    static
    int //returns the idea of moving along the boundary, 0: not moving along, 1: moving forward, -1: moving backwar
    boundary_progress(uint32_t b0, uint32_t b1)
    {
      FASTUIDRAWassert(good_boundary_bits(b0));
      FASTUIDRAWassert(good_boundary_bits(b1));
      enum corner_t c0, c1;

      c0 = corner(b0);
      c1 = corner(b1);

      if (c0 == not_corner || c1 == not_corner)
        {
          return 0;
        }

      if (c0 == next_corner(c1))
        {
          return -1;
        }
      else if (c1 == next_corner(c0))
        {
          return 1;
        }
      else
        {
          return 0;
        }
    }

    static
    bool
    good_boundary_bits(uint32_t b)
    {
      return (on_x_boundary & b) != on_x_boundary
        && (on_y_boundary & b) != on_y_boundary;
    }

  private:
    uint32_t m_flags;
  };

  class SubPath
  {
  public:
    enum
      {
        min_x_edge_of_original_path = 1u,
        max_x_edge_of_original_path = 2u,
        min_y_edge_of_original_path = 4u,
        max_y_edge_of_original_path = 8u,

        starting_subpath = 15u,
      };

    typedef std::vector<SubContourPoint> SubContour;

    explicit
    SubPath(const fastuidraw::TessellatedPath &P);

    const std::vector<SubContour>&
    contours(void) const
    {
      return m_contours;
    }

    const fastuidraw::BoundingBox<double>&
    bounds(void) const
    {
      return m_bounds;
    }

    unsigned int
    num_points(void) const
    {
      return m_num_points;
    }

    const std::string&
    name(void) const
    {
      return m_name;
    }

    fastuidraw::vecN<SubPath*, 2>
    split(int &splitting_coordinate) const;

    uint32_t
    edge_flags(void) const
    {
      return m_edge_flags;
    }

    static
    bool
    contour_is_reducable(const SubContour &C);

  private:
    SubPath(const fastuidraw::BoundingBox<double> &bb,
            std::vector<SubContour> &contours,
            int gen, const std::string &name,
            uint32_t pedge_flags);

    int
    choose_splitting_coordinate(double &s) const;

    double
    compute_splitting_location(int coord, std::vector<double> &work_room,
                               int &number_points_before,
                               int &number_points_after) const;

    static
    void
    copy_contour(SubContour &dst,
                 const fastuidraw::TessellatedPath &src, unsigned int C);

    static
    void
    split_contour(const SubContour &src,
                  int splitting_coordinate, double spitting_value,
                  SubContour &minC, SubContour &maxC);

    static
    fastuidraw::dvec2
    compute_spit_point(fastuidraw::dvec2 a, fastuidraw::dvec2 b,
                       int splitting_coordinate, double spitting_value);

    unsigned int m_num_points;
    fastuidraw::BoundingBox<double> m_bounds;
    std::vector<SubContour> m_contours;
    int m_gen;
    uint32_t m_edge_flags;
    std::string m_name;
  };

  class PointHoard:fastuidraw::noncopyable
  {
  public:
    class ContourPoint
    {
    public:
      ContourPoint(uint32_t v, uint32_t f):
        m_vertex(v),
        m_flags(f)
      {}

      uint32_t m_vertex;
      uint32_t m_flags;
    };

    typedef std::vector<ContourPoint> Contour;
    typedef std::list<Contour> Path;

    explicit
    PointHoard(const fastuidraw::BoundingBox<double> &bounds,
               std::vector<fastuidraw::dvec2> &pts):
      m_converter(bounds.min_point(), bounds.max_point()),
      m_pts(pts)
    {
      FASTUIDRAWassert(!bounds.empty());
    }

    // takes as input the point BEFORE transformation
    unsigned int
    fetch_discretized(const fastuidraw::dvec2 &pt, uint32_t flags);

    // takes as input the point BEFORE transformation
    unsigned int
    fetch_undiscretized(const fastuidraw::dvec2 &pt);

    unsigned int
    fetch_corner(bool is_x_max, bool is_y_max);

    fastuidraw::dvec2
    apply(unsigned int I, unsigned int fudge_count) const
    {
      fastuidraw::dvec2 r(m_ipts[I]), fudge;
      double fudge_r;

      fudge_r = static_cast<double>(fudge_count) * converter().fudge_delta();
      fudge.x() = (m_ipts[I].x() >= CoordinateConverterConstants::box_dim / 2) ? -fudge_r : fudge_r;
      fudge.y() = (m_ipts[I].y() >= CoordinateConverterConstants::box_dim / 2) ? -fudge_r : fudge_r;
      r += fudge;
      return r;
    }

    int
    generate_path(const SubPath &input, Path &output);

    /* Returns the location BEFORE the transformation to the integer bounding box */
    const fastuidraw::dvec2&
    operator[](unsigned int v) const
    {
      FASTUIDRAWassert(v < m_pts.size());
      return m_pts[v];
    }

    /* Returns the location AFTER the transformation to the integer bounding box */
    const fastuidraw::ivec2&
    ipt(unsigned int v) const
    {
      FASTUIDRAWassert(v < m_ipts.size());
      return m_ipts[v];
    }

    const CoordinateConverter&
    converter(void) const
    {
      return m_converter;
    }

    bool
    edge_hugs_boundary(uint32_t valid_edges,
                       unsigned int a, unsigned int b) const;

    bool
    point_is_path_join(unsigned int v)
    {
      FASTUIDRAWassert(v < m_pt_is_path_join.size());
      FASTUIDRAWassert(m_pt_is_path_join.size() == m_pts.size());
      return m_pt_is_path_join[v];
    }

  private:
    int
    add_contour_to_path(const SubPath::SubContour &input, Path &output);

    void
    generate_contour(const SubPath::SubContour &input, std::list<ContourPoint> &output);

    int
    reduce_contour(std::vector<ContourPoint> &C);

    void
    unloop_contour(std::list<ContourPoint> &C,
                   std::vector<Contour> &output);

    CoordinateConverter m_converter;
    std::map<fastuidraw::ivec2, unsigned int> m_map;
    std::vector<fastuidraw::ivec2> m_ipts;
    std::vector<fastuidraw::dvec2> &m_pts;
    std::vector<bool> m_pt_is_path_join;
  };

  /* Trickery on winding numbers. There are two different winding numbers:
   * - the winding number the the GLU is giving us for a polygon
   * - the winding number that we record the polygon as
   * The difference is caused by that a SubPath has a winding_offset
   * that is gotten by collapsing all paths that wrap around the boundary
   * of a SubPath.
   */
  class tesser:fastuidraw::noncopyable
  {
  public:
    tesser(uint32_t edge_flags,
           PointHoard &points,
           const PointHoard::Path &P,
           int winding_offset,
           PerWindingComponentData &hoard);

    ~tesser(void);

    bool
    triangulation_failed(void)
    {
      return m_triangulation_failed;
    }

  private:
    void
    start(void);

    void
    stop(void);

    void
    add_path(const PointHoard::Path &P);

    void
    add_contour(const PointHoard::Contour &C);

    bool
    temp_verts_non_degenerate_triangle(void);

    uint64_t
    region_area(const unsigned int vertex_ids[], unsigned int count) const;

    static
    void
    begin_callBack(FASTUIDRAW_GLUenum type, int winding_number, void *tess);

    static
    void
    vertex_callBack(unsigned int vertex_data, void *tess);

    static
    void
    combine_callback(double x, double y, unsigned int data[4],
                     double weight[4],  unsigned int *outData,
                     void *tess);
    static
    void
    boundary_callback(double *x, double *y,
                      int step,
                      FASTUIDRAW_GLUboolean is_max_x,
                      FASTUIDRAW_GLUboolean is_max_y,
                      unsigned int *outData,
                      void *tess);

    static
    FASTUIDRAW_GLUboolean
    winding_callBack(int winding_number, void *tess);

    static
    void
    emitboundary_callback(int winding,
                          const unsigned int vertex_ids[],
                          unsigned int count,
                          void *tess);

    uint32_t m_edge_flags;
    unsigned int m_point_count;
    fastuidraw_GLUtesselator *m_tess;
    PointHoard &m_points;
    fastuidraw::vecN<unsigned int, 3> m_temp_verts;
    unsigned int m_temp_vert_count;
    bool m_triangulation_failed;
    int m_current_winding, m_winding_offset;
    fastuidraw::reference_counted_ptr<WindingComponentData> m_current_indices;
    PerWindingComponentData &m_hoard;
  };

  class builder:fastuidraw::noncopyable
  {
  public:
    explicit
    builder(const SubPath &P, std::vector<fastuidraw::dvec2> &pts);

    ~builder();

    void
    fill_indices(std::vector<unsigned int> &indices,
                 std::map<int, fastuidraw::c_array<const unsigned int> > &winding_map,
                 unsigned int &even_non_zero_start,
                 unsigned int &zero_start);

    bool
    triangulation_failed(void)
    {
      return m_failed;
    }

    const AAFuzz&
    aa_fuzz(int winding) const
    {
      PerWindingComponentData::const_iterator iter;

      iter = m_hoard.find(winding);
      FASTUIDRAWassert(iter != m_hoard.end());
      FASTUIDRAWassert(iter->second);
      return iter->second->m_aa_fuzz;
    }

  private:
    PerWindingComponentData m_hoard;
    PointHoard m_points;
    bool m_failed;
  };

  class AttributeDataMerger:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_ranges) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

  protected:
    AttributeDataMerger(const fastuidraw::PainterAttributeData &a,
                        const fastuidraw::PainterAttributeData &b,
                        bool common_attribute_chunking):
      m_a(a), m_b(b),
      m_common_attribute_chunking(common_attribute_chunking)
    {
    }

    virtual
    void
    post_process_attributes(unsigned int chunk,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_a,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_b) const = 0;

    virtual
    fastuidraw::range_type<int>
    compute_z_range(unsigned int chunk) const = 0;

    const fastuidraw::PainterAttributeData &m_a, &m_b;

  private:
    bool m_common_attribute_chunking;
  };

  class EdgeAttributeDataMerger:public AttributeDataMerger
  {
  public:
    EdgeAttributeDataMerger(const fastuidraw::PainterAttributeData &a,
                            const fastuidraw::PainterAttributeData &b):
      AttributeDataMerger(a, b, false)
    {}

  protected:
    virtual
    void
    post_process_attributes(unsigned int chunk,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_a,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_b) const;

    virtual
    fastuidraw::range_type<int>
    compute_z_range(unsigned int chunk) const;
  };

  class FillAttributeDataMerger:public AttributeDataMerger
  {
  public:
    FillAttributeDataMerger(const fastuidraw::PainterAttributeData &a,
                            const fastuidraw::PainterAttributeData &b):
      AttributeDataMerger(a, b, true)
    {}

  protected:
    virtual
    void
    post_process_attributes(unsigned int chunk,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_a,
                            fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_b) const
    {
      FASTUIDRAWunused(chunk);
      FASTUIDRAWunused(dst_from_a);
      FASTUIDRAWunused(dst_from_b);
    }

    virtual
    fastuidraw::range_type<int>
    compute_z_range(unsigned int chunk) const
    {
      FASTUIDRAWunused(chunk);
      FASTUIDRAWassert(false);
      return fastuidraw::range_type<int>();
    }
  };

  class AAFuzzAttributeDataFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    AAFuzzAttributeDataFiller(fastuidraw::c_array<const int> windings,
                              const std::vector<fastuidraw::dvec2> *pts,
                              const builder *b):
      m_windings(windings),
      m_pts(*pts),
      m_builder(*b)
    {}

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_ranges) const;
    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

  private:
    void //assume that vertices from before are what is used with which to bevel.
    pack_edge(const AAEdge &E,
              int z,
              fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attr,
              unsigned int &vertex_offset,
              fastuidraw::c_array<fastuidraw::PainterIndex> dst_idx,
              unsigned int &index_offset) const;

    void
    pack_attribute(fastuidraw::dvec2 position, uint32_t type,
                   fastuidraw::dvec2 normal, int z,
                   fastuidraw::PainterAttribute *dst) const;

    void
    pack_attribute(fastuidraw::dvec2 position,
                   fastuidraw::dvec2 normal,
                   fastuidraw::dvec2 normal_next,
                   int z,
                   fastuidraw::PainterAttribute *dst) const;

    fastuidraw::c_array<const int> m_windings;
    const std::vector<fastuidraw::dvec2> &m_pts;
    const builder &m_builder;
  };

  class FillAttributeDataFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    std::vector<fastuidraw::dvec2> m_points;

    /* Carefully organize indices as follows:
     * - first all elements with odd winding number
     * - then all elements with even and non-zero winding number
     * - then all element with zero winding number.
     * By doing so, the following are continuous in the array:
     * - non-zero
     * - odd-even fill rule
     * - complement of odd-even fill
     * - complement of non-zero
     */
    std::vector<unsigned int> m_indices;
    fastuidraw::c_array<const unsigned int> m_nonzero_winding_indices;
    fastuidraw::c_array<const unsigned int> m_zero_winding_indices;
    fastuidraw::c_array<const unsigned int> m_odd_winding_indices;
    fastuidraw::c_array<const unsigned int> m_even_winding_indices;

    /* m_per_fill[w] gives the indices to the triangles
     * with the winding number w. The value points into
     * indices
     */
    std::map<int, fastuidraw::c_array<const unsigned int> > m_per_fill;

    virtual
    void
    compute_sizes(unsigned int &number_attributes,
                  unsigned int &number_indices,
                  unsigned int &number_attribute_chunks,
                  unsigned int &number_index_chunks,
                  unsigned int &number_z_ranges) const;
    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
              fastuidraw::c_array<fastuidraw::PainterIndex> indices,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

    static
    fastuidraw::PainterAttribute
    generate_attribute(const fastuidraw::dvec2 &src)
    {
      fastuidraw::PainterAttribute dst;

      dst.m_attrib0 = fastuidraw::pack_vec4(src.x(), src.y(), 0.0f, 0.0f);
      dst.m_attrib1 = fastuidraw::uvec4(0u, 0u, 0u, 0u);
      dst.m_attrib2 = fastuidraw::uvec4(0u, 0u, 0u, 0u);

      return dst;
    }
  };

  class ScratchSpacePrivate
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    fastuidraw::c_array<const fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
  };

  class SubsetPrivate
  {
  public:
    ~SubsetPrivate(void);

    unsigned int
    select_subsets(ScratchSpacePrivate &scratch,
                   fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
                   const fastuidraw::float3x3 &clip_matrix_local,
                   unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   fastuidraw::c_array<unsigned int> dst);

    bool //return true if this is selected
    select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                                unsigned int max_attribute_cnt,
                                unsigned int max_index_cnt,
                                unsigned int &current);

    void
    make_ready(void);

    fastuidraw::c_array<const int>
    winding_numbers(void)
    {
      FASTUIDRAWassert(m_painter_data != nullptr);
      return fastuidraw::make_c_array(m_winding_numbers);
    }

    const fastuidraw::BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bounds_f;
    }

    const fastuidraw::Path&
    bounding_path(void) const
    {
      return m_bounding_path;
    }

    const fastuidraw::PainterAttributeData&
    painter_data(void)
    {
      FASTUIDRAWassert(m_painter_data != nullptr);
      return *m_painter_data;
    }

    const fastuidraw::PainterAttributeData&
    fuzz_painter_data(void)
    {
      FASTUIDRAWassert(m_fuzz_painter_data != nullptr);
      return *m_fuzz_painter_data;
    }

    bool
    have_children(void) const
    {
      FASTUIDRAWassert(bool(m_children[0]) == bool(m_children[1]));
      return bool(m_children[0]);
    }

    static
    SubsetPrivate*
    create_root_subset(SubPath *P, std::vector<SubsetPrivate*> &out_values);

  private:

    SubsetPrivate(SubPath *P, int max_recursion,
                  std::vector<SubsetPrivate*> &out_value);

    bool //returns true if this was added
    select_subsets_implement(ScratchSpacePrivate &scratch,
                             fastuidraw::c_array<unsigned int> dst,
                             unsigned int max_attribute_cnt,
                             unsigned int max_index_cnt,
                             unsigned int &current);

    void
    make_ready_from_children(void);

    void
    make_ready_from_sub_path(void);

    void
    ready_sizes_from_children(void);

    void
    assign_neighbor_values(SubsetPrivate *parent, int child_id);

    static
    void
    merge_winding_lists(fastuidraw::c_array<const int> inA,
                        fastuidraw::c_array<const int> inB,
                        std::vector<int> *out);

    /* m_ID represents an index into the std::vector<>
     * passed into create_hierarchy() where this element
     * is found.
     */
    unsigned int m_ID;

    /* The bounds of this SubsetPrivate used in
     * select_subsets().
     */
    fastuidraw::BoundingBox<double> m_bounds;
    fastuidraw::BoundingBox<float> m_bounds_f;
    fastuidraw::Path m_bounding_path;

    /* if this SubsetPrivate has children then
     * m_painter_data is made by "merging" the
     * data of m_painter_data from m_children[0]
     * and m_children[1]. We do this merging so
     * that we can avoid recursing if the entirity
     * of the bounding box is contained in the
     * clipping region.
     */
    fastuidraw::PainterAttributeData *m_painter_data;
    std::vector<int> m_winding_numbers;

    fastuidraw::PainterAttributeData *m_fuzz_painter_data;

    bool m_sizes_ready;
    unsigned int m_num_attributes;
    unsigned int m_largest_index_block;
    unsigned int m_aa_largest_attribute_block;
    unsigned int m_aa_largest_index_block;

    /* m_sub_path is non-nullptr only if this SubsetPrivate
     * has no children. In addition, it is set to nullptr
     * and deleted when m_painter_data is created from
     * it.
     */
    SubPath *m_sub_path;
    fastuidraw::vecN<SubsetPrivate*, 2> m_children;
    int m_splitting_coordinate;
  };

  class FilledPathPrivate
  {
  public:
    explicit
    FilledPathPrivate(const fastuidraw::TessellatedPath &P);

    ~FilledPathPrivate();

    SubsetPrivate *m_root;
    std::vector<SubsetPrivate*> m_subsets;
    fastuidraw::Rect m_bounding_box;
  };
}

////////////////////////////////////
// AAFuzz methods
void
AAFuzz::
begin_boundary(void)
{
  FASTUIDRAWassert(m_current.empty());
}

void
AAFuzz::
add_edge(unsigned int p0, unsigned int p1,
         bool edge_drawn, bool p1_is_path_join)
{
  AAEdge E;

  if (!m_current.empty())
    {
      FASTUIDRAWassert(m_current.back().m_end == p0);
      if (m_current.back().m_start == p1)
        {
          /* current edge cancels previous edge */
          m_current.pop_back();
          return;
        }
      m_current.back().m_draw_join_to_next = edge_drawn
        && m_current.back().m_draw_edge;
      m_current.back().m_next = p1;
    }

  E.m_start = p0;
  E.m_end = p1;
  E.m_draw_edge = edge_drawn;
  E.m_draw_join_as_miter = p1_is_path_join;
  E.m_is_closing_edge = false;
  m_current.push_back(E);
}

void
AAFuzz::
end_boundary(void)
{
  if (m_current.empty())
    {
      return;
    }

  FASTUIDRAWassert(m_current.back().m_end == m_current.front().m_start);
  m_current.back().m_next = m_current.front().m_end;
  m_current.back().m_is_closing_edge = true;
  m_current.back().m_draw_join_to_next = m_current.back().m_draw_edge
    && m_current.front().m_draw_edge;

  for(const AAEdge &e : m_current)
    {
      if (e.m_draw_edge)
        {
          m_edge_counts.m_attribute_count += 6;
          m_edge_counts.m_index_count += 12;
          ++m_edge_counts.m_depth_count;

          if (e.m_draw_join_to_next)
            {
              m_edge_counts.m_index_count += 3;

              /* except for the closing edge, beause
               * if the bevel shared with the start
               * point, the z-values would interpolate
               * from the close to the start which would
               * allow the bevel from the closing edge
               * to do overdraw.
               */
              if (e.m_is_closing_edge)
                {
                  m_edge_counts.m_attribute_count += 1;
                }

              if (e.m_draw_join_as_miter)
                {
                  /* Miter join requires an additional triangle
                   * and an additional point.
                   */
                  m_edge_counts.m_index_count += 3;
                  m_edge_counts.m_attribute_count += 1;
                }
            }
        }
      else
        {
          FASTUIDRAWassert(!e.m_draw_join_to_next);
        }
    }
  m_contours.push_back(Contour());
  m_contours.back().swap(m_current);
}

/////////////////////////////////////
// SubPath methods
SubPath::
SubPath(const fastuidraw::BoundingBox<double> &bb,
        std::vector<SubContour> &contours,
        int gen, const std::string &name,
        uint32_t pedge_flags):
  m_num_points(0),
  m_bounds(bb),
  m_gen(gen),
  m_edge_flags(pedge_flags),
  m_name(name)
{
  m_contours.swap(contours);
  for(const SubContour &C : m_contours)
    {
      FASTUIDRAWassert(!C.empty());
      if (!SubPath::contour_is_reducable(C))
        {
          m_num_points += C.size();
        }
    }
}

SubPath::
SubPath(const fastuidraw::TessellatedPath &P):
  m_num_points(0),
  m_bounds(P.bounding_box()),
  m_gen(0),
  m_edge_flags(starting_subpath)
{
  m_contours.reserve(P.number_contours());
  for(unsigned int c = 0, endc = P.number_contours(); c < endc; ++c)
    {
      SubContour tmp;

      copy_contour(tmp, P, c);
      if (!SubPath::contour_is_reducable(tmp))
        {
          m_contours.push_back(SubContour());
          m_contours.back().swap(tmp);
          m_num_points += m_contours.back().size();
        }
    }
}

void
SubPath::
copy_contour(SubContour &dst,
             const fastuidraw::TessellatedPath &src, unsigned int C)
{
  for(unsigned int e = 0, ende = src.number_edges(C); e < ende; ++e)
    {
      fastuidraw::range_type<unsigned int> R;
      uint32_t flags;

      R = src.edge_range(C, e);
      flags = (src.edge_type(C, e) == fastuidraw::PathEnums::starts_new_edge) ?
        uint32_t(SubContourPoint::is_path_join) :
        0u;

      FASTUIDRAWassert(src.segment_data()[R.m_begin].m_type == fastuidraw::TessellatedPath::line_segment);
      dst.push_back(SubContourPoint(src.segment_data()[R.m_begin].m_start_pt, flags));
      for(unsigned int v = R.m_begin + 1; v < R.m_end; ++v)
        {
          FASTUIDRAWassert(src.segment_data()[v].m_type == fastuidraw::TessellatedPath::line_segment);
          SubContourPoint pt(src.segment_data()[v].m_start_pt, 0u);
          dst.push_back(pt);
        }
    }

  if (!src.contour_closed(C) && src.number_edges(C) > 0)
    {
      /* If the contour is not closed, then the last edge's
       * ending point needs to be added because it is not
       * the start point of the contour.
       */
      SubContourPoint pt(src.contour_segment_data(C).back().m_end_pt, 0u);
      dst.push_back(pt);
    }
}

double
SubPath::
compute_splitting_location(int coord, std::vector<double> &work_room,
                           int &number_points_before,
                           int &number_points_after) const
{
  double return_value;

  work_room.clear();
  for(const SubContour &C : m_contours)
    {
      for(const SubContourPoint &P : C)
        {
          /* don't push a value if it is the same
           * as the previous value.
           */
          if (work_room.empty() || work_room.back() != P[coord])
            {
              work_room.push_back(P[coord]);
            }
        }
    }

  std::sort(work_room.begin(), work_room.end());

  /* choose a value that is strictly between two
   * points so that no vertical or horizontal
   * edge from the original path hugs the partition
   * edge.
   */
  unsigned int idx;
  double next_p, prev_p, p;

  idx = work_room.size() / 2;
  p = work_room[idx];
  next_p = (idx + 1u < work_room.size()) ? work_room[idx + 1u] : p;
  prev_p = (idx > 0u) ? work_room[idx - 1u] : p;

  if (fastuidraw::t_abs(next_p - p) >  fastuidraw::t_abs(prev_p - p))
    {
      return_value = 0.5 * (next_p + p);
    }
  else
    {
      return_value = 0.5 * (prev_p + p);
    }

  number_points_before = 0;
  number_points_after = 0;
  for(const SubContour &C : m_contours)
    {
      /* use iterator interface because we need
       * to access the elements in order
       */
      double prev_pt(C.back()[coord]);
      for(const auto &q : C)
        {
          bool prev_b, b;
          double pt(q[coord]);

          prev_b = prev_pt < return_value;
          b = pt < return_value;

          if (b || pt == return_value)
            {
              ++number_points_before;
            }

          if (!b || pt == return_value)
            {
              ++number_points_after;
            }

          if (prev_pt != return_value && prev_b != b)
            {
              ++number_points_before;
              ++number_points_after;
            }

          prev_pt = pt;
        }
    }

  return return_value;
}

int
SubPath::
choose_splitting_coordinate(double &s) const
{
  /* do not allow the box to be too far from being a square.
   * TODO: if the balance of points heavily favors the other
   * side, we should ignore the size_max_ratio. Perhaps a
   * wieght factor between the different in # of points
   * of the sides and the ratio?
   */
  fastuidraw::dvec2 mid_pt;
  mid_pt = 0.5 * (m_bounds.max_point() + m_bounds.min_point());

  if (SubsetConstants::size_max_ratio > 0.0f)
    {
      fastuidraw::dvec2 wh;
      wh = m_bounds.max_point() - m_bounds.min_point();
      if (wh.x() >= SubsetConstants::size_max_ratio * wh.y())
        {
          s = mid_pt[0];
          return 0;
        }
      else if (wh.y() >= SubsetConstants::size_max_ratio * wh.x())
        {
          s = mid_pt[1];
          return 1;
        }
    }

  std::vector<double> work_room;
  fastuidraw::ivec2 number_points_before, number_points_after;
  fastuidraw::ivec2 number_points;
  for (int c = 0; c < 2; ++c)
    {
      mid_pt[c] = compute_splitting_location(c, work_room, number_points_before[c], number_points_after[c]);
    }

  /* choose a splitting that: minimizes
   * number_points_before[i] + number_points_after[i]
   */
  number_points = number_points_before + number_points_after;
  if (number_points.x() < number_points.y())
    {
      s = mid_pt[0];
      return 0;
    }
  else
    {
      s = mid_pt[1];
      return 1;
    }
}

fastuidraw::dvec2
SubPath::
compute_spit_point(const fastuidraw::dvec2 a, fastuidraw::dvec2 b,
                   int splitting_coordinate, double splitting_value)
{
  double t, n, d, aa, bb;
  fastuidraw::dvec2 return_value;

  n = splitting_value - a[splitting_coordinate];
  d = b[splitting_coordinate] - a[splitting_coordinate];
  t = n / d;

  return_value[splitting_coordinate] = splitting_value;

  aa = a[1 - splitting_coordinate];
  bb = b[1 - splitting_coordinate];
  return_value[1 - splitting_coordinate] = (1.0 - t) * aa + t * bb;

  return return_value;
}

void
SubPath::
split_contour(const SubContour &src,
              int splitting_coordinate, double splitting_value,
              SubContour &C0, SubContour &C1)
{
  SubContourPoint prev_pt(src.back());
  for(const SubContourPoint &pt : src)
    {
      bool b0, prev_b0;
      bool b1, prev_b1;
      fastuidraw::dvec2 split_pt;

      prev_b0 = prev_pt[splitting_coordinate] <= splitting_value;
      b0 = pt[splitting_coordinate] <= splitting_value;

      prev_b1 = prev_pt[splitting_coordinate] >= splitting_value;
      b1 = pt[splitting_coordinate] >= splitting_value;

      if (prev_b0 != b0 || prev_b1 != b1)
        {
          split_pt = compute_spit_point(prev_pt, pt,
                                        splitting_coordinate, splitting_value);
        }

      if (prev_b0 != b0)
        {
          uint32_t new_flag, remove_flag, flags;

          /* we get the new flag coming from the splitting coordinate,
           * and we also inherit the logic AND of the points of the split
           * (i.e. if both are foo, then the new pt is also foo)
           */
          new_flag = (splitting_coordinate == 0) ?
            SubContourPoint::on_max_x_boundary :
            SubContourPoint::on_max_y_boundary;

          remove_flag = (splitting_coordinate == 0) ?
            SubContourPoint::on_min_x_boundary :
            SubContourPoint::on_min_y_boundary;

          flags = new_flag | (~remove_flag & pt.flags() & prev_pt.flags());
          flags &= ~SubContourPoint::is_path_join;

          C0.push_back(SubContourPoint(split_pt, flags));
          FASTUIDRAWassert((C0.back().flags() & SubContourPoint::is_path_join) == 0u);
        }

      if (b0)
        {
          C0.push_back(pt);
        }

      if (prev_b1 != b1)
        {
          uint32_t new_flag, remove_flag, flags;

          /* we get the new flag coming from the splitting coordinate,
           * and we also inherit the logic AND of the points of the split
           * (i.e. if both are foo, then the new pt is also foo)
           */
          new_flag = (splitting_coordinate == 0) ?
            SubContourPoint::on_min_x_boundary :
            SubContourPoint::on_min_y_boundary;

          remove_flag = (splitting_coordinate == 0) ?
            SubContourPoint::on_max_x_boundary :
            SubContourPoint::on_max_y_boundary;

          flags = new_flag | (~remove_flag & pt.flags() & prev_pt.flags());
          flags &= ~SubContourPoint::is_path_join;

          C1.push_back(SubContourPoint(split_pt, flags));
          FASTUIDRAWassert((C1.back().flags() & SubContourPoint::is_path_join) == 0u);
        }

      if (b1)
        {
          C1.push_back(pt);
        }

      prev_pt = pt;
    }
}

fastuidraw::vecN<SubPath*, 2>
SubPath::
split(int &splitting_coordinate) const
{
  fastuidraw::vecN<SubPath*, 2> return_value(nullptr, nullptr);
  double mid_pt;

  splitting_coordinate = choose_splitting_coordinate(mid_pt);

  /* now split each contour. */
  fastuidraw::dvec2 B0_max, B1_min;
  B0_max[1 - splitting_coordinate] = m_bounds.max_point()[1 - splitting_coordinate];
  B0_max[splitting_coordinate] = mid_pt;

  B1_min[1 - splitting_coordinate] = m_bounds.min_point()[1 - splitting_coordinate];
  B1_min[splitting_coordinate] = mid_pt;

  fastuidraw::BoundingBox<double> B0(m_bounds.min_point(), B0_max);
  fastuidraw::BoundingBox<double> B1(B1_min, m_bounds.max_point());
  std::vector<SubContour> C0, C1;
  uint32_t flags0(m_edge_flags), flags1(m_edge_flags);

  if (splitting_coordinate == 0)
    {
      flags0 &= ~max_x_edge_of_original_path;
      flags1 &= ~min_x_edge_of_original_path;
    }
  else
    {
      flags0 &= ~max_y_edge_of_original_path;
      flags1 &= ~min_y_edge_of_original_path;
    }

  C0.reserve(m_contours.size());
  C1.reserve(m_contours.size());
  for(const SubContour &S : m_contours)
    {
      C0.push_back(SubContour());
      C1.push_back(SubContour());
      split_contour(S, splitting_coordinate, mid_pt,
                    C0.back(), C1.back());

      if (C0.back().empty())
        {
          C0.pop_back();
        }

      if (C1.back().empty())
        {
          C1.pop_back();
        }
    }

  return_value[0] = FASTUIDRAWnew SubPath(B0, C0, m_gen + 1, m_name + "0", flags0);
  return_value[1] = FASTUIDRAWnew SubPath(B1, C1, m_gen + 1, m_name + "1", flags1);

  return return_value;
}

bool
SubPath::
contour_is_reducable(const SubContour &C)
{
  uint32_t prev(C.back().flags());
  int sum(0);

  for(const auto &q : C)
    {
      int r;

      r = SubContourPoint::boundary_progress(prev, q.flags());
      if (r == 0)
        {
          return false;
        }
      sum += r;
    }
  return sum == 0;
}

//////////////////////////////////////
// PointHoard methods
unsigned int
PointHoard::
fetch_discretized(const fastuidraw::dvec2 &pt, uint32_t flags)
{
  std::map<fastuidraw::ivec2, unsigned int>::iterator iter;
  fastuidraw::ivec2 ipt;
  unsigned int return_value;

  FASTUIDRAWassert(m_pts.size() == m_ipts.size());

  ipt = m_converter.iapply(pt);

  if (flags & SubContourPoint::on_min_x_boundary)
    {
      ipt.x() = 1;
      FASTUIDRAWassert(0 == (flags & SubContourPoint::on_max_x_boundary));
    }

  if (flags & SubContourPoint::on_max_x_boundary)
    {
      ipt.x() = CoordinateConverterConstants::box_dim + 1;
      FASTUIDRAWassert(0 == (flags & SubContourPoint::on_min_x_boundary));
    }

  if (flags & SubContourPoint::on_min_y_boundary)
    {
      ipt.y() = 1;
      FASTUIDRAWassert(0 == (flags & SubContourPoint::on_max_y_boundary));
    }

  if (flags & SubContourPoint::on_max_y_boundary)
    {
      ipt.y() = CoordinateConverterConstants::box_dim + 1;
      FASTUIDRAWassert(0 == (flags & SubContourPoint::on_min_y_boundary));
    }

  iter = m_map.find(ipt);
  if (iter != m_map.end())
    {
      return_value = iter->second;
    }
  else
    {
      return_value = m_pts.size();
      m_pts.push_back(pt);
      m_ipts.push_back(ipt);
      m_pt_is_path_join.push_back(flags & SubContourPoint::is_path_join);
      m_map[ipt] = return_value;
    }
  return return_value;
}

unsigned int
PointHoard::
fetch_undiscretized(const fastuidraw::dvec2 &pt)
{
  unsigned int return_value(m_pts.size());

  m_ipts.push_back(m_converter.iapply(pt));
  m_pts.push_back(pt);
  m_pt_is_path_join.push_back(false);

  return return_value;
}

unsigned int
PointHoard::
fetch_corner(bool is_max_x, bool is_max_y)
{
  fastuidraw::ivec2 ipt(1, 1);
  fastuidraw::dvec2 P(m_converter.bounds().min_point());
  unsigned int return_value;

  if (is_max_x)
    {
      ipt.x() = CoordinateConverterConstants::box_dim + 1;
      P.x() = m_converter.bounds().max_point().x();
    }

  if (is_max_y)
    {
      ipt.y() = CoordinateConverterConstants::box_dim + 1;
      P.y() = m_converter.bounds().max_point().y();
    }

  std::map<fastuidraw::ivec2, unsigned int>::iterator iter;
  iter = m_map.find(ipt);
  if (iter != m_map.end())
    {
      return_value = iter->second;
    }
  else
    {
      return_value = m_pts.size();
      m_pts.push_back(P);
      m_ipts.push_back(ipt);
      m_pt_is_path_join.push_back(false);
      m_map[ipt] = return_value;
    }

  return return_value;
}

bool
PointHoard::
edge_hugs_boundary(uint32_t valid_edges,
                   unsigned int a, unsigned int b) const
{
  fastuidraw::ivec2 pA(m_ipts[a]), pB(m_ipts[b]);
  const fastuidraw::vecN<uint32_t, 2> min_edges(SubPath::min_x_edge_of_original_path,
                                                SubPath::min_y_edge_of_original_path);
  const fastuidraw::vecN<uint32_t, 2> max_edges(SubPath::max_x_edge_of_original_path,
                                                SubPath::max_y_edge_of_original_path);
  const int slack(1);

  for(int coord = 0; coord < 2; ++coord)
    {
      if(pA[coord] <= slack && pB[coord] <= slack
         && (valid_edges & min_edges[coord]) == 0u)
        {
          return true;
        }

      if(pA[coord] >= CoordinateConverterConstants::box_dim - slack
         && pB[coord] >= CoordinateConverterConstants::box_dim - slack
         && (valid_edges & max_edges[coord]) == 0u)
        {
          return true;
        }
    }

  return false;
}

int
PointHoard::
generate_path(const SubPath &input, Path &output)
{
  int return_value(0);

  FASTUIDRAWassert(output.empty());
  const std::vector<SubPath::SubContour> &contours(input.contours());
  for(const SubPath::SubContour &C : contours)
    {
      return_value += add_contour_to_path(C, output);
    }

  return return_value;
}

void
PointHoard::
generate_contour(const SubPath::SubContour &C, std::list<ContourPoint> &output)
{
  FASTUIDRAWassert(!C.empty());
  FASTUIDRAWassert(output.empty());

  int sz(0);
  for(const auto &q : C)
    {
      unsigned int I;

      I = fetch_discretized(q, q.flags());
      /* remove any edges that are after snapping the same point*/
      if (output.empty() || I != output.back().m_vertex)
        {
          output.push_back(ContourPoint(I, q.flags()));
          ++sz;
        }
    }

  while(!output.empty() && output.back().m_vertex == output.front().m_vertex)
    {
      output.pop_back();
      --sz;
    }

  if (sz < 3)
    {
      output.clear();
    }
}

int
PointHoard::
add_contour_to_path(const SubPath::SubContour &C, Path &path_output)
{
  if (C.empty())
    {
      return 0;
    }

  int w(0);
  std::list<ContourPoint> tmp;
  std::vector<Contour> tmp_unlooped;

  generate_contour(C, tmp);
  unloop_contour(tmp, tmp_unlooped);
  for(Contour &v : tmp_unlooped)
    {
      w += reduce_contour(v);
      if (!v.empty())
        {
          path_output.push_back(Contour());
          path_output.back().swap(v);
        }
    }

  return w;
}

int
PointHoard::
reduce_contour(Contour &C)
{
  /* We already have that loops are removed from C, so
   * C can only be reduced if all edges are boundary edges.
   */
  if (C.size() <= 2)
    {
      /* a contour or 2 or fewer points, either has
       * no edges or 2 edges that cancel each other.
       */
      C.clear();
      return 0;
    }

  uint32_t prev(C.back().m_flags);
  int bcount(0);

  for(const auto &q : C)
    {
      int r;

      r = SubContourPoint::boundary_progress(prev, q.m_flags);
      if (r == 0)
        {
          return 0;
        }

      bcount += r;
      prev = q.m_flags;
    }

  C.clear();
  FASTUIDRAWassert(bcount % 4 == 0);
  return -bcount / 4;
}

void
PointHoard::
unloop_contour(std::list<ContourPoint> &C, std::vector<Contour> &output)
{
  /* GLU falls appart if it is passed a contour that has
   * a loop within it; thus we need to identify loops within
   * C and realize those as seperate contours.
   */
  if (C.empty())
    {
      return;
    }

  for(auto i = C.begin(); i != C.end(); ++i)
    {
      unsigned int looking_for(i->m_vertex);
      auto next_i(i);
      ++next_i;

      for(auto j = next_i; j != C.end(); ++j)
        {
          if (looking_for == j->m_vertex)
            {
              /* the elements [i, j) form a loop which
               * has no loops itself (otherwise we would
               * have an earlier j; this we can add directly
               * to output.
               */
              output.push_back(Contour(i, j));

              /* now remove [i, j) from C since it is a distinct
               * loop added to output.
               */
              while(i != j)
                {
                  auto erase_iter(i);
                  ++i;

                  C.erase(erase_iter);
                }
            }
        }
    }

  if (!C.empty())
    {
      output.push_back(Contour(C.begin(), C.end()));
    }
}

////////////////////////////////////////
// tesser methods
tesser::
tesser(uint32_t edge_flags,
       PointHoard &points,
       const PointHoard::Path &P,
       int winding_offset,
       PerWindingComponentData &hoard):
  m_edge_flags(edge_flags),
  m_point_count(0),
  m_points(points),
  m_triangulation_failed(false),
  m_current_winding(0),
  m_winding_offset(winding_offset),
  m_hoard(hoard)
{
  m_tess = fastuidraw_gluNewTess;
  fastuidraw_gluTessCallbackBegin(m_tess, &begin_callBack);
  fastuidraw_gluTessCallbackVertex(m_tess, &vertex_callBack);
  fastuidraw_gluTessCallbackCombine(m_tess, &combine_callback);
  fastuidraw_gluTessCallbackFillRule(m_tess, &winding_callBack);
  fastuidraw_gluTessCallbackBoundaryCornerPoint(m_tess, &boundary_callback);
  fastuidraw_gluTessCallbackEmitBoundary(m_tess, emitboundary_callback);

  start();
  add_path(P);
  stop();
}

tesser::
~tesser(void)
{
  fastuidraw_gluDeleteTess(m_tess);
}

void
tesser::
start(void)
{
  fastuidraw_gluTessBeginPolygon(m_tess, this);
}

void
tesser::
stop(void)
{
  fastuidraw_gluTessEndPolygon(m_tess);
}

void
tesser::
add_path(const PointHoard::Path &P)
{
  for(const auto &q : P)
    {
      add_contour(q);
    }
}

void
tesser::
add_contour(const PointHoard::Contour &C)
{
  FASTUIDRAWassert(!C.empty());

  fastuidraw_gluTessBeginContour(m_tess, FASTUIDRAW_GLU_TRUE);
  for(PointHoard::ContourPoint I : C)
    {
      fastuidraw::vecN<double, 2> p;

      /* TODO: Incrementing the amount by which to apply
       * fudge is not the correct thing to do. Rather, we
       * should only increment and apply fudge on overlapping
       * and degenerate edges.
       */
      p = m_points.apply(I.m_vertex, m_point_count);
      ++m_point_count;

      fastuidraw_gluTessVertex(m_tess, p.x(), p.y(), I.m_vertex);
    }
  fastuidraw_gluTessEndContour(m_tess);
}

bool
tesser::
temp_verts_non_degenerate_triangle(void)
{
  if (m_temp_verts[0] == m_temp_verts[1]
     || m_temp_verts[0] == m_temp_verts[2]
     || m_temp_verts[1] == m_temp_verts[2])
    {
      return false;
    }

  uint64_t twice_area;
  fastuidraw::i64vec2 p0(m_points.ipt(m_temp_verts[0]));
  fastuidraw::i64vec2 p1(m_points.ipt(m_temp_verts[1]));
  fastuidraw::i64vec2 p2(m_points.ipt(m_temp_verts[2]));
  fastuidraw::i64vec2 v(p1 - p0), w(p2 - p0);

  twice_area = fastuidraw::t_abs(v.x() * w.y() - v.y() * w.x());
  if (twice_area == 0)
    {
      return false;
    }

  fastuidraw::i64vec2 u(p2 - p1);
  double vmag, wmag, umag, two_area(twice_area);
  const double min_height(CoordinateConverterConstants::min_height);

  vmag = fastuidraw::t_sqrt(static_cast<double>(dot(v, v)));
  wmag = fastuidraw::t_sqrt(static_cast<double>(dot(w, w)));
  umag = fastuidraw::t_sqrt(static_cast<double>(dot(u, u)));

  /* the distance from an edge to the 3rd
   * point is given as twice the area divided
   * by the length of the edge. We ask that
   * the distance is atleast 1.
   */
  if (two_area < min_height * vmag
      || two_area < min_height * wmag
      || two_area < min_height * umag)
    {
      twice_area = 0u;
      return false;
    }

  return true;
}

uint64_t
tesser::
region_area(const unsigned int vertex_ids[], unsigned int count) const
{
  if (count == 0)
    {
      return 0u;
    }

  /* Use the Surveyor's formula in integer arithmetic
   * to decide if the region whose boundar is passed
   * has area; to keep the numbers smaller center the
   * computation around the first point of the polygon.
   */
  fastuidraw::ivec2 origin(m_points.ipt(vertex_ids[0]));
  int64_t twice_signed_area(0);
  for (unsigned int i = 0; i < count; ++i)
    {
      unsigned int next_i;

      next_i = (i + 1u == count) ? 0u: i + 1u;

      fastuidraw::vecN<int64_t, 2> a(m_points.ipt(vertex_ids[i]) - origin);
      fastuidraw::vecN<int64_t, 2> b(m_points.ipt(vertex_ids[next_i]) - origin);
      twice_signed_area += a.x() * b.y() - b.x() * a.y();
    }

  return uint64_t(fastuidraw::t_abs(twice_signed_area)) >> 1u;
}

void
tesser::
begin_callBack(FASTUIDRAW_GLUenum type, int glu_tess_winding_number, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);
  FASTUIDRAWassert(FASTUIDRAW_GLU_TRIANGLES == type);
  FASTUIDRAWunused(type);

  p->m_temp_vert_count = 0;
  p->m_current_winding = glu_tess_winding_number + p->m_winding_offset;

  fastuidraw::reference_counted_ptr<WindingComponentData> &h(p->m_hoard[p->m_current_winding]);
  if (!h)
    {
      h = FASTUIDRAWnew WindingComponentData();
    }
  p->m_current_indices = h;
}

void
tesser::
vertex_callBack(unsigned int vertex_id, void *tess)
{
  tesser *p;
  p = static_cast<tesser*>(tess);

  if (vertex_id == FASTUIDRAW_GLU_nullptr_CLIENT_ID)
    {
      p->m_triangulation_failed = true;
    }

  /* Cache adds vertices in groups of 3 (triangles),
   * then if all vertices are NOT FASTUIDRAW_GLU_nullptr_CLIENT_ID,
   * then add them.
   */
  p->m_temp_verts[p->m_temp_vert_count] = vertex_id;
  p->m_temp_vert_count++;
  if (p->m_temp_vert_count == 3)
    {
      p->m_temp_vert_count = 0;
      /*
       * if vertex_id is FASTUIDRAW_GLU_nullptr_CLIENT_ID, that means
       * the triangle is junked.
       */
      if (p->m_temp_verts[0] != FASTUIDRAW_GLU_nullptr_CLIENT_ID
          && p->m_temp_verts[1] != FASTUIDRAW_GLU_nullptr_CLIENT_ID
          && p->m_temp_verts[2] != FASTUIDRAW_GLU_nullptr_CLIENT_ID
          && p->temp_verts_non_degenerate_triangle())
        {
          p->m_current_indices->m_triangles.add_index(p->m_temp_verts[0]);
          p->m_current_indices->m_triangles.add_index(p->m_temp_verts[1]);
          p->m_current_indices->m_triangles.add_index(p->m_temp_verts[2]);
        }
    }
}

void
tesser::
combine_callback(double x, double y, unsigned int data[4],
                 double weight[4],  unsigned int *outData,
                 void *tess)
{
  tesser *p;
  fastuidraw::dvec2 pt;
  bool use_sum(true);

  for(unsigned int i = 0; use_sum && i < 4; ++i)
    {
      use_sum = (data[i] != FASTUIDRAW_GLU_nullptr_CLIENT_ID);
    }

  p = static_cast<tesser*>(tess);
  if (use_sum)
    {
      pt.x() = pt.y() = 0.0;
      for(unsigned int i = 0; i < 4; ++i)
        {
          FASTUIDRAWassert(data[i] != FASTUIDRAW_GLU_nullptr_CLIENT_ID);
          pt += weight[i] * p->m_points[data[i]];
        }
    }
  else
    {
      pt = p->m_points.converter().unapply(fastuidraw::dvec2(x, y));
    }
  *outData = p->m_points.fetch_undiscretized(pt);
}

void
tesser::
boundary_callback(double *x, double *y,
                  int istep,
                  FASTUIDRAW_GLUboolean is_max_x,
                  FASTUIDRAW_GLUboolean is_max_y,
                  unsigned int *outData,
                  void *tess)
{
  tesser *p;
  unsigned int idx;
  fastuidraw::ivec2 ipt;

  p = static_cast<tesser*>(tess);
  idx = p->m_points.fetch_corner(is_max_x, is_max_y);
  ipt = p->m_points.ipt(idx);
  *x = ipt.x();
  *y = ipt.y();

  if (outData)
    {
      *outData = idx;
      FASTUIDRAWassert(istep == 0);
    }
  else
    {
      double step;

      step = static_cast<double>(istep) * p->m_points.converter().fudge_delta();
      if (is_max_x)
        {
          *x += step;
        }
      else
        {
          *x -= step;
        }

      if (is_max_y)
        {
          *y += step;
        }
      else
        {
          *y -= step;
        }
    }
}

FASTUIDRAW_GLUboolean
tesser::
winding_callBack(int winding_number, void *tess)
{
  FASTUIDRAWunused(winding_number);
  FASTUIDRAWunused(tess);
  return FASTUIDRAW_GLU_TRUE;
}

void
tesser::
emitboundary_callback(int glu_tess_winding,
                      const unsigned int vertex_ids[],
                      unsigned int count,
                      void *tess)
{
  tesser *p(static_cast<tesser*>(tess));
  uint64_t area;

  area = p->region_area(vertex_ids, count);
  if (area == 0u)
    {
      return;
    }

  int winding(p->m_winding_offset + glu_tess_winding);
  fastuidraw::reference_counted_ptr<WindingComponentData> &h(p->m_hoard[winding]);
  if (!h)
    {
      h = FASTUIDRAWnew WindingComponentData();
    }

  h->m_aa_fuzz.begin_boundary();
  for(unsigned int i = 0; i < count; ++i)
    {
      unsigned int va, vb;
      unsigned int next_i;
      bool draw_edge;
      bool vb_is_path_join;

      next_i = (i + 1u == count) ? 0u: i + 1u;
      va = vertex_ids[i];
      vb = vertex_ids[next_i];

      draw_edge = !p->m_points.edge_hugs_boundary(p->m_edge_flags, va, vb);
      vb_is_path_join = p->m_points.point_is_path_join(vb);
      h->m_aa_fuzz.add_edge(va, vb, draw_edge, vb_is_path_join);
    }
  h->m_aa_fuzz.end_boundary();
}

/////////////////////////////////////////
// builder methods
builder::
builder(const SubPath &P, std::vector<fastuidraw::dvec2> &points):
  m_points(P.bounds(), points)
{
  PointHoard::Path path;
  int winding_offset;

  winding_offset = m_points.generate_path(P, path);
  tesser T(P.edge_flags(), m_points, path, winding_offset, m_hoard);

  m_failed = T.triangulation_failed();

  for (auto iter = m_hoard.begin(); iter != m_hoard.end(); )
    {
      auto prev_iter(iter);
      ++iter;

      if (prev_iter->second->m_triangles.empty())
        {
          m_hoard.erase(prev_iter);
        }
    }

  if (m_hoard.empty())
    {
      fastuidraw::reference_counted_ptr<WindingComponentData> &zero(m_hoard[winding_offset]);
      zero = FASTUIDRAWnew WindingComponentData();

      zero->m_triangles.add_index(m_points.fetch_corner(true, true));
      zero->m_triangles.add_index(m_points.fetch_corner(true, false));
      zero->m_triangles.add_index(m_points.fetch_corner(false, false));

      zero->m_triangles.add_index(m_points.fetch_corner(true, true));
      zero->m_triangles.add_index(m_points.fetch_corner(false, false));
      zero->m_triangles.add_index(m_points.fetch_corner(false, true));
    }
}

builder::
~builder()
{
}

void
builder::
fill_indices(std::vector<unsigned int> &indices,
             std::map<int, fastuidraw::c_array<const unsigned int> > &winding_map,
             unsigned int &even_non_zero_start,
             unsigned int &zero_start)
{
  PerWindingComponentData::iterator iter, end;
  unsigned int total(0), num_odd(0), num_even_non_zero(0), num_zero(0);

  /* compute number indices needed */
  for(const auto &element : m_hoard)
    {
      TriangleList &tri(element.second->m_triangles);
      int winding(element.first);
      unsigned int cnt(tri.count());

      total += cnt;
      if (winding == 0)
        {
          num_zero += cnt;
        }
      else if (is_even(winding))
        {
          num_even_non_zero += cnt;
        }
      else
        {
          num_odd += cnt;
        }
    }

  /* pack as follows:
   *  - odd
   *  - even non-zero
   *  - zero
   */
  unsigned int current_odd(0), current_even_non_zero(num_odd);
  unsigned int current_zero(num_even_non_zero + num_odd);

  indices.resize(total);
  for(const auto &element : m_hoard)
    {
      TriangleList &tri(element.second->m_triangles);
      int winding(element.first);

      if (winding == 0)
        {
          if (tri.count() > 0)
            {
              tri.fill_at(current_zero,
                          fastuidraw::make_c_array(indices),
                          winding_map[winding]);
            }
        }
      else if (is_even(winding))
        {
          if (tri.count() > 0)
            {
              tri.fill_at(current_even_non_zero,
                          fastuidraw::make_c_array(indices),
                          winding_map[winding]);
            }
        }
      else
        {
          if (tri.count() > 0)
            {
              tri.fill_at(current_odd,
                          fastuidraw::make_c_array(indices),
                          winding_map[winding]);
            }
        }
    }

  FASTUIDRAWassert(current_zero == total);
  FASTUIDRAWassert(current_odd == num_odd);
  FASTUIDRAWassert(current_even_non_zero == current_odd + num_even_non_zero);

  even_non_zero_start = num_odd;
  zero_start = current_odd + num_even_non_zero;

}

////////////////////////////////
// AttributeDataMerger methods
void
AttributeDataMerger::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_ranges) const
{
  number_attribute_chunks = fastuidraw::t_max(m_a.attribute_data_chunks().size(),
                                              m_b.attribute_data_chunks().size());
  number_attributes = 0;
  for(unsigned int c = 0; c < number_attribute_chunks; ++c)
    {
      unsigned int a_sz, b_sz;

      a_sz = m_a.attribute_data_chunk(c).size();
      b_sz = m_b.attribute_data_chunk(c).size();
      number_attributes += (a_sz + b_sz);
    }

  number_index_chunks = fastuidraw::t_max(m_a.index_data_chunks().size(),
                                          m_b.index_data_chunks().size());
  number_indices = 0;
  for(unsigned int c = 0; c < number_index_chunks; ++c)
    {
      unsigned int a_sz, b_sz;

      a_sz = m_a.index_data_chunk(c).size();
      b_sz = m_b.index_data_chunk(c).size();
      number_indices += (a_sz + b_sz);
    }

  number_z_ranges = fastuidraw::t_max(m_a.z_ranges().size(),
                                      m_b.z_ranges().size());
}


void
AttributeDataMerger::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> indices,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  FASTUIDRAWunused(zranges);

  for(unsigned int i = 0, dst_offset = 0; i < attrib_chunks.size(); ++i)
    {
      fastuidraw::c_array<fastuidraw::PainterAttribute> dst_a, dst_b;
      fastuidraw::c_array<const fastuidraw::PainterAttribute> src;
      unsigned int start(dst_offset), size(0);

      src = m_a.attribute_data_chunk(i);
      dst_a = attributes.sub_array(dst_offset, src.size());
      dst_offset += dst_a.size();
      size += dst_a.size();
      std::copy(src.begin(), src.end(), dst_a.begin());

      src = m_b.attribute_data_chunk(i);
      dst_b = attributes.sub_array(dst_offset, src.size());
      dst_offset += dst_b.size();
      size += dst_b.size();
      std::copy(src.begin(), src.end(), dst_b.begin());

      post_process_attributes(i, dst_a, dst_b);
      attrib_chunks[i] = attributes.sub_array(start, size);
    }

  /* copy indices is trickier; we need to copy with correct chunking
   * AND adjust the values for the indices coming from m_b (because
   * m_b attributes are placed after m_a attributes).
   */
  for(unsigned int i = 0, dst_offset = 0; i < index_chunks.size(); ++i)
    {
      fastuidraw::c_array<fastuidraw::PainterIndex> dst;
      fastuidraw::c_array<const fastuidraw::PainterIndex> src;
      unsigned int start(dst_offset), size(0);

      index_adjusts[i] = 0;

      src = m_a.index_data_chunk(i);
      if (!src.empty())
        {
          dst = indices.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();
          std::copy(src.begin(), src.end(), dst.begin());
        }

      src = m_b.index_data_chunk(i);
      if (!src.empty())
        {
          unsigned int adjust_chunk, adjust;

          dst = indices.sub_array(dst_offset, src.size());
          dst_offset += dst.size();
          size += dst.size();

          adjust_chunk = (m_common_attribute_chunking) ? 0 : i;
          adjust = m_a.attribute_data_chunk(adjust_chunk).size();
          for(unsigned int k = 0; k < src.size(); ++k)
            {
              dst[k] = src[k] + adjust;
            }
        }
      index_chunks[i] = indices.sub_array(start, size);
    }

  for (unsigned int i = 0; i < zranges.size(); ++i)
    {
      zranges[i] = compute_z_range(i);
    }
}

///////////////////////////////////
// EdgeAttributeDataMerger methods
void
EdgeAttributeDataMerger::
post_process_attributes(unsigned int chunk,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_a,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> dst_from_b) const
{
  FASTUIDRAWunused(dst_from_b);

  /* the order of drawing is a then b, thus
   * we want to increment the elements of a
   * so they are on top of all elements of b
   */
  int add_z(m_b.z_range(chunk).m_end);
  for(fastuidraw::PainterAttribute &A : dst_from_a)
    {
      A.m_attrib1.y() += add_z;
    }
}

fastuidraw::range_type<int>
EdgeAttributeDataMerger::
compute_z_range(unsigned int chunk) const
{
  fastuidraw::range_type<int> R;

  FASTUIDRAWassert(m_a.z_range(chunk).m_begin == 0);
  FASTUIDRAWassert(m_b.z_range(chunk).m_begin == 0);
  R.m_begin = 0;
  R.m_end = m_a.z_range(chunk).m_end + m_b.z_range(chunk).m_end;
  return R;
}

////////////////////////////////////
// AAFuzzAttributeDataFiller methods
void
AAFuzzAttributeDataFiller::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_ranges) const
{
  unsigned int a, f, b;

  f = signed_to_unsigned(m_windings.front());
  b = signed_to_unsigned(m_windings.back());
  a = fastuidraw::t_max(f, b);
  number_z_ranges = number_attribute_chunks = number_index_chunks = a + 1;

  number_attributes = 0;
  number_indices = 0;
  for(int w : m_windings)
    {
      const AAFuzz &aa_fuzz(m_builder.aa_fuzz(w));

      number_attributes += aa_fuzz.edge_counts().m_attribute_count;
      number_indices += aa_fuzz.edge_counts().m_index_count;
    }
}

void
AAFuzzAttributeDataFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> indices,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  FASTUIDRAWassert(attrib_chunks.size() == zranges.size());
  FASTUIDRAWassert(attrib_chunks.size() == index_chunks.size());
  FASTUIDRAWassert(attrib_chunks.size() == zranges.size());
  FASTUIDRAWassert(attrib_chunks.size() == index_adjusts.size());

  /* compute how many attribute, indices and z's per winding */
  unsigned int atr_offset(0), idx_offset(0);
  for(int w : m_windings)
    {
      unsigned int ch;
      unsigned int a_sz, i_sz;
      const AAFuzz &aa_fuzz(m_builder.aa_fuzz(w));

      ch = signed_to_unsigned(w);
      i_sz = aa_fuzz.edge_counts().m_index_count;
      a_sz = aa_fuzz.edge_counts().m_attribute_count;
      attrib_chunks[ch] = attributes.sub_array(atr_offset, a_sz);
      index_chunks[ch] = indices.sub_array(idx_offset, i_sz);
      index_adjusts[ch] = 0;
      zranges[ch].m_begin = 0;
      zranges[ch].m_end = aa_fuzz.edge_counts().m_depth_count;
      atr_offset += a_sz;
      idx_offset += i_sz;
    }

  // for eaching winding number, add the edges
  for (int w : m_windings)
    {
      const AAFuzz &fuzz(m_builder.aa_fuzz(w));
      const std::list<AAFuzz::Contour> &contours(fuzz.contours());
      unsigned int ch, vertex_offset, index_offset;
      int z;
      fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attrib;
      fastuidraw::c_array<fastuidraw::PainterIndex> dst_index;

      ch = signed_to_unsigned(w);
      vertex_offset = 0u;
      index_offset = 0u;
      z = 0;
      dst_attrib = attrib_chunks[ch].const_cast_pointer<fastuidraw::PainterAttribute>();
      dst_index = index_chunks[ch].const_cast_pointer<fastuidraw::PainterIndex>();
      for(const AAFuzz::Contour &C : contours)
        {
          for(const AAEdge &E : C)
            {
              if (E.m_draw_edge)
                {
                  pack_edge(E, zranges[ch].m_end - 1 - z,
                            dst_attrib, vertex_offset,
                            dst_index, index_offset);
                  ++z;
                }
            }
        }
      FASTUIDRAWassert(vertex_offset == dst_attrib.size());
      FASTUIDRAWassert(index_offset == dst_index.size());
      FASTUIDRAWassert(z == zranges[ch].m_end);
    }
}

void
AAFuzzAttributeDataFiller::
pack_edge(const AAEdge &E, int z,
          fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attr,
          unsigned int &vertex_offset,
          fastuidraw::c_array<fastuidraw::PainterIndex> dst_idx,
          unsigned int &index_offset) const
{
  using namespace fastuidraw;

  unsigned int current_start(vertex_offset);
  dvec2 tangent, normal;
  const double sgn[6] =
    {
      -1.0,
       0.0,
      +1.0,
      -1.0,
       0.0,
      +1.0
    };
  const uint32_t types[6] =
    {
      FilledPath::Subset::aa_fuzz_type_on_boundary,
      FilledPath::Subset::aa_fuzz_type_on_path,
      FilledPath::Subset::aa_fuzz_type_on_boundary,

      FilledPath::Subset::aa_fuzz_type_on_boundary,
      FilledPath::Subset::aa_fuzz_type_on_path,
      FilledPath::Subset::aa_fuzz_type_on_boundary,
    };
  const unsigned int tris[12] =
    {
      0, 3, 4,
      0, 4, 1,
      1, 4, 5,
      1, 5, 2
    };

  tangent = m_pts[E.m_end] - m_pts[E.m_start];
  tangent /= tangent.magnitude();
  normal = dvec2(-tangent.y(), tangent.x());

  for (unsigned int k = 0; k < 12; ++k, ++index_offset)
    {
      dst_idx[index_offset] = tris[k] + vertex_offset;
    }

  for (unsigned int k = 0; k < 6; ++k, ++vertex_offset)
    {
      unsigned int q;
      q = (k < 3) ? E.m_start : E.m_end;
      pack_attribute(m_pts[q], types[k],
                     sgn[k] * normal, z,
                     &dst_attr[vertex_offset]);
    }

  if (E.m_draw_join_to_next)
    {
      dvec2 t, n;
      double d, sd;
      unsigned int center, next_start(vertex_offset);
      unsigned int current_outer, next_outer;

      t = m_pts[E.m_next] - m_pts[E.m_end];
      t /= t.magnitude();
      n = dvec2(-t.y(), t.x());
      d = dot(normal, t);
      sd = t_sign(d);

      if (E.m_draw_join_as_miter)
        {
          /* Miter join will add a point, thus the next_start
           * will come one vertex later.
           */
          ++next_start;
        }

      center = current_start + 4u;
      if (E.m_is_closing_edge)
        {
          next_outer = vertex_offset;
          pack_attribute(m_pts[E.m_end],
                         FilledPath::Subset::aa_fuzz_type_on_boundary,
                         -sd * n, z,
                         &dst_attr[vertex_offset++]);
        }

      if (d < 0.0)
        {
          current_outer = current_start + 5u;
          if (!E.m_is_closing_edge)
            {
              next_outer = next_start + 2u;
            }
        }
      else
        {
          current_outer = current_start + 3u;
          if (!E.m_is_closing_edge)
            {
              next_outer = next_start + 0u;
            }
        }

      if (E.m_draw_join_as_miter)
        {
          const double d_tol(0.00001f);
          unsigned int miter;

          miter = vertex_offset;
          if (t_abs(d) > d_tol)
            {
              pack_attribute(m_pts[E.m_end],
                             -sd * normal, -sd * n, z,
                             &dst_attr[vertex_offset++]);
            }
          else
            {
              pack_attribute(m_pts[E.m_end],
                             FilledPath::Subset::aa_fuzz_type_on_boundary,
                             -sd * n, z,
                             &dst_attr[vertex_offset++]);
            }

          dst_idx[index_offset++] = current_outer;
          dst_idx[index_offset++] = miter;
          dst_idx[index_offset++] = center;

          dst_idx[index_offset++] = center;
          dst_idx[index_offset++] = next_outer;
          dst_idx[index_offset++] = miter;
        }
      else
        {
          dst_idx[index_offset++] = current_outer;
          dst_idx[index_offset++] = next_outer;
          dst_idx[index_offset++] = center;
        }
    }
}

void
AAFuzzAttributeDataFiller::
pack_attribute(fastuidraw::dvec2 position, uint32_t type,
               fastuidraw::dvec2 normal, int z,
               fastuidraw::PainterAttribute *dst) const
{
  FASTUIDRAWassert(z >= 0);

  dst->m_attrib0.x() = fastuidraw::pack_float(position.x());
  dst->m_attrib0.y() = fastuidraw::pack_float(position.y());
  dst->m_attrib0.z() = type;
  dst->m_attrib0.w() = z;

  dst->m_attrib1.x() = fastuidraw::pack_float(normal.x());
  dst->m_attrib1.y() = fastuidraw::pack_float(normal.y());
}

void
AAFuzzAttributeDataFiller::
pack_attribute(fastuidraw::dvec2 position,
               fastuidraw::dvec2 normal,
               fastuidraw::dvec2 normal_next,
               int z,
               fastuidraw::PainterAttribute *dst) const
{
  FASTUIDRAWassert(z >= 0);

  dst->m_attrib0.x() = fastuidraw::pack_float(position.x());
  dst->m_attrib0.y() = fastuidraw::pack_float(position.y());
  dst->m_attrib0.z() = fastuidraw::FilledPath::Subset::aa_fuzz_type_on_boundary_miter;
  dst->m_attrib0.w() = z;

  dst->m_attrib1.x() = fastuidraw::pack_float(normal.x());
  dst->m_attrib1.y() = fastuidraw::pack_float(normal.y());
  dst->m_attrib1.z() = fastuidraw::pack_float(normal_next.x());
  dst->m_attrib1.w() = fastuidraw::pack_float(normal_next.y());
}

////////////////////////////////////
// FillAttributeDataFiller methods
void
FillAttributeDataFiller::
compute_sizes(unsigned int &number_attributes,
              unsigned int &number_indices,
              unsigned int &number_attribute_chunks,
              unsigned int &number_index_chunks,
              unsigned int &number_z_ranges) const
{
  using namespace fastuidraw;

  number_z_ranges = 0;
  if (m_per_fill.empty())
    {
      number_attributes = 0;
      number_indices = 0;
      number_attribute_chunks = 0;
      number_index_chunks = 0;
      return;
    }
  number_attributes = m_points.size();
  number_attribute_chunks = 1;

  number_indices = m_odd_winding_indices.size()
    + m_nonzero_winding_indices.size()
    + m_even_winding_indices.size()
    + m_zero_winding_indices.size();

  for(const auto &e : m_per_fill)
    {
      if (e.first != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          number_indices += e.second.size();
        }
    }

  /* now get how big the index_chunks really needs to be
   */
  int smallest_winding(m_per_fill.begin()->first);
  int largest_winding(m_per_fill.rbegin()->first);
  unsigned int largest_winding_idx(FilledPath::Subset::fill_chunk_from_winding_number(largest_winding));
  unsigned int smallest_winding_idx(FilledPath::Subset::fill_chunk_from_winding_number(smallest_winding));
  number_index_chunks = 1 + std::max(largest_winding_idx, smallest_winding_idx);
}

void
FillAttributeDataFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attributes,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attrib_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  using namespace fastuidraw;

  if (m_per_fill.empty())
    {
      return;
    }
  FASTUIDRAWassert(attributes.size() == m_points.size());
  FASTUIDRAWassert(attrib_chunks.size() == 1);
  FASTUIDRAWassert(zranges.empty());
  FASTUIDRAWunused(zranges);

  /* generate attribute data */
  std::transform(m_points.begin(), m_points.end(), attributes.begin(),
                 FillAttributeDataFiller::generate_attribute);
  attrib_chunks[0] = attributes;
  std::fill(index_adjusts.begin(), index_adjusts.end(), 0);

  unsigned int current(0);

#define GRAB_MACRO(enum_name, member_name) do {                     \
    c_array<PainterIndex> dst;                                      \
    dst = index_data.sub_array(current, member_name.size());        \
    std::copy(member_name.begin(),                                  \
              member_name.end(), dst.begin());                      \
    index_chunks[PainterEnums::enum_name] = dst;                    \
    current += dst.size();                                          \
  } while(0)

  GRAB_MACRO(odd_even_fill_rule, m_odd_winding_indices);
  GRAB_MACRO(nonzero_fill_rule, m_nonzero_winding_indices);
  GRAB_MACRO(complement_odd_even_fill_rule, m_even_winding_indices);
  GRAB_MACRO(complement_nonzero_fill_rule, m_zero_winding_indices);

#undef GRAB_MACRO

  for(const auto &e : m_per_fill)
    {
      if (e.first != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          c_array<PainterIndex> dst;
          c_array<const unsigned int> src;
          unsigned int idx;

          idx = FilledPath::Subset::fill_chunk_from_winding_number(e.first);

          src = e.second;
          dst = index_data.sub_array(current, src.size());
          FASTUIDRAWassert(dst.size() == src.size());

          std::copy(src.begin(), src.end(), dst.begin());

          index_chunks[idx] = dst;
          current += dst.size();
        }
    }
}

/////////////////////////////////
// SubsetPrivate methods
SubsetPrivate::
SubsetPrivate(SubPath *Q, int max_recursion,
              std::vector<SubsetPrivate*> &out_values):
  m_ID(out_values.size()),
  m_bounds(Q->bounds()),
  m_bounds_f(fastuidraw::vec2(m_bounds.min_point()),
             fastuidraw::vec2(m_bounds.max_point())),
  m_painter_data(nullptr),
  m_fuzz_painter_data(nullptr),
  m_sizes_ready(false),
  m_sub_path(Q),
  m_children(nullptr, nullptr),
  m_splitting_coordinate(-1)
{
  out_values.push_back(this);
  if (max_recursion > 0
      && m_sub_path->num_points() > SubsetConstants::points_per_subset)
    {
      fastuidraw::vecN<SubPath*, 2> C;

      C = Q->split(m_splitting_coordinate);
      if (C[0]->num_points() < m_sub_path->num_points()
          || C[1]->num_points() < m_sub_path->num_points())
        {
          m_children[0] = FASTUIDRAWnew SubsetPrivate(C[0], max_recursion - 1, out_values);
          m_children[1] = FASTUIDRAWnew SubsetPrivate(C[1], max_recursion - 1, out_values);
          FASTUIDRAWdelete(m_sub_path);
          m_sub_path = nullptr;
        }
      else
        {
          FASTUIDRAWdelete(C[0]);
          FASTUIDRAWdelete(C[1]);
        }
    }

  const fastuidraw::vec2 &m(m_bounds_f.min_point());
  const fastuidraw::vec2 &M(m_bounds_f.max_point());

  m_bounding_path << fastuidraw::vec2(m.x(), m.y())
                  << fastuidraw::vec2(m.x(), M.y())
                  << fastuidraw::vec2(M.x(), M.y())
                  << fastuidraw::vec2(M.x(), m.y())
                  << fastuidraw::Path::contour_close();
}

SubsetPrivate::
~SubsetPrivate(void)
{
  if (m_sub_path != nullptr)
    {
      FASTUIDRAWassert(m_painter_data == nullptr);
      FASTUIDRAWassert(m_fuzz_painter_data == nullptr);
      FASTUIDRAWassert(m_children[0] == nullptr);
      FASTUIDRAWassert(m_children[1] == nullptr);
      FASTUIDRAWdelete(m_sub_path);
    }

  if (m_painter_data != nullptr)
    {
      FASTUIDRAWassert(m_sub_path == nullptr);
      FASTUIDRAWassert(m_fuzz_painter_data != nullptr);
      FASTUIDRAWdelete(m_painter_data);
      FASTUIDRAWdelete(m_fuzz_painter_data);
    }

  if (m_children[0] != nullptr)
    {
      FASTUIDRAWassert(m_sub_path == nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);
      FASTUIDRAWdelete(m_children[0]);
      FASTUIDRAWdelete(m_children[1]);
    }
}

SubsetPrivate*
SubsetPrivate::
create_root_subset(SubPath *P, std::vector<SubsetPrivate*> &out_values)
{
  SubsetPrivate *root;
  root = FASTUIDRAWnew SubsetPrivate(P, SubsetConstants::recursion_depth, out_values);
  return root;
}

unsigned int
SubsetPrivate::
select_subsets(ScratchSpacePrivate &scratch,
               fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
               const fastuidraw::float3x3 &clip_matrix_local,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               fastuidraw::c_array<unsigned int> dst)
{
  unsigned int return_value(0u);

  scratch.m_adjusted_clip_eqs.resize(clip_equations.size());
  for(unsigned int i = 0; i < clip_equations.size(); ++i)
    {
      /* transform clip equations from clip coordinates to
       * local coordinates.
       */
      scratch.m_adjusted_clip_eqs[i] = clip_equations[i] * clip_matrix_local;
    }

  select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, return_value);
  return return_value;
}

bool
SubsetPrivate::
select_subsets_implement(ScratchSpacePrivate &scratch,
                         fastuidraw::c_array<unsigned int> dst,
                         unsigned int max_attribute_cnt,
                         unsigned int max_index_cnt,
                         unsigned int &current)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  vecN<vec2, 4> bb;
  bool unclipped;

  m_bounds_f.inflated_polygon(bb, 0.0f);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, &scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_vec2s);

  //completely clipped
  if (scratch.m_clipped_rect.empty())
    {
      return false;
    }

  //completely unclipped or no children
  if (unclipped || !have_children())
    {
      return select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
    }

  bool r0, r1;

  r0 = m_children[0]->select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, current);
  r1 = m_children[1]->select_subsets_implement(scratch, dst, max_attribute_cnt, max_index_cnt, current);
  if (r0 && r1)
    {
      FASTUIDRAWassert(current >= 2);
      FASTUIDRAWassert(dst[current - 2] == m_children[0]->m_ID);
      FASTUIDRAWassert(dst[current - 1] == m_children[1]->m_ID);

      if (!m_sizes_ready)
        {
          ready_sizes_from_children();
        }

      if (m_num_attributes <= max_attribute_cnt
          && m_largest_index_block <= max_index_cnt
          && m_aa_largest_attribute_block <= max_attribute_cnt
          && m_aa_largest_index_block <= max_index_cnt)
        {
          /* the last two added are m_children[0] and m_children[1];
           * remove those and add this.
           */
          current -= 2;
          dst[current] = m_ID;
          ++current;
          return true;
        }
    }
  return false;
}

bool
SubsetPrivate::
select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                            unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            unsigned int &current)
{
  if (!m_sizes_ready && !have_children() && m_sub_path != nullptr)
    {
      /* we are going to need the attributes because
       * the element will be selected.
       */
      make_ready_from_sub_path();
      FASTUIDRAWassert(m_painter_data != nullptr);
    }

  if (m_sizes_ready
      && m_num_attributes <= max_attribute_cnt
      && m_largest_index_block <= max_index_cnt
      && m_aa_largest_attribute_block <= max_attribute_cnt
      && m_aa_largest_index_block <= max_index_cnt)
    {
      dst[current] = m_ID;
      ++current;
      return true;
    }
  else if (have_children())
    {
      m_children[0]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      m_children[1]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      if (!m_sizes_ready)
        {
          ready_sizes_from_children();
        }
      return false;
    }
  else
    {
      FASTUIDRAWassert(m_sizes_ready);
      FASTUIDRAWassert(!"Childless FilledPath::Subset has too many attributes or indices");
      return false;
    }
}

void
SubsetPrivate::
ready_sizes_from_children(void)
{
  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(!m_sizes_ready);

  m_sizes_ready = true;
  FASTUIDRAWassert(m_children[0]->m_sizes_ready);
  FASTUIDRAWassert(m_children[1]->m_sizes_ready);
  m_num_attributes = m_children[0]->m_num_attributes + m_children[1]->m_num_attributes;

  /* these are upper-bounds, they will get overwritten when the actual creation
   * of the attribute objects is done.
   */
  m_largest_index_block =
    m_children[0]->m_largest_index_block + m_children[1]->m_largest_index_block;

  m_aa_largest_attribute_block =
    m_children[0]->m_aa_largest_attribute_block + m_children[1]->m_aa_largest_attribute_block;

  m_aa_largest_index_block =
    m_children[0]->m_aa_largest_index_block + m_children[1]->m_aa_largest_index_block;
}

void
SubsetPrivate::
make_ready(void)
{
  if (m_painter_data == nullptr)
    {
      if (m_sub_path != nullptr)
        {
          make_ready_from_sub_path();
        }
      else
        {
          make_ready_from_children();
        }
    }
}

void
SubsetPrivate::
merge_winding_lists(fastuidraw::c_array<const int> inA,
                    fastuidraw::c_array<const int> inB,
                    std::vector<int> *out)
{
  std::set<int> wnd;
  std::copy(inA.begin(), inA.end(), std::inserter(wnd, wnd.begin()));
  std::copy(inB.begin(), inB.end(), std::inserter(wnd, wnd.begin()));
  out->resize(wnd.size());
  std::copy(wnd.begin(), wnd.end(), out->begin());
}

void
SubsetPrivate::
make_ready_from_children(void)
{
  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(m_sub_path == nullptr);
  FASTUIDRAWassert(m_painter_data == nullptr);

  m_children[0]->make_ready();
  m_children[1]->make_ready();

  FillAttributeDataMerger merger(m_children[0]->painter_data(),
                                 m_children[1]->painter_data());

  m_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  m_painter_data->set_data(merger);

  merge_winding_lists(m_children[0]->winding_numbers(),
                      m_children[1]->winding_numbers(),
                      &m_winding_numbers);

  m_fuzz_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  EdgeAttributeDataMerger fuzz_merger(m_children[0]->fuzz_painter_data(),
                                      m_children[1]->fuzz_painter_data());
  m_fuzz_painter_data->set_data(fuzz_merger);

  /* overwrite size values to be precise */
  m_sizes_ready = true;
  m_num_attributes = m_painter_data->largest_attribute_chunk();
  m_largest_index_block = m_painter_data->largest_index_chunk();
  m_aa_largest_attribute_block = m_fuzz_painter_data->largest_attribute_chunk();
  m_aa_largest_index_block = m_fuzz_painter_data->largest_index_chunk();
}

void
SubsetPrivate::
make_ready_from_sub_path(void)
{
  FASTUIDRAWassert(m_children[0] == nullptr);
  FASTUIDRAWassert(m_children[1] == nullptr);
  FASTUIDRAWassert(m_sub_path != nullptr);
  FASTUIDRAWassert(m_painter_data == nullptr);
  FASTUIDRAWassert(!m_sizes_ready);

  FillAttributeDataFiller filler;
  builder B(*m_sub_path, filler.m_points);
  unsigned int even_non_zero_start, zero_start;
  unsigned int m1, m2;

  B.fill_indices(filler.m_indices, filler.m_per_fill, even_non_zero_start, zero_start);

  fastuidraw::c_array<const unsigned int> indices_ptr;
  indices_ptr = fastuidraw::make_c_array(filler.m_indices);
  filler.m_nonzero_winding_indices = indices_ptr.sub_array(0, zero_start);
  filler.m_odd_winding_indices = indices_ptr.sub_array(0, even_non_zero_start);
  filler.m_even_winding_indices = indices_ptr.sub_array(even_non_zero_start);
  filler.m_zero_winding_indices = indices_ptr.sub_array(zero_start);

  m_sizes_ready = true;
  m1 = fastuidraw::t_max(filler.m_nonzero_winding_indices.size(),
                         filler.m_zero_winding_indices.size());
  m2 = fastuidraw::t_max(filler.m_odd_winding_indices.size(),
                         filler.m_even_winding_indices.size());
  m_largest_index_block = fastuidraw::t_max(m1, m2);
  m_num_attributes = filler.m_points.size();

  m_winding_numbers.reserve(filler.m_per_fill.size());
  for(const auto & e : filler.m_per_fill)
    {
      FASTUIDRAWassert(!e.second.empty());
      m_winding_numbers.push_back(e.first);
    }

  /* now fill m_painter_data. */
  m_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  m_painter_data->set_data(filler);

  /* fill m_fuzz_painter_data */
  m_fuzz_painter_data = FASTUIDRAWnew fastuidraw::PainterAttributeData();
  if (!m_winding_numbers.empty())
    {
      AAFuzzAttributeDataFiller edge_filler(fastuidraw::make_c_array(m_winding_numbers),
                                            &filler.m_points, &B);
      m_fuzz_painter_data->set_data(edge_filler);
      m_aa_largest_attribute_block = m_fuzz_painter_data->largest_attribute_chunk();
      m_aa_largest_index_block = m_fuzz_painter_data->largest_index_chunk();
    }

  FASTUIDRAWdelete(m_sub_path);
  m_sub_path = nullptr;

  #ifdef FASTUIDRAW_DEBUG
    {
      if (B.triangulation_failed())
        {
          /* On debug builds, print a warning.
           */
          std::cerr << "[" << __FILE__ << ", " << __LINE__
                    << "] Triangulation failed on tessellated path "
                    << this << "\n";
        }
    }
  #endif

}

/////////////////////////////////
// FilledPathPrivate methods
FilledPathPrivate::
FilledPathPrivate(const fastuidraw::TessellatedPath &P):
  m_bounding_box(P.bounding_box())
{
  SubPath *q;
  q = FASTUIDRAWnew SubPath(P);
  m_root = SubsetPrivate::create_root_subset(q, m_subsets);
}

FilledPathPrivate::
~FilledPathPrivate()
{
  FASTUIDRAWdelete(m_root);
}

///////////////////////////////
//fastuidraw::FilledPath::ScratchSpace methods
fastuidraw::FilledPath::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::FilledPath::ScratchSpace::
~ScratchSpace(void)
{
  ScratchSpacePrivate *d;
  d = static_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

/////////////////////////////////
// fastuidraw::FilledPath::Subset methods
fastuidraw::FilledPath::Subset::
Subset(void *d):
  m_d(d)
{
}

const fastuidraw::PainterAttributeData&
fastuidraw::FilledPath::Subset::
painter_data(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->painter_data();
}

const fastuidraw::PainterAttributeData&
fastuidraw::FilledPath::Subset::
aa_fuzz_painter_data(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->fuzz_painter_data();
}

fastuidraw::c_array<const int>
fastuidraw::FilledPath::Subset::
winding_numbers(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->winding_numbers();
}

const fastuidraw::Path&
fastuidraw::FilledPath::Subset::
bounding_path(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->bounding_path();
}

const fastuidraw::Rect&
fastuidraw::FilledPath::Subset::
bounding_box(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->bounding_box().as_rect();
}

unsigned int
fastuidraw::FilledPath::Subset::
fill_chunk_from_winding_number(int winding_number)
{
  /* basic idea:
   * - start counting at number_fill_rule
   * - ordering is: 1, -1, 2, -2, ...
   */
  int value, sg;

  if (winding_number == 0)
    {
      return fastuidraw::PainterEnums::complement_nonzero_fill_rule;
    }

  value = std::abs(winding_number);
  sg = (winding_number < 0) ? 1 : 0;
  return fastuidraw::PainterEnums::number_fill_rule + sg + 2 * (value - 1);
}

unsigned int
fastuidraw::FilledPath::Subset::
fill_chunk_from_fill_rule(enum PainterEnums::fill_rule_t fill_rule)
{
  FASTUIDRAWassert(fill_rule < fastuidraw::PainterEnums::number_fill_rule);
  return fill_rule;
}

unsigned int
fastuidraw::FilledPath::Subset::
aa_fuzz_chunk_from_winding_number(int w)
{
  return signed_to_unsigned(w);
}

///////////////////////////////////////
// fastuidraw::FilledPath methods
fastuidraw::FilledPath::
FilledPath(const TessellatedPath &P)
{
  m_d = FASTUIDRAWnew FilledPathPrivate(P);
}

fastuidraw::FilledPath::
~FilledPath()
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::Rect&
fastuidraw::FilledPath::
bounding_box(void) const
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  return d->m_bounding_box;
}

unsigned int
fastuidraw::FilledPath::
number_subsets(void) const
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  return d->m_subsets.size();
}

fastuidraw::FilledPath::Subset
fastuidraw::FilledPath::
subset(unsigned int I) const
{
  FilledPathPrivate *d;
  SubsetPrivate *p;

  d = static_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWassert(I < d->m_subsets.size());
  p = d->m_subsets[I];
  p->make_ready();

  return Subset(p);
}

fastuidraw::FilledPath::Subset
fastuidraw::FilledPath::
root_subset(void) const
{
  FilledPathPrivate *d;
  d = static_cast<FilledPathPrivate*>(m_d);
  d->m_root->make_ready();
  return Subset(d->m_root);
}

unsigned int
fastuidraw::FilledPath::
select_subsets(ScratchSpace &work_room,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               c_array<unsigned int> dst) const
{
  FilledPathPrivate *d;
  unsigned int return_value;

  d = static_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());
  /* TODO:
   *   - have another method in SubsetPrivate called
   *     "fast_select_subsets" which ignores the requirements
   *     coming from max_attribute_cnt and max_index_cnt.
   *     By ignoring this requirement, we do NOT need
   *     to do call make_ready() for any SubsetPrivate
   *     object chosen.
   *   - have the fast_select_subsets() also return
   *     if paths needed require triangulation.
   *   - if there such, spawn a thread and let the
   *     caller decide if to wait for the thread to
   *     finish before proceeding or to do something
   *     else (like use a lower level of detail that
   *     is ready). Another alternatic is to return
   *     what Subset's need to have triangulation done
   *     and spawn a set of threads to do the job (!)
   *   - All this work means we need to make SubsetPrivate
   *     thread safe (with regards to the SubsetPrivate
   *     being made ready via make_ready()).
   */
  return_value = d->m_root->select_subsets(*static_cast<ScratchSpacePrivate*>(work_room.m_d),
                                           clip_equations, clip_matrix_local,
                                           max_attribute_cnt, max_index_cnt, dst);

  return return_value;
}

unsigned int
fastuidraw::FilledPath::
select_subsets_no_culling(unsigned int max_attribute_cnt,
                          unsigned int max_index_cnt,
                          c_array<unsigned int> dst) const
{
  FilledPathPrivate *d;
  unsigned int return_value(0);

  d = static_cast<FilledPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());
  d->m_root->select_subsets_all_unculled(dst, max_attribute_cnt,
                                         max_index_cnt, return_value);

  return return_value;
}
