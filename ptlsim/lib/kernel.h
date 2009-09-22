
#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <globals.h>
#include <ptlhwdef.h>
#include <elf.h>

static inline W64 loadphys(Waddr addr) {
  addr = floor(signext64(addr, 48), 8);
  W64& data = *(W64*)(Waddr)addr;
  return data;
}

static inline W64 storemask(Waddr addr, W64 data, byte bytemask) {
  addr = floor(signext64(addr, 48), 8);
  W64& mem = *(W64*)(Waddr)addr;
  mem = mux64(expand_8bit_to_64bit_lut[bytemask], mem, data);
  return data;
}

#endif //_KERNEL_H_
