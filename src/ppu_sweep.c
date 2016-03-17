#include "syn.h"
#include "cadc.h"

#include <mailbox.h>
#include <fxv.h>
#include <spr.h>
#include <fxdpt.h>
#include <attrib.h>

#define USEC (98)
#define CADC_NOISE_BITS (1)


typedef enum {
  VR_TMP = 0,
  VR_CAUSAL = 1,
  VR_ACAUSAL = 2,
  
} vreg_t;
 

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
#define VR_TMP_2  21
#define VR_THRESH 22


/** Constants **/
static int loc_a = 0;
static int loc_b = 1;
/*static const time_base_t update_period = 1000 * USEC;*/
static const time_base_t update_period = 10 * USEC;
static const int as_size = 400;


volatile uint32_t* signal       = (uint32_t*)0x3000;
volatile uint32_t* param_thresh = (uint32_t*)(0x3900 + 0);
volatile uint32_t* param_weight = (uint32_t*)(0x3900 + 4);
volatile uint32_t* param_wmax   = (uint32_t*)(0x3900 + 8);
volatile uint32_t* param_lam    = (uint32_t*)(0x3900 + 12);
volatile uint32_t* param_row    = (uint32_t*)(0x3900 + 16);


void cadc_read(fxv_array_t* causal, fxv_array_t* acausal, int loc) {
  cadc_load_causal(VR_CAUSAL, loc);
  cadc_load_acausal_buffered(VR_ACAUSAL, loc);

  fxv_subbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);
  fxv_subbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);

  fxv_store_array(causal, VR_CAUSAL);
  fxv_store_array(acausal, VR_ACAUSAL);
  sync();
}

ATTRIB_UNUSED static void compute_a() {
  /*
     1. shift right by CADC_NOISE_BITS
     2. build the difference of the de-noised readings
     3. subtract the offset from this difference and call the result A
     4. if A minus the threshold is greater than zero: store A in TMP0
     5. if A plus the threshold is less than zero: store A in TMP1
     6. if A was negative, use TMP1 else use TMP0
  */
  // check threshold
  /*fxv_subbm(VR_TMP_0, VR_CAUSAL, VR_THRESH);*/
  /*fxv_subbm(VR_TMP_1, VR_ACAUSAL, VR_THRESH);*/
  /*fxv_splatb(VR_TMP, 0);*/
  /*fxv_cmpb(VR_TMP_0);*/
  /*fxv_sel_gt(VR_CAUSAL, VR_OFFSET, VR_TMP_0);*/
  /*fxv_cmpb(VR_TMP_1);*/
  /*fxv_sel_gt(VR_ACAUSAL, VR_TMP, VR_TMP_1);*/

  // shift out noise bits
  fxv_shb(VR_TMP_0, VR_CAUSAL, -CADC_NOISE_BITS);
  fxv_shb(VR_TMP_1, VR_ACAUSAL, -CADC_NOISE_BITS);

  // compute difference
  fxv_subbfs(VR_TMP, VR_TMP_0, VR_TMP_1);

#if CADC_NOISE_BITS > 1
  fxv_subbfs(VR_TMP_2, VR_TMP, VR_OFFSET);
  fxv_shb(VR_A, VR_TMP_2, CADC_NOISE_BITS-1);
#else
  fxv_subbfs(VR_A, VR_TMP, VR_OFFSET);
#endif

  // thresholding
  fxv_splatb(VR_TMP, 0);
  fxv_subbfs(VR_TMP_0, VR_A, VR_THRESH);
  fxv_addbfs(VR_TMP_1, VR_A, VR_THRESH);
  fxv_cmpb(VR_TMP_0);
  fxv_sel_gt(VR_TMP_0, VR_TMP, VR_A);
  fxv_cmpb(VR_TMP_1);
  fxv_sel_lt(VR_TMP_1, VR_TMP, VR_A);
  fxv_cmpb(VR_A);
  fxv_sel_gt(VR_A, VR_TMP_1, VR_TMP_0);
  fxv_cmpb(VR_A);
}

