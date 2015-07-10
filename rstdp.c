#include "lif.h"
#include <fxdpt.h>
#include <mailbox.h>
#include <sync.h>

#define STIM_SIZE 20
#define TRACE_SIZE 512

static const int16_t lambda = F16(-0.1);
static const int16_t delta_t = F16(1.0);
static const int16_t thresh = F16(0.75);
static const int16_t reset = F16(0.0);


typedef struct {
  uint32_t timestamp;
  uint8_t target;
  int16_t weight;
} spike_t;

typedef struct {
  uint8_t* signal;
  uint8_t monitor_neuron;
  spike_t* stimulation;
  uint32_t stimulation_size;
} rstdp_config_t;

typedef struct {
  uint32_t trace_size;
  int16_t* trace;
} rstdp_result_t;


int16_t trace[TRACE_SIZE];
spike_t stim[STIM_SIZE];
uint8_t signal;
fxv_array_t state;


void start() {
  unsigned i, j;
  uint32_t time;
  fxv_array_t input;
  fxv_array_t spikes;
  rstdp_config_t cfg;
  rstdp_result_t result;
  uint8_t clear_input;

  signal = 0;

  // setup config struct
  cfg.monitor_neuron = 0;
  cfg.stimulation = stim;
  cfg.stimulation_size = STIM_SIZE;
  cfg.signal = &signal;

  // setup result struct
  result.trace_size = 0;
  result.trace = trace;

  mailbox_write(0, (uint8_t*)&cfg, sizeof(rstdp_config_t));
  while( !signal );
  mailbox_read((uint8_t*)&cfg, 0, sizeof(rstdp_config_t));

  // zero trace
  for(i=0; i<result.trace_size; ++i) {
    result.trace[i] = 0;
  }

  // zero input
  for(j=0; j<NUM_HALFWORDS_PER_ARRAY; ++j)
    input.halfwords[j] = 0;
  clear_input = 0;

  for(time=0, i=0; i < cfg.stimulation_size; ++time) {
    while( (i < cfg.stimulation_size) && (time >= cfg.stimulation[i].timestamp) ) {
      input.halfwords[cfg.stimulation[i].target] += cfg.stimulation[i].weight;
      ++i;
      clear_input = 1;
    }

    lif_step(&state,
        &spikes,
        &state,
        &input,
        delta_t,
        lambda,
        thresh,
        reset);
    sync();
    lif_send_spikes(&spikes);

    // record trace
    if( result.trace_size < TRACE_SIZE )
      result.trace[result.trace_size++] = state.halfwords[cfg.monitor_neuron];

    if( clear_input ) {
      for(j=0; j<NUM_HALFWORDS_PER_ARRAY; ++j)
        input.halfwords[j] = 0;
      clear_input = 0;
    }
  }

  mailbox_write(0, (uint8_t*)&result, sizeof(rstdp_result_t));
}

