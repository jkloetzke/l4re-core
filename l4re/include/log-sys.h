/**
 * \file
 * \brief   Log protocol definition
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

namespace L4Re
{
  namespace Log_
  {
    /**
     * \brief Logging-service communication-protocol opcodes.
     * \ingroup api_l4re_protocols
     * \internal
     */
    enum Opcodes { Print };
  };
};
