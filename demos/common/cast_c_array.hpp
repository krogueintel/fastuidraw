#ifndef FASTUIDRAW_DEMO_CAST_C_ARRAY_HPP
#define FASTUIDRAW_DEMO_CAST_C_ARRAY_HPP

#include <vector>
#include <fastuidraw/util/c_array.hpp>

template<typename T>
fastuidraw::c_array<const T>
cast_c_array(const std::vector<T> &p)
{
  return (p.empty()) ?
    fastuidraw::c_array<const T>() :
    fastuidraw::c_array<const T>(&p[0], p.size());
}

template<typename T>
fastuidraw::c_array<const T>
const_cast_c_array(const std::vector<T> &p)
{
  return (p.empty()) ?
    fastuidraw::c_array<const T>() :
    fastuidraw::c_array<const T>(&p[0], p.size());
}

template<typename T>
fastuidraw::c_array<T>
cast_c_array(std::vector<T> &p)
{
  return (p.empty()) ?
    fastuidraw::c_array<T>() :
    fastuidraw::c_array<T>(&p[0], p.size());
}

#endif
