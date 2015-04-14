#pragma once

#include <stdint.h>
#include <fxv.h>
#include <fxdpt.h>
#include <attrib.h>
#include "syn.h"
#include "cadc.h"


//--------------------------------------------------------------------------------
// Preprocessor macros
//--------------------------------------------------------------------------------

// number of slices of the fxv functional unit
//#ifndef NUM_SLICES
//#	define NUM_SLICES (1)
//#endif

//// number of half-word elements of the fxv functional unit
//#ifndef NUM_ELEMS
//#	define NUM_ELEMS (8)
//#endif

//#define NUM_HALFWORDS (NUM_SLICES*NUM_ELEMS)

//--------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------

typedef struct {
	// it seems gcc does not regard reads from asm volatile blocks, when
	// it removes code. Therefore, volatile is necessary here.
	//volatile int16_t wmax[NUM_HALFWORDS];
	//volatile int16_t lambda[NUM_HALFWORDS];
	//volatile int16_t lambda_alpha[NUM_HALFWORDS];

	fxv_array_t wmax;
	fxv_array_t lambda;
	fxv_array_t lambda_alpha;
} stdp_mult_reward_t;


//--------------------------------------------------------------------------------
// Update functions
//--------------------------------------------------------------------------------

ATTRIB_UNUSED static void stdp_mult_reward_init(stdp_mult_reward_t* config,
		int16_t wmax,
		int16_t lambda,
		int16_t alpha) {
	int i;
	int32_t l = lambda;
	int32_t a = alpha;
	int32_t la = (l * a) / 0x8000;
	int16_t lambda_alpha = la & 0xffff;

	for(i=0; i<NUM_HALFWORDS_PER_ARRAY; ++i) {
		config->wmax.halfwords[i] = wmax;
		config->lambda.halfwords[i] = lambda;
		config->lambda_alpha.halfwords[i] = lambda_alpha;
	}
}

ATTRIB_UNUSED static void stdp_mult_reward_apply_reward(stdp_mult_reward_t* config, int16_t reward) {
	asm volatile(
		"fxvsplath 0, %[reward]\n"
		"fxvmulhfs 30, 30, 0\n"             // multiply with lambda
		"fxvmulhfs 29, 29, 0\n"             // multiply with lambda_alpha
		:	/* no output */
		:	[reward] "r" (reward)	
	);
}

ATTRIB_UNUSED static void stdp_mult_reward_load_regs(stdp_mult_reward_t* config) {
	int16_t wmax = config->wmax.halfwords[0];
	int16_t lambda = config->lambda.halfwords[0];
	int16_t lambda_alpha = config->lambda_alpha.halfwords[0];

	asm volatile (
		"fxvsplath 31, %[wmax]\n"
		"fxvsplath 30, %[lambda]\n"
		"fxvsplath 29, %[lambda_alpha]\n"
		: 	/* no output */
		: 	[wmax] "r" (wmax),
			[lambda] "r" (lambda),
			[lambda_alpha] "r" (lambda_alpha)
		:	/* no clobber */
	);
}


ATTRIB_UNUSED static void stdp_mult_reward_load_regs_byte(stdp_mult_reward_t* config) {
	int8_t wmax = config->wmax.bytes[0];
	int8_t lambda = config->lambda.bytes[0];
	int8_t lambda_alpha = config->lambda_alpha.bytes[0];

	asm volatile (
		"fxvsplath 31, %[wmax]\n"
		"fxvsplath 30, %[lambda]\n"
		"fxvsplath 29, %[lambda_alpha]\n"
		: 	/* no output */
		: 	[wmax] "r" (wmax),
			[lambda] "r" (lambda),
			[lambda_alpha] "r" (lambda_alpha)
		:	/* no clobber */
	);
}


