#include "syn.h"
#include "cadc.h"

#include <mailbox.h>
#include <fxv.h>
#include <spr.h>
#include <fxdpt.h>
#include <attrib.h>

#define USEC (98)


/** Definitions for vector registers **/
#define VR_TMP     0
#define VR_CAUSAL  1
#define VR_ACAUSAL 2
#define VR_RST     3
#define VR_NULL_C  4
#define VR_NULL_A  5
#define VR_WEIGHT  6
#define VR_TMP_0   7
#define VR_TMP_1   8
#define VR_A       9
#define VR_WMAX   10
#define VR_U      11
#define VR_T      12
#define VR_V      13
#define VR_C      14
#define VR_WOUT_0 15
#define VR_WOUT_1 16
#define VR_WIN    17
#define VR_LAMBDA 18
#define VR_WOUT   19
#define VR_OFFSET 20


/** Constants **/
static const int loc_a = 0;
static const int loc_b = 1;
static const time_base_t update_period = 500 * USEC;
static const int as_size = 400;


volatile uint32_t* signal = (uint32_t*)0x3000;


void cadc_read(fxv_array_t* causal, fxv_array_t* acausal, int loc) {
  cadc_load_causal(VR_CAUSAL, loc);
  cadc_load_acausal_buffered(VR_ACAUSAL, loc);

  fxv_subbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);
  fxv_subbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);

  fxv_store_array(causal, VR_CAUSAL);
  fxv_store_array(acausal, VR_ACAUSAL);
  sync();
}

ATTRIB_UNUSED static void compute_mult_stdp() {
  fxv_shb(VR_TMP_0, VR_CAUSAL, -1);
  fxv_shb(VR_TMP_1, VR_ACAUSAL, -1);

  fxv_addbfs(VR_TMP, VR_TMP_1, VR_OFFSET);

  fxv_subbfs(VR_A, VR_TMP_0, VR_TMP);
  fxv_shb(VR_A, VR_A, 2);
  /*fxv_subbfs(VR_A, VR_TMP, VR_OFFSET);*/
  fxv_cmpb(VR_A);

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_subbfs(VR_U, VR_WMAX, VR_WIN);
  fxv_mulbfs(VR_T, VR_U, VR_LAMBDA);
  fxv_mulbfs(VR_V, VR_C, VR_WIN);

  fxv_mtacbf(VR_WIN);
  fxv_mabfs(VR_WOUT_0, VR_A, VR_T);
  fxv_mabfs(VR_WOUT_1, VR_A, VR_V);
  fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WOUT_1); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1
  fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT

  fxv_shb(VR_WEIGHT, VR_WOUT, -1);
}


ATTRIB_UNUSED static void compute_inc() {
  fxv_splatb(VR_TMP, 0x01);

  fxv_shb(VR_TMP_0, VR_CAUSAL, -1);
  fxv_shb(VR_TMP_1, VR_ACAUSAL, -1);

  fxv_subbfs(VR_A, VR_TMP_0, VR_TMP_1);
  fxv_subbfs(VR_A, VR_A, VR_OFFSET);
  fxv_cmpb(VR_A);

  fxv_addbfs(VR_WOUT_0, VR_WEIGHT, VR_TMP);
  fxv_subbfs(VR_WOUT_1, VR_WEIGHT, VR_TMP);
  fxv_sel_gt(VR_WEIGHT, VR_WEIGHT, VR_WOUT_0);
  fxv_sel_lt(VR_WEIGHT, VR_WEIGHT, VR_WOUT_1);
}


ATTRIB_UNUSED static void compute_pass_through() {
  fxv_shb(VR_TMP_0, VR_CAUSAL, -1);
  fxv_shb(VR_TMP_1, VR_ACAUSAL, -1);

  fxv_subbfs(VR_A, VR_TMP_0, VR_TMP_1);
  fxv_subbfs(VR_TMP, VR_A, VR_OFFSET);
  fxv_shb(VR_WEIGHT, VR_TMP, 2);
}


