#include <sync.h>
#include <fxv.h>
#include <random.h>
#include <mailbox.h>
#include "syn.h"

static unsigned const NUM_SYN_LOCATIONS = 64 + 4;


void synram_write_all(uint8_t data) {
#define VR_DATA 1
  int i;
  fxv_array_t wdata;

  for(i=0; i<NUM_BYTES_PER_ARRAY; i++)
    wdata.bytes[i] = data;

  sync();
  fxv_load_array(VR_DATA, &wdata);

  for(i=0; i<NUM_SYN_LOCATIONS; ++i) {
    syn_store_decoders(VR_DATA, i);
  }
#undef VR_DATA
}

void synram_write_from_mailbox() {
#define VR_DATA 1
  int i;
  fxv_array_t wdata;

  for(i=0; i<NUM_SYN_LOCATIONS; ++i) {
    mailbox_read((uint8_t*)&wdata, i * sizeof(wdata), sizeof(wdata));
    sync();
    fxv_load_array(VR_DATA, &wdata);
    syn_store_decoders(VR_DATA, i);
  }
#undef VR_DATA
}

void synram_read_all() {
#define VR_DATA 1
  int i;
  fxv_array_t rdata;

  for(i=0; i<NUM_SYN_LOCATIONS; ++i) {
    syn_load_decoders(VR_DATA, i);

    fxv_store_array(&rdata, VR_DATA);
    sync();

    mailbox_write(i * sizeof(rdata),
        (uint8_t*)&rdata,
        sizeof(rdata));
  }
#undef VR_DATA
}

void synram_read() {
  fxv_array_t rdata;

  syn_load_decoders(1, 0);
  fxv_store_array(&rdata, 1);
  sync();
  mailbox_write(0, (uint8_t*)&rdata, sizeof(rdata));
}

void synram_omni_write() {
  uint32_t i;
  volatile uint32_t* ptr = (volatile uint32_t*)(DECODER_BASE_ADDR);

  mailbox_read((volatile uint8_t*)ptr, 0, 33*16*4);
}

void start() {
  /*synram_write();*/
  /*synram_write_all(0x01);*/
  synram_write_from_mailbox();
  /*synram_omni_write();*/
  synram_read_all();

  /*static uint32_t const tmp = 0xdeadface;*/
  /*mailbox_write(0, (uint8_t*)&tmp, sizeof(uint32_t));*/
}

