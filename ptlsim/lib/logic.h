// -*- c++ -*-
//
// Sequential Logic Primitives for C++
//
// Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _LOGIC_H_
#define _LOGIC_H_

#include <globals.h>
#include <superstl.h>

inline vec16b x86_sse_ldvbu(const vec16b* m) { vec16b rd; asm("movdqu %[m],%[rd]" : [rd] "=x" (rd) : [m] "xm" (*m)); return rd; }
inline void x86_sse_stvbu(vec16b* m, const vec16b ra) { asm("movdqu %[ra],%[m]" : [m] "=xm" (*m) : [ra] "x" (ra) : "memory"); }
inline vec8w x86_sse_ldvwu(const vec8w* m) {
	vec8w rd;
	asm("movdqu %[m],%[rd]" : [rd] "=x" (rd) : [m] "m" (*m));
	return rd;
}
//inline vec8w x86_sse_ldvwu(const vec8w* m) { vec8w rd; asm("movdqu %[rd], %[m]" : [rd] "=x" (rd) : [m] "xm" (*m)); return rd; }
inline void x86_sse_stvwu(vec8w* m, const vec8w ra) { asm("movdqu %[ra],%[m]" : [m] "=m" (*m) : [ra] "x" (ra) : "memory"); }

extern ofstream ptl_logfile;
extern ofstream yaml_stats_file;

template <typename T>
struct latch {
  T data;
  T newdata;

  latch() {
    reset();
  }

  void reset(const T& d = T()) {
    data = d;
    newdata = d;
  }

  latch(const T& t) { newdata = t; }

  operator T() const { return data; }

  T& operator =(const T& t) {
    newdata = t; return data;
  }

  void clock(bool clkenable = true) {
    if (clkenable)
      data = newdata;
  }
};

template <typename T, int size>
struct SynchronousRegisterFile {
  SynchronousRegisterFile() {
    reset();
  }

  void reset() {
    for (int i = 0; i < size; i++) {
      data[i].data = 0;
      data[i].newdata = 0;
    }
  }

  latch<T> data[size];

  latch<T>& operator [](int i) {
    return data[i];
  }

  void clock(bool clkenable = true) {
    if (!clkenable)
      return;

    for (int i = 0; i < size; i++) {
      data[i].clock();
    }
  }
};

//
// Queue
//

// Iterate forward through queue from head to tail
#define foreach_forward(Q, i) for (int i = (Q).head; i != (Q).tail; i = add_index_modulo(i, +1, (Q).size))

// Iterate forward through queue from the specified entry until the tail
#define foreach_forward_from(Q, E, i) for (int i = E->index(); i != (Q).tail; i = add_index_modulo(i, +1, (Q).size))

// Iterate forward through queue from the entry after the specified entry until the tail
#define foreach_forward_after(Q, E, i) for (int i = add_index_modulo(E->index(), +1, (Q).size); i != (Q).tail; i = add_index_modulo(i, +1, (Q).size))

// Iterate backward through queue from tail to head
#define foreach_backward(Q, i) for (int i = add_index_modulo((Q).tail, -1, (Q).size); i != add_index_modulo((Q).head, -1, (Q).size); i = add_index_modulo(i, -1, (Q).size))

// Iterate backward through queue from the specified entry until the tail
#define foreach_backward_from(Q, E, i) for (int i = E->index(); i != add_index_modulo((Q).head, -1, (Q).size); i = add_index_modulo(i, -1, (Q).size))

// Iterate backward through queue from the entry before the specified entry until the head
#define foreach_backward_before(Q, E, i) for (int i = add_index_modulo(E->index(), -1, (Q).size); ((i != add_index_modulo((Q).head, -1, (Q).size)) && (E->index() != (Q).head)); i = add_index_modulo(i, -1, (Q).size))

template <class T, int SIZE>
struct FixedQueue: public array<T, SIZE> {
  int head; // used for allocation
  int tail; // used for deallocation
  int count; // count of entries

  static const int size = SIZE;

  FixedQueue() {
    reset();
  }

  void flush() {
    head = tail = count = 0;
  }

  void reset() {
    head = tail = count = 0;
  }

  int remaining() const {
    return max((SIZE - count) - 1, 0);
  }

  bool empty() const {
    return (!count);
  }

  bool full() const {
    return (!remaining());
  }

  T* alloc() {
    if (!remaining())
      return NULL;

    T* entry = &(*this)[tail];

    tail = add_index_modulo(tail, +1, SIZE);
    count++;

    return entry;
  }

  T* push() {
    return alloc();
  }

  T* push(const T& data) {
    T* slot = push();
    if (!slot) return NULL;
    *slot = data;
    return slot;
  }

  T* enqueue(const T& data) {
    return push(data);
  }

  void commit(T& entry) {
    assert(entry.index() == head);
    count--;
    head = add_index_modulo(head, +1, SIZE);
  }

  void annul(T& entry) {
    // assert(entry.index() == add_index_modulo(tail, -1, SIZE));
    count--;
    tail = add_index_modulo(tail, -1, SIZE);
  }

  T* pop() {
    if (empty()) return NULL;
    tail = add_index_modulo(tail, -1, SIZE);
    count--;
    return &(*this)[tail];
  }

  T* peek() {
    if (empty())
      return NULL;
    return &(*this)[head];
  }

  T* dequeue() {
    if (empty())
      return NULL;
    count--;
    T* entry = &(*this)[head];
    head = add_index_modulo(head, +1, SIZE);
    return entry;
  }

  void commit(T* entry) { commit(*entry); }
  void annul(T* entry) { annul(*entry); }

  T* pushhead() {
    if (full()) return NULL;
    head = add_index_modulo(head, -1, SIZE);
    count++;
    return &(*this)[head];
  }

  T* pophead() {
    if (empty()) return NULL;
    T* p = &(*this)[head];
    count--;
    head = add_index_modulo(head, +1, SIZE);
    return p;
  }

  T* peekhead() {
    if (empty()) return NULL;
    return &(*this)[head];
  }

  T* peektail() {
    if (empty()) return NULL;
    int t = add_index_modulo(tail, -1, SIZE);
    return &(*this)[t];
  }

