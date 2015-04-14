#pragma once

#include <stdint.h>
#include <fxv.h>
#include <fxdpt.h>
#include <attrib.h>
#include "syn.h"
#include "cadc.h"


//--------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------

typedef struct {
	int8_t wmax;
	int8_t lambda;
	int8_t lambda_alpha;
} stdp_mult_reward_byte_t;


//--------------------------------------------------------------------------------
// Update functions
//--------------------------------------------------------------------------------

ATTRIB_UNUSED static void stdp_mult_reward_byte_init(stdp_mult_reward_byte_t* config,
		int8_t wmax,
		int8_t lambda,
		int8_t alpha) {
	int i;
	int32_t l = lambda;
	int32_t a = alpha;
	int32_t la = (l * a) / 0x80;

	config->wmax = wmax;
	config->lambda = lambda;
	config->lambda_alpha = la & 0xff;
}

ATTRIB_UNUSED static void stdp_mult_reward_byte_apply_reward(stdp_mult_reward_byte_t* config, int8_t reward) {
	asm volatile(
		"fxvsplatb 0, %[reward]\n"
		"fxvmulbfs 30, 30, 0\n"             // multiply with lambda
		"fxvmulbfs 29, 29, 0\n"             // multiply with lambda_alpha
		:	/* no output */
		:	[reward] "r" (reward)	
	);
}

ATTRIB_UNUSED static void stdp_mult_reward_byte_load_regs(stdp_mult_reward_byte_t* config) {
	asm volatile (
		"fxvsplatb 31, %[wmax]\n"
		"fxvsplatb 30, %[lambda]\n"
		"fxvsplatb 29, %[lambda_alpha]\n"
		: 	/* no output */
		: 	[wmax] "r" (config->wmax),
			[lambda] "r" (config->lambda),
			[lambda_alpha] "r" (config->lambda_alpha)
		:	/* no clobber */
	);
}


#define VR_LAMBDA 30
#define VR_WMAX 31
#define VR_C 29

#define VR_AP 1
#define VR_AM 2
#define VR_WIN 3
#define VR_WOUT 4 
#define VR_A 5
#define VR_U 6
#define VR_V 7
#define VR_T 8 
#define VR_WOUT_0 9
#define VR_WOUT_1 10

#define VR_B_AP 11
#define VR_B_AM 12
#define VR_B_WIN 13
#define VR_B_WOUT 14 
#define VR_B_A 15
#define VR_B_U 16
#define VR_B_V 17
#define VR_B_T 18 
#define VR_B_WOUT_0 19
#define VR_B_WOUT_1 20

#define VR_C_AP 21
#define VR_C_AM 22
#define VR_D_AP 23
#define VR_D_AM 24

#define VR_TMP_0 25
#define VR_TMP_1 26
#define VR_RST 27


