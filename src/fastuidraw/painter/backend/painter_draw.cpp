/*!
 * \file painter_draw.cpp
 * \brief file painter_draw.cpp
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
#include <fastuidraw/painter/backend/painter_draw.hpp>

namespace
{

  enum map_status_t
    {
      status_mapped,
      status_waiting_for_actions_to_complete,
      status_unmapped,
    };

  class PainterDrawPrivate
  {
  public:
    explicit
    PainterDrawPrivate(fastuidraw::PainterDraw *p):
      m_map_status(status_mapped),
      m_action_count(0),
      m_attribs_written(0),
      m_indices_written(0),
      m_data_store_written(0),
      m_p(p)
    {}

    enum map_status_t m_map_status;
    unsigned int m_action_count;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::DelayedAction> > m_actions;
    unsigned int m_attribs_written, m_indices_written, m_data_store_written;
    fastuidraw::PainterDraw *m_p;
  };

  class DelayedActionPrivate
  {
  public:
    DelayedActionPrivate(void):
      m_cmd(nullptr),
      m_slot(0)
    {}

    PainterDrawPrivate *m_cmd;
    unsigned int m_slot;
  };
}

/////////////////////////////////////////
// fastuidraw::PainterDraw::DelayedAction methods
fastuidraw::PainterDraw::DelayedAction::
DelayedAction(void)
{
  m_d = FASTUIDRAWnew DelayedActionPrivate();
}

fastuidraw::PainterDraw::DelayedAction::
~DelayedAction(void)
{
  DelayedActionPrivate *d;
  d = static_cast<DelayedActionPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterDraw::DelayedAction::
perform_action(void)
{
  DelayedActionPrivate *d;
  d = static_cast<DelayedActionPrivate*>(m_d);

  FASTUIDRAWassert(d->m_cmd != nullptr);
  FASTUIDRAWassert(d->m_slot < d->m_cmd->m_actions.size());
  FASTUIDRAWassert(d->m_cmd->m_actions[d->m_slot] == this);

  action(d->m_cmd->m_p);

  d->m_cmd->m_actions[d->m_slot] = reference_counted_ptr<DelayedAction>();
  d->m_cmd->m_action_count--;
  if (d->m_cmd->m_action_count == 0 && d->m_cmd->m_map_status == status_waiting_for_actions_to_complete)
    {
      d->m_cmd->m_p->complete_unmapping();
    }

  d->m_cmd = nullptr;
  d->m_slot = 0;
}


/////////////////////////////////////////
// fastuidraw::PainterDraw methods
fastuidraw::PainterDraw::
PainterDraw(void)
{
  m_d = FASTUIDRAWnew PainterDrawPrivate(this);
}

fastuidraw::PainterDraw::
~PainterDraw(void)
{
  PainterDrawPrivate *d;
  d = static_cast<PainterDrawPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PainterDraw::
add_action(const reference_counted_ptr<DelayedAction> &h) const
{
  PainterDrawPrivate *d;
  DelayedActionPrivate *hd;

  FASTUIDRAWassert(h);

  d = static_cast<PainterDrawPrivate*>(m_d);
  hd = static_cast<DelayedActionPrivate*>(h->m_d);

  FASTUIDRAWassert(hd->m_cmd == nullptr);
  FASTUIDRAWassert(d->m_map_status != status_unmapped);

  hd->m_cmd = d;
  hd->m_slot = d->m_actions.size();
  ++d->m_action_count;
  d->m_actions.push_back(h);
}

void
fastuidraw::PainterDraw::
unmap(unsigned int attributes_written,
      unsigned int indices_written,
      unsigned int data_store_written)
{
  PainterDrawPrivate *d;
  d = static_cast<PainterDrawPrivate*>(m_d);

  FASTUIDRAWassert(d->m_map_status == status_mapped);

  d->m_attribs_written = attributes_written;
  d->m_indices_written = indices_written;
  d->m_data_store_written = data_store_written;
  d->m_map_status = status_waiting_for_actions_to_complete;
  if (d->m_action_count == 0)
    {
      complete_unmapping();
    }
}

void
fastuidraw::PainterDraw::
complete_unmapping(void)
{
  PainterDrawPrivate *d;
  d = static_cast<PainterDrawPrivate*>(m_d);

  FASTUIDRAWassert(d->m_map_status == status_waiting_for_actions_to_complete);
  FASTUIDRAWassert(d->m_action_count == 0);
  unmap_implement(d->m_attribs_written, d->m_indices_written, d->m_data_store_written);
  d->m_map_status = status_unmapped;
  m_attributes.reset();
  m_header_attributes.reset();
  m_indices.reset();
  m_store.reset();
}

bool
fastuidraw::PainterDraw::
unmapped(void) const
{
  PainterDrawPrivate *d;
  d = static_cast<PainterDrawPrivate*>(m_d);
  return d->m_map_status == status_unmapped;
}
