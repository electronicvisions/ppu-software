#pragma once

#include <stdint.h>
#include "memory_map.h"

typedef struct {
	uint32_t row_mask;
	uint8_t addr;
} spike_t;


static void spikes_send(spike_t* sp) {
	volatile uint32_t* ptr = (volatile uint32_t*)(SPIKE_BASE_ADDR + sp->addr);
	*ptr = sp->row_mask;
}