ATTRIB_UNUSED static void stdp_mult_reward_byte_6_8_update(stdp_mult_reward_byte_t* config,
		unsigned row_start,
		unsigned row_stop) {
	unsigned row;
	unsigned rst_select = RST_CAUSAL | RST_ACAUSAL;

	if( row_stop < row_start )
		return;

#ifdef STDP_MULT_REWARD_TEST_LOAD
	cadc_test_set(0, F8(0.75), F8(0.5));
#endif
	fxv_zero_set(0);
	fxv_splatb(VR_RST, rst_select);

	// load correlation from the first row
#ifdef STDP_MULT_REWARD_TEST_LOAD
	//cadc_test_load_causal(VR_AP, row_start);
	//cadc_test_load_acausal(VR_AM, row_start);
	//cadc_test_load_causal(VR_B_AP, row_start +1);
	//cadc_test_load_acausal(VR_B_AM, row_start +1);
	cadc_test_load_row(VR_AP, VR_AM, VR_B_AP, VR_B_AM, row_start);
#else
	//cadc_load_causal(VR_AP, row_start);
	//cadc_load_acausal_buffered(VR_AM, row_start);
	//cadc_load_causal_buffered(VR_B_AP, row_start +1);
	//cadc_load_acausal_buffered(VR_B_AM, row_start +1);
	cadc_load_row(VR_AP, VR_AM, VR_B_AP, VR_B_AM, row_start);
#endif

	syn_reset(row_start, VR_RST, COND_ALWAYS);
	syn_reset(row_start +1, VR_RST, COND_ALWAYS);

	for(row=row_start; row <= row_stop; row += 2) {
		// load weights
		syn_load_weights(VR_WIN, row);
		syn_load_weights_buffered(VR_B_WIN, row +1);

		if( row <= row_stop - 2 ) {
			// load correlation
#ifdef STDP_MULT_REWARD_TEST_LOAD
			//cadc_test_load_causal(VR_C_AP, row +2);
			//cadc_test_load_acausal(VR_C_AM, row +2);
			//cadc_test_load_causal(VR_D_AP, row +3);
			//cadc_test_load_acausal(VR_D_AM, row +3);
			cadc_test_load_row_buffered(VR_C_AP, VR_C_AM, VR_D_AP, VR_D_AM, row +2);
#else
			//cadc_load_causal(VR_C_AP, row +2);
			//cadc_load_acausal_buffered(VR_C_AM, row +2);
			//cadc_load_causal_buffered(VR_D_AP, row +3);
			//cadc_load_acausal_buffered(VR_D_AM, row +3);
			cadc_load_row_buffered(VR_C_AP, VR_C_AM, VR_D_AP, VR_D_AM, row +2);
#endif

			syn_reset(row +2, VR_RST, COND_ALWAYS);
			syn_reset(row +3, VR_RST, COND_ALWAYS);
		}

		{
			// compute STDP
			fxv_shb(VR_TMP_0, VR_AP, -1);
			fxv_shb(VR_TMP_1, VR_AM, -1);

			fxv_subbfs(VR_A, VR_TMP_0, VR_TMP_1);
			fxv_cmpb(VR_A);
			
			fxv_shb(VR_WIN, VR_WIN, 1);

			fxv_subbfs(VR_U, VR_WMAX, VR_WIN);
			fxv_mulbfs(VR_T, VR_U, VR_LAMBDA);
			fxv_mulbfs(VR_V, VR_C, VR_WIN);

			fxv_mtacbf(VR_WIN);
			fxv_mabfs(VR_WOUT_0, VR_A, VR_T);
			fxv_mabfs(VR_WOUT_1, VR_A, VR_V);
			fxv_sel_lt(VR_WOUT, VR_WOUT_0, VR_WOUT_1);  // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1

			fxv_shb(VR_WOUT, VR_WOUT, -1);
		}
		
		{
			// compute STDP
			fxv_shb(VR_B_AP, VR_B_AP, -1);
			fxv_shb(VR_B_AM, VR_B_AM, -1);

			fxv_subbfs(VR_B_A, VR_B_AP, VR_B_AM);
			fxv_cmpb(VR_B_A);
			
			fxv_shb(VR_B_WIN, VR_B_WIN, 1);

			fxv_subbfs(VR_B_U, VR_WMAX, VR_B_WIN);
			fxv_mulbfs(VR_B_T, VR_B_U, VR_LAMBDA);
			fxv_mulbfs(VR_B_V, VR_C, VR_B_WIN);

			fxv_mtacbf(VR_B_WIN);
			fxv_mabfs(VR_B_WOUT_0, VR_B_A, VR_B_T);
			fxv_mabfs(VR_B_WOUT_1, VR_B_A, VR_B_V);
			fxv_sel_lt(VR_B_WOUT, VR_B_WOUT_0, VR_B_WOUT_1);  // VR_WOUT <- VR_WOUT_0 if !lt else VR_WOUT_1

			fxv_shb(VR_B_WOUT, VR_B_WOUT, -1);
		}
		
		fxv_mov(VR_AP, VR_C_AP);
		fxv_mov(VR_AM, VR_C_AM);
		fxv_mov(VR_B_AP, VR_D_AP);
		fxv_mov(VR_B_AM, VR_D_AM);

		// store results
		syn_store_weights(VR_WOUT, row);
		syn_store_weights(VR_B_WOUT, row +1);
	}
}

#undef VR_LAMBDA
#undef VR_WMAX
#undef VR_C

#undef VR_A
#undef VR_U
#undef VR_V
#undef VR_T
#undef VR_WOUT_0
#undef VR_WOUT_1
#undef VR_AP
#undef VR_AM
#undef VR_WIN
#undef VR_WOUT

#undef VR_B_A
#undef VR_B_U
#undef VR_B_V
#undef VR_B_T
#undef VR_B_WOUT_0
#undef VR_B_WOUT_1
#undef VR_B_AP
#undef VR_B_AM
#undef VR_B_WIN
#undef VR_B_WOUT

#undef VR_C_AP
#undef VR_C_AM
#undef VR_D_AP
#undef VR_D_AM

#undef VR_TMP_0
#undef VR_TMP_1

#undef VR_RST


