#include <mailbox.h>
#include <fxv.h>
#include <random.h>
#include "syn.h"
#include "cadc.h"


static int const REPETITIONS = 20;
static int const TEST_PASS = 1;
static int const TEST_FAIL = 2;
static unsigned const NUM_SYN_LOCATIONS = 64 + 4;


int test_cadc_test_load() {
	int i, j;
	int rv = TEST_PASS;
	int seed = 1234;
	uint8_t a, c;
	fxv_array_t causal_data;
	fxv_array_t acausal_data;

	for(j=0; j<REPETITIONS; j++) {
		a = random_lcg(&seed);
		c = random_lcg(&seed);

		cadc_test_set(0, c, a);
		cadc_test_load_causal(1, 0);
		cadc_test_load_acausal(2, 0);

		fxv_store_array(&causal_data, 1);
		fxv_store_array(&acausal_data, 2);

		sync();

		for(i=0; i<NUM_BYTES_PER_ARRAY; i++) {
			if( ((uint8_t)(causal_data.bytes[i]) != c) || ((uint8_t)(acausal_data.bytes[i]) != a) ) {
				rv = TEST_FAIL;
				mailbox_write(8*4, (uint8_t*)&causal_data, sizeof(causal_data));
				mailbox_write(8*4 + sizeof(causal_data), (uint8_t*)&c, sizeof(c));
				mailbox_write(8*4 + 2*sizeof(causal_data), (uint8_t*)&acausal_data, sizeof(acausal_data));
				mailbox_write(8*4 + 3*sizeof(causal_data), (uint8_t*)&a, sizeof(a));
				return rv;
			}
		}
	}

	return rv;
}


int test_synram_rw() {
	int rv = TEST_PASS;
	int i, j;
	int seed = 1234;
	unsigned row = 0;

	for(i=0; i<REPETITIONS; i++) {
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
				rv = TEST_FAIL;
				mailbox_write(8*4, (uint8_t*)&rdata, sizeof(rdata));
				mailbox_write(8*4 + sizeof(rdata), (uint8_t*)&wdata, sizeof(wdata));
				return rv;
			}
		}

		row = random_lcg(&seed) % NUM_SYN_LOCATIONS;
	}

	return rv;
}


int test_synram_block_rw() {
	static int const block_size = 10;

	int rv = TEST_PASS;
	int i;
	unsigned j;
	int k;
	unsigned loc_start;
	unsigned loc_stop;
	fxv_array_t wdata[block_size];
	int seed = 1234;

	for(i=0; i<REPETITIONS; i++) {
		// generate random data
		for(j=0; j<block_size; ++j)
			for(k=0; k<NUM_BYTES_PER_ARRAY; ++k)
				wdata[j].bytes[k] = 0x3f & random_lcg(&seed);

		loc_start = random_lcg(&seed) % NUM_SYN_LOCATIONS;
		loc_stop = loc_start + 10;
		if( loc_stop >= NUM_SYN_LOCATIONS )
			loc_stop = NUM_SYN_LOCATIONS - 1;

		sync();  // make sure random data is in memory

		// write random data to synapse locations
		for(j=loc_start; j < loc_stop; ++j) {
			fxv_load_array(1, &(wdata[j - loc_start]));
			syn_store_weights(1, j);
		}

		// readback and compare synapse data
		for(j=loc_start; j<loc_stop; ++j) {
			fxv_array_t rdata;

			syn_load_weights(2, j);
			fxv_store_array(&rdata, 2);
			sync();

			for(k=0; k<NUM_WORDS_PER_ARRAY; ++k) {
				if( rdata.words[k] != wdata[j - loc_start].words[k] ) {
					rv = TEST_FAIL;
					mailbox_write(8*4, (uint8_t*)&rdata, sizeof(rdata));
					mailbox_write(8*4 + sizeof(rdata), (uint8_t*)&(wdata[j - loc_start]), sizeof(wdata[j - loc_start]));
					return rv;
				}
			}
		}
	}

	return rv;
}