  T& operator ()(int index) {
    index = add_index_modulo(head, index, SIZE);
    return (*this)[index];
  }

  const T& operator ()(int index) const {
    index = add_index_modulo(head, index, SIZE);
    return (*this)[index];
  }

  ostream& print(ostream& os) const {
    os << "Queue<", SIZE, ">: head ", head, " to tail ", tail, " (", count, " entries):", endl;
    foreach_forward((*this), i) {
      const T& entry = (*this)[i];
      os << "  slot ", intstring(i, 3), ": ", entry, endl;
    }

    return os;
  }
};

template <class T, int SIZE>
ostream& operator <<(ostream& os, FixedQueue<T, SIZE>& queue) {
  return queue.print(os);
}

template <class T, int SIZE>
struct Queue: public FixedQueue<T, SIZE> {
  typedef FixedQueue<T, SIZE> base_t;

  Queue() {
    reset();
  }

  void reset() {
    base_t::reset();
    foreach (i, SIZE) {
      (*this)[i].init(i);
    }
  }

  T* alloc() {
    T* p = base_t::alloc();
    if likely (p) p->validate();
    return p;
  }
};

template <class T, int size>
ostream& operator <<(ostream& os, const Queue<T, size>& queue) {
  os << "Queue<", size, "]: head ", queue.head, " to tail ", queue.tail, " (", queue.count, " entries):", endl;
  foreach_forward(queue, i) {
    const T& entry = queue[i];
    os << "  ", entry, endl;
  }
  return os;
}

template <typename T, int size>
struct HistoryBuffer: public array<T, size> {
  int current;
  T prevoldest;

  void reset() {
    current = size-1;
    setzero(this->data);
  }

  HistoryBuffer() {
    reset();
  }

  //
  // Enqueue t at the tail of the queue, making the results
  // visible for possible dequeueing by an earlier pipeline
  // stage within the same cycle (i.e., forwarding is used).
  // If this is not desirable, use enqueuesync() instead.
  //
  void add(const T& t) {
    current = add_index_modulo(current, +1, size);
    prevoldest = this->data[current];
    this->data[current] = t;
  }

  /*
   * Undo last addition
   */
  void undo() {
    this->data[current] = prevoldest;
    current = add_index_modulo(current, -1, size);
  }

  /*
   * Index backwards in time: 0 = most recent addition
   */
  T& operator [](int index) {
    int idx = add_index_modulo(current, -index, size);
    //assert(inrange(idx, 0, size-1));
    return this->data[idx];
  }

  const T& operator [](int index) const {
    int idx = add_index_modulo(current, -index, size);
    //assert(inrange(idx, 0, size-1));
    return this->data[idx];
  }
};

template <class T, int size>
ostream& operator <<(ostream& os, HistoryBuffer<T, size>& history) {
  os << "HistoryBuffer[", size, "]: current = ", history.current, ", prevoldest = ", history.prevoldest, endl;
  for (int i = 0; i < size; i++) {
    os << "  ", history[i], endl;
  }
  return os;
}

//
// Fully Associative Arrays
//

template <typename T> struct InvalidTag { static const T INVALID; };
template <> struct InvalidTag<W64> { static const W64 INVALID = 0xffffffffffffffffULL; };
template <> struct InvalidTag<W32> { static const W32 INVALID = 0xffffffff; };
template <> struct InvalidTag<W16> { static const W16 INVALID = 0xffff; };
template <> struct InvalidTag<W8> { static const W8 INVALID = 0xff; };

//
// The replacement policy is pseudo-LRU using a most recently used
// bit vector (mLRU), as described in the paper "Performance Evaluation
// of Cache Replacement Policies for the SPEC CPU2000 Benchmark Suite"
// by Al-Zoubi et al. Essentially we maintain one MRU bit per way and
// set the bit for the way when that way is accessed. The way to evict
// is the first way without its MRU bit set. If all MRU bits become
// set, they are all reset and we start over. Surprisingly, this
// simple method performs as good as, if not better than, true LRU
// or tree-based hot sector LRU.
//

template <typename T, int ways>
struct FullyAssociativeTags {
  bitvec<ways> evictmap;
  T tags[ways];

  static const T INVALID = InvalidTag<T>::INVALID;

  FullyAssociativeTags() {
    reset();
  }

  void reset() {
    evictmap = 0;
    foreach (i, ways) {
      tags[i] = INVALID;
    }
  }

  void use(int way) {
    evictmap[way] = 1;
    // Performance is somewhat better with this off with higher associativity caches:
    // if (evictmap.allset()) evictmap = 0;
  }

  //
  // This is a clever way of doing branch-free matching
  // with conditional moves and addition. It relies on
  // having at most one matching entry in the array;
  // otherwise the algorithm breaks:
  //
  int match(T target) {
    int way = 0;
    foreach (i, ways) {
      way += (tags[i] == target) ? (i + 1) : 0;
    }

    return way - 1;
  }

  int probe(T target) {
    int way = match(target);
    if (way < 0) return -1;

    use(way);
    return way;
  }

  int lru() const {
    return (evictmap.allset()) ? 0 : (~evictmap).lsb();
  }

  int select(T target, T& oldtag) {
    int way = probe(target);
    if (way < 0) {
      way = lru();
      if (evictmap.allset()) evictmap = 0;
      oldtag = tags[way];
      tags[way] = target;
    }
    use(way);
    if (evictmap.allset()) {
        evictmap = 0;
        use(way);
    }
    return way;
  }

  int select(T target) {
    T dummy;
    return select(target, dummy);
  }

  void invalidate_way(int way) {
    tags[way] = INVALID;
    evictmap[way] = 0;
  }

  int invalidate(T target) {
    int way = probe(target);
    if (way < 0) return -1;
    invalidate_way(way);
    return way;
  }

  const T& operator [](int index) const { return tags[index]; }

  T& operator [](int index) { return tags[index]; }
  int operator ()(T target) { return probe(target); }

