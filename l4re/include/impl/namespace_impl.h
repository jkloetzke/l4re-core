/**
 * \file
 * \brief  Namespace client stub implementation
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include <l4/re/namespace>

#include <l4/util/util.h>
#include <l4/sys/cxx/ipc_client>
#include <l4/sys/assert.h>

#include <string.h>

L4_RPC_DEF(L4Re::Namespace::query);
L4_RPC_DEF(L4Re::Namespace::register_obj);
L4_RPC_DEF(L4Re::Namespace::unlink);

namespace L4Re {

long
Namespace::_query(char const *name, unsigned len,
                  L4::Cap<void> const &target,
                  l4_umword_t *local_id, bool iterate) const noexcept
{
  l4_assert(target.is_valid());

  L4::Cap<Namespace> ns = c();
  L4::Ipc::Array<char const, unsigned long> _name(len, name);

  while (_name.length > 0)
    {
      L4::Ipc::Snd_fpage cap;
      L4::Opcode dummy;
      int err = query_t::call(ns, _name,
                              L4::Ipc::Small_buf(target.cap(),
                                                 local_id
                                                   ? L4_RCV_ITEM_LOCAL_ID
                                                   : 0),
                              cap, dummy, _name);
      if (err < 0)
        return err;

      bool const partly = err & Partly_resolved;
      if (cap.id_received())
        {
          *local_id = cap.data();
          return _name.length;
        }

      if (partly && iterate)
        ns = L4::cap_cast<Namespace>(target);
      else
        return err;
    }

  return _name.length;
}

long
Namespace::query(char const *name, unsigned len, L4::Cap<void> const &target,
                 int timeout, l4_umword_t *local_id, bool iterate) const noexcept
{
  if (L4_UNLIKELY(len == 0))
    return -L4_EINVAL;

  if (L4_UNLIKELY(timeout < 0))
    return -L4_EINVAL;

  long ret;
  long rem = timeout;
  long to = 0;

  if (rem)
    to = 10;

  do
    {
      ret = _query(name, len, target, local_id, iterate);

      if (ret >= 0)
        return ret;

      if (L4_UNLIKELY(ret != -L4_EAGAIN))
        return ret;

      if (rem == to)
        return ret;

      l4_sleep(to);

      if (rem > 0)
        {
          rem -= to;
          if (rem < 0)
            {
              to = rem = 0;
              continue; // one final try
            }
        }

      if (to < 100)
        to += to;
    }
  while (486);
}

long
Namespace::query(char const *name, L4::Cap<void> const &target,
                 int timeout, l4_umword_t *local_id,
                 bool iterate) const noexcept
{
  return query(name, __builtin_strlen(name), target,
               timeout, local_id, iterate);
}

}
