#pragma once

#include <stdint.h>
#include "libnux/fxv.h"
#include "libnux/attrib.h"

#include "memory_map.h"



#define _syn_load(reg, base, addr, buffer_enable) do { \
	uint32_t offset = (addr) | (buffer_enable); \
	asm ( \
		"fxvinx " #reg ", %[weights], %[offset]\n" \
		:	/* no outputs */ \
		:	[weights] "b" (base), \
			[offset] "r" (offset) \
	); \
} while(0)

#define syn_load_weights(x, y) _syn_load(x, FXVIO_WEIGHT_BASE, y, 0)
#define syn_load_weights_buffered(x, y) _syn_load(x, FXVIO_WEIGHT_BASE, y, FXVIO_BUFFER_ENABLE_MASK)
#define syn_load_decoders(x, y) _syn_load(x, FXVIO_DECODER_BASE, y, 0)
#define syn_load_decoders_buffered(x, y) _syn_load(x, FXVIO_DECODER_BASE, y, FXVIO_BUFFER_ENABLE_MASK)

#define _syn_store(reg, base, addr) do { \
	uint32_t offset = addr; \
	asm ( \
		"fxvoutx " #reg ", %[weights], %[offset]\n" \
		:	/* no outputs */ \
		:	[weights] "b" (base), \
			[offset] "r" (offset) \
	); \
} while(0)

#define syn_store_weights(x, y) _syn_store(x, FXVIO_WEIGHT_BASE, y)
#define syn_store_decoders(x, y) _syn_store(x, FXVIO_DECODER_BASE, y)


typedef enum {
	RST_CAUSAL  = 0x1,
	RST_ACAUSAL = 0x2
} syn_reset_t;

typedef uint32_t syn_addr_t;


#define _syn_reset(row, select, cond) do { \
	uint32_t base = FXVIO_CAUSAL_BASE; \
	uint32_t offset = row; \
	\
	asm ( \
		"fxvoutx " #select ", %[base], %[offset], %[condition]\n" \
		:	/* no output */ \
		:	[base] "b" (base), \
			[offset] "r" (offset), \
			[condition] "n" (cond) \
	); \
} while(0)

#define syn_reset(a, b, c) _syn_reset(a, b, c)


/*
 * Mockup code for pack/unpack instructions
 * */
/*void syn_set_weights(syn_addr_t start, syn_addr_t stop, fxv_array_t* v, fxv_reorder_t const r) {
	syn_addr_t addr;

	switch(r) {
		case REORDER_TYPE_BYTE:
			fxv_load_array(0, v);
			for( ; start <= stop; start++) {
				syn_store_weights(0, start, r, 0);
			}
			break;

		case REORDER_TYPE_HALFWORD:
			fxv_load_array(0, v);
			fxv_load_array(1, v+1);
			fxv_pack(2, 0, 1, REORDER_TYPE_HALFWORD, 0);
			fxv_pack(3, 0, 1, REORDER_TYPE_HALFWORD, 1);

			for( ; start <= stop; start++) {
				if( (start % 2) == 0 )
					syn_store_weights(2, start);
				else
					syn_store_weights(3, start);
			}
			break;
	}
}*/