ATTRIB_UNUSED static void compute_mult_stdp() {
  compute_a();

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

ATTRIB_UNUSED static void compute_add_stdp() {
  compute_a();

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  /*fxv_subbfs(VR_U, VR_WMAX, VR_WIN);*/
  /*fxv_mulbfs(VR_T, VR_U, VR_LAMBDA);*/
  /*fxv_mulbfs(VR_V, VR_C, VR_WIN);*/

  fxv_mtacbf(VR_WIN);
  fxv_mabfs(VR_WOUT_0, VR_A, VR_LAMBDA);
  /*fxv_mabfs(VR_WOUT_1, VR_A, VR_V);*/
  /*fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WOUT_1); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1*/
  /*fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT*/

  fxv_shb(VR_WEIGHT, VR_WOUT_0, -1);
}

ATTRIB_UNUSED static void compute_mult_stdp_symm() {
  compute_a();

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_subbfs(VR_U, VR_WMAX, VR_WIN);
  fxv_mulbfs(VR_T, VR_U, VR_LAMBDA);
  fxv_mulbfs(VR_V, VR_U, VR_C);

  fxv_mtacbf(VR_WIN);
  fxv_mabfs(VR_WOUT_0, VR_A, VR_T);
  fxv_mabfs(VR_WOUT_1, VR_A, VR_V);
  fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WOUT_1); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1
  fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT

  fxv_shb(VR_WEIGHT, VR_WOUT, -1);
}

ATTRIB_UNUSED static void compute_mult_stdp_symm_neg() {
  compute_a();

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_mulbfs(VR_T, VR_LAMBDA, VR_WIN);
  fxv_mulbfs(VR_V, VR_C, VR_WIN);

  fxv_mtacbf(VR_WIN);
  fxv_mabfs(VR_WOUT_0, VR_A, VR_T);
  fxv_mabfs(VR_WOUT_1, VR_A, VR_V);
  fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WOUT_1); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1
  fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT

  fxv_shb(VR_WEIGHT, VR_WOUT, -1);
}

ATTRIB_UNUSED static void compute_box() {
  compute_a();

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_sel_lt(VR_WOUT, VR_LAMBDA, VR_C); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1
  fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT

  fxv_shb(VR_WEIGHT, VR_WOUT, -1);
}

ATTRIB_UNUSED static void compute_mult_stdp_offset() {
  compute_a();

  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_subbfs(VR_U, VR_WMAX, VR_WIN);
  fxv_mulbfs(VR_T, VR_U, VR_LAMBDA);
  fxv_mulbfs(VR_V, VR_C, VR_WIN);


  fxv_mtacbf(VR_WIN);
  fxv_mabfs(VR_WOUT_0, VR_A, VR_T);
  fxv_mabfs(VR_WOUT_1, VR_A, VR_V);

  /*fxv_subbfs(VR_WOUT_0, VR_WOUT_0, VR_TMP);*/

  fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WIN); // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1
  fxv_sel_eq(VR_WOUT, VR_WOUT, VR_WIN);      // VR_WOUT <- VR_WIN if eq else VR_WOUT

  /*fxv_splatb(VR_TMP, 0x40);*/
  /*fxv_subbfs(VR_WOUT_1, VR_WOUT, VR_TMP);*/
  /*fxv_cmpb(VR_WOUT_1);*/
  /*fxv_sel_lt(VR_WOUT, VR_TMP, VR_WOUT);*/

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
  compute_a();
  /*fxv_mov(VR_WEIGHT, VR_A);*/
  fxv_addbfs(VR_WEIGHT,VR_WEIGHT,VR_A);
  /*fxv_addbm(VR_WEIGHT,VR_WEIGHT,VR_A);*/
}