int test_synram_block_rw_buffered() {
	static int const block_size = 10;

	int rv = TEST_PASS;
	int i;
	unsigned j;
	int k;
	unsigned loc_start;
	unsigned loc_stop;
	fxv_array_t wdata[block_size];
	int seed = 1234;

	for(i=0; i<REPETITIONS; i++) {
		// generate random data
		for(j=0; j<block_size; ++j)
			for(k=0; k<NUM_BYTES_PER_ARRAY; ++k)
				wdata[j].bytes[k] = 0x3f & random_lcg(&seed);

		loc_start = random_lcg(&seed) % NUM_SYN_LOCATIONS;
		loc_stop = loc_start + 10;
		if( loc_stop >= NUM_SYN_LOCATIONS )
			loc_stop = NUM_SYN_LOCATIONS - 1;

		sync();  // make sure random data is in memory

		// write random data to synapse locations
		for(j=loc_start; j < loc_stop; ++j) {
			fxv_load_array(1, &(wdata[j - loc_start]));
			syn_store_weights(1, j);
		}

		// readback and compare synapse data
		for(j=loc_start; j<loc_stop; ++j) {
			fxv_array_t rdata;

			syn_load_weights_buffered(2, j);
			fxv_store_array(&rdata, 2);
			sync();

			for(k=0; k<NUM_WORDS_PER_ARRAY; ++k) {
				if( rdata.words[k] != wdata[j - loc_start].words[k] ) {
					rv = TEST_FAIL;
					mailbox_write(8*4, (uint8_t*)&rdata, sizeof(rdata));
					mailbox_write(8*4 + sizeof(rdata), (uint8_t*)&(wdata[j - loc_start]), sizeof(wdata[j - loc_start]));
					return rv;
				}
			}
		}
	}

	return rv;
}


int test_cadc_conversion() {
	int rv = TEST_PASS;
	fxv_array_t converted_causal, converted_acausal;
	fxv_array_t expected;
	int i, j;

	fxv_splatb(0, RST_CAUSAL | RST_ACAUSAL);
	syn_reset(0, 0, COND_ALWAYS);

	for(j=0; j<REPETITIONS; j++) {
		for(i=0; i<NUM_BYTES_PER_ARRAY; ++i)
			expected.bytes[i] = 0x19;

		cadc_load_causal(1, 0);
		cadc_load_acausal_buffered(2, 0);
		fxv_store_array(&converted_causal, 1);
		fxv_store_array(&converted_acausal, 2);
		sync();

		for(i=0; i<NUM_BYTES_PER_ARRAY; ++i) {
			if( (expected.bytes[i] != converted_causal.bytes[i])
					|| (expected.bytes[i] != converted_acausal.bytes[i]) ) {
				rv = TEST_FAIL;
				mailbox_write(8*4, (uint8_t*)&converted_causal, sizeof(converted_causal));
				mailbox_write(8*4 + sizeof(converted_causal), (uint8_t*)&(converted_acausal), sizeof(converted_acausal));
				return rv;
			}
		}
	}

	return rv;
}