  stringbuf& printway(stringbuf& os, int i) const {
    os << "  way ", intstring(i, -2), ": ";
    if (tags[i] != INVALID) {
      os << "tag 0x", hexstring(tags[i], sizeof(T)*8);
      if (evictmap[i]) os << " (MRU)";
    } else {
      os << "<invalid>";
    }
    return os;
  }

  stringbuf& print(stringbuf& os) const {
    foreach (i, ways) {
      printway(os, i);
      os << endl;
    }
    return os;
  }

  ostream& print(ostream& os) const {
    stringbuf sb;
    print(sb);
    os << sb;
    return os;
  }
};

template <typename T, int ways>
ostream& operator <<(ostream& os, const FullyAssociativeTags<T, ways>& tags) {
  return tags.print(os);
}

template <typename T, int ways>
stringbuf& operator <<(stringbuf& sb, const FullyAssociativeTags<T, ways>& tags) {
  return tags.print(sb);
}

//
// Associative array implemented using vectorized
// comparisons spread across multiple byte slices
// and executed in parallel.
//
// This implementation is roughly 2-4x as fast
// as the naive scalar code on SSE2 machines,
// especially for larger arrays.
//
// Very small arrays (less than 8 entries) should
// use the normal scalar FullyAssociativeTags
// for best performance. Both classes use the
// same principle for very fast one-hot matching.
//
// Limitations:
//
// - Every tag in the array must be unique,
//   except for the invalid tag (all 1s)
//
// - <size> can be from 1 to 128. Technically
//   up to 254, however element 255 cannot
//   be used. Matching is done in groups
//   of 16 elements in parallel.
//
// - <width> in bits can be from 1 to 64
//

template <int size, int width, int padsize = 0>
struct FullyAssociativeTagsNbitOneHot {
  typedef vec16b vec_t;
  typedef W64 base_t;

  static const int slices = (width + 7) / 8;
  static const int chunkcount = (size+15) / 16;
  static const int padchunkcount = (padsize+15) / 16;

  vec16b tags[slices][chunkcount + padchunkcount] alignto(16);
  base_t tagsmirror[size]; // for fast scalar access
  bitvec<size> valid;
  bitvec<size> evictmap;

  FullyAssociativeTagsNbitOneHot() {
    reset();
  }

  void reset() {
    valid = 0;
    evictmap = 0;
    memset(tags, 0xff, sizeof(tags));
    memset(tagsmirror, 0xff, sizeof(tagsmirror));
  }

  int match(const vec16b* targetslices) const {
    vec16b sum = x86_sse_zerob();

    foreach (i, chunkcount) {
      vec16b eq = *((vec16b*)&index_bytes_plus1_vec16b[i]);
      foreach (j, slices) {
        eq = x86_sse_pandb(x86_sse_pcmpeqb(tags[j][i], targetslices[j]), eq);
      }
      sum = x86_sse_psadbw(sum, eq);
    }

    int idx = (x86_sse_pextrw<0>(sum) + x86_sse_pextrw<4>(sum));

    return idx-1;
  }

  static void prep(vec16b* targetslices, base_t tag) {
    foreach (i, slices) {
      targetslices[i] = x86_sse_dupb((byte)tag);
      tag >>= 8;
    }
  }

  int match(base_t tag) const {
    vec16b targetslices[16];
    prep(targetslices, tag);
    return match(targetslices);
  }

  int search(base_t tag) const {
    return match(tag);
  }

  int operator()(base_t tag) const {
    return search(tag);
  }

  void update(int index, base_t tag) {
    // Spread it across all the words
    base_t t = tag;
    foreach (i, slices) {
      *(((byte*)(&tags[i])) + index) = (byte)t;
      t >>= 8;
    }

    tagsmirror[index] = tag;
    valid[index] = 1;
    evictmap[index] = 1;
  }

  class ref {
    friend class FullyAssociativeTagsNbitOneHot;

    FullyAssociativeTagsNbitOneHot<size, width, padsize>& tags;
    int index;

    ref();

  public:
    inline ref(FullyAssociativeTagsNbitOneHot& tags_, int index_): tags(tags_), index(index_) { }

    inline ~ref() { }

    inline ref& operator =(base_t tag) {
      tags.update(index, tag);
      return *this;
    }

    inline ref& operator =(const ref& other) {
      tags.update(index, other.tagsmirror[other.index]);
      return *this;
    }
  };

  friend class ref;

  ref operator [](int index) { return ref(*this, index); }
  base_t operator [](int index) const { return tagsmirror[index]; }

  bool isvalid(int index) {
    return valid[index];
  }

  int insertslot(int idx, base_t tag) {
    valid[idx] = 1;
    (*this)[idx] = tag;
    return idx;
  }

  int insert(base_t tag) {
    if (valid.allset()) return -1;
    int idx = (~valid).lsb();
    return insertslot(idx, tag);
  }

  void invalidateslot(int index) {
    valid[index] = 0;
    (*this)[index] = 0xffffffffffffffffULL; // invalid marker
  }

  void validateslot(int index) {
    valid[index] = 1;
  }

  int invalidate(base_t target) {
    int index = match(target);
    if (index < 0) return 0;
    invalidateslot(index);
    return 1;
  }

  bitvec<size> masked_match(base_t targettag, base_t tagmask) {
    bitvec<size> m;

    foreach (i, size) {
      base_t tag = tagsmirror[i];
      m[i] = ((tag & tagmask) == targettag);
    }

    return m;
  }

  void masked_invalidate(const bitvec<size>& slotmask) {
    foreach (i, size) {
      if unlikely (slotmask[i]) invalidateslot(i);
    }
  }

  void use(int way) {
    evictmap[way] = 1;

    if (evictmap.allset()) {
      evictmap = 0;
      evictmap[way] = 1;
    }
  }

  int probe(base_t target) {
    int way = match(target);
    if (way < 0) return way;
    use(way);
    return way;
  }

  int lru() const {
    return (evictmap.allset()) ? 0 : (~evictmap).lsb();
  }

  int select(base_t target, base_t& oldtag) {
    int way = probe(target);
    if (way < 0) {
      way = lru();
      if (evictmap.allset()) evictmap = 0;
      oldtag = tagsmirror[way];
      update(way, target);
    }
    use(way);
    return way;
  }