#define VR_ZERO 0
#define VR_A0P 1
#define VR_A1P 2
#define VR_A0M 3
#define VR_A1M 4
#define VR_W0U 5
#define VR_W1U 6
#define VR_W0L 7
#define VR_W1L 8
#define VR_WL 9
#define VR_WR 10
#define VR_APL 11
#define VR_APR 12
#define VR_AML 13
#define VR_AMR 14
#define VR_ASCALE 28
#define VR_WSCALE 27
#define VR_ONE 26
#define VR_RST 19

static void helper_mult_reward_fs() {
	// compute STDP
#define VR_LAMBDA 30
#define VR_WMAX 31
#define VR_C 29
#define VR_A 15
#define VR_U 16
#define VR_V 17
#define VR_T 18
	fxv_mulhfs(VR_WL, VR_WL, VR_WSCALE);  // w <- w * WSCALE
	fxv_cmph(VR_WL);                      // VCR <- (w > 0, w < 0, w == 0)
	fxv_addhfs_lt(VR_WL, VR_WL, VR_ONE);  // w <- w + 1 if VCR.lt

	fxv_subhfs(VR_A, VR_APL, VR_AML);     // a <- a+ - a-
	fxv_cmph(VR_A);                       // VCR <- (a > 0, a < 0, a == 0)

	fxv_mulhm(VR_A, VR_A, VR_ASCALE);     // a <- a * ASCALE
	fxv_subhfs(VR_U, VR_WMAX, VR_WL);     // u <- wmax - w
	fxv_mulhfs(VR_T, VR_U, VR_LAMBDA);    // t <- u * lambda
	fxv_mulhfs(VR_V, VR_C, VR_WL);        // v <- c * w
	fxv_mtachf(VR_WL);                    // ACC <- w
	fxv_mahfs_gt(VR_WL, VR_A, VR_T);      // w <- a * t + w if VCR.gt
	fxv_mahfs_lt(VR_WL, VR_A, VR_V);      // w <- a * v + w if VCR.lt
	fxv_addhm(VR_WL, VR_WL, VR_WL);      // w <- w + w  aka 2 * w

	// now repeat for the right half
	fxv_mulhfs(VR_WR, VR_WR, VR_WSCALE);  // w <- w * WSCALE
	fxv_cmph(VR_WR);                      // VCR <- (w > 0, w < 0, w == 0)
	fxv_addhfs_lt(VR_WR, VR_WR, VR_ONE);  // w <- w + 1 if VCR.lt

	fxv_subhfs(VR_A, VR_APR, VR_AMR);     // a <- a+ - a-
	fxv_cmph(VR_A);                       // VCR <- (a > 0, a < 0, a == 0)

	fxv_mulhm(VR_A, VR_A, VR_ASCALE);     // a <- a * ASCALE
	fxv_subhfs(VR_U, VR_WMAX, VR_WR);     // u <- wmax - w
	fxv_mulhfs(VR_T, VR_U, VR_LAMBDA);    // t <- u * lambda
	fxv_mulhfs(VR_V, VR_C, VR_WR);        // v <- c * w
	fxv_mtachf(VR_WR);                    // ACC <- w
	fxv_mahfs_gt(VR_WR, VR_A, VR_T);      // w <- a * t + w if VCR.gt
	fxv_mahfs_lt(VR_WR, VR_A, VR_V);      // w <- a * v + w if VCR.lt
	fxv_addhm(VR_WR, VR_WR, VR_WR);      // w <- w + w  aka 2 * w
#undef VR_LAMBDA
#undef VR_WMAX
#undef VR_C
#undef VR_A
#undef VR_U
#undef VR_V
#undef VR_T
}

