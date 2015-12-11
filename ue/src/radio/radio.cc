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

#include "srslte/srslte.h"
extern "C" {
#include "srslte/rf/rf.h"
}
#include "radio/radio.h"

namespace srslte {

bool radio::init()
{
  return init((char*) "");
}

bool radio::init(char *args)
{
  printf("Opening RF device...\n");
  if (rf_open(&rf_device, args)) {
    fprintf(stderr, "Error opening RF device\n");
    return false;
  }
  agc_enabled = false; 
  bzero(zeros, burst_settle_max_samples*sizeof(cf_t));
  return true;    
}

bool radio::init_agc()
{
  return init_agc((char*) "");
}

void radio::set_tx_rx_gain_offset(float offset) {
  rf_set_tx_rx_gain_offset(&rf_device, offset);  
}

void radio::tx_offset(int offset_)
{
  offset = offset_; 
}

bool radio::init_agc(char *args)
{
  printf("Opening RF device with threaded RX Gain control ...\n");
  if (rf_open_th(&rf_device, args, false)) {
    fprintf(stderr, "Error opening RF device\n");
    return false;
  }
  rf_set_rx_gain(&rf_device, 40);
  rf_set_tx_gain(&rf_device, 40);

  burst_settle_samples = 0; 
  burst_settle_time_rounded = 0; 
  is_start_of_burst = true; 
  agc_enabled = true; 

  return true;    
}
bool radio::rx_at(void* buffer, uint32_t nof_samples, srslte_timestamp_t rx_time)
{
  fprintf(stderr, "Not implemented\n");
  return false; 
}

bool radio::rx_now(void* buffer, uint32_t nof_samples, srslte_timestamp_t* rxd_time)
{
  if (rf_recv_with_time(&rf_device, buffer, nof_samples, true, &rxd_time->full_secs, &rxd_time->frac_secs) > 0) {
    return true; 
  } else {
    return false; 
  }
}

void radio::get_time(srslte_timestamp_t *now) {
  rf_get_time(&rf_device, &now->full_secs, &now->frac_secs);  
}

// TODO: Use Calibrated values for this 
float radio::set_tx_power(float power)
{
  if (power > 10) {
    power = 10; 
  }
  if (power < -50) {
    power = -50; 
  }
  float gain = power + 74;
  rf_set_tx_gain(&rf_device, gain);
  return gain; 
}

float radio::get_max_tx_power()
{
  return 10;
}

float radio::get_rssi()
{
  return rf_get_rssi(&rf_device);
}

bool radio::has_rssi()
{
  return rf_has_rssi(&rf_device);
}

bool radio::tx(void* buffer, uint32_t nof_samples, srslte_timestamp_t tx_time)
{
  if (is_start_of_burst) {
    if (burst_settle_samples != 0) {
      srslte_timestamp_t tx_time_pad; 
      srslte_timestamp_copy(&tx_time_pad, &tx_time);
      srslte_timestamp_sub(&tx_time_pad, 0, burst_settle_time_rounded); 
      save_trace(1, &tx_time_pad);
      rf_send_timed2(&rf_device, zeros, burst_settle_samples, tx_time_pad.full_secs, tx_time_pad.frac_secs, true, false);
    }        
    is_start_of_burst = false;     
  }
  
  // Save possible end of burst time 
  srslte_timestamp_copy(&end_of_burst_time, &tx_time);
  srslte_timestamp_add(&end_of_burst_time, 0, (double) nof_samples/cur_tx_srate); 
  
  save_trace(0, &tx_time);
  if (rf_send_timed2(&rf_device, buffer, nof_samples+offset, tx_time.full_secs, tx_time.frac_secs, false, false) > 0) {
    offset = 0; 
    return true; 
  } else {
    return false; 
  }
}

uint32_t radio::get_tti_len()
{
  return sf_len; 
}

void radio::set_tti_len(uint32_t sf_len_)
{
  sf_len = sf_len_; 
}

bool radio::tx_end()
{
  save_trace(2, &end_of_burst_time);
  rf_send_timed2(&rf_device, zeros, 0, end_of_burst_time.full_secs, end_of_burst_time.frac_secs, false, true);
  is_start_of_burst = true; 
}

void radio::start_trace() {
  trace_enabled = true; 
}

void radio::set_tti(uint32_t tti_) {
  tti = tti_; 
}

void radio::write_trace(std::string filename)
{
  tr_local_time.writeToBinary(filename + ".local");
  tr_is_eob.writeToBinary(filename + ".eob");
  tr_usrp_time.writeToBinary(filename + ".usrp");
  tr_tx_time.writeToBinary(filename + ".tx");
}

void radio::save_trace(uint32_t is_eob, srslte_timestamp_t *tx_time) {
  if (trace_enabled) {
    tr_local_time.push_cur_time_us(tti);
    srslte_timestamp_t usrp_time; 
    rf_get_time(&rf_device, &usrp_time.full_secs, &usrp_time.frac_secs);
    tr_usrp_time.push(tti, srslte_timestamp_uint32(&usrp_time));
    tr_tx_time.push(tti, srslte_timestamp_uint32(tx_time));
    tr_is_eob.push(tti, is_eob);
  }
}

void radio::set_rx_freq(float freq)
{
  rf_set_rx_freq(&rf_device, freq);
}

void radio::set_rx_gain(float gain)
{
  rf_set_rx_gain(&rf_device, gain);
}

double radio::set_rx_gain_th(float gain)
{
  return rf_set_rx_gain_th(&rf_device, gain);
}

void radio::set_master_clock_rate(float rate)
{
  rf_set_master_clock_rate(&rf_device, rate);
}

void radio::set_rx_srate(float srate)
{
  rf_set_rx_srate(&rf_device, srate);
}

void radio::set_tx_freq(float freq)
{
  rf_set_tx_freq(&rf_device, freq);  
}

void radio::set_tx_gain(float gain)
{
  rf_set_tx_gain(&rf_device, gain);
}

float radio::get_tx_gain()
{
  return rf_get_tx_gain(&rf_device);
}

float radio::get_rx_gain()
{
  return rf_get_rx_gain(&rf_device);
}

void radio::set_tx_srate(float srate)
{
  cur_tx_srate = rf_set_tx_srate(&rf_device, srate);
  burst_settle_samples = (uint32_t) (cur_tx_srate * burst_settle_time);
  if (burst_settle_samples > burst_settle_max_samples) {
    burst_settle_samples = burst_settle_max_samples;
    fprintf(stderr, "Error setting TX srate %.1f MHz. Maximum frequency for zero prepadding is 30.72 MHz\n", srate*1e-6);
  }
  burst_settle_time_rounded = (double) burst_settle_samples/cur_tx_srate;
}

void radio::start_rx()
{
  rf_start_rx_stream(&rf_device);
}

void radio::stop_rx()
{
  rf_stop_rx_stream(&rf_device);
}

void radio::register_msg_handler(rf_msg_handler_t h)
{
  rf_register_msg_handler(&rf_device, h);
}

  
}

