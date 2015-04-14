#include <sync.h>
#include <fxv.h>
#include <random.h>
#include <mailbox.h>
#include "syn.h"

static unsigned const NUM_SYN_LOCATIONS = 64 + 4;

void synram_write() {
	int i, j;
	int seed = 1234;
	unsigned row = 0;

  fxv_array_t wdata;
  fxv_array_t rdata;

  for(j=0; j<NUM_BYTES_PER_ARRAY; j++) {
    wdata.bytes[j] = random_lcg(&seed) & 0x3f;
  }

  sync();
  fxv_load_array(1, &wdata);
  syn_store_weights(1, row);
  syn_load_weights(2, row);
  fxv_store_array(&rdata, 2);
  sync();

  for(j=0; j<NUM_WORDS_PER_ARRAY; j++) {
    if( rdata.words[j] != wdata.words[j] ) {
      mailbox_write(0, (uint8_t*)&rdata, sizeof(rdata));
      mailbox_write(0 + sizeof(rdata), (uint8_t*)&wdata, sizeof(wdata));
      return;
    }
  }

  row = random_lcg(&seed) % NUM_SYN_LOCATIONS;
}

void start() {
  synram_write();
}
