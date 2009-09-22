//
// PTLsim: Cycle Accurate x86-64 Simulator
// Memory Management
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#ifdef PTLSIM_HYPERVISOR
#include <ptlxen.h>
#else
#include <kernel.h>
#endif
#include <mm.h>
#include <datastore.h>
#include <mm-private.h>

extern ostream logfile;

extern void early_printk(const char* s);

#define ENABLE_MM_LOGGING

static const int mm_event_buffer_size = 16384; // i.e. 256 KB

#ifdef ENABLE_MM_LOGGING
MemoryManagerEvent mm_event_buffer[mm_event_buffer_size];
#endif

MemoryManagerEvent* mm_event_buffer_head = null;
MemoryManagerEvent* mm_event_buffer_tail = null;
MemoryManagerEvent* mm_event_buffer_end = null;

int mm_logging_fd = -1;
bool enable_inline_mm_logging = false;
bool enable_mm_validate = false;

void ptl_mm_set_logging(const char* mm_log_filename, int mm_event_buffer_size, bool set_enable_inline_mm_logging) {
  //
  // The event buffer itself is allocated at boot time and set up in ptl_mm_init().
  // This just changes the output file for the first time.
  //
  if (mm_logging_fd < 0) {
    if (mm_log_filename) {
      mm_logging_fd = sys_open(mm_log_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else {
      mm_event_buffer_head = null;
      mm_event_buffer_tail = null;
      mm_event_buffer_end = null;
    }
  }

  //
  // Flush any entries that have accumulated since booting
  //
  ptl_mm_flush_logging();

  enable_inline_mm_logging = set_enable_inline_mm_logging;
}

void ptl_mm_set_validate(bool set_enable_mm_validate) {
  enable_mm_validate = set_enable_mm_validate;
}

void ptl_mm_flush_logging() {
  if likely (!mm_event_buffer_head) return;
  if unlikely (mm_logging_fd < 0) return;

  int count = mm_event_buffer_tail - mm_event_buffer_head;
  int bytes = count * sizeof(MemoryManagerEvent);

  assert(sys_write(mm_logging_fd, mm_event_buffer_head, bytes) == bytes);
  mm_event_buffer_tail = mm_event_buffer_head;
}

void ptl_mm_add_event(int event, int pool, void* caller, void* address, W32 bytes, int slab = 0) {
  if likely (!mm_event_buffer_head) return;

  MemoryManagerEvent* e = mm_event_buffer_tail;
  e->event = event;
  e->pool = pool;
  e->caller = LO32(Waddr(caller));
  e->address = LO32(Waddr(address));
  e->bytes = bytes;
  e->slab = slab;

  if unlikely (enable_inline_mm_logging) {
    cerr << "mm: ", *e, endl;
  }

  mm_event_buffer_tail++;
  if unlikely (mm_event_buffer_tail >= mm_event_buffer_end) ptl_mm_flush_logging();
}

//
// Which pages are mapped to slab cache pages?
//
// We need to know this to correctly free objects
// allocated at arbitrary addresses.
//
// The full 4 GB address space is covered by 1048576
// bits, or 131072 bytes in the bitmap
//
bitvec<PTL_PAGE_POOL_SIZE> page_is_slab_bitmap;

struct AddressSizeSpan {
  void* address;
  Waddr size;
  
  AddressSizeSpan() { }
  
  AddressSizeSpan(void* address, Waddr size) {
    this->address = address;
    this->size = size;
  }
};

//
// Memory manager validation (for debugging only)
//
bool print_validate_errors = 1;
void* object_of_interest = 0;

bool is_inside_ptlsim(const void* pp) {
#ifdef PTLSIM_HYPERVISOR
  Waddr p = Waddr(pp);
  if likely (inrange(p, Waddr(PTLSIM_VIRT_BASE + (4096 * PTLSIM_FIRST_READ_ONLY_PAGE)), Waddr(bootinfo.heap_end)-1)) return true;
  if likely (!p) return true;
  if likely (print_validate_errors) {
    cerr << "Error: pointer ", (void*)p, " is not in PTL space from ", bootinfo.heap_start, "-", bootinfo.heap_end, " (called from ", getcaller(), "); object of interest = ", object_of_interest, endl, flush;
    logfile << "Error: pointer ", (void*)p, " is not in PTL space from ", bootinfo.heap_start, "-", bootinfo.heap_end, " (called from ", getcaller(), "); object of interest = ", object_of_interest, endl, flush;
  }
  return false;
#else
  return true;
#endif
}

bool is_inside_ptlsim_heap(const void* pp) {
#ifdef PTLSIM_HYPERVISOR
  Waddr p = Waddr(pp);
  if likely (inrange(p, Waddr(bootinfo.heap_start), Waddr(bootinfo.heap_end)-1)) return true;
  if likely (!p) return true;
  if likely (print_validate_errors) {
    cerr << "Error: pointer ", (void*)p, " is not in PTL heap from ", bootinfo.heap_start, "-", bootinfo.heap_end, " (called from ", getcaller(), "); object of interest = ", object_of_interest, endl, flush;
    logfile << "Error: pointer ", (void*)p, " is not in PTL heap from ", bootinfo.heap_start, "-", bootinfo.heap_end, " (called from ", getcaller(), "); object of interest = ", object_of_interest, endl, flush;
  }
  return false;
#else
  return true;
#endif
}

//
// Extent Allocator, suitable for use as a memory allocator
// at both the page level and sub-block level.
//
// This allocator requires no overhead on allocated blocks,
// however, when freeing blocks, the number of bytes to free
// must be explicitly specified.
//
// Blocks can be freed from the middle of an allocated
// region if desired, but the range of the free must not
// overlap an area that is already free.
//
// The parameters CHUNKSIZE, SIZESLOTS and HASHSLOTS must
// be a power of two. 
//
// Allocations smaller than one chunk are rounded up to a
// chunk, i.e. CHUNKSIZE is the minimum granularity. The
// chunk size must be at least 32 bytes on 32-bit platforms
// and 64 bytes on 64-bit platforms, to fit the tracking
// structures in the smallest empty block size.
//
// To initialize the allocator, use free() to add a large
// initial pool of storage to it.
//
template <int CHUNKSIZE, int SIZESLOTS, int HASHSLOTS>
struct ExtentAllocator {
  struct FreeExtentBase {
    selflistlink sizelink;
    selflistlink startaddrlink;
    selflistlink endaddrlink;
  };

  struct FreeExtent: public FreeExtentBase {
    Waddr size;
    byte padding[CHUNKSIZE - (sizeof(FreeExtentBase) + sizeof(Waddr))];
    static FreeExtent* sizelink_to_self(selflistlink* link) { return (link) ? (FreeExtent*)(link - 0) : null; } 
    static FreeExtent* startaddrlink_to_self(selflistlink* link) { return (link) ? (FreeExtent*)(link - 1) : null; }
    static FreeExtent* endaddrlink_to_self(selflistlink* link) { return (link) ? (FreeExtent*)(link - 2) : null; }

    ostream& print(ostream& os) {
      return os << this, ": size ", intstring(size, 7), " = ", intstring(size * CHUNKSIZE, 10), 
        " bytes (range ", (void*)this, " to ", (void*)(this + size), "); sizelink ", FreeExtentBase::sizelink, ", startaddrlink ", FreeExtentBase::startaddrlink, 
        ", endaddrlink ", FreeExtentBase::endaddrlink;
    }
  };

  int extent_size_to_slot(size_t size) const {
    return min(size-1, (size_t)(SIZESLOTS-1));
  }

  int addr_to_hash_slot(void* addr) const {
    W64 key = (Waddr(addr) >> log2(CHUNKSIZE));
    return foldbits<log2(HASHSLOTS)>(key);
  }

  selflistlink* free_extents_by_size[SIZESLOTS];
  selflistlink* free_extents_by_startaddr_hash[HASHSLOTS];
  selflistlink* free_extents_by_endaddr_hash[HASHSLOTS];
  int extent_count;

  W64 current_bytes_free;
  W64 current_bytes_allocated;
  W64 peak_bytes_allocated;
  W64 allocs;
  W64 frees;
  W64 extents_reclaimed;
  W64 extent_reclaim_reqs;
  W64 chunks_allocated;

  void reset() {
    extent_count = 0;

    foreach (i, SIZESLOTS) {
      free_extents_by_size[i] = null;
    }

    foreach (i, HASHSLOTS) {
      free_extents_by_startaddr_hash[i] = null;
    }

    foreach (i, HASHSLOTS) {
      free_extents_by_endaddr_hash[i] = null;
    }

    current_bytes_free = 0;
    current_bytes_allocated = 0;
    peak_bytes_allocated = 0;
    allocs = 0;
    frees = 0;
    extents_reclaimed = 0;
    extent_reclaim_reqs = 0;
    chunks_allocated = 0;
  }

  FreeExtent* find_extent_by_startaddr(void* addr) const {
    int slot = addr_to_hash_slot(addr);
    FreeExtent* r = FreeExtent::startaddrlink_to_self(free_extents_by_startaddr_hash[slot]);

    while (r) {
      if unlikely (r == (FreeExtent*)addr) return r;
      r = FreeExtent::startaddrlink_to_self(r->startaddrlink.next);
    }

    return null;
  }

  FreeExtent* find_extent_by_endaddr(void* addr) const {
    int slot = addr_to_hash_slot(addr);
    FreeExtent* r = FreeExtent::endaddrlink_to_self(free_extents_by_endaddr_hash[slot]);

    while (r) {
      if unlikely ((r + r->size) == (FreeExtent*)addr) return r;
      r = FreeExtent::endaddrlink_to_self(r->endaddrlink.next);
    }

    return null;
  }

  void alloc_extent(FreeExtent* r) {
    bool DEBUG = 0;

    if (DEBUG) logfile << "alloc_extent(", r, "): sizelink ", r->sizelink, ", startaddrlink ", r->startaddrlink, ", endaddrlink ", r->endaddrlink, ", extent_count ", extent_count, endl;

    current_bytes_free -= (r->size * CHUNKSIZE);
    r->sizelink.unlink();
    r->startaddrlink.unlink();
    r->endaddrlink.unlink();
    extent_count--;
    assert(extent_count >= 0);
  }

  FreeExtent* find_extent_in_size_slot(size_t size, int sizeslot) {
    bool DEBUG = 0;

    FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[sizeslot]);

    if (r) {
      if (DEBUG) cerr << "find_extent_in_size_slot(size ", size, ", slot ", sizeslot, "): r = ", r, endl;
    }

    while (r) {
      if likely (r->size < size) {
        if (DEBUG) cerr << "  ", r, " too small: only ", r->size, " chunks", endl;
        r = FreeExtent::sizelink_to_self(r->sizelink.next);
        continue;
      }

      alloc_extent(r);

      if likely (size == r->size) {
        if (DEBUG) cerr << "  Exact match: ", r, endl;
        return r;
      }
      
      int remaining_size = r->size - size;
      FreeExtent* rsplit = r + size;
      if (DEBUG) cerr << "rsplit = ", rsplit, ", size ", size, ", r->size = ", r->size, ", remaining_size = ", remaining_size, endl, flush;

      free_extent(rsplit, remaining_size);
      
      return r;
    }

    return null;
  }

  FreeExtent* find_free_extent_of_size(size_t size) {
    if unlikely (!size) return null;

    for (int i = extent_size_to_slot(size); i < SIZESLOTS; i++) {
      FreeExtent* r = find_extent_in_size_slot(size, i);
      if unlikely (r) return r;
    }

    return null;
  }

  void free_extent(FreeExtent* r, size_t size) {
    static const bool DEBUG = 0;

    if unlikely ((!r) | (!size)) return;
    //
    // <F1> AAA [now-F] AA <F2>
    //
    // Need to quickly find F1 and F2
    //
    if (DEBUG) {
      cout << "free_extent(", r, ", ", size, "): from ", r, " to ", (r + size), endl, flush;
      cout << "  Add to size slot ", extent_size_to_slot(size), " @ ", &free_extents_by_size[extent_size_to_slot(size)], endl, flush;
      cout << "  Add to startaddr slot ", addr_to_hash_slot(r), " @ ", &free_extents_by_startaddr_hash[addr_to_hash_slot(r)], endl, flush;
      cout << "  Add to endaddr slot ", addr_to_hash_slot(r + r->size), " @ ", &free_extents_by_endaddr_hash[addr_to_hash_slot(r + r->size)], endl, flush;
    }

    r->sizelink.reset();
    r->startaddrlink.reset();
    r->size = 0;
    current_bytes_free += (size * CHUNKSIZE);

    FreeExtent* right = find_extent_by_startaddr(r + size);

    //
    // Try to merge with right extent if possible
    //

    if unlikely (right) {
      right->sizelink.unlink();
      right->startaddrlink.unlink();
      right->endaddrlink.unlink();
      size = size + right->size;
      if (DEBUG) cout << "  Merge with right extent ", right, " of size ", right->size, " to form new extent of total size ", size, endl;
      extent_count--;
      assert(extent_count >= 0);
    }

    FreeExtent* left = find_extent_by_endaddr(r);

    if likely (left) {
      left->sizelink.unlink();
      left->startaddrlink.unlink();
      left->endaddrlink.unlink();
      size = size + left->size;
      r -= left->size;
      if (DEBUG) cout << "  Merge with left extent ", left, " of size ", left->size, " to form new extent of total size ", size, endl;
      extent_count--;
      assert(extent_count >= 0);
    }

    r->size = size;
    r->sizelink.addto(free_extents_by_size[extent_size_to_slot(size)]);
    r->startaddrlink.addto(free_extents_by_startaddr_hash[addr_to_hash_slot(r)]);
    r->endaddrlink.addto(free_extents_by_endaddr_hash[addr_to_hash_slot(r + r->size)]);

    extent_count++;
    assert(extent_count > 0);
  }

  void* alloc(size_t size) {
    bool DEBUG = 0;
    if unlikely (enable_mm_validate) fast_validate();
    if (DEBUG) cerr << "ExtentAllocator<", CHUNKSIZE, ">::alloc(", size, ")", endl, flush;

    size = ceil(size, CHUNKSIZE) >> log2(CHUNKSIZE);
    void* addr = (void*)find_free_extent_of_size(size);
    if (DEBUG) cerr << "ExtentAllocator<", CHUNKSIZE, ">::alloc(", size, "): found free extent ", addr, endl, flush;

    if unlikely (!addr) return null;
    allocs++;
    current_bytes_allocated += (size * CHUNKSIZE);
    peak_bytes_allocated = max(peak_bytes_allocated, current_bytes_allocated);

    if unlikely (enable_mm_validate) fast_validate();

    return addr;
  }

  void free(void* p, size_t size) {
    if unlikely (enable_mm_validate) fast_validate();

    size = ceil(size, CHUNKSIZE) >> log2(CHUNKSIZE);
    free_extent((FreeExtent*)p, size);
    frees++;
    current_bytes_allocated = min(current_bytes_allocated - (size * CHUNKSIZE), 0ULL);

    if unlikely (enable_mm_validate) fast_validate();
  }

  void add_to_free_pool(void* p, size_t size) {
    chunks_allocated++;
    size = ceil(size, CHUNKSIZE) >> log2(CHUNKSIZE);
    free_extent((FreeExtent*)p, size);
  }

  int reclaim_unused_extents(AddressSizeSpan* ass, int asscount, int sizealign) {
    static const int DEBUG = 0;

    int minchunks = ceil(sizealign, CHUNKSIZE) >> log2(CHUNKSIZE);

    int n = 0;

    extent_reclaim_reqs++;

    for (int i = extent_size_to_slot(minchunks); i < SIZESLOTS; i++) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);

      while (r) {
        //
        // Example:
        //
        // ..ffffff ffffffff fff.....
        //   aaaaaa aaaaaaaa aaa.....
        //   ffffff          fff
        //          -return-
        //

        Waddr rstart = (Waddr)r;
        Waddr rend = rstart + (r->size * CHUNKSIZE);

        Waddr first_full_page = ceil(rstart, sizealign);
        Waddr last_full_page = floor(rend, sizealign);
        Waddr bytes_in_middle = last_full_page - first_full_page;

        if (DEBUG) {
          cout << "  Trying to reclaim extent "; r->print(cout); cout << " (", bytes_in_middle, " bytes in middle)", endl;
        }

        if likely (!bytes_in_middle) {
          r = FreeExtent::sizelink_to_self(r->sizelink.next);          
          continue;
        }

        // These are full pages that we can return to the system
        if unlikely (n == asscount) return n;

        Waddr full_page_bytes = last_full_page - first_full_page;
        if (DEBUG) cout << "    Adding reclaimed full page extent at ", (void*)first_full_page, " of ", full_page_bytes, " bytes (", full_page_bytes / sizealign, " pages)", endl;
        ass[n++] = AddressSizeSpan((void*)first_full_page, full_page_bytes);

        Waddr bytes_at_end_of_first_page = ceil(rstart, sizealign) - rstart; 
        Waddr bytes_at_start_of_last_page = rend - floor(rend, sizealign);

        alloc_extent(r);
        extents_reclaimed++;

        if unlikely (bytes_at_end_of_first_page) {
          if (DEBUG) cout << "    Adding ", bytes_at_end_of_first_page, " bytes at end of first page @ ", r, " back to free pool", endl;
          free(r, bytes_at_end_of_first_page);
        }

        if unlikely (bytes_at_start_of_last_page) {
          void* p = (void*)(rend - bytes_at_start_of_last_page);
          if (DEBUG) cout << "    Adding ", bytes_at_start_of_last_page, " bytes at start of last page @ ", p, " back to free pool", endl;
          free(p, bytes_at_start_of_last_page);
        }

        //
        // Start again since we may have invalidated the next entry,
        // or moved some big entries in the current list back into
        // one of the smaller lists (smaller than the page size)
        //
        r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);
      }
    }

    return n;
  }

  bool fast_validate() {
    int n = 0;

    foreach (i, SIZESLOTS) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);

      if (!r) continue;

      assert(is_inside_ptlsim(r));

      while (r) {
        object_of_interest = r;
        assert(is_inside_ptlsim(r));
        assert(is_inside_ptlsim(r->sizelink.prev));
        assert(is_inside_ptlsim_heap(r->sizelink.next));
        assert(is_inside_ptlsim(r->startaddrlink.prev));
        assert(is_inside_ptlsim_heap(r->startaddrlink.next));
        assert(is_inside_ptlsim(r->endaddrlink.prev));
        assert(is_inside_ptlsim_heap(r->endaddrlink.next));

        r = FreeExtent::sizelink_to_self(r->sizelink.next);
      }
    }

    foreach (i, HASHSLOTS) {
      selflistlink* link;

      link = free_extents_by_startaddr_hash[i];
      while (link) {
        assert(is_inside_ptlsim(link));
        assert(is_inside_ptlsim(link->prev));
        assert(is_inside_ptlsim_heap(link->next));
        link = link->next;
      }

      link = free_extents_by_endaddr_hash[i];
      while (link) {
        assert(is_inside_ptlsim(link));
        assert(is_inside_ptlsim(link->prev));
        assert(is_inside_ptlsim_heap(link->next));
        link = link->next;
      }
    }

    return true;
  }

  bool full_validate() {
    // Collect all regions
    FreeExtent** extarray = new FreeExtent*[extent_count];

    int n = 0;

    foreach (i, SIZESLOTS) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);
      if (!r) continue;

      assert(is_inside_ptlsim_heap(r));

      while (r) {
        if (n >= extent_count) {
          cerr << "ERROR (chunksize ", CHUNKSIZE, "): ", n, " exceeds extent count ", extent_count, endl, flush;
          cerr << *this;
          return false;
        }
        extarray[n++] = r;
        r = FreeExtent::sizelink_to_self(r->sizelink.next);
      }
    }

    foreach (i, extent_count) {
      FreeExtent* r = extarray[i];
      Waddr start = (Waddr)r;
      Waddr end = start + (r->size * CHUNKSIZE);
      foreach (j, extent_count) {
        if (j == i) continue;
        FreeExtent* rr = extarray[j];
        Waddr rrstart = (Waddr)rr;
        Waddr rrend = rrstart + (rr->size * CHUNKSIZE);

        // ........rrrrrrrrrrr............
        // .....ssssssss..................

        if (inrange(start, rrstart, rrend-1) | inrange(end, rrstart, rrend-1)) {
          cerr << "ERROR (chunksize ", CHUNKSIZE, "): overlap between extent ", r, " (", (r->size * CHUNKSIZE), " bytes; ",  (void*)start, " to ", (void*)end, ") "
            "and extent ", rr, " (", (rr->size * CHUNKSIZE), " bytes; ", (void*)rrstart, " to ", (void*)rrend, ")", endl, flush;
          cerr << *this;
          return false;
        }
      }
    }

    delete[] extarray;

    return true;
  }

  size_t largest_free_extent_bytes() const {
    for (int i = SIZESLOTS-1; i >= 0; i--) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);
      if likely (!r) continue;
      return ((i+1) * CHUNKSIZE);
    }

    return 0;
  }

  size_t total_free_bytes() const {
    W64 real_free_bytes = 0;

    foreach (i, SIZESLOTS) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);
      if likely (!r) continue;

      while (r) {
        real_free_bytes += r->size * CHUNKSIZE;
        r = FreeExtent::sizelink_to_self(r->sizelink.next);
      }
    }

    if (real_free_bytes != current_bytes_free) {
      logfile << "WARNING: real_free_bytes = ", real_free_bytes, " but current_bytes_free ", current_bytes_free, endl;
    }

    return current_bytes_free;
  }

  ostream& print(ostream& os) const {
    os << "ExtentAllocator<", CHUNKSIZE, ", ", SIZESLOTS, ", ", HASHSLOTS, ">: ", extent_count, " extents:", endl;

    os << "Extents by size:", endl;
    W64 total_bytes = 0;
    foreach (i, SIZESLOTS) {
      FreeExtent* r = FreeExtent::sizelink_to_self(free_extents_by_size[i]);
      if (!r) continue;
      os << "  Size slot ", intstring(i, 7), " = ", intstring((i+1) * CHUNKSIZE, 10), " bytes (root @ ", &free_extents_by_size[i], "):", endl;
      W64 min_expected_bytes = ((i+1) * CHUNKSIZE);
      while (r) {
        os << "    ";
        r->print(os);
        os << endl;

        W64 bytes = (r->size * CHUNKSIZE);
        assert(bytes >= min_expected_bytes);
        total_bytes += bytes;

        r = FreeExtent::sizelink_to_self(r->sizelink.next);
      }
    }
    os << "  Total free bytes: ", total_bytes, endl;
    os << endl;

    os << "Extents by startaddr hash:", endl;
    foreach (i, HASHSLOTS) {
      FreeExtent* r = FreeExtent::startaddrlink_to_self(free_extents_by_startaddr_hash[i]);
      if (!r) continue;
      os << "  Hash slot ", intstring(i, 7), ":", endl;
      while (r) {
        os << "    ";
        os << r->print(os);
        os << endl;
        r = FreeExtent::startaddrlink_to_self(r->startaddrlink.next);
      }
    }
    os << endl;

    os << "Extents by endaddr hash:", endl;
    foreach (i, HASHSLOTS) {
      FreeExtent* r = FreeExtent::endaddrlink_to_self(free_extents_by_endaddr_hash[i]);
      if (!r) continue;
      os << "  Hash slot ", intstring(i, 7), ":", endl;
      while (r) {
        os << "    ";
        os << r->print(os);
        os << endl;
        r = FreeExtent::endaddrlink_to_self(r->endaddrlink.next);
      }
    }
    os << endl;

    return os;
  }

  DataStoreNode& capture_stats(DataStoreNode& root) {
    root.add("current-bytes-allocated", current_bytes_allocated);
    root.add("peak-bytes-allocated", peak_bytes_allocated);
    root.add("allocs", allocs);
    root.add("frees", frees);
    root.add("extents-reclaimed", extents_reclaimed);
    root.add("extent-reclaim-reqs", extent_reclaim_reqs);
    root.add("chunks-allocated", chunks_allocated);
    return root;
  }
};

