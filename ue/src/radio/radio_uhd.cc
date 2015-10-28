  /**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 The srsUE Developers. See the
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

#include "srslte/srslte.h"
#include "radio/radio_uhd.h"
#include <string.h>

// This is for the channel emulator
#include <fftw3.h>

namespace srslte {

bool radio_uhd::init()
{
  return init((char*) "");
}

bool radio_uhd::init(char *args)
{
  printf("Opening UHD device...\n");
  if (cuhd_open(args, &uhd)) {
    fprintf(stderr, "Error opening uhd\n");
    return false;
  }
  agc_enabled = false; 
  bzero(zeros, burst_settle_max_samples*sizeof(cf_t));
  return true;    
}

bool radio_uhd::init_agc()
{
  return init_agc((char*) "");
}

void radio_uhd::set_tx_rx_gain_offset(float offset) {
  cuhd_set_tx_rx_gain_offset(uhd, offset);  
}

void radio_uhd::tx_offset(int offset_)
{
  offset = offset_; 
}

bool radio_uhd::init_agc(char *args)
{
  printf("Opening UHD device with threaded RX Gain control ...\n");
  if (cuhd_open_th(args, &uhd, false)) {
    fprintf(stderr, "Error opening uhd\n");
    return false;
  }
  cuhd_set_rx_gain(uhd, 40);
  cuhd_set_tx_gain(uhd, 40);

  burst_settle_samples = 0; 
  burst_settle_time_rounded = 0; 
  is_start_of_burst = true; 
  agc_enabled = true; 

  return true;    
}
bool radio_uhd::rx_at(void* buffer, uint32_t nof_samples, srslte_timestamp_t rx_time)
{
  fprintf(stderr, "Not implemented\n");
  return false; 
}

bool radio_uhd::rx_now(void* buffer, uint32_t nof_samples, srslte_timestamp_t* rxd_time)
{
  void *recv_ptr = buffer; 
  if (en_channel_emulator && nof_samples == nsamples) {
    recv_ptr = temp_buffer_in;
  }
  if (cuhd_recv_with_time(uhd, recv_ptr, nof_samples, true, &rxd_time->full_secs, &rxd_time->frac_secs) > 0) {
    if (en_channel_emulator && nof_samples == nsamples) {
      channel_emulator(temp_buffer_in, (cf_t*) buffer);
    }
    return true; 
  } else {
    return false; 
  }
}

void radio_uhd::get_time(srslte_timestamp_t *now) {
  cuhd_get_time(uhd, &now->full_secs, &now->frac_secs);  
}

// TODO: Use Calibrated values for this 
float radio_uhd::set_tx_power(float power)
{
  if (power > 10) {
    power = 10; 
  }
  if (power < -30) {
    power = 30; 
  }
  float gain = power + 74;
  if (agc_enabled) {
    cuhd_set_tx_gain_th(uhd, gain);
  } else {
    cuhd_set_tx_gain(uhd, gain);
  }
  return power; 
}

float radio_uhd::get_max_tx_power()
{
  return 10;
}

float radio_uhd::get_rssi()
{
  return cuhd_get_rssi(uhd);
}

bool radio_uhd::has_rssi()
{
  return cuhd_has_rssi(uhd);
}

bool radio_uhd::tx(void* buffer, uint32_t nof_samples, srslte_timestamp_t tx_time)
{
  if (is_start_of_burst) {
    if (burst_settle_samples != 0) {
      srslte_timestamp_t tx_time_pad; 
      srslte_timestamp_copy(&tx_time_pad, &tx_time);
      srslte_timestamp_sub(&tx_time_pad, 0, burst_settle_time_rounded); 
      save_trace(1, &tx_time_pad);
      cuhd_send_timed2(uhd, zeros, burst_settle_samples, tx_time_pad.full_secs, tx_time_pad.frac_secs, true, false);
    }        
    is_start_of_burst = false;     
  }
  
  void *tx_ptr = buffer; 
  /*
  if (en_channel_emulator) {
    tx_ptr = temp_buffer_out;
    channel_emulator((cf_t*) buffer, temp_buffer_out);
  }
  */
  // Save possible end of burst time 
  srslte_timestamp_copy(&end_of_burst_time, &tx_time);
  srslte_timestamp_add(&end_of_burst_time, 0, (double) nof_samples/cur_tx_srate); 
  
  save_trace(0, &tx_time);
  if (cuhd_send_timed2(uhd, tx_ptr, nof_samples+offset, tx_time.full_secs, tx_time.frac_secs, false, false) > 0) {
    offset = 0; 
    return true; 
  } else {
    return false; 
  }
}