void start() {
  fxv_array_t a, c, w, diff, offset;
  time_base_t t;
  int i, j;
  uint8_t as[as_size];

  *signal = 0;
  fxv_zero_vrf();  // clear registers

  for(i=0; i<as_size; ++i)
    as[i] = 0;

  // reset correlation
  fxv_splatb(VR_RST, RST_CAUSAL | RST_ACAUSAL);
  syn_reset(loc_a, VR_RST, COND_ALWAYS);
  syn_reset(loc_b, VR_RST, COND_ALWAYS);

  // reset values from CADC calibration
  fxv_splatb(VR_NULL_C, 0xff - 0x50);
  fxv_splatb(VR_NULL_A, 0xff - 0x64);

  // load initial reset values
  cadc_load_causal(VR_CAUSAL, 0);
  cadc_load_acausal_buffered(VR_ACAUSAL, 0);

  // correct for CADC bug
  fxv_addbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);
  fxv_addbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);

  // determine offset
  fxv_subbm(VR_TMP, VR_CAUSAL, VR_ACAUSAL);
  fxv_shb(VR_OFFSET, VR_TMP, -1); // make fractional positive number

  // subtract margin for noise
  /*fxv_subbm(VR_NULL_C, VR_NULL_C, 16);*/
  /*fxv_subbm(VR_NULL_A, VR_NULL_A, 16);*/

  /*fxv_store_array(&a, VR_NULL_C);*/
  /*sync();*/
  /*mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));*/

  // set weights to initial value
  fxv_splatb(VR_WEIGHT, 0x20);

  // init parameters
  fxv_splatb(VR_WMAX, F8_MAX);
  /*fxv_splatb(VR_LAMBDA, F8(0.4));*/
  fxv_splatb(VR_LAMBDA, F8_MAX);
  /*int32_t la = F8(0.4) * F8_MAX / 0x80;*/
  /*fxv_splatb(VR_C, la & 0xff);*/
  fxv_mov(VR_C, VR_LAMBDA);

  // wait for signal
  while( !*signal );

  // loop for weight update
  i = 0;
  j = 0;
  /*while( 1 ) {*/
  for(i=0; i<1; ) {
    // record current time
    t = get_time_base();

    cadc_load_causal(VR_CAUSAL, loc_a);
    cadc_load_acausal_buffered(VR_ACAUSAL, loc_a);

    syn_reset(loc_a, VR_RST, COND_ALWAYS);

    /*fxv_subbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);*/
    /*fxv_subbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);*/
    fxv_addbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);
    fxv_addbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);

    // compute STDP
    compute_mult_stdp();
    /*compute_inc();*/
    /*compute_pass_through();*/

    fxv_store_array(&w, VR_WEIGHT);
    fxv_store_array(&c, VR_CAUSAL);
    fxv_store_array(&a, VR_ACAUSAL);
    fxv_store_array(&diff, VR_A);
    fxv_store_array(&offset, VR_OFFSET);
    sync();

    int8_t x = *((int8_t*)(&c) + 4);
    int8_t y = *((int8_t*)(&a) + 4);
    int8_t z = *((int8_t*)(&diff) + 4);
    if( (j < as_size) && (z != 0) ) {
        as[j++] = x;
        as[j++] = y;
        as[j++] = z;
    }

    mailbox_write(0 * sizeof(fxv_array_t), (uint8_t*)&w, sizeof(fxv_array_t));
    mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)&offset, sizeof(fxv_array_t));
    /*mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)&c, sizeof(fxv_array_t));*/
    /*mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));*/
    mailbox_write(3 * sizeof(fxv_array_t), (uint8_t*)&i, sizeof(i));
    /*mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)&(w.bytes[4]), 4);*/
    mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)as, as_size);

    // wait until next loop
    while( (get_time_base() - t) < update_period );

    ++i;
  }
}

