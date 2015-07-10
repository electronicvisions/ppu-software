#pragma once

#include <fxv.h>
#include <stdint.h>

extern void lif_step(fxv_array_t* state_out,
    fxv_array_t* spike_out,
    fxv_array_t* state_in,
    fxv_array_t* input,
    int16_t delta_t,
    int16_t lambda,
    int16_t thresh,
    int16_t reset);

extern void lif_send_spikes(fxv_array_t* spikes);

