#ifndef __UE_STATUS_H__
#define __UE_STATUS_H__

typedef struct
{
    float       signal_power;
    float       noise_power;
    float       processing_latency;
    float       wrong_frames;
    float       received_frames;
    float       transmitted_frames;
    float       modcod;
    float       mabr;
    float       sinr;
    float       rsrp;
    float       rsrq;
    float       rssi;
    float       cfo;
    float       sfo;
    float       turbo_iters;
    float       harq_retxs;
    float       arq_retx;
    float       latency;
    float       throughput;
    float       mcs;
    float       radio_buffer_status;
}ue_status_t;

#endif /* __UE_STATUS_H__ */
