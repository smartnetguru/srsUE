/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

/******************************************************************************
 * File:        metrics_accord.h
 * Description: UE metrics TCP server. Provides metrics in XML format.
 *****************************************************************************/

#ifndef METRICS_ACCORD_H
#define METRICS_ACCORD_H

#include "ue_metrics_interface.h"

class accord_server;

namespace srsue {

class metrics_accord
{
public:
  metrics_accord();
  ~metrics_accord();

  void init(ue_metrics_interface *u, int port=33335);
  void stop();
  void toggle_print(bool b);

private:
  accord_server *server;
};

} // namespace srsue

#endif // METRICS_ACCORD_H
