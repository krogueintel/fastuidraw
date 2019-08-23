/*!
 * \file int_path.hpp
 * \brief file int_path.hpp
 *
 * Copyright 2017 by Intel.
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

#ifndef FASTUIDRAW_INT_PATH_HPP
#define FASTUIDRAW_INT_PATH_HPP

#include <vector>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/fill_rule.hpp>
#include <fastuidraw/text/glyph_render_data_texels.hpp>

#include <private/array2d.hpp>
#include <private/bounding_box.hpp>
#include <private/util_private.hpp>

namespace fastuidraw
{
  namespace detail
  {
    class IntBezierCurve
    {
    public:

      template<typename T>
      class transformation
      {
      public:
        transformation(T sc = T(1), vecN<T, 2> tr = vecN<T, 2>(T(0))):
          m_scale(sc),
          m_translate(tr)
        {}

        T
        scale(void) const
        {
          return m_scale;
        }

        vecN<T, 2>
        translate(void) const
        {
          return m_translate;
        }

        template<typename U>
        transformation<U>
        cast(void) const
        {
          return transformation<U>(U(m_scale), vecN<U, 2>(m_translate));
        }

        vecN<T, 2>
        operator()(vecN<T, 2> p) const
        {
          return m_translate + m_scale * p;
        }

      private:
        T m_scale;
        vecN<T, 2> m_translate;
      };

      class ID_t
      {
      public:
        ID_t(void):
          m_contourID(-1),
          m_curveID(-1)
        {}

        bool
        operator<(const ID_t &rhs) const
        {
          return m_contourID < rhs.m_contourID
            || (m_contourID == rhs.m_contourID
                && m_curveID < rhs.m_curveID);
        }

        bool
        operator==(const ID_t &rhs) const
        {
          return m_contourID == rhs.m_contourID
            && m_curveID == rhs.m_curveID;
        }

        unsigned int m_contourID;
        unsigned int m_curveID;
      };

      IntBezierCurve(const ID_t &pID, const IntBezierCurve &curve):
        m_ID(pID),
        m_control_pts(curve.m_control_pts),
        m_num_control_pts(curve.m_num_control_pts),
        m_as_polynomial_fcn(curve.m_as_polynomial_fcn),
        m_derivatives_cancel(curve.m_derivatives_cancel),
        m_num_derivatives_cancel(curve.m_num_derivatives_cancel),
        m_bb(curve.m_bb)
      {
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, pt1, ivec2(), ivec2()),
        m_num_control_pts(2)
      {
        process_control_pts();
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &ct,
                     const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, ct, pt1, ivec2()),
        m_num_control_pts(3)
      {
        process_control_pts();
      }

      IntBezierCurve(const ID_t &pID, const ivec2 &pt0, const ivec2 &ct0,
                     const ivec2 &ct1, const ivec2 &pt1):
        m_ID(pID),
        m_control_pts(pt0, ct0, ct1, pt1),
        m_num_control_pts(4)
      {
        process_control_pts();
      }

      IntBezierCurve(const ID_t &pID, c_array<const ivec2> pts):
        m_ID(pID),
        m_num_control_pts(pts.size())
      {
        FASTUIDRAWassert(pts.size() <= 4);
        std::copy(pts.begin(), pts.end(), m_control_pts.begin());
        process_control_pts();
      }

      ~IntBezierCurve()
      {}

      const ID_t&
      ID(void) const
      {
        return m_ID;
      }

      c_array<const ivec2>
      control_pts(void) const
      {
        return c_array<const ivec2>(m_control_pts.c_ptr(), m_num_control_pts);
      }

      /* BEWARE! Changing point in a contour means one needs to make
       * sure that the neighbor curve also changes a point.
       */
      void
      set_pt(unsigned int i, const ivec2 &pvalue)
      {
        FASTUIDRAWassert(i < m_num_control_pts);
        m_control_pts[i] = pvalue;
        process_control_pts();
      }

      void
      set_front_pt(const ivec2 &pvalue)
      {
        set_pt(0, pvalue);
      }

      void
      set_back_pt(const ivec2 &pvalue)
      {
        set_pt(degree(), pvalue);
      }

      const BoundingBox<int>&
      bounding_box(void) const
      {
        return m_bb;
      }

      BoundingBox<int>
      bounding_box(const transformation<int> &tr) const
      {
        BoundingBox<int> R;
        R.union_point(tr(m_bb.min_point()));
        R.union_point(tr(m_bb.max_point()));
        return R;
      }

      static
      bool
      are_ordered_neighbors(const IntBezierCurve &curve0,
                            const IntBezierCurve &curve1)
      {
        return curve0.control_pts().back() == curve1.control_pts().front();
      }

      int
      degree(void) const
      {
        FASTUIDRAWassert(m_num_control_pts > 0);
        return m_num_control_pts - 1;
      }

      c_array<const vec2>
      derivatives_cancel(void) const
      {
        return c_array<const vec2>(m_derivatives_cancel.c_ptr(),
                                   m_num_derivatives_cancel);
      }

      c_array<const int>
      as_polynomial(int coord) const
      {
        return c_array<const int>(m_as_polynomial_fcn[coord].c_ptr(), m_num_control_pts);
      }

      vecN<c_array<const int>, 2>
      as_polynomial(void) const
      {
        return vecN<c_array<const int>, 2>(as_polynomial(0), as_polynomial(1));
      }

      vec2
      eval(float t) const;

    private:
      void
      process_control_pts(void);

      void
      compute_derivatives_cancel_pts(void);

      c_array<int>
      as_polynomial(int coord)
      {
        return c_array<int>(m_as_polynomial_fcn[coord].c_ptr(), m_num_control_pts);
      }

      ID_t m_ID;
      vecN<ivec2, 4> m_control_pts;
      unsigned int m_num_control_pts;
      vecN<ivec4, 2> m_as_polynomial_fcn;

      // where dx/dt +- dy/dt = 0
      vecN<vec2, 6> m_derivatives_cancel;
      unsigned int m_num_derivatives_cancel;
      BoundingBox<int> m_bb;
    };

    class IntContour
    {
    public:
      IntContour(void)
      {}

      void
      add_curve(const IntBezierCurve &curve)
      {
        FASTUIDRAWassert(m_curves.empty() || IntBezierCurve::are_ordered_neighbors(m_curves.back(), curve));
        m_curves.push_back(curve);
      }

      bool
      closed(void)
      {
        return !m_curves.empty()
          && IntBezierCurve::are_ordered_neighbors(m_curves.back(), m_curves.front());
      }

      const std::vector<IntBezierCurve>&
      curves(void) const
      {
        return m_curves;
      }

      const IntBezierCurve&
      curve(unsigned int curveID) const
      {
        FASTUIDRAWassert(curveID < m_curves.size());
        return m_curves[curveID];
      }

      /* Convert cubic curves into quadratic curves; A given cubic
       * is converted into 1, 2, or 4 quadratic curves depending
       * on the distance between its end points. If the distance
       * is atleast thresh_4_quads, then if it borken into 4 quadratics,
       * if the distance is atleast thresh_2_quads (but less than
       * thresh_4_quads), then it is broken into 2 quadratic curves.
       * If the distance is less than thresh_2_quads, then it is realized
       * as a line quadratic. The distance values are the distances
      * of the points AFTER tr is applied.
       */
      void
      replace_cubics_with_quadratics(const IntBezierCurve::transformation<int> &tr,
                                     int thresh_4_quads, int thresh_2_quads,
                                     ivec2 texel_size);

      /* replace each cubic with 4 quadratics */
      void
      replace_cubics_with_quadratics(void);

      /* Convert those quadratic curves that have small curvature into
       * line segments.
       */
      void
      convert_flat_quadratics_to_lines(float thresh);

      /* Collapse any curve that after transformation is contained
       * within a texel (size of texel is given by texel_size) to
       * a point.
       */
      void
      collapse_small_curves(const IntBezierCurve::transformation<int> &tr,
                            ivec2 texel_size);

      /* Call the sequence:
       *  1. replace_cubics_with_quadratics(tr, 6, 4, texel_size)
       *  2. convert_flat_quadratics_to_lines()
       *  3. collapse_small_curves()
       */
      void
      filter(float curvature_collapse,
             const IntBezierCurve::transformation<int> &tr, ivec2 texel_size);

      void
      add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const;

    private:
      std::vector<IntBezierCurve> m_curves;
    };

    class IntPath
    {
    public:
      IntPath(void)
      {}

      void
      move_to(const ivec2 &pt);

      void
      line_to(const ivec2 &pt);

      void
      conic_to(const ivec2 &control_pt, const ivec2 &pt);

      void
      cubic_to(const ivec2 &control_pt0, const ivec2 &control_pt1, const ivec2 &pt);

      bool
      empty(void) const
      {
        return m_contours.empty();
      }

      c_array<const IntContour>
      contours(void) const
      {
        return make_c_array(m_contours);
      }

      /* Add this IntPath with a transforamtion tr applied to it to a
       * pre-exising (and possibly empty) Path.
       * \param tr transformation to apply to data of path
       */
      void
      add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const;

      /* replace each cubic with 4 quadratics */
      void
      replace_cubics_with_quadratics(void);

      /* Convert cubic curves into quadratic curves; A given cubic
       * is converted into 1, 2, or 4 quadratic curves depending
       * on the distance between its end points. If the distance
       * is atleast thresh_4_quads, then if it borken into 4 quadratics,
       * if the distance is atleast thresh_2_quads (but less than
       * thresh_4_quads), then it is broken into 2 quadratic curves.
       * If the distance is less than thresh_2_quads, then it is realized
       * as a line quadratic.
       */
      void
      replace_cubics_with_quadratics(const IntBezierCurve::transformation<int> &tr,
                                     int thresh_4_quads, int thresh_2_quads,
                                     ivec2 texel_size);

      /* Convert those quadratic curves that have small curvature into
       * line segments.
       */
      void
      convert_flat_quadratics_to_lines(float thresh);

      /* Collapse any curve that after transformation is contained
       * within a texel (size of texel is given by texel_size) to
       * a point.
       */
      void
      collapse_small_curves(const IntBezierCurve::transformation<int> &tr,
                            ivec2 texel_size);

      /* Filter the Path as follows:
       *  1. Collapse any curves that are within a texel
       *  2. Curves of tiny curvature are realized as a line
       *  3. Cubics are broken into quadratics
       * The transformation tr is -NOT- applied to the path,
       * it is used as the transformation from IntContour
       * coordinates to texel coordinates. The value of texel_size
       * gives the size of a texel with the texel at (0, 0)
       * starting at (0, 0) [in texel coordinates].
       */
      void
      filter(float curvature_collapse,
             const IntBezierCurve::transformation<int> &tr,
             ivec2 texel_size);


      /* Compute distance field data, where distance values are
       * sampled at the center of each texel.
       * \param texel_size the size of each texel in coordinates
       *                   AFTER tr is applied
       * \param image_sz size of the distance field to make
       * \param tr transformation to apply to data of path
       */
      void
      extract_render_data(const ivec2 &texel_size, const ivec2 &image_sz,
                          float max_distance,
                          IntBezierCurve::transformation<int> tr,
                          const CustomFillRuleBase &fill_rule,
                          GlyphRenderDataTexels *dst) const;

    private:
      IntBezierCurve::ID_t
      computeID(void);

      fastuidraw::ivec2 m_last_pt;
      std::vector<IntContour> m_contours;
    };

  } //namespace fastuidraw
} //namespace detail

#endif
