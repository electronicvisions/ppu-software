#include "lif.h"

volatile uint32_t* out_syndrv_spike      = (uint32_t*)0x1c000040;
volatile uint32_t* out_neuron_post_lower = (uint32_t*)0x1a000100;
volatile uint32_t* out_serdes_spike_a    = (uint32_t*)0x10000000;
volatile uint32_t* out_serdes_spike_b    = (uint32_t*)0x10000001;


void lif_step(fxv_array_t* state_out,
    fxv_array_t* spike_out,
    fxv_array_t* state_in,
    fxv_array_t* input,
    int16_t delta_t,
    int16_t lambda,
    int16_t thresh,
    int16_t reset) {
#define VR_STATE 31
#define VR_INPUT 30
#define VR_LAMBDA 29
#define VR_U 28
#define VR_DELTA_T 27
#define VR_V 26
#define VR_CMP 25
#define VR_RESET 24
#define VR_THRESH 23
#define VR_SPIKE 22
#define VR_ONE 21

  fxv_load_array(VR_STATE, state_in);
  fxv_load_array(VR_INPUT, input);
  fxv_splath(VR_LAMBDA, lambda);
  fxv_splath(VR_DELTA_T, delta_t);
  fxv_splath(VR_THRESH, thresh);
  fxv_splath(VR_RESET, reset);
  fxv_splath(VR_SPIKE, 0);
  fxv_splath(VR_ONE, 1);

  // integration
  fxv_mulhfs(VR_U, VR_LAMBDA, VR_STATE);

  fxv_mtachf(VR_STATE);
  fxv_matachfs(VR_U, VR_DELTA_T);
  fxv_mahfs(VR_STATE, VR_INPUT, VR_DELTA_T);

  /*fxv_mulhfs(VR_U, VR_LAMBDA, VR_STATE);
  fxv_addhfs(VR_V, VR_U, VR_INPUT);
  fxv_addhfs(VR_STATE, VR_STATE, VR_V);*/

  /*fxv_splath(VR_INPUT, 0x0100);
  fxv_addhfs(VR_STATE, VR_STATE, VR_INPUT);*/


  // spike threshold
  fxv_subhfs(VR_CMP, VR_STATE, VR_THRESH);
  fxv_cmph(VR_CMP);
  fxv_sel_gt(VR_STATE, VR_STATE, VR_RESET);
  fxv_sel_gt(VR_SPIKE, VR_SPIKE, VR_ONE);

  fxv_store_array(state_out, VR_STATE);
  fxv_store_array(spike_out, VR_SPIKE);

#undef VR_THRESH
#undef VR_RESET
#undef VR_CMP
#undef VR_V
#undef VR_DELTA_T
#undef VR_U
#undef VR_LAMBDA
#undef VR_STATE
#undef VR_INPUT
}


void lif_send_spikes(fxv_array_t* spikes) {
  uint32_t bits = 0;
  int i;

  for(i=0; i<NUM_HALFWORDS_PER_ARRAY; ++i) {
    if( spikes->halfwords[i] > 0 )
      bits |= (1 << (31 - i));
  }

  if( bits != 0 ) {
    *out_neuron_post_lower = bits;
    *out_serdes_spike_a = bits;
  }
}

