/*!
 * \file util.hpp
 * \brief file util.hpp
 *
 * Adapted from: WRATHUtil.hpp and type_tag.hpp of WRATH:
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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef FASTUIDRAW_UTIL_HPP
#define FASTUIDRAW_UTIL_HPP

#include <stdint.h>
#include <stddef.h>


/*!\addtogroup Utility
 * @{
 */

/*!
 * Macro to round up an uint32_t to a multiple or 4
 */
#define FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(X) (((X) + 3u) & ~3u)

/*!
 * Macro to return how many blocks of size 4 to contain
 * a given size, i.e. the smallest integer N so that
 * 4 * N >= X where X is an uint32_t.
 */
#define FASTUIDRAW_NUMBER_BLOCK4_NEEDED(X) (FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(X) >> 2u)

/*!\def FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS
 * Macro that gives the maximum value that can be
 * held with a given number of bits. Caveat:
 * if X is 32 (or higher), bad things may happen
 * from overflow.
 * \param X number bits
 */
#define FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(X) ( (uint32_t(1) << uint32_t(X)) - uint32_t(1) )

/*!\def FASTUIDRAW_MASK
 * Macro that generates a 32-bit mask from number
 * bits and location of bit0 to use
 * \param BIT0 first bit of mask
 * \param NUMBITS nuber bits of mask
 */
#define FASTUIDRAW_MASK(BIT0, NUMBITS) (FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(NUMBITS) << uint32_t(BIT0))

/*!\def FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS_U64
 * Macro that gives the maximum value that can be
 * held with a given number of bits, returning an
 * unsigned 64-bit integer.
 * \param X number bits
 */
#define FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS_U64(X) ( (uint64_t(1) << uint64_t(X)) - uint64_t(1) )

/*!\def FASTUIDRAW_MASK_U64
 * Macro that generates a 64-bit mask from number
 * bits and location of bit0 to use
 * \param BIT0 first bit of mask
 * \param NUMBITS nuber bits of mask
 */
#define FASTUIDRAW_MASK_U64(BIT0, NUMBITS) (FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS_U64(NUMBITS) << uint64_t(BIT0))

/*!\def FASTUIDRAWunused
 * Macro to stop the compiler from reporting
 * an argument as unused. Typically used on
 * those arguments used in FASTUIDRAWassert invocation
 * but otherwise unused.
 * \param X expression of which to ignore the value
 */
#define FASTUIDRAWunused(X) do { (void)(X); } while(0)

/*!\def FASTUIDRAWassert
 * If FASTUIDRAW_DEBUG is defined, checks if the statement
 * is true and if it is not true prints to std::cerr and
 * then aborts. If FastUIDRAW_DEBUG is not defined, then
 * macro is empty (and thus the condition is not evaluated).
 * \param X condition to check
 */
#ifdef FASTUIDRAW_DEBUG
#define FASTUIDRAWassert(X) do {                        \
    if (!(X)) {                                          \
      fastuidraw::assert_fail("Assertion '" #X "' failed", __FILE__, __LINE__);  \
    } } while(0)
#else
#define FASTUIDRAWassert(X)
#endif

/*!\def FASTUIDRAWmessaged_assert
 * Regardless FASTUIDRAW_DEBUG is defined or not, checks if
 * the statement is true and if it is not true prints to
 * std::cerr. If FASTUIDRAW_DEBUG is defined also aborts.
 * \param X condition to check
 * \param Y string to print if condition is false
 */
#define FASTUIDRAWmessaged_assert(X, Y) do {                    \
    if (!(X)) {                                                 \
      fastuidraw::assert_fail(Y, __FILE__, __LINE__);           \
    } } while(0)

/*!\def FASTUIDRAWstatic_assert
 * Conveniance for using static_assert where message
 * is the condition stringified.
 */
#define FASTUIDRAWstatic_assert(X) static_assert(X, #X)

namespace fastuidraw
{
  /*!
   * Private function used by macro FASTUIDRAWassert, do NOT call.
   */
  void
  assert_fail(const char *str, const char *file, int line);
}

namespace fastuidraw
{
  /*!
   * \brief
   * Conveniant typedef for C-style strings.
   */
  typedef const char *c_string;

  /*!
   * \brief
   * Enumeration for simple return codes for functions
   * for success or failure.
   */
  enum return_code
    {
      /*!
       * Routine failed
       */
      routine_fail,

      /*!
       * Routine suceeded
       */
      routine_success
    };

  /*!
   * Returns the floor of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K <= x < 2^{K+1}
   */
  uint32_t
  uint32_log2(uint32_t v);

  /*!
   * Returns the floor of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K <= x < 2^{K+1}
   */
  uint64_t
  uint64_log2(uint64_t v);

  /*!
   * Returns the number of bits required to hold a 32-bit
   * unsigned integer value.
   */
  uint32_t
  number_bits_required(uint32_t v);