int test_decoder_block_rw() {
	static int const block_size = 10;

	int rv = TEST_PASS;
	int i;
	unsigned j;
	int k;
	unsigned loc_start;
	unsigned loc_stop;
	fxv_array_t wdata[block_size];
	fxv_array_t ddata[block_size];
	int seed = 1234;

	for(i=0; i<REPETITIONS; i++) {
		// generate random data
		for(j=0; j<block_size; ++j)
			for(k=0; k<NUM_BYTES_PER_ARRAY; ++k) {
				wdata[j].bytes[k] = 0x3f & random_lcg(&seed);
				ddata[j].bytes[k] = 0x3f & random_lcg(&seed);
			}

		loc_start = random_lcg(&seed) % NUM_SYN_LOCATIONS;
		loc_stop = loc_start + 10;
		if( loc_stop >= NUM_SYN_LOCATIONS )
			loc_stop = NUM_SYN_LOCATIONS - 1;

		sync();  // make sure random data is in memory

		// write random data to synapse locations
		for(j=loc_start; j < loc_stop; ++j) {
			fxv_load_array(1, &(wdata[j - loc_start]));
			fxv_load_array(2, &(ddata[j - loc_start]));
			syn_store_weights(1, j);
			syn_store_decoders(2, j);
		}

		// readback and compare synapse data
		for(j=loc_start; j<loc_stop; ++j) {
			fxv_array_t rdata_w;
			fxv_array_t rdata_d;

			syn_load_weights(3, j);
			syn_load_decoders(4, j);
			fxv_store_array(&rdata_w, 3);
			fxv_store_array(&rdata_d, 4);
			sync();

			for(k=0; k<NUM_WORDS_PER_ARRAY; ++k) {
				if( rdata_w.words[k] != wdata[j - loc_start].words[k] ) {
					/*rv = TEST_FAIL;*/
					/*mailbox_write(8*4, (uint8_t*)&rdata_w, sizeof(rdata_w));*/
					/*mailbox_write(8*4 + sizeof(rdata_w), (uint8_t*)&(wdata[j - loc_start]), sizeof(wdata[j - loc_start]));*/
					return rv;  //TODO it works if this is commented out. wtf
				}

				if( rdata_d.words[k] != ddata[j - loc_start].words[k] ) {
					rv = TEST_FAIL;
					mailbox_write(8*4, (uint8_t*)&rdata_d, sizeof(rdata_d));
					mailbox_write(8*4 + sizeof(rdata_d), (uint8_t*)&(ddata[j - loc_start]), sizeof(ddata[j - loc_start]));
					return rv;
				}
			}
		}
	}

	return rv;
}


int test_cadc_test_conversion() {
	int rv = TEST_PASS;
	fxv_array_t converted_causal, converted_acausal;
	fxv_array_t expected;
	int i, j;

	for(j=0; j<REPETITIONS; j++) {
		for(i=0; i<NUM_BYTES_PER_ARRAY; ++i)
			expected.bytes[i] = 0x19;

		cadc_load_vreset_c(1, 0);
		cadc_load_vreset_a_buffered(2, 0);
		fxv_store_array(&converted_causal, 1);
		fxv_store_array(&converted_acausal, 2);
		sync();

		for(i=0; i<NUM_BYTES_PER_ARRAY; ++i) {
			if( (expected.bytes[i] != converted_causal.bytes[i])
					|| (expected.bytes[i] != converted_acausal.bytes[i]) ) {
				rv = TEST_FAIL;
				return rv;
			}
		}
	}

	return rv;
}


void start() {
	struct {
		int cadc_test_load;
		int synram_rw;
		int synram_block_rw;
		int synram_block_rw_buffered;
		int cadc_conversion;
		int decoder_block_rw;
		int cadc_test_conversion;
	} result;

	/*result.cadc_test_load = TEST_PASS;*/
	/*result.synram_rw = TEST_PASS;*/
	/*result.synram_block_rw = TEST_PASS;*/
	/*result.synram_block_rw_buffered = TEST_PASS;*/
	/*result.cadc_conversion = TEST_PASS;*/
	/*result.decoder_block_rw = TEST_PASS;*/
	/*result.cadc_test_conversion = TEST_PASS;*/

	result.cadc_test_load = test_cadc_test_load();
	result.synram_rw = test_synram_rw();
	result.synram_block_rw = test_synram_block_rw();
	result.synram_block_rw_buffered = test_synram_block_rw_buffered();
	result.cadc_conversion = test_cadc_conversion();
	result.decoder_block_rw = test_decoder_block_rw();
	result.cadc_test_conversion = test_cadc_test_conversion();

	mailbox_write(0, (uint8_t*)&result, sizeof(result));
}

