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
volatile uint32_t* param_c      = (uint32_t*)(0x3900 + 16);


ATTRIB_UNUSED static void compute_weight_incr()
{
  fxv_shb(VR_WIN, VR_WEIGHT, 1);

  fxv_splatb(VR_TMP_0, 0x02);
  fxv_addbfs(VR_WOUT,VR_WIN,VR_TMP_0);

  fxv_shb(VR_WEIGHT, VR_WOUT, -1);
}


void start() {
  fxv_array_t a, c, w, diff, offset;
  time_base_t t;
  int i, j;
  /*uint8_t as[as_size];*/

  /**signal = 0;*/
  fxv_zero_vrf();  // clear registers

  /*for(i=0; i<as_size; ++i)*/
    /*as[i] = 0;*/

  // reset correlation
  fxv_splatb(VR_RST, RST_CAUSAL | RST_ACAUSAL);
  /*syn_reset(loc_a, VR_RST, COND_ALWAYS);*/
  /*syn_reset(loc_b, VR_RST, COND_ALWAYS);*/

  // reset values from CADC calibration
  fxv_splatb(VR_NULL_C, 0x0);
  /*fxv_splatb(VR_NULL_A, 0x0);*/
  /*fxv_splatb(VR_NULL_C, 0xff - 0xe6);*/
  fxv_splatb(VR_NULL_A, 0xb);
  /*fxv_splatb(VR_NULL_C, 0xff - 0x50);*/
  /*fxv_splatb(VR_NULL_A, 0xff - 0x64);*/

  // load initial reset values
  /*cadc_load_causal(VR_CAUSAL, 0);*/
  /*cadc_load_acausal_buffered(VR_ACAUSAL, 0);*/

  /*// correct for CADC bug*/
  /*fxv_addbm(VR_CAUSAL, VR_CAUSAL, VR_NULL_C);*/
  /*fxv_shb(VR_CAUSAL, VR_CAUSAL, -CADC_NOISE_BITS);*/
  /*fxv_addbm(VR_ACAUSAL, VR_ACAUSAL, VR_NULL_A);*/
  /*fxv_shb(VR_ACAUSAL, VR_ACAUSAL, -CADC_NOISE_BITS);*/

  // determine offset
  // leave it to zero for now
  /*fxv_subbm(VR_OFFSET, VR_CAUSAL, VR_ACAUSAL);*/
  /*fxv_shb(VR_OFFSET, VR_TMP, -CADC_NOISE_BITS); // make fractional positive number*/

  // subtract margin for noise
  /*fxv_subbm(VR_NULL_C, VR_NULL_C, 16);*/
  /*fxv_subbm(VR_NULL_A, VR_NULL_A, 16);*/

  /*fxv_store_array(&a, VR_NULL_C);*/
  /*sync();*/
  /*mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));*/

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
  fxv_splatb(VR_WEIGHT, *param_weight);
  fxv_splatb(VR_THRESH, *param_thresh);
  fxv_splatb(VR_WMAX, *param_wmax);
  fxv_splatb(VR_LAMBDA, *param_lam);
  fxv_splatb(VR_C, *param_c);

  loc_a =  *param_lam;

  // loop for weight update
  i = 0;
  j = 0;
  /*while( 1 ) {*/
  /*for(loc_a=0; loc_a<64; loc_a+=4) {*/
  {

    loc_b = loc_a + 1;
    // record current time
    t = get_time_base();

    syn_load_weights(VR_WEIGHT, loc_a);

    // compute STDP
    compute_weight_incr();

    syn_store_weights(VR_WEIGHT, loc_a);


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
    /*mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)&offset, sizeof(fxv_array_t));*/
    mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&diff, sizeof(fxv_array_t));
    mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)&c, sizeof(fxv_array_t));
    mailbox_write(3 * sizeof(fxv_array_t), (uint8_t*)&a, sizeof(fxv_array_t));
    /*mailbox_write(3 * sizeof(fxv_array_t), (uint8_t*)&i, sizeof(i));*/
    /*mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)&(w.bytes[4]), 4);*/
    /*mailbox_write(3 * sizeof(fxv_array_t) + sizeof(i), (uint8_t*)as, as_size);*/
    mailbox_write(4 * sizeof(fxv_array_t), (uint8_t*)param_lam, sizeof(uint32_t));

    /**signal = 0x42000000 + i;*/

    // wait until next loop
    /*while( (get_time_base() - t) < update_period );*/

    ++i;
  }
}

