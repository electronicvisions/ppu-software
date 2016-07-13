#include "syn.h"
#include "cadc.h"

#include <mailbox.h>
#include <fxv.h>
#include <spr.h>
#include <fxdpt.h>
#include <attrib.h>

#define CADC_NOISE_BITS (1)


/** Definitions for vector registers **/
#define VR_CAUSAL_0 0
#define VR_ACAUSAL_0 1
#define VR_CAUSAL_1 2
#define VR_ACAUSAL_1 3
#define VR_RST 4
#define RST_CAUSAL 0x1
#define RST_ACAUSAL 0x2


/** Constants **/
static uint32_t loc_a = 0;
static int loc_b = 1;

volatile uint32_t* param_row    = (uint32_t*)(0x3900);
volatile uint32_t* signal    = (uint32_t*)(0x3900 + 4);

volatile uint32_t* post = (uint32_t*) 0x1a000101;

void start() {
    uint8_t own_row;
    fxv_array_t a0,a1,c0,c1;
    fxv_zero_vrf();  // clear registers
    *signal = 0;
    own_row = *param_row * 2;
    loc_a = own_row;
    loc_b = own_row + 1;
    fxv_splatb(VR_RST, RST_CAUSAL | RST_ACAUSAL);
    syn_reset(loc_a, VR_RST, COND_ALWAYS);
    syn_reset(loc_b, VR_RST, COND_ALWAYS);
    while (!*signal) ; 
    *signal = 0;
    cadc_load_row(VR_CAUSAL_0, VR_ACAUSAL_0, VR_CAUSAL_1, VR_ACAUSAL_1, own_row);
    fxv_store_array(&c0, VR_CAUSAL_0);
    fxv_store_array(&c1, VR_CAUSAL_1);
    fxv_store_array(&a0, VR_ACAUSAL_0);
    fxv_store_array(&a1, VR_ACAUSAL_1);
    sync();
    mailbox_write(0 * sizeof(fxv_array_t), (uint8_t*)&c0, sizeof(fxv_array_t));
    mailbox_write(1 * sizeof(fxv_array_t), (uint8_t*)&c1, sizeof(fxv_array_t));
    mailbox_write(2 * sizeof(fxv_array_t), (uint8_t*)&a0, sizeof(fxv_array_t));
    mailbox_write(3 * sizeof(fxv_array_t), (uint8_t*)&a1, sizeof(fxv_array_t));
    mailbox_write(4 * sizeof(fxv_array_t), (uint8_t*)param_row, sizeof(uint32_t));
    
}

