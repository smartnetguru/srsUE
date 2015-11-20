#include "metrics_accord/accord_server.h"

#include <ctime>
#include <float.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include "metrics_accord/parser.h"

#define BUF_LEN 10240

using boost::asio::ip::tcp;
using namespace std;

static bool running    = true;

char const * const prefixes[2][9] =
{
  {   "",   "m",   "u",   "n",    "p",    "f",    "a",    "z",    "y", },
  {   "",   "k",   "M",   "G",    "T",    "P",    "E",    "Z",    "Y", },
};

uint8_t get_stats_format[12] = {0xed, 0xcb, 0x12, 0x34,
                                0x00, 0x00, 0x00, 0x01,
                                0x00, 0x00, 0x10, 0x05};

bool is_get_stats(uint8_t *buf, int len)
{
  bool ret = true;
  for(int i=0;i<len&&i<12;i++)
    if(buf[i] != get_stats_format[i])
      ret = false;
  return ret;
}

string hex_string(uint8_t *data, int size)
{
  stringstream ss;
  int c = 0;

  ss << std::hex << setfill('0');
  while(c < size) {
    ss << "             " << setw(4) << static_cast<unsigned>(c) << ": ";
    int tmp = (size-c < 16) ? size-c : 16;
    for(int i=0;i<tmp;i++) {
      ss << setw(2) << static_cast<unsigned>(data[c++]) << " ";
    }
    ss << "\n";
  }
  return ss.str();
}

void tcp_connection::start()
{
  uint8_t buf[16];
  bzero(buf, 16);

  boost::system::error_code error;
  size_t len = socket_.read_some(boost::asio::buffer(buf), error);
  if(error) {
    cout << "Error reading from socket" << endl;
    return;
  }

  if(is_get_stats(buf, len))
  {

    string xml = s->get_metrics_xml();

    boost::system::error_code ignored_error;
    boost::asio::write(socket_, boost::asio::buffer(xml),
                       boost::asio::transfer_all(), ignored_error);
  }
}

void *thread_loop(void *s)
{
  boost::asio::io_service *io_service = (boost::asio::io_service*)s;
  cout << "Starting server" << endl;
  io_service->run();
}

accord_server::accord_server(srsue::ue_metrics_interface *ue_, int port)
  :tcp(io_service, this, port)
{
  ue                      = ue_;
  do_print                = false;
  n_reports               = 10;
  metrics_report_period_s = 0;
  tstart.tv_nsec          = 0;
  tstart.tv_sec           = 0;
  tend.tv_nsec            = 0;
  tend.tv_sec             = 0;
}

void accord_server::start()
{
  clock_gettime(CLOCK_MONOTONIC, &tstart);
  pthread_create(&thread, NULL, &thread_loop, &io_service);
}

void accord_server::stop()
{
  cout << "Stopping server" << endl;
  io_service.stop();
  cout << "Server stopped - exiting" << endl;
}

void accord_server::toggle_print(bool b)
{
  do_print = b;
}

string accord_server::get_metrics_xml()
{
  char send_buf[BUF_LEN];

  if(ue->get_metrics(metrics)) {
    print_metrics(metrics);
  } else {
    print_disconnect();
  }

  clock_gettime(CLOCK_MONOTONIC, &tend);
  metrics_report_period_s = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec)
      - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
  clock_gettime(CLOCK_MONOTONIC, &tstart);

  metrics_to_status(metrics, &status);

  if(ue_status_to_xml(&status, send_buf, BUF_LEN))
    cout << "Error generating xml string" << endl;

  return string(send_buf);
}

