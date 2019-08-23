#ifndef FASTUIDRAW_DEMO_CELL_GROUP_HPP
#define FASTUIDRAW_DEMO_CELL_GROUP_HPP

#include "ostream_utility.hpp"
#include "PainterWidget.hpp"

using namespace fastuidraw;
class CellGroup:public PainterWidget
{
public:
  CellGroup(CellGroup *qparent):
    PainterWidget(qparent)
  {
    m_clipped = false;
  }

  virtual
  ~CellGroup(void)
  {}

  vec2 m_bb_min, m_bb_max;

protected:

  vec2 m_bb_against_parent_min, m_bb_against_parent_max;

  virtual
  void
  pre_paint(void);

};

#endif
