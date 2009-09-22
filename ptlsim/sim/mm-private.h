// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Memory Management (private data)
//
// Copyright 2004-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _MM_PRIVATE_H_
#define _MM_PRIVATE_H_

#include <globals.h>

enum {
  PTL_MM_EVENT_ALLOC,
  PTL_MM_EVENT_FREE,
  PTL_MM_EVENT_RECLAIM_START,
  PTL_MM_EVENT_RECLAIM_CALL,
  PTL_MM_EVENT_RECLAIM_END,
  PTL_MM_EVENT_CLEANUP,
  PTL_MM_EVENT_INIT,
  PTL_MM_EVENT_COUNT,
};

static const char* event_names[PTL_MM_EVENT_COUNT] = {"alloc", "free ", "rec-s", "rec-c", "rec-e", "clnup", "init "};

enum {
  PTL_MM_POOL_PAGES,
  PTL_MM_POOL_SLAB,
  PTL_MM_POOL_GENERAL,
  PTL_MM_POOL_ALL,
  PTL_MM_POOL_COUNT,
};

static const char* pool_names[PTL_MM_POOL_COUNT] = {"page", "slab", "gen ", "all "};

struct MemoryManagerEvent {
  W8  event;
  W8  pool;
  W16 slab;
  W32 caller;
  W32 address;
  W32 bytes;

  MemoryManagerEvent() { }

  MemoryManagerEvent(int event, int pool, void* caller, void* address, W32 bytes, int slab = 0) {
    this->event = event;
    this->pool = pool;
    this->caller = LO32(Waddr(caller));
    this->address = LO32(Waddr(address));
    this->bytes = bytes;
    this->slab = slab;
  }

  ostream& print(ostream& os) const {
    os << event_names[event], ' ';

    switch (event) {
    case PTL_MM_EVENT_ALLOC:
    case PTL_MM_EVENT_FREE:
      os << pool_names[pool], " 0x", hexstring(address, 32), ' ', intstring(bytes, 10), " bytes by caller 0x", hexstring(caller, 32);
      if (pool == PTL_MM_POOL_SLAB) os << " (slab ", intstring(slab, 4), ")";
      break;
    case PTL_MM_EVENT_RECLAIM_START:
    case PTL_MM_EVENT_RECLAIM_END:
      os << bytes, " bytes";
      break;
    case PTL_MM_EVENT_RECLAIM_CALL:
      os << "call ", (void*)address;
      break;
    case PTL_MM_EVENT_CLEANUP:
      os << "cleanup pass";
      break;
    case PTL_MM_EVENT_INIT:
      os << "initialize: heap range: ", (void*)caller, "-", (void*)address, " (", bytes, " bytes)";
      break;
    default:
      abort();
      break;
    }
    return os;
  }
};

PrintOperator(MemoryManagerEvent);

#endif // _MM_PRIVATE_H_