ATTRIB_UNUSED static void stdp_mult_reward_update(stdp_mult_reward_t* config,
		unsigned row_start,
		unsigned row_stop) {
	unsigned row;

	if( row_stop < row_start )
		return;

#ifdef STDP_MULT_REWARD_TEST_LOAD
	cadc_test_set(0, 0x7f, 0x00);
#endif
	fxv_zero_set(0);

	fxv_splath(VR_ASCALE, 0x80);
	fxv_splath(VR_WSCALE, 0x4000);
	fxv_splath(VR_ONE, 0x7fff);
	fxv_splatb(VR_RST, RST_CAUSAL | RST_ACAUSAL);

	for(row=row_start; row <= row_stop; row += 4) {
		// load correlation from top row
#ifdef STDP_MULT_REWARD_TEST_LOAD
		cadc_test_load_causal(VR_A0P, row);
		cadc_test_load_causal(VR_A1P, row+1);
		cadc_test_load_acausal(VR_A0M, row);
		cadc_test_load_acausal(VR_A1M, row+1);
#else
		cadc_load_causal(VR_A0P, row);
		cadc_load_causal_buffered(VR_A1P, row+1);
		cadc_load_acausal_buffered(VR_A0M, row);
		cadc_load_acausal_buffered(VR_A1M, row+1);
#endif
		// load packed weights from two rows
		syn_load_weights(VR_W0U, row);
		syn_load_weights_buffered(VR_W1U, row+1);
		syn_load_weights(VR_W0L, row+2);
		syn_load_weights_buffered(VR_W1L, row+3);
		// unpack weights
		fxv_unpack_to_halfwords_padded(VR_WL, VR_WR, VR_W0U, VR_W0L, 2);

		// unpack correlation
		fxv_unpack_to_halfwords(VR_APL, VR_APR, 0, VR_A0P);
		fxv_unpack_to_halfwords(VR_AML, VR_AMR, 0, VR_A0M);

		helper_mult_reward_fs();
		
		// store results
		fxv_pack_halfwords_padded(VR_W0U, VR_W0L, VR_WL, VR_WR, 2);
		syn_store_weights(VR_W0U, row);
		syn_store_weights(VR_W0L, row + 2);
		syn_reset(row, VR_RST, COND_ALWAYS);
		
		// unpack weights
		fxv_unpack_to_halfwords_padded(VR_WL, VR_WR, VR_W1U, VR_W1L, 2);

		// unpack correlation
		fxv_unpack_to_halfwords(VR_APL, VR_APR, 0, VR_A1P);
		fxv_unpack_to_halfwords(VR_AML, VR_AMR, 0, VR_A1M);

		helper_mult_reward_fs();

		// store results
		fxv_pack_halfwords_padded(VR_W1U, VR_W1L, VR_WL, VR_WR, 2);
		syn_store_weights(VR_W1U, row + 1);
		syn_store_weights(VR_W1L, row + 3);
		syn_reset(row, VR_RST, COND_ALWAYS);
	}
}


