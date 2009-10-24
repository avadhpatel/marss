
#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <globals.h>
#include <ptlhwdef.h>
//#include <elf.h>

/*
static inline W64 loadphys(Waddr addr) {
	W64 data = 0;
	// Read 64 bits from ram
	data = ldq(addr);
	return data;
}

static inline W64 storemask(Waddr addr, W64 data, byte bytemask) {
	// First we will read 8 byte data from memory then merge it with
	// the new data with bytemask and then will update the memory with
	// new data value
	
	// First Read 64 bits from ram
	W64 old_data = 0;
	old_data = ldq(addr);
	W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask], old_data, data);
	stq(addr, data);
	return data;
}
*/
#endif //_KERNEL_H_