  int select(base_t target) {
    base_t dummy;
    return select(target, dummy);
  }

  ostream& printid(ostream& os, int slot) const {
    base_t tag = (*this)[slot];
    os << intstring(slot, 3), ": ";
    os << hexstring(tag, 64);
    os << " ";
    foreach (i, slices) {
      const byte b = *(((byte*)(&tags[i])) + slot);
      os << " ", hexstring(b, 8);
    }
    if (!valid[slot]) os << " <invalid>";
    return os;
  }

  ostream& print(ostream& os) const {
    foreach (i, size) {
      printid(os, i);
      os << endl;
    }
    return os;
  }
};

template <int size, int width, int padsize>
ostream& operator <<(ostream& os, const FullyAssociativeTagsNbitOneHot<size, width, padsize>& tags) {
  return tags.print(os);
}

template <typename T, typename V>
struct NullAssociativeArrayStatisticsCollector {
  static void inserted(V& elem, T newtag, int way) { }
  static void replaced(V& elem, T oldtag, T newtag, int way) { }
  static void probed(V& elem, T tag, int way, bool hit) { }
  static void overflow(T tag) { }
  static void locked(V& slot, T tag, int way) { }
  static void unlocked(V& slot, T tag, int way) { }
  static void invalidated(V& elem, T oldtag, int way) { }
};

template <typename T, typename V, int ways, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct FullyAssociativeArray {
  FullyAssociativeTags<T, ways> tags;
  V data[ways];

  FullyAssociativeArray() {
    reset();
  }

  void reset() {
    tags.reset();
    foreach (i, ways) { data[i].reset(); }
  }

  V* probe(T tag) {
    int way = tags.probe(tag);
    stats::probed((way < 0) ? data[0] : data[way], tag, way, (way >= 0));
    return (way < 0) ? NULL : &data[way];
  }

  V* match(T tag) {
    int way = tags.match(tag);
    return (way < 0) ? NULL : &data[way];
  }

  V* select(T tag, T& oldtag) {
    int way = tags.select(tag, oldtag);

    V& slot = data[way];

    if ((way >= 0) & (tag == oldtag)) {
      stats::probed(slot, tag, way, 1);
    } else {
      if (oldtag == tags.INVALID)
        stats::inserted(slot, tag, way);
      else stats::replaced(slot, oldtag, tag, way);
    }

    return &slot;
  }

  V* select(T tag) {
    T dummy;
    return select(tag, dummy);
  }

  int wayof(const V* line) const {
    int way = (line - (const V*)&data);
#if 0
    assert(inrange(way, 0, ways-1));
#endif
    return way;
  }

  T tagof(V* line) {
    int way = wayof(line);
    return tags.tags[way];
  }

  void invalidate_way(int way) {
    stats::invalidated(data[way], tags[way], way);
    tags.invalidate_way(way);
    data[way].reset();
  }

  void invalidate_line(V* line) {
    invalidate_way(wayof(line));
  }

  int invalidate(T tag) {
    int way = tags.probe(tag);
    if (way < 0) return -1;
    invalidate_way(way);
    return way;
  }

  V& operator [](int way) { return data[way]; }

  V* operator ()(T tag) { return select(tag); }

  ostream& print(ostream& os) const {
    foreach (i, ways) {
      stringbuf sb;
      tags.printway(sb, i);
      os << padstring(sb, -40), " -> ";
      data[i].print(os, tags.tags[i]);
      os << endl;
    }
    return os;
  }
};

template <typename T, typename V, int ways>
ostream& operator <<(ostream& os, const FullyAssociativeArray<T, V, ways>& assoc) {
  return assoc.print(os);
}

template <typename T, typename V, int setcount, int waycount, int linesize, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct AssociativeArray {
  typedef FullyAssociativeArray<T, V, waycount, stats> Set;
  Set sets[setcount];

  AssociativeArray() {
    reset();
  }

  void reset() {
    foreach (set, setcount) {
      sets[set].reset();
    }
  }

  static int setof(T addr) {
    return bits(addr, log2(linesize), log2(setcount));
  }

  static T tagof(T addr) {
    return floor(addr, linesize);
  }

  V* probe(T addr) {
    return sets[setof(addr)].probe(tagof(addr));
  }

  V* match(T addr) {
    return sets[setof(addr)].match(tagof(addr));
  }

  V* select(T addr, T& oldaddr) {
    return sets[setof(addr)].select(tagof(addr), oldaddr);
  }

  V* select(T addr) {
    T dummy;
    return sets[setof(addr)].select(tagof(addr), dummy);
  }

  int invalidate(T addr) {
    return sets[setof(addr)].invalidate(tagof(addr));
  }

  ostream& print(ostream& os) const {
    os << "AssociativeArray<", setcount, " sets, ", waycount, " ways, ", linesize, "-byte lines>:", endl;
    foreach (set, setcount) {
      os << "  Set ", set, ":", endl;
      os << sets[set];
    }
    return os;
  }
};

template <typename T, typename V, int size, int ways, int linesize>
ostream& operator <<(ostream& os, const AssociativeArray<T, V, size, ways, linesize>& aa) {
  return aa.print(os);
}

//
// Lockable version of associative arrays:
//

template <typename T, int ways>
struct LockableFullyAssociativeTags {
  bitvec<ways> evictmap;
  bitvec<ways> unlockedmap;
  T tags[ways];

  static const T INVALID = InvalidTag<T>::INVALID;

  LockableFullyAssociativeTags() {
    reset();
  }

  void reset() {
    evictmap = 0;
    unlockedmap.setall();
    foreach (i, ways) {
      tags[i] = INVALID;
    }
  }

  void use(int way) {
    evictmap[way] = 1;
    // Performance is somewhat better with this off with higher associativity caches:
    // if (evictmap.allset()) evictmap = 0;
  }

  //
  // This is a clever way of doing branch-free matching
  // with conditional moves and addition. It relies on
  // having at most one matching entry in the array;
  // otherwise the algorithm breaks:
  //
  int match(T target) {
    int way = 0;
    foreach (i, ways) {
      way += (tags[i] == target) ? (i + 1) : 0;
    }

    return way - 1;
  }

