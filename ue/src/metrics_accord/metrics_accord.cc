/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2015 The srsUE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
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

#include "metrics_accord/metrics_accord.h"
#include "metrics_accord/accord_server.h"

namespace srsue{

metrics_accord::metrics_accord()
{
  server = NULL;
}

metrics_accord::~metrics_accord()
{
  if(server)
    delete server;
}

void metrics_accord::init(ue_metrics_interface *u, int port)
{
  server = new accord_server(u, port);
  server->start();
}

void metrics_accord::stop()
{
  server->stop();
}

void metrics_accord::toggle_print(bool b)
{
  server->toggle_print(b);
}

} // namespace srsue
