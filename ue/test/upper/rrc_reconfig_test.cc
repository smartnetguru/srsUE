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

#include <iostream>
#include <srslte/srslte.h>
#include "common/log_stdout.h"
#include "liblte/hdr/liblte_rrc.h"
#include "liblte/hdr/liblte_mme.h"

void nas_test() {
  srslte::log_stdout log1("NAS");
  log1.set_level(srslte::LOG_LEVEL_DEBUG);
  log1.set_hex_limit(-1);

  uint32_t nas_message_len  = 63;
  uint8_t  nas_message[128] = {0x27,0x3f,0xc5,0x09,0x08,0x01,0x07,0x42,0x01,0x49,0x06,0x00,0x00,0xf1,0x10,0x00,
                               0x01,0x00,0x18,0x52,0x01,0xc1,0x01,0x09,0x0c,0x0b,0x79,0x65,0x73,0x69,0x6e,0x74,
                               0x65,0x72,0x6e,0x65,0x74,0x05,0x01,0x0a,0x0a,0x14,0x08,0x50,0x0b,0xf6,0x00,0xf1,
                               0x10,0x00,0x01,0x01,0x72,0xea,0xf1,0x07,0x17,0x42,0x59,0x49,0x64,0x01,0x01};

  uint8                               pd;
  uint8                               msg_type;
  LIBLTE_BYTE_MSG_STRUCT              buf;
  LIBLTE_MME_ATTACH_ACCEPT_MSG_STRUCT attach_accept;
  LIBLTE_MME_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST_MSG_STRUCT  act_def_eps_bearer_context_req;

  memcpy(buf.msg, nas_message, nas_message_len);
  buf.N_bytes = nas_message_len;
  liblte_mme_parse_msg_header(&buf, &pd, &msg_type);
  switch(msg_type)
  {
  case LIBLTE_MME_MSG_TYPE_ATTACH_ACCEPT:
      liblte_mme_unpack_attach_accept_msg(&buf, &attach_accept);
      liblte_mme_unpack_activate_default_eps_bearer_context_request_msg(&attach_accept.esm_msg, &act_def_eps_bearer_context_req);
      break;
  case LIBLTE_MME_MSG_TYPE_ATTACH_REJECT:
      break;
  case LIBLTE_MME_MSG_TYPE_AUTHENTICATION_REQUEST:

      break;
  case LIBLTE_MME_MSG_TYPE_AUTHENTICATION_REJECT:

      break;
  case LIBLTE_MME_MSG_TYPE_IDENTITY_REQUEST:

      break;
  case LIBLTE_MME_MSG_TYPE_SECURITY_MODE_COMMAND:

      break;
  case LIBLTE_MME_MSG_TYPE_SERVICE_REJECT:

      break;
  case LIBLTE_MME_MSG_TYPE_ESM_INFORMATION_REQUEST:

      break;
  case LIBLTE_MME_MSG_TYPE_EMM_INFORMATION:

      break;
  default:
      break;
  }
}

void basic_test() {
  srslte::log_stdout log1("RRC");
  log1.set_level(srslte::LOG_LEVEL_DEBUG);
  log1.set_hex_limit(-1);

  LIBLTE_BIT_MSG_STRUCT           bit_buf;
  LIBLTE_RRC_DL_CCCH_MSG_STRUCT   dl_ccch_msg;

  uint32_t rrc_message_len  = 33;
  uint8_t  rrc_message[128] = {0x60,0x72,0x98,0x08,0xfd,0x50,0x39,0x83,0x81,0xb9,0x88,0x7b,0x08,
                               0x18,0x04,0x01,0xe5,0xc1,0x73,0xf0,0x43,0x91,0x30,0x00,0x09,0xd0,
                               0x20,0x00,0x8d,0x81,0x80,0x60,0x00};

  srslte_bit_unpack_vector(rrc_message, bit_buf.msg, rrc_message_len*8);
  bit_buf.N_bits = rrc_message_len*8;
  liblte_rrc_unpack_dl_ccch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &dl_ccch_msg);

  printf("done\n");
}


int main(int argc, char **argv) {
  basic_test();
  nas_test();
}
