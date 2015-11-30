#ifndef ACCORD_SERVER_H
#define ACCORD_SERVER_H

#include <pthread.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "ue_metrics_interface.h"
#include "metrics_accord/ue_status.h"

class accord_server;

class tcp_connection
  : public boost::enable_shared_from_this<tcp_connection>
{
public:
  typedef boost::shared_ptr<tcp_connection> pointer;

  static pointer create(boost::asio::io_service& io_service, accord_server *s)
  {
    return pointer(new tcp_connection(io_service, s));
  }

  boost::asio::ip::tcp::socket& socket()
  {
    return socket_;
  }

  void start();

private:
  tcp_connection(boost::asio::io_service& io_service, accord_server *s_)
    : socket_(io_service)
  {
    s = s_;
  }

  boost::asio::ip::tcp::socket socket_;
  std::string message_;
  accord_server *s;
};

class tcp_server
{
public:
  tcp_server(boost::asio::io_service& io_service, accord_server *s_, int port)
    : acceptor_(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
  {
    s = s_;
    start_accept();
  }

private:
  void start_accept()
  {
    tcp_connection::pointer new_connection =
      tcp_connection::create(acceptor_.get_io_service(), s);

    acceptor_.async_accept(new_connection->socket(),
        boost::bind(&tcp_server::handle_accept, this, new_connection,
          boost::asio::placeholders::error));
  }

  void handle_accept(tcp_connection::pointer new_connection,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_connection->start();
    }

    start_accept();
  }

  boost::asio::ip::tcp::acceptor acceptor_;
  accord_server *s;
};

class accord_server
{
public:
  accord_server(srsue::ue_metrics_interface *ue_, int port);
  void start();
  void stop();
  void toggle_print(bool b);

  std::string get_metrics_xml();

  void metrics_to_status(srsue::ue_metrics_t metrics, ue_status_t *status);
  void print_disconnect();
  void print_metrics(srsue::ue_metrics_t &metrics);
  std::string float_to_eng_string(float f, int digits);
  std::string float_to_string(float f, int digits);


private:
  srsue::ue_metrics_interface *ue;
  srsue::ue_metrics_t metrics;
  ue_status_t   status;

  boost::asio::io_service io_service;
  tcp_server tcp;
  pthread_t thread;

  float metrics_report_period_s;
  bool  do_print;
  int   n_reports;
  struct timespec tstart, tend;
};

#endif // ACCORD_SERVER_H
