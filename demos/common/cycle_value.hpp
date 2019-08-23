#ifndef FASTUIDRAW_DEMO_CYCLE_VALUE_HPP
#define FASTUIDRAW_DEMO_CYCLE_VALUE_HPP

void
cycle_value(unsigned int &value, bool decrement, unsigned int limit_value);

template<typename T>
void
cycle_value(T &value, bool decrement, unsigned int limit_value)
{
  unsigned int v(value);
  cycle_value(v, decrement, limit_value);
  value = T(v);
}

#endif
