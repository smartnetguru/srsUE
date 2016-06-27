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
#include "radio/radio.h"

//#define START_TUNTAP
//#define USE_RADIO

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/

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
  args->tx_freq = -1.0;
  args->rx_freq = -1.0;
  args->rx_gain = 50.0; 
  args->tx_gain = 80.0; 
  args->ip_address = "192.168.3.2";
}

void usage(prog_args_t *args, char *prog) {
  printf("Usage: %s [gGIrv] -f rx_frequency (in Hz) -f tx_frequency (in Hz)\n", prog);
  printf("\t-g RF RX gain [Default %.1f]\n", args->rx_gain);
  printf("\t-G RF TX gain [Default %.1f]\n", args->tx_gain);
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
  
  void init(srsue::rlc *rlc_, srslte::log *log_h_, std::string ip_address) {
    log_h = log_h_; 
    rlc   = rlc_; 
    
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

  void release_pucch_srs() {}
  void ra_problem() {}
  void write_pdu_bcch_bch(srsue::byte_buffer_t *sdu) {}
  void write_pdu_bcch_dlsch(srsue::byte_buffer_t *sdu) {}
  void write_pdu_pcch(srsue::byte_buffer_t *sdu) {}
  void max_retx_attempted(){}
  void in_sync() {};
  void out_of_sync() {};

  void write_pdu(uint32_t lcid, srsue::byte_buffer_t *sdu)
  {
    int n = write(tun_fd, sdu->buffer, sdu->N_bytes);
    if (n != sdu->N_bytes) {
      log_h->error("TUN/TAP write failure n=%d, nof_bytes=%d\n", n, sdu->N_bytes);
      return; 
    }
    log_h->info("Wrote %d bytes from TUN fd=%d\n", sdu->N_bytes, tun_fd);      
  }
  
private:
  int tun_fd;
  bool running; 
  srslte::log *log_h;
  srsue::buffer_pool *pool;
  srsue::rlc *rlc; 
  
  int init_tuntap(char *ip_address) {
    tun_fd = setup_if_addr(ip_address);
    if (tun_fd<0) {
      fprintf(stderr, "Error setting up IP %s\n", ip_address);
      return -1;
    }
    
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
          rlc->write_sdu(0, pdu);
          
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
    //srslte_verbose = SRSLTE_VERBOSE_INFO;
    printf("Log level info\n");
  }
  if (srsapps_verbose == 2) {
    log_out.set_level(srslte::LOG_LEVEL_DEBUG);
    srslte_verbose = SRSLTE_VERBOSE_DEBUG;
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
  
  my_tester.init(&rlc, &log_out, prog_args.ip_address);
  
  // Setup a single UM bearer 
  LIBLTE_RRC_RLC_CONFIG_STRUCT cfg; 
  cfg.rlc_mode = LIBLTE_RRC_RLC_MODE_UM_BI;
  rlc.add_bearer(0, &cfg);
  
  bool running = true; 
      
  while(running) {
    log_out.console("Main running\n");
    sleep(1);
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
