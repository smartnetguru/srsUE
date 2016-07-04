#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>

#include "srslte/utils/debug.h"
#include "mac/mac.h"
#include "phy/phy.h"
#include "common/threads.h"
#include "common/common.h"
#include "common/buffer_pool.h"
#include "common/log_stdout.h"
#include "upper/rlc.h"
#include "upper/rrc.h"
#include "radio/radio.h"

#define START_TUNTAP
#define USE_RADIO

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/

#define LCID 3

typedef struct {
  uint32_t rnti; 
  float rx_freq; 
  float tx_freq; 
  float rx_gain;
  float tx_gain;
  std::string ip_address;
}prog_args_t;

uint32_t srsapps_verbose = 0; 

prog_args_t prog_args; 

void args_default(prog_args_t *args) {
  args->rnti    = 30; 
  args->tx_freq = 2.505e9;
  args->rx_freq = 2.625e9;
  args->rx_gain = 50.0; 
  args->tx_gain = 80.0; 
  args->ip_address = "192.168.3.2";
}

void usage(prog_args_t *args, char *prog) {
  printf("Usage: %s [gGIrfFv]\n", prog);
  printf("\t-f RX frequency [Default %.1f MHz]\n", args->rx_freq/1e6);
  printf("\t-F TX frequency [Default %.1f MHz]\n", args->tx_freq/1e6);
  printf("\t-g RX gain [Default %.1f]\n", args->rx_gain);
  printf("\t-G TX gain [Default %.1f]\n", args->tx_gain);
  printf("\t-I IP address [Default %s]\n", args->ip_address.c_str());
  printf("\t-r C-RNTI [Default %d]\n", args->rnti);
  printf("\t-v [increase verbosity, default none]\n");
}

void parse_args(prog_args_t *args, int argc, char **argv) {
  int opt;
  args_default(args);
  while ((opt = getopt(argc, argv, "gGfFIrv")) != -1) {
    switch (opt) {
    case 'g':
      args->rx_gain = atof(argv[optind]);
      break;
    case 'G':
      args->tx_gain = atof(argv[optind]);
      break;
    case 'f':
      args->rx_freq = atof(argv[optind]);
      break;
    case 'F':
      args->tx_freq = atof(argv[optind]);
      break;
    case 'I':
      args->ip_address = argv[optind];
      break;
    case 'r':
      args->rnti = atoi(argv[optind]);
      break;
    case 'v':
      srsapps_verbose++;
      break;
    default:
      usage(args, argv[0]);
      exit(-1);
    }
  }
  if (args->rx_freq < 0 || args->tx_freq < 0) {
    usage(args, argv[0]);
    exit(-1);
  }
}
                            
int setup_if_addr(char *ip_addr);
  