  /*!
   * Returns the number of bits required to hold a 32-bit
   * unsigned integer value.
   */
  uint64_t
  uint64_number_bits_required(uint64_t v);

  /*!
   * Returns true if a uint32_t is
   * an exact non-zero power of 2.
   * \param v uint32_t to query
   */
  inline
  bool
  is_power_of_2(uint32_t v)
  {
    return v && !(v & (v - uint32_t(1u)));
  }

  /*!
   * Returns true if a uint64_t is
   * an exact non-zero power of 2.
   * \param v uint64_t to query
   */
  inline
  bool
  uint64_is_power_of_2(uint64_t v)
  {
    return v && !(v & (v - uint64_t(1u)));
  }

  /*!
   * Given v > 0, compute N so that N is a power
   * of 2 and so that N / 2 < v <= N. When v is 0,
   * returns 0.
   */
  inline
  uint32_t
  next_power_of_2(uint32_t v)
  {
    /* taken from Bithacks */
    --v;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    ++v;

    return v;
  }

  /*!
   * Given v > 0, compute N so that N is a power
   * of 2 and so that N / 2 < v <= N. When v is 0,
   * returns 0.
   */
  inline
  uint64_t
  uint64_next_power_of_2(uint64_t v)
  {
    /* taken from Bithacks */
    --v;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    v |= v >> 32u;
    ++v;

    return v;
  }

  /*!
   * Given if a bit should be up or down returns
   * an input value with that bit made to be up
   * or down.
   * \param input_value value to return with the named bit(s) changed
   * \param to_apply if true, return value has bits made up, otherwise has bits down
   * \param bitfield_value bits to make up or down as according to to_apply
   */
  inline
  uint32_t
  apply_bit_flag(uint32_t input_value, bool to_apply,
                 uint32_t bitfield_value)
  {
    return to_apply ?
      input_value | bitfield_value:
      input_value & (~bitfield_value);
  }

  /*!
   * Given if a bit should be up or down returns
   * an input value with that bit made to be up
   * or down.
   * \param input_value value to return with the named bit(s) changed
   * \param to_apply if true, return value has bits made up, otherwise has bits down
   * \param bitfield_value bits to make up or down as according to to_apply
   */
  inline
  uint64_t
  uint64_apply_bit_flag(uint64_t input_value, bool to_apply,
                        uint64_t bitfield_value)
  {
    return to_apply ?
      input_value | bitfield_value:
      input_value & (~bitfield_value);
  }