void accord_server::metrics_to_status(srsue::ue_metrics_t metrics, ue_status_t *status)
{
  status->signal_power        = metrics.phy.dl.rsrp;;
  status->noise_power         = metrics.phy.dl.n;
  status->processing_latency  = 2;                        //TODO
  status->wrong_frames        = metrics.mac.rx_errors;
  status->received_frames     = metrics.mac.rx_pkts;
  status->transmitted_frames  = metrics.mac.tx_pkts;
  status->modcod              = metrics.phy.dl.mcs;
  status->mabr                = metrics.phy.mabr;
  status->sinr                = metrics.phy.dl.sinr;
  status->rsrp                = metrics.phy.dl.rsrp;
  status->rsrq                = metrics.phy.dl.rsrq;
  status->rssi                = metrics.phy.dl.rssi;
  status->cfo                 = metrics.phy.sync.cfo;
  status->sfo                 = metrics.phy.sync.sfo;
  status->turbo_iters         = metrics.phy.dl.turbo_iters;
  status->harq_retxs          = metrics.mac.rx_errors;    //TODO
  status->arq_retx            = 0;                        //TODO
  status->latency             = 2;                        //TODO
  status->throughput          = (float) metrics.mac.rx_brate/metrics_report_period_s;
  status->mcs                 = metrics.phy.dl.mcs;
  status->radio_buffer_status = (metrics.uhd.uhd_error) ? 0 : 1;
}

string accord_server::float_to_string(float f, int digits)
{
  ostringstream os;
  const int    precision = (f == 0.0) ? digits-1 : digits - log10(fabs(f))-2*DBL_EPSILON;
  os << setw(6) << fixed << setprecision(precision) << f;
  return os.str();
}

string accord_server::float_to_eng_string(float f, int digits)
{
  const int degree = (f == 0.0) ? 0 : lrint( floor( log10( fabs( f ) ) / 3) );

  string factor;

  if ( abs( degree ) < 9 )
  {
    if(degree < 0)
      factor = prefixes[0][ abs( degree ) ];
    else
      factor = prefixes[1][ abs( degree ) ];
  } else {
    return "failed";
  }

  const double scaled = f * pow( 1000.0, -degree );
  if (degree != 0) {
    return float_to_string(scaled, digits) + factor;
  } else {
    return " " + float_to_string(scaled, digits) + factor;
  }
}

void accord_server::print_metrics(srsue::ue_metrics_t &metrics)
{
  if(!do_print)
    return;

  if(++n_reports > 10)
  {
    n_reports = 0;
    cout << endl;
    cout << "--Signal--------------DL------------------------------UL----------------------" << endl;
    cout << "  rsrp    pl    cfo   mcs   snr turbo  brate   bler   mcs   buff  brate   bler" << endl;
  }
  cout << float_to_string(metrics.phy.dl.rsrp, 2);
  cout << float_to_string(metrics.phy.dl.pathloss, 2);
  cout << float_to_eng_string(metrics.phy.sync.cfo, 2);
  cout << float_to_string(metrics.phy.dl.mcs, 2);
  cout << float_to_string(metrics.phy.dl.sinr, 2);
  cout << float_to_string(metrics.phy.dl.turbo_iters, 2);
  cout << float_to_eng_string((float) metrics.mac.rx_brate/metrics_report_period_s, 2);
  if (metrics.mac.rx_pkts > 0) {
    cout << float_to_string((float) 100*metrics.mac.rx_errors/metrics.mac.rx_pkts, 2) << "%";
  } else {
    cout << float_to_string(0, 2) << "%";
  }
  cout << float_to_string(metrics.phy.ul.mcs, 2);
  cout << float_to_eng_string((float) metrics.mac.ul_buffer, 2);
  cout << float_to_eng_string((float) metrics.mac.tx_brate/metrics_report_period_s, 2);
  if (metrics.mac.tx_pkts > 0) {
    cout << float_to_string((float) 100*metrics.mac.tx_errors/metrics.mac.tx_pkts, 2) << "%";
  } else {
    cout << float_to_string(0, 2) << "%";
  }
  cout << endl;

  if(metrics.uhd.uhd_error) {
    cout << "UHD status:"
         << "  O=" << metrics.uhd.uhd_o
         << ", U=" << metrics.uhd.uhd_u
         << ", L=" << metrics.uhd.uhd_l << endl;
  }

}

void accord_server::print_disconnect()
{
  if(do_print) {
    cout << "--- disconnected ---" << endl;
  }
}