template <int CHUNKSIZE, int SIZESLOTS, int HASHSLOTS>
ostream& operator <<(ostream& os, const ExtentAllocator<CHUNKSIZE, SIZESLOTS, HASHSLOTS>& alloc) {
  return alloc.print(os);
}

//
// Slab cache allocator
//
// Minimum object size is 16 bytes (for 256 objects per page)
//

struct SlabAllocator;
ostream& operator <<(ostream& os, const SlabAllocator& slaballoc);

//
// In PTLxen, the hypervisor is running on the bare hardware
// and must handle all page allocation itself.
//
// In userspace PTLsim, we use pagealloc only for its
// statistics counters.
//
ExtentAllocator<4096, 512, 512> pagealloc;

static const W32 SLAB_PAGE_MAGIC = 0x42414c53; // 'SLAB'

struct SlabAllocator {
#ifdef __x86_64__
  // 64-bit x86-64: 8 + 8 = 16 bytes
  struct FreeObjectHeader: public selflistlink { };
#else
  // 32-bit x86: 4 + 4 + 8 = 16 bytes
  struct FreeObjectHeader: public selflistlink { W64 pad; };
#endif

  static const int GRANULARITY = sizeof(FreeObjectHeader);

  // 32 bytes on both x86-64 and 32-bit x86
  struct PageHeader {
    selflistlink link;
    FreeObjectHeader* freelist;
    SlabAllocator* allocator;
    W32s freecount;
    W32 magic;