  int probe(T target) {
    int way = match(target);
    if (way < 0) return -1;

    use(way);
    return way;
  }

  int lru() const {
    if (!unlockedmap) return -1;
    bitvec<ways> w = (~evictmap) & unlockedmap;
    return (*w) ? w.lsb() : 0;
  }

  int select(T target, T& oldtag) {
    int way = probe(target);
    if (way < 0) {
      way = lru();
      if (way < 0) return -1;
      if (evictmap.allset()) evictmap = 0;
      oldtag = tags[way];
      tags[way] = target;
    }
    use(way);
    return way;
  }

  int select(T target) {
    T dummy;
    return select(target, dummy);
  }

  int select_and_lock(T tag, bool& firstlock, T& oldtag) {
    int way = select(tag, oldtag);
    if (way < 0) return way;
    firstlock = unlockedmap[way];
    lock(way);
    return way;
  }

  int select_and_lock(T tag, bool& firstlock) {
    T dummy;
    return select_and_lock(tag, firstlock, dummy);
  }

  int select_and_lock(T target) { bool dummy; return select_and_lock(target, dummy); }

  void invalidate_way(int way) {
    tags[way] = INVALID;
    evictmap[way] = 0;
    unlockedmap[way] = 1;
  }

  int invalidate(T target) {
    int way = probe(target);
    if (way < 0) return -1;
    invalidate_way(way);
  }

  bool islocked(int way) const { return !unlockedmap[way]; }

  void lock(int way) { unlockedmap[way] = 0; }
  void unlock(int way) { unlockedmap[way] = 1; }

  const T& operator [](int index) const { return tags[index]; }

  T& operator [](int index) { return tags[index]; }
  int operator ()(T target) { return probe(target); }

  stringbuf& printway(stringbuf& os, int i) const {
    os << "  way ", intstring(i, -2), ": ";
    if (tags[i] != INVALID) {
      os << "tag 0x", hexstring(tags[i], sizeof(T)*8);
      if (evictmap[i]) os << " (MRU)";
      if (!unlockedmap[i]) os << " (locked)";
    } else {
      os << "<invalid>";
    }
    return os;
  }

  stringbuf& print(stringbuf& os) const {
    foreach (i, ways) {
      printway(os, i);
      os << endl;
    }
    return os;
  }

  ostream& print(ostream& os) const {
    stringbuf sb;
    print(sb);
    os << sb;
    return os;
  }
};

template <typename T, int ways>
ostream& operator <<(ostream& os, const LockableFullyAssociativeTags<T, ways>& tags) {
  return tags.print(os);
}

template <typename T, int ways>
stringbuf& operator <<(stringbuf& sb, const LockableFullyAssociativeTags<T, ways>& tags) {
  return tags.print(sb);
}

template <typename T, typename V, int ways, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct LockableFullyAssociativeArray {
  LockableFullyAssociativeTags<T, ways> tags;
  V data[ways];

  LockableFullyAssociativeArray() {
    reset();
  }

  void reset() {
    tags.reset();
    foreach (i, ways) { data[i].reset(); }
  }

  V* probe(T tag) {
    int way = tags.probe(tag);
    stats::probed((way < 0) ? data[0] : data[way], tag, way, (way >= 0));
    return (way < 0) ? NULL : &data[way];
  }

  V* select(T tag, T& oldtag) {
    int way = tags.select(tag, oldtag);

    if (way < 0) {
      stats::overflow(tag);
      return NULL;
    }

    V& slot = data[way];

    if ((way >= 0) & (tag == oldtag)) {
      stats::probed(slot, tag, way, 1);
    } else {
      if (oldtag == tags.INVALID)
        stats::inserted(slot, tag, way);
      else stats::replaced(slot, oldtag, tag, way);
    }

    return &slot;
  }

  V* select(T tag) {
    T dummy;
    return select(tag, dummy);
  }

  V* select_and_lock(T tag, bool& firstlock, T& oldtag) {
    int way = tags.select_and_lock(tag, firstlock, oldtag);

    if (way < 0) {
      stats::overflow(tag);
      return NULL;
    }

    V& slot = data[way];

    if (tag == oldtag) {
      stats::probed(slot, tag, way, 1);
    } else {
      if (oldtag == tags.INVALID)
        stats::inserted(slot, tag, way);
      else stats::replaced(slot, oldtag, tag, way);
      stats::locked(slot, tag, way);
    }

    return &slot;
  }

  V* select_and_lock(T tag, bool& firstlock) {
    T dummy;
    return select_and_lock(tag, firstlock, dummy);
  }

  V* select_and_lock(T tag) { bool dummy; return select_and_lock(tag, dummy); }

  int wayof(const V* line) const {
    int way = (line - (const V*)&data);
#if 0
    assert(inrange(way, 0, ways-1));
#endif
    return way;
  }

  T tagof(V* line) {
    int way = wayof(line);
    return tags.tags[way];
  }

  void invalidate_way(int way) {
    unlock_way(way);
    stats::invalidated(data[way], tags[way], way);
    tags.invalidate_way(way);
    data[way].reset();
  }

  void invalidate_line(V* line) {
    invalidate_way(wayof(line));
  }

  int invalidate(T tag) {
    int way = tags.probe(tag);
    if (way < 0) return -1;
    invalidate_way(way);
    return way;
  }

  void unlock_way(int way) {
    stats::unlocked(data[way], tags[way], way);
    tags.unlock(way);
  }

  void unlock_line(V* line) {
    unlock_way(wayof(line));
  }

  int unlock(T tag) {
    int way = tags.probe(tag);
    if (way < 0) return;
    unlock_way(way);
    if (tags.islocked(way)) stats::unlocked(data[way], tags[way], way);
    return way;
  }

  V& operator [](int way) { return data[way]; }

  V* operator ()(T tag) { return select(tag); }

  ostream& print(ostream& os) const {
    foreach (i, ways) {
      stringbuf sb;
      tags.printway(sb, i);
      os << padstring(sb, -40), " -> ";
      data[i].print(os, tags.tags[i]);
      os << endl;
    }
    return os;
  }
};