// Define dummy RLC always transmitts
class tester : public srsue::pdcp_interface_rlc, 
               public srsue::rrc_interface_rlc,
               public srsue::rrc_interface_phy,
               public srsue::rrc_interface_mac,
               public srsue::ue_interface,
               public thread
{
public:
  
  tester() {
    state = srsue::RRC_STATE_SIB1_SEARCH;
  }
  
  void init(srsue::phy *phy_, srsue::mac *mac_, srsue::rlc *rlc_, srslte::log *log_h_, std::string ip_address) {
    log_h = log_h_; 
    rlc   = rlc_; 
    mac   = mac_; 
    phy   = phy_; 
    
#ifdef START_TUNTAP
    if (init_tuntap((char*) ip_address.c_str())) {
      log_h->error("Initiating IP address\n");
    }
#endif

    pool = srsue::buffer_pool::get_instance();
    
    // Start reader thread
    running=true; 
    start();

  }
  

  void sib_search()
  {
    bool      searching = true;
    uint32_t  tti ;
    uint32_t  si_win_start, si_win_len;
    uint16_t  period;
    uint32_t  nof_sib1_trials = 0; 
    const int SIB1_SEARCH_TIMEOUT = 30; 

    while(searching)
    {
      switch(state)
      {
      case srsue::RRC_STATE_SIB1_SEARCH:
        // Instruct MAC to look for SIB1
        while(!phy->status_is_sync()){
          usleep(50000);
        }
        usleep(10000); 
        tti          = mac->get_current_tti();
        si_win_start = sib_start_tti(tti, 2, 5);
        mac->bcch_start_rx(si_win_start, 1);
        log_h->info("Instructed MAC to search for SIB1, win_start=%d, win_len=%d\n",
                      si_win_start, 1);
        nof_sib1_trials++;
        if (nof_sib1_trials >= SIB1_SEARCH_TIMEOUT) {
          log_h->info("Timeout while searching for SIB1. Resynchronizing SFN...\n");
          log_h->console("Timeout while searching for SIB1. Resynchronizing SFN...\n");
          phy->resync_sfn();
          nof_sib1_trials = 0; 
        }
        break;
      case srsue::RRC_STATE_SIB2_SEARCH:
        // Instruct MAC to look for SIB2
        usleep(10000);
        tti          = mac->get_current_tti();
        period       = liblte_rrc_si_periodicity_num[sib1.sched_info[0].si_periodicity];
        si_win_start = sib_start_tti(tti, period, 0);
        si_win_len   = liblte_rrc_si_window_length_num[sib1.si_window_length];

        mac->bcch_start_rx(si_win_start, si_win_len);
        log_h->info("Instructed MAC to search for SIB2, win_start=%d, win_len=%d\n",
                      si_win_start, si_win_len);

        break;
      default:
        searching = false;
        break;
      }
      usleep(100000);
    }
  }
  
  bool is_sib_received() {
    return state == srsue::RRC_STATE_WAIT_FOR_CON_SETUP; 
  }


  void release_pucch_srs() {}
  void ra_problem() {}
  void write_pdu_bcch_bch(srsue::byte_buffer_t *pdu) {}
  void write_pdu_bcch_dlsch(srsue::byte_buffer_t *pdu) 
  {
    log_h->info_hex(pdu->msg, pdu->N_bytes, "BCCH DLSCH message received.");
    log_h->info("BCCH DLSCH message Stack latency: %ld us\n", pdu->get_latency_us());
    LIBLTE_RRC_BCCH_DLSCH_MSG_STRUCT dlsch_msg;
    srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
    bit_buf.N_bits = pdu->N_bytes*8;
    pool->deallocate(pdu);
    liblte_rrc_unpack_bcch_dlsch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &dlsch_msg);

    if (dlsch_msg.N_sibs > 0) {
      if (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1 == dlsch_msg.sibs[0].sib_type && srsue::RRC_STATE_SIB1_SEARCH == state) {
        // Handle SIB1
        memcpy(&sib1, &dlsch_msg.sibs[0].sib.sib1, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT));
        log_h->info("SIB1 received, CellID=%d, si_window=%d, sib2_period=%d\n",
                      sib1.cell_id&0xfff,
                      liblte_rrc_si_window_length_num[sib1.si_window_length],
                      liblte_rrc_si_periodicity_num[sib1.sched_info[0].si_periodicity]);
        std::stringstream ss;
        for(int i=0;i<sib1.N_plmn_ids;i++){
          ss << " PLMN Id: MCC " << sib1.plmn_id[i].id.mcc << " MNC " << sib1.plmn_id[i].id.mnc;
        }
        log_h->console("SIB1 received, CellID=%d, %s\n",
                        sib1.cell_id&0xfff,
                        ss.str().c_str());
        
        state = srsue::RRC_STATE_SIB2_SEARCH;
        mac->bcch_stop_rx();
        //TODO: Use all SIB1 info

      } else if (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2 == dlsch_msg.sibs[0].sib_type && srsue::RRC_STATE_SIB2_SEARCH == state) {
        // Handle SIB2
        memcpy(&sib2, &dlsch_msg.sibs[0].sib.sib2, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT));
        log_h->console("SIB2 received\n");
        log_h->info("SIB2 received\n");
        state = srsue::RRC_STATE_WAIT_FOR_CON_SETUP;
        mac->bcch_stop_rx();
        apply_sib2_configs();
      }
    }
  }
  void write_pdu_pcch(srsue::byte_buffer_t *sdu) {}
  void max_retx_attempted(){}
  void in_sync() {};
  void out_of_sync() {};

  void write_pdu(uint32_t lcid, srsue::byte_buffer_t *sdu)
  {
    srslte_vec_fprint_byte(stdout, sdu->msg, sdu->N_bytes);
    int n = write(tun_fd, sdu->msg, sdu->N_bytes);
    if (n != sdu->N_bytes) {
      log_h->error("TUN/TAP write failure n=%d, nof_bytes=%d\n", n, sdu->N_bytes);
      return; 
    }
    log_h->info("Wrote %d bytes to TUN fd=%d\n", sdu->N_bytes, tun_fd);      
    pool->deallocate(sdu);
  }
  