  /*!
   * Pack the lowest N bits of a value at a bit.
   * \param bit0 bit location of return value at which to pack
   * \param num_bits number of bits from value to pack
   * \param value value to pack
   */
  inline
  uint32_t
  pack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    uint32_t mask;
    mask = (1u << num_bits) - 1u;
    FASTUIDRAWassert(bit0 + num_bits <= 32u);
    FASTUIDRAWassert(value <= mask);
    return (value & mask) << bit0;
  }

  /*!
   * Pack the lowest N bits of a value at a bit.
   * \param bit0 bit location of return value at which to pack
   * \param num_bits number of bits from value to pack
   * \param value value to pack
   */
  inline
  uint64_t
  uint64_pack_bits(uint64_t bit0, uint64_t num_bits, uint64_t value)
  {
    uint64_t mask;
    mask = (uint64_t(1u) << num_bits) - uint64_t(1u);
    FASTUIDRAWassert(bit0 + num_bits <= 64u);
    FASTUIDRAWassert(value <= mask);
    return (value & mask) << bit0;
  }

  /*!
   * Unpack N bits from a bit location.
   * \param bit0 starting bit from which to unpack
   * \param num_bits number bits to unpack
   * \param value value from which to unpack
   */
  inline
  uint32_t
  unpack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    FASTUIDRAWassert(bit0 + num_bits <= 32u);

    uint32_t mask;
    mask = (uint32_t(1u) << num_bits) - uint32_t(1u);
    return (value >> bit0) & mask;
  }

  /*!
   * Unpack N bits from a bit location.
   * \param bit0 starting bit from which to unpack
   * \param num_bits number bits to unpack
   * \param value value from which to unpack
   */
  inline
  uint64_t
  uint64_unpack_bits(uint64_t bit0, uint64_t num_bits, uint64_t value)
  {
    FASTUIDRAWassert(bit0 + num_bits <= 64u);

    uint64_t mask;
    mask = (uint64_t(1u) << num_bits) - uint64_t(1u);
    return (value >> bit0) & mask;
  }

  /*!
   * Returns a float pack into a 32-bit unsigned integer.
   * \param f value to pack
   */
  inline
  uint32_t
  pack_float(float f)
  {
    // casting to const char* first
    // prevents from breaking stricting
    // aliasing rules
    const char *q;
    q = reinterpret_cast<const char*>(&f);
    return *reinterpret_cast<const uint32_t*>(q);
  }

  /*!
   * Unpack a float from a 32-bit unsigned integer.
   * \param v value from which to unpack
   */
  inline
  float
  unpack_float(uint32_t v)
  {
    // casting to const char* first
    // prevents from breaking stricting
    // aliasing rules
    const char * q;
    q = reinterpret_cast<const char*>(&v);
    return *reinterpret_cast<const float*>(q);
  }

  /*!
   * \brief
   * A class reprenting the STL range
   * [m_begin, m_end).
   */
  template<typename T>
  class range_type
  {
  public:
    /*!
     * Typedef to identify template argument type
     */
    typedef T type;

    /*!
     * Iterator to first element
     */
    type m_begin;

    /*!
     * iterator to one past the last element
     */
    type m_end;

    /*!
     * Ctor.
     * \param b value with which to initialize m_begin
     * \param e value with which to initialize m_end
     */
    range_type(T b, T e):
      m_begin(b),
      m_end(e)
    {}

    /*!
     * Empty ctor, m_begin and m_end are uninitialized.
     */
    range_type(void)
    {}

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * m_end - m_begin
     * \endcode
     */
    template<typename W = T>
    W
    difference(void) const
    {
      return m_end - m_begin;
    }

    /*!
     * Increment both \ref m_begin and \ref m_end.
     * \param v value by which to increment
     */
    template<typename W>
    void
    operator+=(const W &v)
    {
      m_begin += v;
      m_end += v;
    }

    /*!
     * Decrement both \ref m_begin and \ref m_end.
     * \param v value by which to decrement
     */
    template<typename W>
    void
    operator-=(const W &v)
    {
      m_begin -= v;
      m_end -= v;
    }

    /*!
     * Make sure that \ref m_begin is no more than \ref m_end,
     * requires that the type T supports comparison < operator
     * and assignment = operator.
     */
    void
    sanitize(void)
    {
      if (m_end < m_begin)
        {
          T t;
          t = m_end;
          m_end = m_begin;
          m_begin = m_end;
        }
    }
  };

  /*!
   * For type T's which support comparison swap, makes
   * sure that the returned \ref range_type has that
   * range_type::m_begin < range_type::m_end
   */
  template<typename T>
  range_type<T>
  create_range(T a, T b)
  {
    if (a < b)
      {
        return range_type<T>(a, b);
      }
    else
      {
        return range_type<T>(b, a);
      }
  }

  /*!
   * \brief
   * Class for which copy ctor and assignment operator
   * are private functions.
   */
  class noncopyable
  {
  public:
    noncopyable(void)
    {}

  private:
    noncopyable(const noncopyable &obj) = delete;

    noncopyable&
    operator=(const noncopyable &rhs) = delete;
  };

  /*!
   * \brief
   * Class for type traits to indicate true.
   * Functionally, a simplified version of
   * std::true_type.
   */
  class true_type
  {
  public:
    /*!
     * Typedef for value_type.
     */
    typedef bool value_type;

    /*!
     * constexpr for the value.
     */
    static constexpr value_type value = true;

    /*!
     * implicit cast operator to bool to return false.
     */
    constexpr value_type operator()() const noexcept
    {
      return false;
    }
  };

  /*!
   * \brief
   * Class for type traits to indicate true.
   * Functionally, a simplified version of
   * std::false_type.
   */
  class false_type
  {
  public:
    /*!
     * Typedef for value_type.
     */
    typedef bool value_type;

    /*!
     * constexpr for the value.
     */
    static constexpr value_type value = false;

    /*!
     * implicit cast operator to bool to return false.
     */
    constexpr value_type operator()() const noexcept
    {
      return false;
    }
  };

  /*!
   * Provideds functionality of std::remove_const so
   * that we do not depend on std.
   */
  template<typename T>
  class remove_const
  {
  public:
    /*!
     * The type of \ref type will be the same
     * as T but with the const-ness removed.
     */
    typedef T type;
  };

  /*!
   * Provideds functionality of std::remove_const so
   * that we do not depend on std.
   */
  template<typename T>
  class remove_const<T const>
  {
  public:
    /*!
     * The type of \ref type will be the same
     * as T but with the const-ness removed.
     */
    typedef T type;
  };

  /*!
   * Provideds functionality of std::is_const so
   * that we do not depend on std.
   */
  template<typename T>
  class is_const : public false_type
  {
  };

  ///@cond
  template<typename T>
  class is_const<T const> : public true_type
  {
  };
  ///@endcond

  /*!
   * Typedef to give same const-ness of type T
   * to a type S.
   */
  template<typename T, typename S>
  class same_const
  {
  public:
    /*!
     * The type of \ref type will be the same
     * as S but with the const-ness of T.
     */
    typedef typename remove_const<S>::type type;
  };

  ///@cond
  template<typename T, typename S>
  class same_const<T const, S>
  {
  public:
    typedef typename remove_const<S>::type const type;
  };
  ///@endcond

}
/*! @} */

#endif
