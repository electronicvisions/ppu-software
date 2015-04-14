#pragma once

#include <attrib.h>

ATTRIB_UNUSED static void sync() {
	asm volatile("sync");
}