private:
  int tun_fd;
  bool running; 
  srslte::log *log_h;
  srsue::buffer_pool *pool;
  srsue::rlc *rlc; 
  srsue::mac *mac; 
  srsue::phy *phy;  
  srsue::bit_buffer_t bit_buf;
  srsue::rrc_state_t state; 
  LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT sib1;
  LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT sib2;

  
  // Determine SI messages scheduling as in 36.331 5.2.3 Acquisition of an SI message
  uint32_t sib_start_tti(uint32_t tti, uint32_t period, uint32_t x) {
    return (period*10*(1+tti/(period*10))+x)%10240; // the 1 means next opportunity
  }
    
  int init_tuntap(char *ip_address) {
    tun_fd = setup_if_addr(ip_address);
    if (tun_fd<0) {
      fprintf(stderr, "Error setting up IP %s\n", ip_address);
      return -1;
    }
    
    printf("Created tun/tap interface at IP %s\n", ip_address);

    return 0; 
  }
  
  void run_thread() {
    struct iphdr   *ip_pkt;
    uint32_t        idx = 0;
    int32_t         N_bytes;
    srsue::byte_buffer_t  *pdu = pool->allocate();

    log_h->info("TUN/TAP reader thread running\n");

    while(running) {
      N_bytes = read(tun_fd, &pdu->msg[idx], SRSUE_MAX_BUFFER_SIZE_BYTES-SRSUE_BUFFER_HEADER_OFFSET);
      log_h->info("Read %d bytes from TUN fd=%d\n", N_bytes, tun_fd);
      if(N_bytes > 0)
      {
        pdu->N_bytes = idx + N_bytes;
        ip_pkt       = (struct iphdr*)pdu->msg;

        // Check if entire packet was received
        if(ntohs(ip_pkt->tot_len) == pdu->N_bytes)
        {
          log_h->info_hex(pdu->msg, pdu->N_bytes, "UL PDU");

          // Send PDU directly to PDCP
          pdu->timestamp = bpt::microsec_clock::local_time();
          rlc->write_sdu(LCID, pdu);
          
          pdu = pool->allocate();
          idx = 0;
        } else{
          idx += N_bytes;
        }
      }else{
        log_h->error("Failed to read from TUN interface - gw receive thread exiting.\n");
        break;
      }
    }
  }
  
  
  void apply_sib2_configs()
  {
    if(srsue::RRC_STATE_WAIT_FOR_CON_SETUP != state){
      log_h->error("State must be RRC_STATE_WAIT_FOR_CON_SETUP to handle SIB2.\n");
      return;
    }

    // RACH-CONFIGCOMMON
    if (sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.present) {
      mac->set_param(srsue::mac_interface_params::RA_NOFGROUPAPREAMBLES,
                    liblte_rrc_message_size_group_a_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.size_of_ra]);
      mac->set_param(srsue::mac_interface_params::RA_MESSAGESIZEA,
                    liblte_rrc_message_size_group_a_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_size]);
      mac->set_param(srsue::mac_interface_params::RA_MESSAGEPOWEROFFSETB,
                    liblte_rrc_message_power_offset_group_b_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_pwr_offset_group_b]);
    }
    mac->set_param(srsue::mac_interface_params::RA_NOFPREAMBLES,
                  liblte_rrc_number_of_ra_preambles_num[sib2.rr_config_common_sib.rach_cnfg.num_ra_preambles]);
    mac->set_param(srsue::mac_interface_params::RA_POWERRAMPINGSTEP,
                  liblte_rrc_power_ramping_step_num[sib2.rr_config_common_sib.rach_cnfg.pwr_ramping_step]);
    mac->set_param(srsue::mac_interface_params::RA_INITRECEIVEDPOWER,
                  liblte_rrc_preamble_initial_received_target_power_num[sib2.rr_config_common_sib.rach_cnfg.preamble_init_rx_target_pwr]);
    mac->set_param(srsue::mac_interface_params::RA_PREAMBLETRANSMAX,
                  liblte_rrc_preamble_trans_max_num[sib2.rr_config_common_sib.rach_cnfg.preamble_trans_max]);
    mac->set_param(srsue::mac_interface_params::RA_RESPONSEWINDOW,
                  liblte_rrc_ra_response_window_size_num[sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size]);
    mac->set_param(srsue::mac_interface_params::RA_CONTENTIONTIMER,
                  liblte_rrc_mac_contention_resolution_timer_num[sib2.rr_config_common_sib.rach_cnfg.mac_con_res_timer]);
    mac->set_param(srsue::mac_interface_params::HARQ_MAXMSG3TX,
                  sib2.rr_config_common_sib.rach_cnfg.max_harq_msg3_tx);

    log_h->info("Set RACH ConfigCommon: NofPreambles=%d, ResponseWindow=%d, ContentionResolutionTimer=%d ms\n",
          liblte_rrc_number_of_ra_preambles_num[sib2.rr_config_common_sib.rach_cnfg.num_ra_preambles],
          liblte_rrc_ra_response_window_size_num[sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size],
          liblte_rrc_mac_contention_resolution_timer_num[sib2.rr_config_common_sib.rach_cnfg.mac_con_res_timer]);

    // PDSCH ConfigCommon
    phy->set_param(srsue::phy_interface_params::PDSCH_RSPOWER,
                  sib2.rr_config_common_sib.pdsch_cnfg.rs_power);
    phy->set_param(srsue::phy_interface_params::PDSCH_PB,
                  sib2.rr_config_common_sib.pdsch_cnfg.p_b);

    // PUSCH ConfigCommon
    phy->set_param(srsue::phy_interface_params::PUSCH_EN_64QAM, 0); // This will be set after attach
    phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_OFFSET,
                  sib2.rr_config_common_sib.pusch_cnfg.pusch_hopping_offset);
    phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_N_SB,
                  sib2.rr_config_common_sib.pusch_cnfg.n_sb);
    phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_INTRA_SF,
                  sib2.rr_config_common_sib.pusch_cnfg.hopping_mode == LIBLTE_RRC_HOPPING_MODE_INTRA_AND_INTER_SUBFRAME?1:0);
    phy->set_param(srsue::phy_interface_params::DMRS_GROUP_HOPPING_EN,
                  sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_hopping_enabled?1:0);
    phy->set_param(srsue::phy_interface_params::DMRS_SEQUENCE_HOPPING_EN,
                  sib2.rr_config_common_sib.pusch_cnfg.ul_rs.sequence_hopping_enabled?1:0);
    phy->set_param(srsue::phy_interface_params::PUSCH_RS_CYCLIC_SHIFT,
                  sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift);
    phy->set_param(srsue::phy_interface_params::PUSCH_RS_GROUP_ASSIGNMENT,
                  sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch);

    log_h->info("Set PUSCH ConfigCommon: HopOffset=%d, RSGroup=%d, RSNcs=%d, N_sb=%d\n",
      sib2.rr_config_common_sib.pusch_cnfg.pusch_hopping_offset,
      sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch,
      sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift,
      sib2.rr_config_common_sib.pusch_cnfg.n_sb);

    // PUCCH ConfigCommon
    phy->set_param(srsue::phy_interface_params::PUCCH_DELTA_SHIFT,
                  liblte_rrc_delta_pucch_shift_num[sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift]);
    phy->set_param(srsue::phy_interface_params::PUCCH_CYCLIC_SHIFT,
                  sib2.rr_config_common_sib.pucch_cnfg.n_cs_an);
    phy->set_param(srsue::phy_interface_params::PUCCH_N_PUCCH_1,
                  sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an);
    phy->set_param(srsue::phy_interface_params::PUCCH_N_RB_2,
                  sib2.rr_config_common_sib.pucch_cnfg.n_rb_cqi);

    log_h->info("Set PUCCH ConfigCommon: DeltaShift=%d, CyclicShift=%d, N1=%d, NRB=%d\n",
          liblte_rrc_delta_pucch_shift_num[sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift],
          sib2.rr_config_common_sib.pucch_cnfg.n_cs_an,
          sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an,
          sib2.rr_config_common_sib.pucch_cnfg.n_rb_cqi);

    // UL Power control config ConfigCommon
      phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_NOMINAL_PUSCH, sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_ALPHA, 
                  round(10*liblte_rrc_ul_power_control_alpha_num[sib2.rr_config_common_sib.ul_pwr_ctrl.alpha]));
    phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_NOMINAL_PUCCH, sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F1, 
                  liblte_rrc_delta_f_pucch_format_1_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1]);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F1B, 
                  liblte_rrc_delta_f_pucch_format_1b_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1b]);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2, 
                  liblte_rrc_delta_f_pucch_format_2_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2]);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2A, 
                  liblte_rrc_delta_f_pucch_format_2a_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2a]);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2B, 
                  liblte_rrc_delta_f_pucch_format_2b_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2b]);
    phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_MSG3, sib2.rr_config_common_sib.ul_pwr_ctrl.delta_preamble_msg3);

    
    // PRACH Configcommon
    phy->set_param(srsue::phy_interface_params::PRACH_ROOT_SEQ_IDX,
                  sib2.rr_config_common_sib.prach_cnfg.root_sequence_index);
    phy->set_param(srsue::phy_interface_params::PRACH_HIGH_SPEED_FLAG,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag?1:0);
    phy->set_param(srsue::phy_interface_params::PRACH_FREQ_OFFSET,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset);
    phy->set_param(srsue::phy_interface_params::PRACH_ZC_CONFIG,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config);
    phy->set_param(srsue::phy_interface_params::PRACH_CONFIG_INDEX,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index);

    log_h->info("Set PRACH ConfigCommon: SeqIdx=%d, HS=%d, FreqOffset=%d, ZC=%d, ConfigIndex=%d\n",
                  sib2.rr_config_common_sib.prach_cnfg.root_sequence_index,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag?1:0,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config,
                  sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index);

    // SRS ConfigCommon
    if (sib2.rr_config_common_sib.srs_ul_cnfg.present) {
      phy->set_param(srsue::phy_interface_params::SRS_CS_BWCFG, sib2.rr_config_common_sib.srs_ul_cnfg.bw_cnfg);
      phy->set_param(srsue::phy_interface_params::SRS_CS_SFCFG, sib2.rr_config_common_sib.srs_ul_cnfg.subfr_cnfg);
      phy->set_param(srsue::phy_interface_params::SRS_CS_ACKNACKSIMUL, sib2.rr_config_common_sib.srs_ul_cnfg.ack_nack_simul_tx);
    }

    log_h->info("Set SRS ConfigCommon: BW-Configuration=%d, SF-Configuration=%d, ACKNACK=%d\n",
                  sib2.rr_config_common_sib.srs_ul_cnfg.bw_cnfg,
                  sib2.rr_config_common_sib.srs_ul_cnfg.subfr_cnfg,
                  sib2.rr_config_common_sib.srs_ul_cnfg.ack_nack_simul_tx);
    
    phy->configure_ul_params();
  }


  
};