    FreeObjectHeader* getbase() const { return (FreeObjectHeader*)floor(Waddr(this), PAGE_SIZE); }

    static PageHeader* headerof(void* p) {
      PageHeader* h = (PageHeader*)(floor(Waddr(p), PAGE_SIZE) + PAGE_SIZE - sizeof(PageHeader));
      return h;
    }

    bool validate() {
      int n = 0;
      selflistlink* link = freelist;
      int objsize = allocator->objsize;
      while (link) {
        assert(inrange(Waddr(link), floor(Waddr(this), 4096), floor(Waddr(this), 4096) + 4095));
        assert(((Waddr(link) - floor(Waddr(this), 4096)) % objsize) == 0);
        assert(is_inside_ptlsim_heap(link));
        assert(is_inside_ptlsim_heap(link->prev));
        assert(is_inside_ptlsim_heap(link->next));
        n++;
        link = link->next;
      }

      assert(n == freecount); 
      return true;
    }
  };

  W64 current_objs_allocated;
  W64 peak_objs_allocated;

  W64 current_bytes_allocated;
  W64 peak_bytes_allocated;

  W64 current_pages_allocated;
  W64 peak_pages_allocated;

  W64 allocs;
  W64 frees;
  W64 page_allocs;
  W64 page_frees;
  W64 reclaim_reqs;

