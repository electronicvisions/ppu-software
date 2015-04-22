#include <sync.h>
#include <fxv.h>
#include <random.h>
#include <mailbox.h>
#include "syn.h"

static unsigned const NUM_SYN_LOCATIONS = 64 + 4;
static unsigned const NUM_WRITES = 10;


static void synram_write_all(uint8_t data) {
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

static void synram_write_from_mailbox() {
#define VR_DATA 1
#define VR_TMP 2
  unsigned i, k;
  fxv_array_t wdata;

  /*mailbox_read((uint8_t*)&wdata, 0, sizeof(wdata));*/
  /*sync();*/
  /*fxv_load_array(VR_DATA, &wdata);*/

  for(i=0; i<NUM_SYN_LOCATIONS; ++i) {
    mailbox_read((uint8_t*)&wdata, i * sizeof(wdata), sizeof(wdata));
    sync();
    fxv_load_array(VR_DATA, &wdata);
    syn_load_decoders(VR_TMP, i);

    for(k=0; k<NUM_WRITES; ++k) {
      syn_store_decoders(VR_DATA, i);
    }
  }
#undef VR_TMP
#undef VR_DATA
}

static void synram_write_once(unsigned loc, fxv_array_t* data) {
#define VR_DATA 1
#define VR_TMP 2
  unsigned k;

  fxv_load_array(VR_DATA, &data);
  syn_load_decoders(VR_TMP, loc);

  for(k=0; k<NUM_WRITES; ++k)
    syn_store_decoders(VR_DATA, loc);

#undef VR_TMP
#undef VR_DATA
}

static void synram_read_all() {
#define VR_DATA 1
  int i;
  fxv_array_t rdata;

  for(i=0; i<NUM_SYN_LOCATIONS; ++i) {
    syn_load_decoders(VR_DATA, i);
    syn_load_decoders(VR_DATA, i);

    fxv_store_array(&rdata, VR_DATA);
    sync();

    mailbox_write(i * sizeof(rdata),
        (uint8_t*)&rdata,
        sizeof(rdata));
  }
#undef VR_DATA
}


static void synram_repeated_read() {
#define VR_DATA 1
  int i, j, k;
  fxv_array_t rdata;

  for(j=0; j<64; ++j) {
    for(i=0; i<1; ++i) {
      syn_load_decoders(VR_DATA, i);
      syn_load_decoders(VR_DATA, i);

      fxv_store_array(&rdata, VR_DATA);
      sync();

      mailbox_write((j*1 + i) * sizeof(rdata),
          (uint8_t*)&rdata,
          sizeof(rdata));
    }

    for(k=0; k<50*j; ++k) asm volatile("nop");
  }
#undef VR_DATA
}

static void synram_read() {
  fxv_array_t rdata;

  syn_load_decoders(1, 0);
  fxv_store_array(&rdata, 1);
  sync();
  mailbox_write(0, (uint8_t*)&rdata, sizeof(rdata));
}

static void synram_omni_write() {
  volatile uint32_t* ptr = (volatile uint32_t*)(DECODER_BASE_ADDR);

  mailbox_read((volatile uint8_t*)ptr, 0, 33*16*4);
}

void start() {
  unsigned k;
  fxv_array_t wdata;

  for(k=0; k<NUM_BYTES_PER_ARRAY; ++k)
    wdata.bytes[k] = 0x3f;
  sync();

  /*synram_write();*/
  /*synram_write_all(0x3f);*/
  synram_write_from_mailbox();
  synram_write_once(0, &wdata);
  /*synram_omni_write();*/
  /*for(k=0; k<250; ++k) asm volatile("nop");*/
  synram_read_all();
  /*synram_repeated_read();*/

  /*static uint32_t const tmp = 0xdeadface;*/
  /*mailbox_write(0, (uint8_t*)&tmp, sizeof(uint32_t));*/
}