template <typename T, typename V, int ways>
ostream& operator <<(ostream& os, const LockableFullyAssociativeArray<T, V, ways>& assoc) {
  return assoc.print(os);
}

template <typename T, typename V, int setcount, int waycount, int linesize, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct LockableAssociativeArray {
  typedef LockableFullyAssociativeArray<T, V, waycount, stats> Set;
  Set sets[setcount];

  LockableAssociativeArray() {
    reset();
  }

  void reset() {
    foreach (set, setcount) {
      sets[set].reset();
    }
  }

  static int setof(T addr) {
    return bits(addr, log2(linesize), log2(setcount));
  }

  static T tagof(T addr) {
    return floor(addr, linesize);
  }

  V* probe(T addr) {
    return sets[setof(addr)].probe(tagof(addr));
  }

  V* select(T addr, T& oldaddr) {
    return sets[setof(addr)].select(tagof(addr), oldaddr);
  }

  V* select(T addr) {
    T dummy;
    return select(addr, dummy);
  }

  void invalidate(T addr) {
    sets[setof(addr)].invalidate(tagof(addr));
  }

  V* select_and_lock(T addr, bool& firstlock, T& oldtag) {
    V* line = sets[setof(addr)].select_and_lock(tagof(addr), firstlock, oldtag);
    return line;
  }

  V* select_and_lock(T addr, bool& firstlock) {
    W64 dummy;
    return select_and_lock(addr, firstlock, dummy);
  }

  V* select_and_lock(T addr) { bool dummy; return select_and_lock(addr, dummy); }

  ostream& print(ostream& os) const {
    os << "LockableAssociativeArray<", setcount, " sets, ", waycount, " ways, ", linesize, "-byte lines>:", endl;
    foreach (set, setcount) {
      os << "  Set ", set, ":", endl;
      os << sets[set];
    }
    return os;
  }
};

template <typename T, typename V, int size, int ways, int linesize>
ostream& operator <<(ostream& os, const LockableAssociativeArray<T, V, size, ways, linesize>& aa) {
  return aa.print(os);
}

template <typename T, int setcount, int linesize>
struct DefaultCacheIndexingFunction {
  static inline Waddr setof(T address) { return bits(address, log2(linesize), log2(setcount)); }
};

template <typename T, int setcount, int linesize>
struct XORCacheIndexingFunction {
  static inline Waddr setof(T address) {
    address >>= log2(linesize);

    const int tagbits = (sizeof(Waddr) * 8) - log2(linesize);
    address = lowbits(address, tagbits);
    return foldbits<log2(setcount)>(address);
  }
};

template <typename T, int setcount, int linesize>
struct CRCCacheIndexingFunction {
  static inline Waddr setof(T address) {
    Waddr slot = 0;
    address >>= log2(linesize);
    CRC32 crc;
    crc << address;
    W32 v = crc;

    return foldbits<log2(setcount)>(v);
  }
};

template <typename T, typename V, int setcount, int waycount, int linesize, typename indexfunc = DefaultCacheIndexingFunction<T, setcount, linesize>, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct LockableCommitRollbackAssociativeArray {
  typedef LockableFullyAssociativeArray<T, V, waycount, stats> Set;
  Set sets[setcount];

  struct ClearList {
    W16 set;
    W16 way;
  };

  //
  // Technically (setcount * waycount) will cover everything,
  // but this is not true if we allow lines to be explicitly
  // invalidated between commits. In this case, a potentially
  // unlimited buffer would be required as noted below.
  //
  // Therefore, we choose a small size, above which it becomes
  // more efficient to just invalidate everything without
  // traversing a clear list.
  //
  ClearList clearlist[64];
  int cleartail;
  bool clearlist_exceeded;

  LockableCommitRollbackAssociativeArray() {
    reset();
  }

  void reset() {
    foreach (set, setcount) {
      sets[set].reset();
    }
    cleartail = 0;
    clearlist_exceeded = 0;
  }

  static int setof(T addr) {
    return indexfunc::setof(addr);
  }

  static T tagof(T addr) {
    return floor(addr, linesize);
  }

  V* probe(T addr) {
    return sets[setof(addr)].probe(tagof(addr));
  }

  V* select(T addr, T& oldaddr) {
    return sets[setof(addr)].select(tagof(addr), oldaddr);
  }

  V* select(T addr) {
    T dummy;
    return select(addr, dummy);
  }

  void invalidate(T addr) {
    sets[setof(addr)].invalidate(tagof(addr));
  }

  V* select_and_lock(T addr, bool& firstlock, T& oldtag) {
    V* line = sets[setof(addr)].select_and_lock(tagof(addr), firstlock, oldtag);
    if unlikely (!line) return NULL;
    if likely (firstlock) {
      int set = setof(addr);
      int way = sets[set].wayof(line);
      if unlikely (cleartail >= lengthof(clearlist)) {
        //
        // Too many lines are locked to keep track of: this can
        // happen if some lines are intentionally invalidated
        // before the final commit or rollback; these invalidates
        // do not remove the corresponding slot from the clearlist,
        // so the list may still overflow. In this case, just bulk
        // process every set and every way.
        //
        clearlist_exceeded = 1;
      } else {
        ClearList& c = clearlist[cleartail++];
        c.set = set;
        c.way = way;
      }
    }
    return line;
  }

  V* select_and_lock(T addr, bool& firstlock) {
    W64 dummy;
    return select_and_lock(addr, firstlock, dummy);
  }

  V* select_and_lock(T addr) { bool dummy; return select_and_lock(addr, dummy); }

  void unlock_all_and_invalidate() {
    if unlikely (clearlist_exceeded) {
      foreach (setid, setcount) {
        Set& set = sets[setid];
        foreach (wayid, waycount) set.invalidate_way(wayid);
      }
    } else {
      foreach (i, cleartail) {
        ClearList& c = clearlist[i];
#if 0
        assert(c.set < setcount);
        assert(c.way < waycount);
#endif
        Set& set = sets[c.set];
        V& line = set[c.way];
        set.invalidate_line(&line);
      }
    }
    cleartail = 0;
    clearlist_exceeded = 0;
#if 0
    foreach (s, setcount) {
      Set& set = sets[s];
      foreach (way, waycount) {
        V& line = set[way];
        T tag = set.tagof(&line);
        if ((tag != set.tags.INVALID)) {
          assert(false);
        }
      }
    }
#endif
  }

  void unlock_all() {
    if unlikely (clearlist_exceeded) {
      foreach (setid, setcount) {
        Set& set = sets[setid];
        foreach (wayid, waycount) set.unlock_way(wayid);
      }
    } else {
      foreach (i, cleartail) {
        ClearList& c = clearlist[i];
#if 0
        assert(c.set < setcount);
        assert(c.way < waycount);
#endif
        Set& set = sets[c.set];
        V& line = set[c.way];
        set.unlock_line(&line);
      }
    }
    cleartail = 0;
    clearlist_exceeded = 0;
  }

  ostream& print(ostream& os) const {
    os << "LockableAssociativeArray<", setcount, " sets, ", waycount, " ways, ", linesize, "-byte lines>:", endl;
    foreach (set, setcount) {
      os << "  Set ", set, ":", endl;
      os << sets[set];
    }
    return os;
  }
};