  PageHeader* free_pages;
  PageHeader* partial_pages;
  PageHeader* full_pages;
  W16s free_page_count;
  W16 objsize;
  W16 max_objects_per_page;
  W16 padding;
  
  static const int FREE_PAGE_HI_THRESH = 4;
  static const int FREE_PAGE_LO_THRESH = 1;

  SlabAllocator() { }

  void reset(int objsize) {
    free_pages = null;
    partial_pages = null;
    full_pages = null;
    free_page_count = 0;
    this->objsize = objsize;
    max_objects_per_page = ((PAGE_SIZE - sizeof(PageHeader)) / objsize);
    current_objs_allocated = 0;
    peak_objs_allocated = 0;
    current_bytes_allocated = 0;
    peak_bytes_allocated = 0;
    current_pages_allocated = 0;
    peak_pages_allocated = 0;
    allocs = 0;
    frees = 0;
    page_allocs = 0;
    page_frees = 0;
    reclaim_reqs = 0;
  }

  static SlabAllocator* pointer_to_slaballoc(void* p) {
    Waddr pfn = (((Waddr)p) - PTL_PAGE_POOL_BASE) >> 12;
    if (pfn >= PTL_PAGE_POOL_SIZE) return null; // must be some other kind of page

    if (!page_is_slab_bitmap[pfn]) return null;

    PageHeader* page = PageHeader::headerof(p);

    if unlikely (page->magic != SLAB_PAGE_MAGIC) {
      cerr << "Slab corruption: p = ", p, ", h = ", page, ", allocator ", page->allocator, ", magic = 0x", hexstring(page->magic, 32), ", caller ", getcaller(), endl, flush;
      assert(false);
    }

    return page->allocator;
  }

