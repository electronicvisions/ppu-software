#pragma once

#include <stdint.h>
#include <attrib.h>

extern uint8_t mailbox_base;
extern uint8_t mailbox_end;


ATTRIB_UNUSED static uint32_t mailbox_write(uint32_t offset, uint8_t* src, uint32_t size) {
	volatile uint8_t* ptr = &mailbox_base + offset;
	uint32_t i;

	for(i=size; (ptr < &mailbox_end) && (i > 0); ptr++, i--) {
		*ptr = *(src++);
	}

	return ptr - &mailbox_base + offset;
}


ATTRIB_UNUSED static uint32_t mailbox_read(uint8_t* dest, uint32_t offset, uint32_t size) {
	volatile uint8_t* ptr = &mailbox_base + offset;
	uint32_t i;

	for(i=size; (dest < &mailbox_end) && (i > 0); ptr++, i--) {
		*(dest++) = *(ptr);
	}

	return ptr - &mailbox_base + offset;
}