uint32_t radio_uhd::get_tti_len()
{
  return sf_len; 
}

void radio_uhd::set_tti_len(uint32_t sf_len_)
{
  sf_len = sf_len_; 
}

bool radio_uhd::tx_end()
{
  save_trace(2, &end_of_burst_time);
  cuhd_send_timed2(uhd, zeros, 0, end_of_burst_time.full_secs, end_of_burst_time.frac_secs, false, true);
  is_start_of_burst = true; 
}

void radio_uhd::start_trace() {
  trace_enabled = true; 
}

void radio_uhd::set_tti(uint32_t tti_) {
  tti = tti_; 
}

void radio_uhd::write_trace(std::string filename)
{
  tr_local_time.writeToBinary(filename + ".local");
  tr_is_eob.writeToBinary(filename + ".eob");
  tr_usrp_time.writeToBinary(filename + ".usrp");
  tr_tx_time.writeToBinary(filename + ".tx");
}

void radio_uhd::save_trace(uint32_t is_eob, srslte_timestamp_t *tx_time) {
  if (trace_enabled) {
    tr_local_time.push_cur_time_us(tti);
    srslte_timestamp_t usrp_time; 
    cuhd_get_time(uhd, &usrp_time.full_secs, &usrp_time.frac_secs);
    tr_usrp_time.push(tti, srslte_timestamp_uint32(&usrp_time));
    tr_tx_time.push(tti, srslte_timestamp_uint32(tx_time));
    tr_is_eob.push(tti, is_eob);
  }
}

void radio_uhd::set_rx_freq(float freq)
{
  cuhd_set_rx_freq(uhd, freq);
}

void radio_uhd::set_rx_gain(float gain)
{
  cuhd_set_rx_gain(uhd, gain);
}

double radio_uhd::set_rx_gain_th(float gain)
{
  return cuhd_set_rx_gain_th(uhd, gain);
}

void radio_uhd::set_master_clock_rate(float rate)
{
  cuhd_set_master_clock_rate(uhd, rate);
}

void radio_uhd::set_rx_srate(float srate)
{
  cuhd_set_rx_srate(uhd, srate);
}

void radio_uhd::set_tx_freq(float freq)
{
  cuhd_set_tx_freq_offset(uhd, freq, lo_offset);  
}

void radio_uhd::set_tx_gain(float gain)
{
  cuhd_set_tx_gain(uhd, gain);
}

float radio_uhd::get_tx_gain()
{
  return cuhd_get_tx_gain(uhd);
}

float radio_uhd::get_rx_gain()
{
  return cuhd_get_rx_gain(uhd);
}

void radio_uhd::set_tx_srate(float srate)
{
  cur_tx_srate = cuhd_set_tx_srate(uhd, srate);
  burst_settle_samples = (uint32_t) (cur_tx_srate * burst_settle_time);
  if (burst_settle_samples > burst_settle_max_samples) {
    burst_settle_samples = burst_settle_max_samples;
    fprintf(stderr, "Error setting TX srate %.1f MHz. Maximum frequency for zero prepadding is 30.72 MHz\n", srate*1e-6);
  }
  burst_settle_time_rounded = (double) burst_settle_samples/cur_tx_srate;
}