template <typename T, typename V, int size, int ways, int linesize>
ostream& operator <<(ostream& os, const LockableCommitRollbackAssociativeArray<T, V, size, ways, linesize>& aa) {
  return aa.print(os);
}

//
// Lockable cache arrays supporting commit/rollback
//
// This structure implements the dirty-and-locked scheme to prevent speculative
// data from propagating to lower levels of the cache hierarchy until it can be
// committed.
//
// Any stores into the cache (signalled by select_and_lock()) back up the old
// cache line and add this to an array for later rollback purposes.
//
// At commit(), all locked lines are unlocked and the backed up cache lines are
// simply discarded, leaving them free to be replaced or written back.
//
// At rollback() all locked lines are invalidated in both this cache and any
// higher levels (via the invalidate_upwards() callback), thereby forcing
// clean copies to be refetched as needed after the rollback.
//

template <typename T, typename V, int setcount, int waycount, int linesize, int maxdirty, typename stats = NullAssociativeArrayStatisticsCollector<T, V> >
struct CommitRollbackCache: public LockableCommitRollbackAssociativeArray<T, V, setcount, waycount, linesize, stats> {
  typedef LockableCommitRollbackAssociativeArray<T, V, setcount, waycount, linesize, stats> array_t;

  struct BackupCacheLine {
    W64* addr;
    W64 data[linesize / sizeof(W64)];
  };

  BackupCacheLine stores[maxdirty];
  BackupCacheLine* storetail;

  CommitRollbackCache() {
    reset();
  }

  void reset() {
    array_t::reset();
    storetail = stores;
  }

  //
  // Invalidate lines in higher level caches if needed
  //
  void invalidate_upwards(T addr);

  void invalidate(T addr) {
    array_t::invalidate(addr);
    invalidate_upwards(addr);
  }

  V* select_and_lock(T addr, T& oldaddr) {
    addr = floor(addr, linesize);

    bool firstlock;
    V* line = array_t::select_and_lock(addr, firstlock, oldaddr);
    if (!line) return NULL;

    if (firstlock) {
      W64* linedata = (W64*)addr;
      storetail->addr = linedata;
      foreach (i, lengthof(storetail->data)) storetail->data[i] = linedata[i];
      storetail++;
    }

    return line;
  }

  V* select_and_lock(T addr) {
    T dummy;
    return select_and_lock(addr, dummy);
  }

  void commit() {
    array_t::unlock_all();
    storetail = stores;
  }

  void rollback() {
    array_t::unlock_all_and_invalidate();

    BackupCacheLine* cl = stores;
    while (cl < storetail) {
      W64* linedata = cl->addr;
      foreach (i, lengthof(storetail->data)) linedata[i] = cl->data[i];
      invalidate_upwards((W64)cl->addr);
      cl++;
    }
    storetail = stores;
  }

  void complete() { }
};

template <int size, int padsize = 0>
struct FullyAssociativeTags8bit {
  typedef vec16b vec_t;
  typedef byte base_t;

  static const int chunkcount = (size+15) / 16;
  static const int padchunkcount = (padsize+15) / 16;

  vec_t tags[chunkcount + padchunkcount] alignto(16);
  bitvec<size> valid;

  W64 getvalid() { return valid.integer(); }

  FullyAssociativeTags8bit() {
    reset();
  }

  base_t operator [](int i) const {
    return ((base_t*)&tags)[i];
  }

  base_t& operator [](int i) {
    return ((base_t*)&tags)[i];
  }

  bool isvalid(int index) {
    return valid[index];
  }

  void reset() {
    valid = 0;
    W64* p = (W64*)&tags;
    foreach (i, ((chunkcount + padchunkcount)*16)/8) p[i] = 0xffffffffffffffffLL;
  }

  static const vec_t prep(base_t tag) {
    return x86_sse_dupb(tag);
  }

  int insertslot(int idx, base_t tag) {
    valid[idx] = 1;
    (*this)[idx] = tag;
    return idx;
  }

  int insert(base_t tag) {
    if (valid.allset()) return -1;
    int idx = (~valid).lsb();
    return insertslot(idx, tag);
  }

  bitvec<size> match(const vec_t target) const {
    bitvec<size> m = 0;

    foreach (i, chunkcount) {
      m = m.accum(i*16, 16, x86_sse_pmovmskb(x86_sse_pcmpeqb(target, tags[i])));
    }

    return m & valid;
  }

  bitvec<size> match(base_t target) const {
    return match(prep(target));
  }

  bitvec<size> matchany(const vec_t target) const {
    bitvec<size> m = 0;

    vec_t zero = prep(0);

    foreach (i, chunkcount) {
      m = m.accum(i*16, 16, x86_sse_pmovmskb(x86_sse_pcmpeqb(x86_sse_pandb(tags[i], target), zero)));
    }

    return (~m) & valid;
  }