static void helper_mult_reward_6_16_fs() {
	// compute STDP
#define VR_LAMBDA 30
#define VR_WMAX 31
#define VR_C 29
#define VR_A 15
#define VR_U 16
#define VR_V 17
#define VR_T 18
	fxv_subhfs(VR_A, VR_APL, VR_AML);     // a <- a+ - a-
	fxv_cmph(VR_A);                       // VCR <- (a > 0, a < 0, a == 0)

	fxv_mulhm(VR_A, VR_A, VR_ASCALE);     // a <- a * ASCALE
	fxv_subhfs(VR_U, VR_WMAX, VR_WL);     // u <- wmax - w
	fxv_mulhfs(VR_T, VR_U, VR_LAMBDA);    // t <- u * lambda
	fxv_mulhfs(VR_V, VR_C, VR_WL);        // v <- c * w
	fxv_mtachf(VR_WL);                    // ACC <- w
	fxv_mahfs_gt(VR_WL, VR_A, VR_T);      // w <- a * t + w if VCR.gt
	fxv_mahfs_lt(VR_WL, VR_A, VR_V);      // w <- a * v + w if VCR.lt
	fxv_addhm(VR_WL, VR_WL, VR_WL);
	//fxv_addhm_gt(VR_WL, VR_WL, VR_WL);       // w <- w + w  aka 2 * w  if VCR.gt
	//fxv_addhm_lt(VR_WL, VR_WL, VR_WL);       // w <- w + w  aka 2 * w  if VCR.lt

	// now repeat for the right half
	fxv_subhfs(VR_A, VR_APR, VR_AMR);     // a <- a+ - a-
	fxv_cmph(VR_A);                       // VCR <- (a > 0, a < 0, a == 0)

	fxv_mulhm(VR_A, VR_A, VR_ASCALE);     // a <- a * ASCALE
	fxv_subhfs(VR_U, VR_WMAX, VR_WR);     // u <- wmax - w
	fxv_mulhfs(VR_T, VR_U, VR_LAMBDA);    // t <- u * lambda
	fxv_mulhfs(VR_V, VR_C, VR_WR);        // v <- c * w
	fxv_mtachf(VR_WR);                    // ACC <- w
	fxv_mahfs_gt(VR_WR, VR_A, VR_T);      // w <- a * t + w if VCR.gt
	fxv_mahfs_lt(VR_WR, VR_A, VR_V);      // w <- a * v + w if VCR.lt
	fxv_addhm(VR_WR, VR_WR, VR_WR);
	//fxv_addhm_gt(VR_WR, VR_WR, VR_WR);       // w <- w + w  aka 2 * w  if VCR.gt
	//fxv_addhm_lt(VR_WR, VR_WR, VR_WR);       // w <- w + w  aka 2 * w  if VCR.lt
#undef VR_LAMBDA
#undef VR_WMAX
#undef VR_C
#undef VR_A
#undef VR_U
#undef VR_V
#undef VR_T
}


ATTRIB_UNUSED static void stdp_mult_reward_6_16_update(stdp_mult_reward_t* config,
		unsigned row_start,
		unsigned row_stop) {
	unsigned row;

	if( row_stop < row_start )
		return;

#ifdef STDP_MULT_REWARD_TEST_LOAD
	cadc_test_set(0, F8(0.75), F8(0.5));
#endif
	fxv_zero_set(0);

	fxv_splath(VR_ASCALE, 0x80);
	fxv_splath(VR_WSCALE, 0x4000);
	fxv_splath(VR_ONE, 0x7fff);
	fxv_splatb(VR_RST, RST_CAUSAL | RST_ACAUSAL);

	for(row=row_start; row <= row_stop; row += 1) {
		// load correlation from top row
#ifdef STDP_MULT_REWARD_TEST_LOAD
#else
		cadc_load_causal(VR_A0P, row);
		cadc_load_acausal(VR_A0M, row);
#endif
		// load packed weights from two rows
		syn_load_weights(VR_W0U, row);
		// unpack weights
		fxv_unpack_to_halfwords_padded(VR_WL, VR_WR, VR_W0U, VR_ZERO, 1);

		// unpack correlation
		fxv_unpack_to_halfwords(VR_APL, VR_APR, 0, VR_A0P);
		fxv_unpack_to_halfwords(VR_AML, VR_AMR, 0, VR_A0M);

		helper_mult_reward_6_16_fs();
		
		// store results
		fxv_pack_halfwords_padded(VR_W0U, VR_W0L, VR_WL, VR_WR, 2);
		syn_store_weights(VR_W0U, row);
		syn_reset(row, VR_RST, COND_ALWAYS);
	}
}

#undef VR_ZERO
#undef VR_A0P
#undef VR_A1P
#undef VR_A0M
#undef VR_A1M
#undef VR_W0U
#undef VR_W1U
#undef VR_W0L
#undef VR_W1L
#undef VR_WL
#undef VR_WR
#undef VR_APL
#undef VR_APR
#undef VR_AML
#undef VR_AMR
#undef VR_ASCALE
#undef VR_WSCALE
#undef VR_ONE
#undef VR_RST



