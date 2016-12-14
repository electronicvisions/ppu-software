#pragma once

#include "libnux/sync.h"
#include "libnux/attrib.h"

#include "memory_map.h"


#define _cadc_load_templ(_base, reg, addr, buffer_enable) do {\
	uint32_t offset = (addr) | (buffer_enable); \
	\
	asm ( \
		"fxvinx " #reg ", %[base], %[offset]\n" \
		:	/* no outputs */ \
		:	[base] "b" (_base), \
			[offset] "r" (offset) \
	); \
} while(0)


#define cadc_load_causal(reg, addr) _cadc_load_templ(FXVIO_CAUSAL_BASE, reg, addr, 0)
#define cadc_load_acausal(reg, addr) _cadc_load_templ(FXVIO_ACAUSAL_BASE, reg, addr, 0)
#define cadc_load_causal_buffered(reg, addr) _cadc_load_templ(FXVIO_CAUSAL_BASE, reg, addr, FXVIO_BUFFER_ENABLE_MASK)
#define cadc_load_acausal_buffered(reg, addr) _cadc_load_templ(FXVIO_ACAUSAL_BASE, reg, addr, FXVIO_BUFFER_ENABLE_MASK)
#define cadc_test_load_causal(reg, addr) _cadc_load_templ(FXVIO_CAUSAL_BASE | FXVIO_TEST_MASK, reg, addr, 0)
#define cadc_test_load_acausal(reg, addr) _cadc_load_templ(FXVIO_ACAUSAL_BASE | FXVIO_TEST_MASK, reg, addr, 0)
#define cadc_load_vreset_c(reg, addr) _cadc_load_templ(FXVIO_VRESET_C_BASE, reg, addr, 0)
#define cadc_load_vreset_a(reg, addr) _cadc_load_templ(FXVIO_VRESET_A_BASE, reg, addr, 0)
#define cadc_load_vreset_c_buffered(reg, addr) _cadc_load_templ(FXVIO_VRESET_C_BASE, reg, addr, FXVIO_BUFFER_ENABLE_MASK)
#define cadc_load_vreset_a_buffered(reg, addr) _cadc_load_templ(FXVIO_VRESET_A_BASE, reg, addr, FXVIO_BUFFER_ENABLE_MASK)


#define _cadc_load_row(ap0, am0, ap1, am1, addr, buffer_enable) do {\
	register uint32_t base_causal = FXVIO_CAUSAL_BASE | (buffer_enable); \
	register uint32_t base_causal_b = FXVIO_CAUSAL_BASE | FXVIO_BUFFER_ENABLE_MASK; \
	register uint32_t base_acausal = FXVIO_ACAUSAL_BASE | FXVIO_BUFFER_ENABLE_MASK; \
	\
	asm ( \
		"fxvinx " #ap0 ", %[base_causal], %[offset_0]\n" \
		"fxvinx " #am0 ", %[base_acausal], %[offset_0]\n" \
		"fxvinx " #ap1 ", %[base_causal_b], %[offset_1]\n" \
		"fxvinx " #am1 ", %[base_acausal], %[offset_1]\n" \
		:	/* no outputs */ \
		:	[base_causal] "b" (base_causal), \
			[base_causal_b] "b" (base_causal_b), \
			[base_acausal] "b" (base_acausal), \
			[offset_0] "r" (addr), \
			[offset_1] "r" (addr+1) \
	); \
} while(0)

#define cadc_load_row(a, b, c, d, e) _cadc_load_row(a, b, c, d, e, 0)
#define cadc_load_row_buffered(a, b, c, d, e) _cadc_load_row(a, b, c, d, e, FXVIO_BUFFER_ENABLE_MASK)
#define cadc_test_load_row(a, b, c, d, e) _cadc_load_row(a, b, c, d, e, FXVIO_BUFFER_ENABLE_MASK | FXVIO_TEST_MASK)
#define cadc_test_load_row_buffered(a, b, c, d, e) _cadc_load_row(a, b, c, d, e, FXVIO_BUFFER_ENABLE_MASK | FXVIO_TEST_MASK)


ATTRIB_UNUSED static void cadc_test_set(uint16_t addr, uint8_t causal, uint8_t acausal) {
	volatile fxv_array_t v;

	// TODO change to use splath
	v.bytes[0] = causal;
	v.bytes[1] = acausal;

	sync();
	asm volatile (
		"fxvlax 0, 0, %[v]\n"
		"fxvoutx 0, %[iobase], %[offset]\n"
		:	/* no output */
		:	[v] "r" (&v),
			[iobase] "r" (FXVIO_CAUSAL_BASE | FXVIO_TEST_MASK),
			[offset] "r" (addr)
	);
}