  FreeObjectHeader* alloc_from_page(PageHeader* page) {  
    FreeObjectHeader* obj = page->freelist;
    if unlikely (!obj) return obj;

    obj->unlink();
    assert(page->freecount > 0);
    page->freecount--;
    
    if unlikely (!page->freelist) {
      assert(page->freecount == 0);
      page->link.unlink();
      page->link.addto((selflistlink*&)full_pages);
    }

    allocs++;

    current_objs_allocated++;
    peak_objs_allocated = max(current_objs_allocated, peak_objs_allocated);

    current_bytes_allocated += objsize;
    peak_bytes_allocated = max(current_bytes_allocated, peak_bytes_allocated);

    return obj;
  }

  PageHeader* alloc_new_page() {
    //
    // We need the pages in the low 2 GB of the address space so we can use
    // page_is_slab_bitmap to find out if it's a slab or genalloc page:
    //
    byte* rawpage = (byte*)ptl_mm_alloc_private_32bit_page();
    if unlikely (!rawpage) return null;

    PageHeader* page = PageHeader::headerof(rawpage);

    page_allocs++;
    current_pages_allocated++;
    peak_pages_allocated = max(current_pages_allocated, peak_pages_allocated);

    page->magic = SLAB_PAGE_MAGIC;
    page->link.reset();
    page->freelist = null;
    page->freecount = 0;
    page->allocator = this;

    Waddr pfn = (((Waddr)page) - PTL_PAGE_POOL_BASE) >> 12;
    assert(pfn < PTL_PAGE_POOL_SIZE);
    page_is_slab_bitmap[pfn] = 1;

    FreeObjectHeader* obj = page->getbase();
    FreeObjectHeader* prevobj = (FreeObjectHeader*)&page->freelist;

    foreach (i, max_objects_per_page) {
      prevobj->next = obj;
      obj->prev = prevobj;
      obj->next = null;
      prevobj = obj;
      obj = (FreeObjectHeader*)(((byte*)obj) + objsize);
    }

    page->freecount = max_objects_per_page;

    return page;
  }

  void* alloc() {
    if unlikely (enable_mm_validate) validate();

    PageHeader* page = (partial_pages) ? partial_pages : free_pages;

    if unlikely (!page) {
      page = alloc_new_page();
      assert(page);
      page->link.addto((selflistlink*&)partial_pages);
    }

    if likely (page == free_pages) {
      page->link.unlink();
      page->link.addto((selflistlink*&)partial_pages);
      free_page_count--;
      assert(free_page_count >= 0);
    }

    FreeObjectHeader* obj = alloc_from_page(page);
    assert(obj);

    return (void*)obj;
  }

  void free(void* p) {
    if unlikely (enable_mm_validate) validate();

    frees++;
    if likely (current_objs_allocated > 0) current_objs_allocated--;
    current_bytes_allocated -= min((W64)objsize, current_bytes_allocated);

    FreeObjectHeader* obj = (FreeObjectHeader*)p;
    PageHeader* page = PageHeader::headerof(p);

    // assert(((Waddr(p) - Waddr(page->getbase())) % objsize) == 0);

    bool page_was_previously_full = (page->freecount == 0);

    obj->reset();
    obj->addto((selflistlink*&)page->freelist);

    assert(page->freecount <= max_objects_per_page);
    page->freecount++;

    if unlikely (page->freecount == max_objects_per_page) {
      page->link.unlink();
      page->link.addto((selflistlink*&)free_pages);
      free_page_count++;
    } else if unlikely (page_was_previously_full) {
      page->link.unlink();
      page->link.addto((selflistlink*&)partial_pages);
    }

    if (free_page_count >= FREE_PAGE_HI_THRESH) {
      reclaim(FREE_PAGE_LO_THRESH);
    }

    if unlikely (enable_mm_validate) validate();
  }

  int reclaim(int limit = 0) {
    // Return some of the pages to the main allocator all at once
    int n = 0;
    reclaim_reqs++;
    while (free_page_count > limit) {
      PageHeader* page = free_pages;
      if (!page) break;
      assert(page->freecount == max_objects_per_page);
      page->link.unlink();

      Waddr pfn = (Waddr(page->getbase()) - PTL_PAGE_POOL_BASE) >> 12;
      assert(pfn < PTL_PAGE_POOL_SIZE);
      page_is_slab_bitmap[pfn] = 0;

      page->magic = 0;
      ptl_mm_free_private_page((void*)page->getbase());
      page_frees++;
      if likely (current_pages_allocated > 0) current_pages_allocated--;
      n++;
      free_page_count--;
    }
    return n;
  }

  ostream& print_page_chain(ostream& os, PageHeader* page) const {
    while (page) {
      bitvec<4096/16> occupied_slots;
      occupied_slots++;

      FreeObjectHeader* pagebase = page->getbase();

      os << "  Page ", page, ": free list size ", page->freecount, ", prev ", page->link.prev, ", next ", page->link.next, ":", endl;
      FreeObjectHeader* obj = page->freelist;
      int c = 0;
      while (obj) {
        int slot = (Waddr(obj) - Waddr(pagebase)) / objsize;
        os << "    Free object ", obj, " in slot ", slot, " (prev ", obj->prev, ", next ", obj->next, ")", endl;
        assert(inrange(slot, 0, int(max_objects_per_page)));
        occupied_slots[slot] = 0;
        obj = (FreeObjectHeader*)obj->next;
        c++;
      }
      if (c != page->freecount) {
        os << "    WARNING: c = ", c, " vs page->freecount ", page->freecount, endl, flush;
      }

      foreach (i, max_objects_per_page) {
        if (!occupied_slots[i]) continue;
        byte* p = (byte*)pagebase + (i *objsize);
        if (occupied_slots[i]) os << "    Used object ", p, " in slot ", i, endl;
      }
      page = (PageHeader*)page->link.next;
    }
    return os;
  }

  ostream& print(ostream& os) const {
    os << "SlabAllocator<", objsize, "-byte objects = ", max_objects_per_page, " objs per page>:", endl;

    os << "Free Pages (", free_page_count, " pages):", endl;
    print_page_chain(os, free_pages);

    os << "Partial Pages:", endl;
    print_page_chain(os, partial_pages);

    os << "Full Pages:", endl;
    print_page_chain(os, full_pages);

    return os;
  }

  bool validate() {
    selflistlink* link;

    link = (free_pages) ? &(free_pages->link) : null;
    while (link) {
      assert(is_inside_ptlsim(link));
      assert(is_inside_ptlsim(link->prev));
      assert(is_inside_ptlsim_heap(link->next));
      PageHeader* h = (PageHeader*)link;
      assert(h->validate());
      assert(h->freecount == max_objects_per_page);
      link = link->next;
    }

    link = (partial_pages) ? &(partial_pages->link) : null;
    while (link) {
      assert(is_inside_ptlsim(link));
      assert(is_inside_ptlsim(link->prev));
      assert(is_inside_ptlsim_heap(link->next));
      PageHeader* h = (PageHeader*)link;
      assert(h->validate());
      assert(inrange(h->freecount, 1, max_objects_per_page-1));
      link = link->next;
    }

    link = (full_pages) ? &(full_pages->link) : null;
    while (link) {
      assert(is_inside_ptlsim(link));
      assert(is_inside_ptlsim(link->prev));
      assert(is_inside_ptlsim_heap(link->next));
      PageHeader* h = (PageHeader*)link;
      assert(h->validate());
      assert(h->freecount == 0);
      link = link->next;
    }

    return true;
  }