// Create classes
srslte::log_stdout log_out("ALL");
srsue::phy my_phy;
srsue::mac my_mac;
srsue::rlc rlc;
srslte::radio my_radio; 

// Local classes for testing
tester my_tester; 

int main(int argc, char *argv[])
{
  
  parse_args(&prog_args, argc, argv);

  if (srsapps_verbose == 1) {
    log_out.set_level(srslte::LOG_LEVEL_INFO);
    log_out.set_hex_limit(100);
    printf("Log level info\n");
  }
  if (srsapps_verbose == 2) {
    log_out.set_level(srslte::LOG_LEVEL_DEBUG);
    log_out.set_hex_limit(100);
    printf("Log level debug\n");
  }

  // Init Radio and PHY
#ifdef USE_RADIO
  my_radio.init();
#else
  my_radio.init(NULL, "dummy");
#endif
  
  my_radio.set_tx_freq(prog_args.tx_freq);
  my_radio.set_tx_gain(prog_args.tx_gain);
  my_radio.set_rx_freq(prog_args.rx_freq);
  my_radio.set_rx_gain(prog_args.tx_gain);
  my_radio.set_tx_adv(0);
      
  my_phy.init(&my_radio, &my_mac, &my_tester, &log_out, 1);
  my_phy.set_crnti(prog_args.rnti);
  my_mac.init(&my_phy, &rlc, &my_tester, &log_out);
  rlc.init(&my_tester, &my_tester, &my_tester, &log_out, &my_mac);
  
  my_tester.init(&my_phy, &my_mac, &rlc, &log_out, prog_args.ip_address);
  
  // Setup a single UM bearer 
  LIBLTE_RRC_RLC_CONFIG_STRUCT cfg; 
  bzero(&cfg, sizeof(LIBLTE_RRC_RLC_CONFIG_STRUCT));
  cfg.rlc_mode = LIBLTE_RRC_RLC_MODE_UM_BI;
  cfg.dl_um_bi_rlc.t_reordering = LIBLTE_RRC_T_REORDERING_MS100; 
  cfg.dl_um_bi_rlc.sn_field_len = LIBLTE_RRC_SN_FIELD_LENGTH_SIZE5;   
  rlc.add_bearer(LCID, &cfg);
  
  bool running = true; 
  
     
  while(running) {
    if (my_tester.is_sib_received()) {
      my_phy.pdcch_ul_search(SRSLTE_RNTI_USER, prog_args.rnti);
      my_phy.pdcch_dl_search(SRSLTE_RNTI_USER, prog_args.rnti);

      log_out.console("Main running\n");
      sleep(1);
    } else {
      my_tester.sib_search();
    }
  }
  my_phy.stop();
  my_mac.stop();
}