  bitvec<size> matchany(base_t target) const {
    return matchany(prep(target));
  }

  int search(const vec_t target) const {
    bitvec<size> bitmap = match(target);
    int idx = bitmap.lsb();
    if (!bitmap) idx = -1;
    return idx;
  }

  int extract(const vec_t target) {
    int idx = search(target);
    if (idx >= 0) valid[idx] = 0;
    return idx;
  }

  int search(base_t tag) const {
    return search(prep(tag));
  }

  bitvec<size> extract(base_t tag) {
    return extract(prep(tag));
  }

  void invalidateslot(int index) {
    valid[index] = 0;
  }

  const bitvec<size>& invalidatemask(const bitvec<size>& mask) {
    valid &= ~mask;
    return mask;
  }

  bitvec<size> invalidate(const vec_t target) {
    return invalidatemask(match(target));
  }

  bitvec<size> invalidate(base_t target) {
    return invalidate(prep(target));
  }

  void collapse(int index) {
    base_t* tagbase = (base_t*)&tags;
    base_t* base = tagbase + index;
    vec_t* dp = (vec_t*)base;
    vec_t* sp = (vec_t*)(base + sizeof(base_t));

    foreach (i, chunkcount) {
      x86_sse_stvbu(dp++, x86_sse_ldvbu(sp++));
    }

    valid = valid.remove(index);
  }

  void decrement(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_psubusb(tags[i], prep(amount)); }
  }

  void increment(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_paddusb(tags[i], prep(amount)); }
  }

  ostream& printid(ostream& os, int slot) const {
    int tag = (*this)[slot];
    if (valid[slot])
      os << intstring(tag, 3);
    else os << "???";
    return os;
  }

  ostream& print(ostream& os) const {
    foreach (i, size) {
      printid(os, i);
      os << " ";
    }
    return os;
  }
};

template <int size, int padsize>
ostream& operator <<(ostream& os, const FullyAssociativeTags8bit<size, padsize>& tags) {
  return tags.print(os);
}

template <int size, int padsize = 0>
struct FullyAssociativeTags16bit {
  typedef vec8w vec_t;
  typedef W16 base_t;

  static const int chunkcount = ((size*2)+15) / 16;
  static const int padchunkcount = ((padsize*2)+15) / 16;

  vec_t tags[chunkcount + padchunkcount] alignto(16);
  bitvec<size> valid;

  W64 getvalid() { return valid.integer(); }

  FullyAssociativeTags16bit() {
    reset();
  }

  base_t operator [](int i) const {
    return ((base_t*)&tags)[i];
  }

  base_t& operator [](int i) {
    return ((base_t*)&tags)[i];
  }

  bool isvalid(int index) {
    return valid[index];
  }

  void reset() {
    valid = 0;
    W64* p = (W64*)&tags;
    foreach (i, ((chunkcount + padchunkcount)*16)/8) p[i] = 0xffffffffffffffffLL;
  }

  static const vec_t prep(base_t tag) {
    return x86_sse_dupw(tag);
  }

  int insertslot(int idx, base_t tag) {
    valid[idx] = 1;
    (*this)[idx] = tag;
    return idx;
  }

  int insert(base_t tag) {
    if (valid.allset()) return -1;
    int idx = (~valid).lsb();
    return insertslot(idx, tag);
  }

  bitvec<size> match(const vec_t target) const {
    bitvec<size> m = 0;

    foreach (i, chunkcount) {
      m = m.accum(i*8, 8, x86_sse_pmovmskw(x86_sse_pcmpeqw(target, tags[i])));
    }

    return m & valid;
  }

  bitvec<size> match(base_t target) const {
    return match(prep(target));
  }

  bitvec<size> matchany(const vec_t target) const {
    bitvec<size> m = 0;

    vec_t zero = prep(0);

    foreach (i, chunkcount) {
      m = m.accum(i*8, 8, x86_sse_pmovmskw(x86_sse_pcmpeqw(x86_sse_pandw(tags[i], target), zero)));
    }

    return (~m) & valid;
  }

  bitvec<size> matchany(base_t target) const {
    return matchany(prep(target));
  }

  int search(const vec_t target) const {
    bitvec<size> bitmap = match(target);
    int idx = bitmap.lsb();
    if (!bitmap) idx = -1;
    return idx;
  }

  int extract(const vec_t target) {
    int idx = search(target);
    if (idx >= 0) valid[idx] = 0;
    return idx;
  }

  int search(base_t tag) const {
    return search(prep(tag));
  }

  bitvec<size> extract(base_t tag) {
    return extract(prep(tag));
  }

  void invalidateslot(int index) {
    valid[index] = 0;
  }

  const bitvec<size>& invalidatemask(const bitvec<size>& mask) {
    valid &= ~mask;
    return mask;
  }

  bitvec<size> invalidate(const vec_t target) {
    return invalidatemask(match(target));
  }

  bitvec<size> invalidate(base_t target) {
    return invalidate(prep(target));
  }

  void collapse(int index) {
    base_t* tagbase = (base_t*)&tags;
    base_t* base = tagbase + index;
    vec_t* dp = (vec_t*)base;
    vec_t* sp = (vec_t*)(base + 1);

    foreach (i, chunkcount) {
      x86_sse_stvwu(dp++, x86_sse_ldvwu(sp++));
    }

    valid = valid.remove(index);
  }

  void decrement(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_psubusw(tags[i], prep(amount)); }
  }

  void increment(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_paddusw(tags[i], prep(amount)); }
  }

  ostream& printid(ostream& os, int slot) const {
    int tag = (*this)[slot];
    if (valid[slot])
      os << intstring(tag, 3);
    else os << "???";
    return os;
  }

  ostream& print(ostream& os) const {
    foreach (i, size) {
      printid(os, i);
      os << " ";
    }
    return os;
  }
};

template <int size, int padsize>
ostream& operator <<(ostream& os, const FullyAssociativeTags16bit<size, padsize>& tags) {
  return tags.print(os);
}

#endif // _LOGIC_H_