  size_t total_free_bytes() const {
    PageHeader* page;
    W64 bytes = 0;

    page = free_pages;
    while (page) {
      bytes += page->freecount * objsize;
      page = (PageHeader*)page->link.next;
    }

    page = partial_pages;
    while (page) {
      bytes += page->freecount * objsize;
      page = (PageHeader*)page->link.next;
    }

    page = full_pages;
    while (page) {
      if (page->freecount != 0) {
        logfile << "ERROR: supposedly full page ", page->getbase(), " in slab ", this, " for objsize ", objsize, " had free count ", page->freecount, " instead of zero as expected", endl;
        logfile << *this;
        assert(page->freecount == 0);
      }
      bytes += page->freecount * objsize;
      page = (PageHeader*)page->link.next;
    }

    return bytes;
  }

  DataStoreNode& capture_stats(DataStoreNode& root) {
    root.add("current-objs-allocated", current_objs_allocated);
    root.add("peak-objs-allocated", peak_objs_allocated);

    root.add("current-bytes-allocated", current_bytes_allocated);
    root.add("peak-bytes-allocated", peak_bytes_allocated);

    root.add("current-pages-allocated", current_pages_allocated);
    root.add("peak-pages-allocated", peak_pages_allocated);

    root.add("allocs", allocs);
    root.add("frees", frees);
    root.add("page-allocs", page_allocs);
    root.add("page-frees", page_frees);
    root.add("reclaim-reqs", reclaim_reqs);
    return root;
  }
};

ostream& operator <<(ostream& os, const SlabAllocator& slaballoc) {
  return slaballoc.print(os);
}

//
// Memory Management
//

static const int GEN_ALLOC_GRANULARITY = 64;

ExtentAllocator<GEN_ALLOC_GRANULARITY, 4096, 2048> genalloc;

// Objects larger than this will be allocated from the general purpose allocator
static const int SLAB_ALLOC_LARGE_OBJ_THRESH = 1024;
static const int SLAB_ALLOC_SLOT_COUNT = (SLAB_ALLOC_LARGE_OBJ_THRESH / SlabAllocator::GRANULARITY);

SlabAllocator slaballoc[SLAB_ALLOC_SLOT_COUNT];

W64 ptl_mm_dump_free_bytes(ostream& os) {
  W64 slaballoc_free_bytes = 0;
  foreach (i, SLAB_ALLOC_SLOT_COUNT) {
    slaballoc_free_bytes += slaballoc[i].total_free_bytes();
  }

  W64 genalloc_free_bytes = genalloc.total_free_bytes();
  W64 pagealloc_free_bytes = pagealloc.total_free_bytes();

  os << "Free bytes:", endl;
  os << "  Page allocator:     ", intstring(pagealloc_free_bytes, 10), endl;
  os << "  Secondary allocators:", endl;
  os << "    Slab allocator:   ", intstring(slaballoc_free_bytes, 10), endl;
  os << "    General allocator:", intstring(genalloc_free_bytes, 10), endl;
  os << "    Total:            ", intstring(slaballoc_free_bytes + genalloc_free_bytes, 10), endl;

  return slaballoc_free_bytes + genalloc_free_bytes;
}

void ptl_mm_dump(ostream& os) {
  ptl_mm_dump_free_bytes(os);

#ifdef PTLSIM_HYPERVISOR
  os << "Page allocator:", endl;
  pagealloc.print(os);
#endif

  os << "General allocator:", endl;
  genalloc.print(os);

  foreach (i, SLAB_ALLOC_SLOT_COUNT) {
    os << "Slab Allocator ", i, " (", slaballoc[i].total_free_bytes(), " bytes free):", endl;
    slaballoc[i].print(os);
  }

  os << "End of memory dump", endl;
  os << flush;
}

#ifdef PTLSIM_HYPERVISOR

extern ostream logfile;

//
// Full-system PTLsim running on the bare hardware:
//
void* ptl_mm_try_alloc_private_pages(Waddr bytecount, int prot, Waddr base, void* caller) {
  void* p = pagealloc.alloc(bytecount);
  ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_PAGES, caller, p, bytecount);
  return p;
}

void* ptl_mm_alloc_private_pages(Waddr bytecount, int prot, Waddr base) { 
  static const int retry_count = 64;

  foreach (i, retry_count) {
    void* p = ptl_mm_try_alloc_private_pages(bytecount, prot, base, getcaller());
    if likely (p) return p;
    logfile << "Before reclaim round ", i, ": largest free physical extent: ", pagealloc.largest_free_extent_bytes(), " bytes vs ", bytecount, " required bytes", endl;
    ptl_mm_dump_free_bytes(logfile);
    // The urgency MAX_URGENCY (currently 65536) means "free everything possible at all costs":
    ptl_mm_reclaim(bytecount, ((i == (retry_count-2)) ? MAX_URGENCY : i));
    logfile << "After reclaim round ", i, ": largest free physical extent: ", pagealloc.largest_free_extent_bytes(), " bytes", endl, flush;
    ptl_mm_dump_free_bytes(logfile);
  }

  cerr << "ptl_mm_alloc_private_pages(", bytecount, " bytes): failed to reclaim some memory (called from ", (void*)getcaller(), ")", endl, flush;
  logfile << "ptl_mm_alloc_private_pages(", bytecount, " bytes): failed to reclaim some memory (called from ", (void*)getcaller(), ")", endl, flush;

  ptl_mm_dump(logfile);

  cerr << flush;
  assert(false);
}

void* ptl_mm_alloc_private_32bit_pages(Waddr bytecount, int prot, Waddr base) {
  return ptl_mm_alloc_private_pages(bytecount, prot, base);
}

void ptl_mm_free_private_pages(void* addr, Waddr bytecount) {
  assert(addr);
  ptl_mm_add_event(PTL_MM_EVENT_FREE, PTL_MM_POOL_PAGES, getcaller(), addr, bytecount);
  pagealloc.free(floorptr(addr, PAGE_SIZE), ceil(bytecount, PAGE_SIZE));
}

void ptl_mm_zero_private_pages(void* addr, Waddr bytecount) {
  memset(addr, 0, bytecount);
}

#else

void* ptl_mm_try_alloc_private_pages(Waddr bytecount, int prot, Waddr base, void* caller) {
  int flags = MAP_ANONYMOUS|MAP_NORESERVE | (base ? MAP_FIXED : 0);
  flags |= (inside_ptlsim) ? MAP_SHARED : MAP_PRIVATE;
  if (base == 0) base = PTL_PAGE_POOL_BASE;
  void* addr = sys_mmap((void*)base, ceil(bytecount, PAGE_SIZE), prot, flags, 0, 0);
  ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_PAGES, caller, addr, bytecount);
  if (addr) {
    pagealloc.allocs++;
    pagealloc.current_bytes_allocated += ceil(bytecount, PAGE_SIZE);
    pagealloc.peak_bytes_allocated = max(pagealloc.peak_bytes_allocated, pagealloc.current_bytes_allocated);
  }

  return addr;
}

void* ptl_mm_alloc_private_pages(Waddr bytecount, int prot, Waddr base) {
  return ptl_mm_try_alloc_private_pages(bytecount, prot, base, getcaller());
}