void start() {
  fxv_array_t a, c, w, diff, offset;
  time_base_t t;
  int i;//, j;
  /*uint8_t as[as_size];*/

  /**signal = 0;*/
  fxv_zero_vrf();  // clear registers

  // set threshold on a+ and a-
  /*fxv_splatb(VR_THRESH, 4);*/
  fxv_splatb(VR_THRESH, 10);

  // set weights to initial value
  /*fxv_splatb(VR_WEIGHT, 0x20);*/
  fxv_splatb(VR_WEIGHT, 0x30);

  // init parameters
  fxv_splatb(VR_WMAX, F8_MAX);
  /*fxv_splatb(VR_LAMBDA, F8(0.4));*/
  fxv_splatb(VR_LAMBDA, F8_MAX);
  /*int32_t la = F8(0.4) * F8_MAX / 0x80;*/
  /*fxv_splatb(VR_C, la & 0xff);*/

  fxv_splatb(VR_WEIGHT, *param_weight);
  fxv_splatb(VR_THRESH, *param_thresh);
  fxv_splatb(VR_WMAX, *param_wmax);
  fxv_splatb(VR_LAMBDA, *param_lam);
  loc_a = *param_row;
  loc_b = loc_a + 1;

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
  fxv_shb(VR_CAUSAL, VR_CAUSAL, -CADC_NOISE_BITS);
  fxv_addbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);
  fxv_shb(VR_ACAUSAL, VR_ACAUSAL, -CADC_NOISE_BITS);

  // determine offset
  fxv_subbm(VR_OFFSET, VR_CAUSAL, VR_ACAUSAL);
  /*fxv_shb(VR_OFFSET, VR_TMP, -CADC_NOISE_BITS); // make fractional positive number*/

  // subtract margin for noise
  /*fxv_subbm(VR_NULL_C, VR_NULL_C, 16);*/
  /*fxv_subbm(VR_NULL_A, VR_NULL_A, 16);*/

  /*fxv_store_array(&a, VR_NULL_C);*/
  /*sync();*/
  /*mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));*/

  fxv_mov(VR_C, VR_LAMBDA);

  // wait for signal
  while( !*signal );

  // load parameters
  /*asm volatile (*/
      /*[>"lwz 9, 0x3004(0)\n\t"<]*/
      /*"lis 9, 0\n\t"*/
      /*"addi 9, 9, 0x20\n\t"*/
      /*"fxvsplatb 6, 9\n\t"*/
    /*: [> no output <]*/
    /*: [> no input <]*/
    /*: "9" [> clobber <]*/
  /*);*/



  // loop for weight update
  i = 0;
  /*j = 0;*/
  while( *signal ) {
  /*for(i=0; i<20; ) {*/
  /*{*/
    // record current time
    t = get_time_base();

    cadc_load_causal(VR_CAUSAL, loc_a);
    cadc_load_acausal_buffered(VR_ACAUSAL, loc_a);
    /*cadc_load_vreset_c(VR_CAUSAL, loc_a);*/
    /*cadc_load_vreset_a_buffered(VR_ACAUSAL, loc_a);*/

    /*syn_reset(loc_a, VR_RST, COND_ALWAYS);*/

    /*fxv_subbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);*/
    /*fxv_subbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);*/
    fxv_addbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);
    fxv_addbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);

    // compute STDP
    compute_mult_stdp();
    /*compute_mult_stdp_symm();*/
    /*compute_mult_stdp_symm_neg();*/
    /*compute_box();*/
    /*compute_mult_stdp_offset();*/
    /*compute_inc();*/
    /*compute_pass_through();*/

    syn_reset(loc_a, VR_RST, COND_GT);
    syn_reset(loc_a, VR_RST, COND_LT);

    fxv_store_array(&w, VR_WEIGHT);
    fxv_store_array(&c, VR_CAUSAL);
    fxv_store_array(&a, VR_ACAUSAL);
    fxv_store_array(&diff, VR_A);
    fxv_store_array(&offset, VR_OFFSET);
    sync();

    /*int8_t x = *((int8_t*)(&c) + 4);*/
    /*int8_t y = *((int8_t*)(&a) + 4);*/
    /*int8_t z = *((int8_t*)(&diff) + 4);*/
    /*if( (j < as_size) && (z != 0) ) {*/
        /*as[j++] = x;*/
        /*as[j++] = y;*/
        /*as[j++] = z;*/
    /*}*/

    mailbox_write(0 * sizeof(fxv_array_t), (uint8_t*)&w, sizeof(fxv_array_t));
    mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)param_weight,
                                            sizeof(fxv_array_t));
    mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&diff, sizeof(fxv_array_t));
    mailbox_write(3 * sizeof(fxv_array_t), (uint8_t*)&c, sizeof(fxv_array_t));
    mailbox_write(4 * sizeof(fxv_array_t), (uint8_t*)&offset, sizeof(fxv_array_t));
    mailbox_write(5 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));
    mailbox_write(6 * sizeof(fxv_array_t), (uint8_t*)&i, sizeof(fxv_array_t));
    /*mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)&(w.bytes[4]), 4);*/
    /*mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)as, as_size);*/

    /**signal = 0x42000000 + i;*/

    // wait until next loop
    while( (get_time_base() - t) < update_period );

    ++i;
  }
}