/******************* This is copied from srsue gw **********************/
int setup_if_addr(char *ip_addr)
{
    
  char *dev = (char*) "tun_srsue";

  // Construct the TUN device
  int tun_fd = open("/dev/net/tun", O_RDWR);
  if(0 > tun_fd)
  {
    perror("open");
    return(-1);
  }

  struct ifreq ifr;
  
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  strncpy(ifr.ifr_ifrn.ifrn_name, dev, IFNAMSIZ);
  if(0 > ioctl(tun_fd, TUNSETIFF, &ifr))
  {
    perror("ioctl");
    return -1;
  }

  // Bring up the interface
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(0 > ioctl(sock, SIOCGIFFLAGS, &ifr))
  {
    perror("socket");
    return -1;
  }
  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  if(0 > ioctl(sock, SIOCSIFFLAGS, &ifr))
  {
    perror("ioctl");
    return -1;
  }

  // Setup the IP address    
  sock                                                   = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family                                 = AF_INET;
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = inet_addr(ip_addr);
  if(0 > ioctl(sock, SIOCSIFADDR, &ifr))
  {
    perror("ioctl");
    return -1;
  }
  ifr.ifr_netmask.sa_family                                 = AF_INET;
  ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr = inet_addr("255.255.255.0");
  if(0 > ioctl(sock, SIOCSIFNETMASK, &ifr))
  {
    perror("ioctl");
    return -1;
  }

  return(tun_fd);
}