void* ptl_mm_alloc_private_32bit_pages(Waddr bytecount, int prot, Waddr base) {
#ifdef __x86_64__
  int flags = MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE | (base ? MAP_FIXED : MAP_32BIT);
#else
  int flags = MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE | (base ? MAP_FIXED : 0);
#endif
  void* addr = sys_mmap((void*)base, ceil(bytecount, PAGE_SIZE), prot, flags, 0, 0);  
  ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_PAGES, getcaller(), addr, bytecount);
  if (addr) {
    pagealloc.allocs++;
    pagealloc.current_bytes_allocated += ceil(bytecount, PAGE_SIZE);
    pagealloc.peak_bytes_allocated = max(pagealloc.peak_bytes_allocated, pagealloc.current_bytes_allocated);
  }

  return addr;
}

void ptl_mm_free_private_pages(void* addr, Waddr bytecount) {
  bytecount = ceil(bytecount, PAGE_SIZE);

  pagealloc.frees++;
  pagealloc.current_bytes_allocated -= min(pagealloc.current_bytes_allocated, (W64)bytecount);
  sys_munmap(addr, bytecount);
  ptl_mm_add_event(PTL_MM_EVENT_FREE, PTL_MM_POOL_PAGES, getcaller(), addr, bytecount);
}

void ptl_mm_zero_private_pages(void* addr, Waddr bytecount) {
  sys_madvise((void*)floor((Waddr)addr, PAGE_SIZE), bytecount, MADV_DONTNEED);
}

#endif

void* ptl_mm_alloc_private_page() {
  return ptl_mm_alloc_private_pages(PAGE_SIZE);
}

void* ptl_mm_try_alloc_private_page() {
  return ptl_mm_try_alloc_private_pages(PAGE_SIZE);
}

void* ptl_mm_alloc_private_32bit_page() {
  return ptl_mm_alloc_private_32bit_pages(PAGE_SIZE);
}

void ptl_mm_free_private_page(void* addr) {
  ptl_mm_free_private_pages(addr, PAGE_SIZE);
}

void ptl_mm_zero_private_page(void* addr) {
  ptl_mm_zero_private_pages(addr, PAGE_SIZE);
}

void ptl_mm_init(byte* heap_start, byte* heap_end) {
  page_is_slab_bitmap.reset();

#ifdef PTLSIM_HYPERVISOR
  pagealloc.reset();
  pagealloc.free(heap_start, heap_end - heap_start);
#else
  // No special actions required
#endif
  genalloc.reset();  

  foreach (i, SLAB_ALLOC_SLOT_COUNT) {
    slaballoc[i].reset((i+1) * SlabAllocator::GRANULARITY);
  }

#ifdef ENABLE_MM_LOGGING
  mm_event_buffer_head = mm_event_buffer;
  mm_event_buffer_end = mm_event_buffer_head + mm_event_buffer_size;
  mm_event_buffer_tail = mm_event_buffer_head;

  ptl_mm_add_event(PTL_MM_EVENT_INIT, PTL_MM_POOL_ALL, heap_start, heap_end, (heap_end - heap_start));

  /*
    For a dynamically sized event buffer:

    assert(mm_event_buffer_size > 4096);
    mm_event_buffer_head = ptl_mm_alloc_private_pages_for_objects<MemoryManagerEvent>(MM_EVENT_BUFFER_SIZE);
    mm_event_buffer_end = mm_event_buffer_head + mm_event_buffer_size;
    mm_event_buffer_tail = mm_event_buffer_head;
    
    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_PAGES, null, mm_event_buffer_head, MM_EVENT_BUFFER_SIZE * sizeof(MemoryManagerEvent));
  */
#endif
}

static const int GEN_ALLOC_CHUNK_SIZE = 64*1024; // 64 KB (16 pages)

void* ptl_mm_alloc(size_t bytes, void* caller) {
  // General purpose malloc

  if unlikely (!bytes) return null;

  if likely ((bytes <= SLAB_ALLOC_LARGE_OBJ_THRESH)) {
    //
    // Allocate from slab
    //
    bytes = ceil(bytes, SlabAllocator::GRANULARITY);
    int slot = (bytes >> log2(SlabAllocator::GRANULARITY))-1;
    assert(slot < SLAB_ALLOC_SLOT_COUNT);
    void* p = slaballoc[slot].alloc();
    if unlikely (!p) {
      ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_SLAB, caller, null, bytes, slot);
      ptl_mm_reclaim(bytes);
      p = slaballoc[slot].alloc();
    }
    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_SLAB, caller, p, bytes, slot);
    return p;
  } else {
    //
    // Allocate from general allocation pool
    // Make sure the first byte of the user
    // allocation is 16-byte aligned; otherwise
    // SSE vector ops will get alignment faults.
    //
    bytes = ceil(bytes + 16, 16);

    W64* p = (W64*)genalloc.alloc(bytes);
    if unlikely (!p) {
      ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_GENERAL, caller, null, bytes);

      // Add some storage to the pool
      Waddr pagebytes = max((Waddr)ceil(bytes, PAGE_SIZE), (Waddr)GEN_ALLOC_CHUNK_SIZE);
      //
      // We need the pages in the low 2 GB of the address space so we can use
      // page_is_slab_bitmap to find out if it's a slab or genalloc page. For
      // PTLsim/X, this is not required since we have complete control over the
      // page tables and can put the entire page pool in one 2 GB aligned block.
      //
      int prot = PROT_READ|PROT_WRITE|PROT_EXEC;
      void* newpool = ptl_mm_try_alloc_private_pages(pagebytes, prot, 0, getcaller());
      if unlikely (!newpool) {
        size_t largest_free_extent = pagealloc.largest_free_extent_bytes();
        logfile << "mm: attempted to allocate ", bytes, " bytes: failed allocation of new gen pool chunk (",
          pagebytes, " bytes) failed: largest_free_extent ", largest_free_extent, " vs rounded up orig bytes ",
          ceil(bytes, 4096), endl;

        if likely (largest_free_extent >= ceil(bytes, 4096)) {
          //
          // Try a smaller allocation since default chunk size won't fit
          // If we get here, largest_free_extent must be < pagebytes:
          //
          pagebytes = largest_free_extent;
          newpool = ptl_mm_try_alloc_private_pages(pagebytes, prot, 0, getcaller());
          // Must have some space for at least this amount:
          assert(newpool);
        } else {
          logfile << "mm: reclaim needed since alloc request exceeds largest free extent", endl;
          pagebytes = ceil(bytes, PAGE_SIZE);
          newpool = ptl_mm_alloc_private_pages(pagebytes, prot, 0);
        }
      }

      if unlikely (!newpool) {
#ifdef PTLSIM_HYPERVISOR
        cerr << pagealloc, flush;
#endif
        cerr << genalloc, flush;
        assert(false);
      }
      genalloc.add_to_free_pool(newpool, pagebytes);

      p = (W64*)genalloc.alloc(bytes);
      assert(p);
    }

    *p = bytes;
    p += 2; // skip over hidden size word and pad-to-16-bytes word

    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_GENERAL, caller, p, bytes);
    return p;
  }

  return null;
}

void* ptl_mm_alloc(size_t bytes) {
  return ptl_mm_alloc(bytes, getcaller());
}

//
// Try to allocate some memory, but do not invoke the reclaim
// mechanism unless the memory is immediately available
//

