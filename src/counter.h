#pragma once 

#include "memory_map.h"
#include <stdint.h>

static uint32_t get_counter(uint8_t neuron){
    volatile uint32_t* ptr = (volatile uint32_t*) (COUNTER_BASE_ADDR + (neuron << 2));
    return *ptr;
}