void radio_uhd::start_rx()
{
  cuhd_start_rx_stream(uhd);
}

void radio_uhd::stop_rx()
{
  cuhd_stop_rx_stream(uhd);
}

void radio_uhd::register_msg_handler(cuhd_msg_handler_t h)
{
  cuhd_register_msg_handler(h);
}


/*************************************************************************
 * 
 *                      CHANNEL EMULATOR 
 * 
 *************************************************************************/

/** FIXME: Dealloc all this mallocs */
bool radio_uhd::channel_emulator_init(const char *filename, int *path_tap_, int Npaths_, int Ncoeff_, int nsamples_, int ntti_) {
  Ntaps    = 16;
  Npaths   = Npaths_;
  Ncoeff   = Ncoeff_;
  nsamples = nsamples_;
  ntti     = ntti_;
  path_tap = (int*) malloc(sizeof(int)*Npaths);
  memcpy(path_tap, path_tap_, sizeof(int)*Npaths);
  ;
  
  temp_buffer_in  = (cf_t*) fftwf_malloc(sizeof(cf_t)*nsamples);
  temp_buffer_out = (cf_t*) fftwf_malloc(sizeof(cf_t)*nsamples);
  read_buffer     = (cf_t*) fftwf_malloc(sizeof(cf_t)*Ncoeff*Npaths*ntti);
  in_ifft         = (cf_t*) fftwf_malloc(sizeof(cf_t)*nsamples);
  out_ifft        = (cf_t*) fftwf_malloc(sizeof(cf_t)*nsamples);
  taps            = (cf_t*) fftwf_malloc(sizeof(cf_t)*nsamples*Ntaps);
  if (!in_ifft || !out_ifft || !taps || !temp_buffer_in || !temp_buffer_out) {
    return false; 
  }
  bzero(in_ifft, sizeof(cf_t)*nsamples);
  bzero(taps, sizeof(cf_t)*nsamples*Ntaps);
  ifft_plan = fftwf_plan_dft_1d(nsamples, 
                                   reinterpret_cast<fftwf_complex*>(in_ifft), 
                                   reinterpret_cast<fftwf_complex*>(out_ifft), 
                                   FFTW_BACKWARD, 0U);

  FILE *fr = fopen(filename, "r");
  if (!fr) {
    fprintf(stderr, "Error opening file: %s\n", filename);
    return false;
  }

  int n = fread(read_buffer, ntti*Ncoeff*Npaths*sizeof(cf_t), 1, fr); 
  if (n<0) {
    perror("fread");
    return false;
  }
  fclose(fr);
  temp = read_buffer; 
  tti_cnt=0;
  
  en_channel_emulator = true; 
  
  return true; 
}

void radio_uhd::channel_emulator(cf_t *input, cf_t *output) {
  if (en_channel_emulator) {
    int i,j; 
    
    float fft_norm = 1/(float) Ncoeff;
    for (i=0;i<Npaths;i++) {
      memcpy(in_ifft, &temp[Ncoeff/2+i*Ncoeff], Ncoeff/2*sizeof(cf_t));
      memcpy(&in_ifft[nsamples-Ncoeff/2], &temp[i*Ncoeff], Ncoeff/2*sizeof(cf_t));
      fftwf_execute(ifft_plan);  
      srslte_vec_sc_prod_cfc(out_ifft, fft_norm, out_ifft, nsamples);
      for (j=0;j<nsamples;j++) {
        taps[j*Ntaps+path_tap[i]] = out_ifft[j];
      }      
    }
    for (i=0;i<nsamples;i++) {
      output[i] = srslte_vec_dot_prod_ccc(&input[i], &taps[i*Ntaps], Ntaps);      
    }  
    temp += Ncoeff*Npaths;
    tti_cnt++;
    if (tti_cnt==ntti) {
      temp    = read_buffer;
      tti_cnt = 0;
    }
  }
}
  
} // namespace srslte