void* ptl_mm_try_alloc(size_t bytes) {
  // General purpose malloc

  if unlikely (!bytes) return null;

  if likely ((bytes <= SLAB_ALLOC_LARGE_OBJ_THRESH)) {
    //
    // Allocate from slab
    //
    bytes = ceil(bytes, SlabAllocator::GRANULARITY);
    int slot = (bytes >> log2(SlabAllocator::GRANULARITY))-1;
    assert(slot < SLAB_ALLOC_SLOT_COUNT);
    void* p = slaballoc[slot].alloc();
    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_SLAB, getcaller(), p, bytes, slot);
    return p;
  } else {
    //
    // Allocate from general allocation pool
    // Make sure the first byte of the user
    // allocation is 16-byte aligned; otherwise
    // SSE vector ops will get alignment faults.
    //
    bytes = ceil(bytes + 16, 16);

    W64* p = (W64*)genalloc.alloc(bytes);
    if unlikely (!p) {
      ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_GENERAL, getcaller(), null, bytes);
      return null;
    }
    *p = bytes;
    p += 2; // skip over hidden size word and pad-to-16-bytes word

    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_GENERAL, getcaller(), p, bytes);
    return p;
  }
}


//
// Allocate a block aligned to a (1 << alignbits) boundary.
//
// This always uses the slab allocator, so only small allocations
// of less than 1024 bytes are allowed. If you need a larger
// aligned extent, use the page allocator.
//
void* ptl_mm_alloc_aligned(int alignbits) {
  Waddr bytes = 1 << alignbits;

  if unlikely (bytes > SLAB_ALLOC_LARGE_OBJ_THRESH) return null;

  //
  // Allocate from slab
  //
  bytes = ceil(bytes, SlabAllocator::GRANULARITY);
  int slot = (bytes >> log2(SlabAllocator::GRANULARITY))-1;
  assert(slot < SLAB_ALLOC_SLOT_COUNT);
  void* p = slaballoc[slot].alloc();
  if unlikely (!p) {
    ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_SLAB, getcaller(), null, bytes, slot);
    ptl_mm_reclaim(bytes);
    p = slaballoc[slot].alloc();
  }

  assert(lowbits(Waddr(p), alignbits) == 0);
  ptl_mm_add_event(PTL_MM_EVENT_ALLOC, PTL_MM_POOL_SLAB, getcaller(), p, bytes, slot);

  return p;
}

size_t ptl_mm_getsize(void* p) {
  SlabAllocator* sa;

  if likely (sa = SlabAllocator::pointer_to_slaballoc(p)) {
    return sa->objsize;
  } else {
    // 16 bytes of padding used to store the size with proper alignment
    W64* pp = ((W64*)p)-2;
    Waddr bytes = *pp;
    return bytes;
  }
}

void ptl_mm_free(void* p, void* caller) {
  SlabAllocator* sa;

  static const bool DEBUG = 0;

  if likely (sa = SlabAllocator::pointer_to_slaballoc(p)) {
    //
    // From slab allocation pool: all objects on a given page are the same size
    //
    ptl_mm_add_event(PTL_MM_EVENT_FREE, PTL_MM_POOL_SLAB, caller, p, sa->objsize, sa - slaballoc);
    sa->free(p);
  } else {
    //
    // Pointer is in the general allocation pool.
    // The word prior to the start of the block specifies
    // the block size.
    //

    W64* pp = ((W64*)p)-2;
    Waddr bytes = *pp;

    ptl_mm_add_event(PTL_MM_EVENT_FREE, PTL_MM_POOL_GENERAL, caller, p, bytes);
    genalloc.free(pp, bytes);
  }
}

void ptl_mm_free(void* p) {
  ptl_mm_free(p, getcaller());
}

void ptl_mm_validate() {
  logfile << "ptl_mm_validate() called by ", getcaller(), endl;
  pagealloc.fast_validate();
  genalloc.fast_validate();
  
  foreach (i, SLAB_ALLOC_SLOT_COUNT) {
    slaballoc[i].validate();
  }
}

//
// When we run out of memory, we let the user register
// reclaim handlers. These get called in registered order
// to trim down various structures (like caches) as much
// as possible before retrying the allocation. The reclaim
// handler must not allocate memory under any circumstances!
//

mm_reclaim_handler_t reclaim_handler_list[64];
int reclaim_handler_list_count = 0;

bool ptl_mm_register_reclaim_handler(mm_reclaim_handler_t handler) {
  if (reclaim_handler_list_count == lengthof(reclaim_handler_list)) {
    logfile << "Too many memory manager reclaim handlers while registering ", handler, endl;
    assert(false);
    return false;
  }

  reclaim_handler_list[reclaim_handler_list_count++] = handler;
  return true;
}

//
// Clean up any deferred frees, for instance for pages with all objects
// free. These pages can be returned to the main pool rather than being
// retained as a slab cache page.
//
void ptl_mm_cleanup() {
  ptl_mm_add_event(PTL_MM_EVENT_CLEANUP, PTL_MM_POOL_ALL, getcaller(), null, 0);

  foreach (i, SLAB_ALLOC_SLOT_COUNT) {
    slaballoc[i].reclaim();
  }

  AddressSizeSpan ass[1024];

  int n;

  while ((n = genalloc.reclaim_unused_extents(ass, lengthof(ass), PAGE_SIZE))) {
    // cout << "Reclaimed ", n, " extents", endl;

    foreach (i, n) {
      ptl_mm_free_private_pages(ass[i].address, ass[i].size);
    }
  }
}

//
// Return unused sub-allocator resources to the main page allocator
// in case of an out-of-memory condition. This may free up some space
// for other types of big allocations.
//
void ptl_mm_reclaim(size_t bytes, int urgency) {
  ptl_mm_add_event(PTL_MM_EVENT_RECLAIM_START, PTL_MM_POOL_ALL, getcaller(), null, bytes);

  foreach (i, reclaim_handler_list_count) {
    mm_reclaim_handler_t handler = reclaim_handler_list[i];
    if likely (handler) {
      ptl_mm_add_event(PTL_MM_EVENT_RECLAIM_CALL, PTL_MM_POOL_ALL, getcaller(), (void*)handler, bytes);
      reclaim_handler_list[i](bytes, urgency);
    }
  }

  ptl_mm_cleanup();

  ptl_mm_add_event(PTL_MM_EVENT_RECLAIM_END, PTL_MM_POOL_ALL, getcaller(), null, bytes);
}

DataStoreNode& ptl_mm_capture_stats(DataStoreNode& root) {
#ifndef PTLSIM_HYPERVISOR
  pagealloc.capture_stats(root("page"));
  genalloc.capture_stats(root("general"));
  DataStoreNode& slab = root("slab"); {
    slab.summable = 1;
    slab.identical_subtrees = 1;
    foreach (i, SLAB_ALLOC_SLOT_COUNT) {
      stringbuf sizestr; sizestr << slaballoc[i].objsize;
      slaballoc[i].capture_stats(slab(sizestr));
    }
  }
#endif
  return root;
}

asmlinkage void* malloc(size_t size) {
  return ptl_mm_alloc(size, getcaller());
}

asmlinkage void free(void* ptr) {
  ptl_mm_free(ptr, getcaller());
}

void* operator new(size_t sz) {
  return ptl_mm_alloc(sz, getcaller());
}

void operator delete(void* m) {
  ptl_mm_free(m, getcaller());
}

void* operator new[](size_t sz) {
  return ptl_mm_alloc(sz, getcaller());
}

void operator delete[](void* m) {
  ptl_mm_free(m, getcaller());
}
