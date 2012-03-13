// -*- c++ -*-
//
// Super Standard Template Library
//
// Faster and more optimized than stock STL implementation,
// plus includes various customized features
//
// Copyright 1997-2008 Matt T. Yourst <yourst@yourst.com>
//
// Modifications for MARSSx86
// Copyright 2009 Avadh Patel <avadh4all@gmail.com>
//
// This program is free software; it is licensed under the
// GNU General Public License, Version 2.
//

#ifndef _SUPERSTL_H_
#define _SUPERSTL_H_

#include <iostream>
#include <fstream>

//
// Formatting
//
#define FMT_ZEROPAD 1 /* pad with zero */
#define FMT_SIGN    2 /* unsigned/signed long */
#define FMT_PLUS    4 /* show plus */
#define FMT_SPACE   8 /* space if plus */
#define FMT_LEFT	  16 /* left justified */
#define FMT_SPECIAL	32 /* 0x */
#define FMT_LARGE	  64 /* use 'ABCDEF' instead of 'abcdef' */

int current_vcpuid();

extern bool force_synchronous_streams;

// Number Formatting
char* format_number(char* buf, char* end, W64 num, int base, int size, int precision, int type);
int format_integer(char* buf, int bufsize, W64s v, int size = 0, int flags = 0, int base = 10, int precision = 0);
int format_float(char* buf, int bufsize, double v, int precision = 6, int pad = 0);

//
// Division functions
//
#ifdef __x86_64__

#define do_div(n,base) ({					\
	W32 __base = (base);				\
	W32 __rem;						\
	__rem = ((W64)(n)) % __base;			\
	(n) = ((W64)(n)) / __base;				\
	__rem;							\
 })

#else

// 32-bit x86
#define do_div(n,base) ({ \
	W32 __upper, __low, __high, __mod, __base; \
	__base = (base); \
	asm("":"=a" (__low), "=d" (__high):"A" (n)); \
	__upper = __high; \
	if (__high) { \
		__upper = __high % (__base); \
		__high = __high / (__base); \
	} \
	asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
	asm("":"=A" (n):"a" (__low),"d" (__high)); \
	__mod; \
})

#endif

typedef std::ostream ostream;
typedef std::istream istream;
typedef std::ofstream ofstream;
typedef std::ifstream ifstream;

namespace superstl {

  //
  // String buffer
  //

  template<typename T>
  class dynarray;

#define stringbuf_smallbufsize 256
  class stringbuf;

  stringbuf& operator <<(stringbuf& os, const char* v);
  stringbuf& operator <<(stringbuf& os, const char v);

  class stringbuf {
  public:
    stringbuf() { buf = NULL; reset(); }
    stringbuf(int length) {
      buf = NULL;
      reset(length);
    }

    stringbuf(const stringbuf& sb) {
        *this << sb;
    }

    void reset(int length = stringbuf_smallbufsize);

    ~stringbuf();

    int remaining() const {
      return (buf + length) - p;
    }

    operator char*() const {
      return buf;
    }

    void resize(int newlength);

    void expand() {
      resize(length*2);
    }

    void reserve(int extra);

	stringbuf strip();

	void split(dynarray<stringbuf*> &bufArray, const char *chr);

    int size() const { return p - buf; }
    bool empty() const { return (size() == 0); }
    bool set() const { return !empty(); }

    stringbuf& operator =(const char* str) {
      if unlikely (!str) {
        reset();
        return *this;
      }
      reset(strlen(str)+1);
      *this << str;
      return *this;
    }

    stringbuf& operator =(const stringbuf& str) {
      const char* s = (const char*)str;
      if unlikely (!s) {
        reset();
        return *this;
      }
      reset(strlen(s)+1);
      *this << s;
      return *this;
    }

    bool operator ==(const stringbuf& s) {
      return strequal((char*)(*this), (char*)s);
    }

    bool operator ==(const char *s) {
        return strequal((char*)(*this), s);
    }

    bool operator !=(const stringbuf& s) {
      return !strequal((char*)(*this), (char*)s);
    }

    bool operator !=(const char *s) {
        return !strequal((char*)(*this), s);
    }

  public:
    char smallbuf[stringbuf_smallbufsize];
    char* buf;
    char* p;
    int length;
  };

  //
  // Inserters
  //

#define DefineIntegerInserter(T, signedtype) \
  static inline stringbuf& operator <<(stringbuf& os, const T v) { \
    char buf[128]; \
    format_integer(buf, sizeof(buf), ((signedtype) ? (W64s)v : (W64)v)); \
    return os << buf; \
  }

  DefineIntegerInserter(signed short, 1);
  DefineIntegerInserter(signed int, 0);
  DefineIntegerInserter(signed long, 0);
  DefineIntegerInserter(signed long long, 0);
  DefineIntegerInserter(unsigned short, 0);
  DefineIntegerInserter(unsigned int, 0);
  DefineIntegerInserter(unsigned long, 0);
  DefineIntegerInserter(unsigned long long, 0);

#define DefineFloatInserter(T, digits) \
  static inline stringbuf& operator <<(stringbuf& os, const T v) { \
    char buf[128]; \
    format_float(buf, sizeof(buf), v, digits); \
    return os << buf; \
  }

  DefineFloatInserter(float, 6);
  DefineFloatInserter(double, 16);

  static inline stringbuf& operator <<(stringbuf& os, const bool v) {
    return os << (int)v;
  }

#undef DefineInserter

#define PrintOperator(T) static inline ostream& operator <<(ostream& os, const T& obj) { return obj.print(os); }

  static inline stringbuf& operator <<(stringbuf& os, const stringbuf& sb) {
    os << ((char*)sb);
    return os;
  }

  template <class T>
  static inline stringbuf& operator <<(stringbuf& os, const T* v) {
    char buf[128];
    format_integer(buf, sizeof(buf), (W64)(Waddr)v, 0, FMT_SPECIAL, 16);
    return os << buf;
  }

  //
  // A much more intuitive syntax than STL provides:
  //
  template <class T>
  static inline stringbuf& operator ,(stringbuf& os, const T& v) {
    return os << v;
  }

  //
  // ostream class
  //
  static const char endl[] = "\n";
  struct iosflush { iosflush() {} };
  extern const iosflush flush;

  static inline ostream& operator <<(ostream& os, const iosflush& v) {
    os.flush();
    return os;
  }


  // Some generic functions to write to ostream
#define OUTPUT_TO_OSTREAM(T) \
  static inline ostream& operator <<(ostream& os, const T& t) { \
	  return os.write(reinterpret_cast<const char*>(&t), sizeof(T)); \
  }

  static inline ostream& operator <<(ostream& os, const stringbuf& v) { stringbuf sb; sb << (char*)v; os.write((char*)sb, sb.size()); return os; }

  static inline ostream& operator <<(ostream& os, const W8& v) {
	  return os << (unsigned int)(v);
  }

  static inline ostream& operator <<(ostream& os, const W8s& v) {
	  return os << (signed int)(v);
  }

  static inline ostream& operator ,(ostream& os, char* c) {
	  return os << c;
  }

  template <typename T>
  static inline ostream& operator ,(ostream& os, const T& v) {
    return os << v;
  }



#define DeclareStringBufToStream(T) inline ostream& operator <<(ostream& os, const T& arg) { stringbuf sb; sb << arg; os << sb; return os; }

  // Print bits as a string:
  struct bitstring {
    W64 bits;
    int n;
    bool reverse;

    bitstring() { }

    bitstring(const W64 bits, const int n, bool reverse = false) {
      assert(n <= 64);
      this->bits = bits;
      this->n = n;
      this->reverse = reverse;
    }
  };

  stringbuf& operator <<(stringbuf& os, const bitstring& bs);

  DeclareStringBufToStream(bitstring);

  struct bitmaskstring {
    W64 bits;
    W64 mask;
    int n;
    bool reverse;

    bitmaskstring() { }

    bitmaskstring(const W64 bits, W64 mask, const int n, bool reverse = false) {
      assert(n <= 64);
      this->bits = bits;
      this->mask = mask;
      this->n = n;
      this->reverse = reverse;
    }
  };

  stringbuf& operator <<(stringbuf& os, const bitmaskstring& bs);

  DeclareStringBufToStream(bitmaskstring);

  struct hexstring {
    W64 value;
    int n;

    hexstring() { }

    hexstring(const W64 value, const int n) {
      this->value = value;
      this->n = n;
    }
  };

  stringbuf& operator <<(stringbuf& os, const hexstring& hs);

  DeclareStringBufToStream(hexstring);

  static inline ostream& operator ,(ostream& os, const byte* v) {
	  return os << "0x", hexstring((unsigned long)(v), 64);
  }

  struct bytestring {
    const byte* bytes;
    int n;
    int splitat;

    bytestring() { }

    bytestring(const byte* bytes, int n, int splitat = 16) {
      this->bytes = bytes;
      this->n = n;
      this->splitat = splitat;
    }
  };

  stringbuf& operator <<(stringbuf& os, const bytestring& bs);

  DeclareStringBufToStream(bytestring);

  struct bytemaskstring {
    const byte* bytes;
    W64 mask;
    int n;
    int splitat;

    bytemaskstring() { }

    bytemaskstring(const byte* bytes, W64 mask, int n, int splitat = 16) {
      assert(n <= 64);
      this->bytes = bytes;
      this->mask = mask;
      this->n = n;
      this->splitat = splitat;
    }
  };

  stringbuf& operator <<(stringbuf& os, const bytemaskstring& bs);

  DeclareStringBufToStream(bytemaskstring);

  struct intstring {
    W64s value;
    int width;

    intstring() { }

    intstring(W64s value, int width) {
      this->value = value;
      this->width = width;
    }
  };

  stringbuf& operator <<(stringbuf& os, const intstring& is);

  DeclareStringBufToStream(intstring);

  struct floatstring {
    double value;
    int width;
    int precision;

    floatstring() { }

    floatstring(double value, int width = 0, int precision = 6) {
      this->value = value;
      this->width = width;
      this->precision = precision;
    }
  };

  stringbuf& operator <<(stringbuf& os, const floatstring& fs);

  DeclareStringBufToStream(floatstring);

  struct padstring {
    const char* value;
    int width;
    char pad;

    padstring() { }

    padstring(const char* value, int width, char pad = ' ') {
      this->value = value;
      this->width = width;
      this->pad = pad;
    }
  };

  stringbuf& operator <<(stringbuf& os, const padstring& s);

  DeclareStringBufToStream(padstring);

  struct percentstring {
    double fraction;
    int width;

    percentstring() { }

    percentstring(W64s value, W64s total, int width = 7) {
      fraction = (total) ? (double(value) / double(total)) : 0;
      this->width = width;
    }
  };

  static inline stringbuf& operator <<(stringbuf& os, const percentstring& ps) {
    double f = ps.fraction * 100.;
    W64s intpart = W64s(f);
    W64s fracpart = clipto(W64s(((f - double(intpart)) * 100) + 0.5), W64s(0), W64s(99));

    stringbuf sbfrac;
    sbfrac << fracpart;

    stringbuf sb;
    sb << intpart, '.', padstring(sbfrac, 2, '0'), '%';

    os << padstring(sb, ps.width);
    return os;
  }

  DeclareStringBufToStream(percentstring);

  struct substring {
    const char* str;
    int length;

    substring() { }

    substring(const char* str, int start, int length) {
      int r = strlen(str);
      this->length = min(length, r - start);
      this->str = str + min(start, r);
    }
  };

  stringbuf& operator <<(stringbuf& os, const substring& s);

  DeclareStringBufToStream(substring);

  //
  // String tools
  //
  int stringsubst(stringbuf& sb, const char* pattern, const char* find, const char* replace);
  int stringsubst(stringbuf& sb, const char* pattern, const char* find[], const char* replace[], int substcount);

  class readline {
  public:
    readline(char* p, size_t l): buf(p), len(l) { }
    char* buf;
    size_t len;
  };

  //inline istream& operator ,(istream& is, const readline& v) { return is >> v; }

  static inline istream& operator >>(istream& is, const readline& v) {
    is.read(v.buf, v.len);
    return is;
  }

  template <typename T>
  static inline ifstream& operator >>(ifstream& is, T& v) {
    is.read((char*)(&v), sizeof(v));
    return is;
  }

  static inline istream& operator >>(istream& is, stringbuf& sb) {
    is.read(sb.buf, sb.length);
    return is;
  }

  //
  // Global streams:
  //
  extern istream& cin;
  extern ostream& cout;
  extern ostream& cerr;


  //
  // Turn a raw-memory into an array of <T> filled with <value>
  //
  template <typename T, bool> struct ArrayConstructor {
    inline static void init(T* noalias p, int length) {
      foreach (i, length) { new(p + i) T(); }
    }
  };

  // Specialization for primitive initialization
  template <typename T> struct ArrayConstructor<T, true> {
    inline static void init(T* noalias p, size_t length) { }
  };

  template <typename T, bool> struct ArrayInitializer {
    inline static void init(T* noalias p, size_t length, const T value) {
      foreach (i, length) { new(p + i) T(value); }
    }
  };

  // Specialization for primitive initialization
  template <typename T> struct ArrayInitializer<T, true> {
    inline static void init(T* noalias p, size_t length, const T value) {
      foreach (i, length) { p[i] = value; }
    }
  };

  template <typename T>
  static inline T* renew(T* p, size_t oldcount, size_t newcount) {
    if unlikely (newcount <= oldcount) return p;
    T* pp = (T*)malloc(sizeof(T) * newcount);
    if unlikely (!p) { assert(oldcount == 0); }

    if likely (p && p != pp) {
      arraycopy(pp, p, oldcount);
      free(p);
    }

    ArrayConstructor<T, isprimitive(T) | ispointer(T)>::init(pp + oldcount, newcount - oldcount);

    return pp;
  }

  template <class T>
  struct range {
    T lo, hi;
    range() { }
    range(T v) { this->lo = v; this->hi = v; }
    range(T lo, T hi) { this->lo = lo; this->hi = hi; }
    range<T>& operator ()(T lo, T hi) { this->lo = lo; this->hi = hi; return *this; }
    range<T>& operator ()(T v) { this->lo = v; this->hi = v; return *this; }
    bool contains(T p) const { return ((p >= lo) && (p <= hi)); }
    T size() const { return abs(hi - lo); }
    bool operator &(T p) const { return contains(p); }
    bool operator ~() const { return size(); }
    ostream& print(ostream& os) const {
      return os << '[', lo, ' ', hi, ']';
    }
  };

  template <typename T>
  static inline ostream& operator <<(ostream& os, const range<T>& r) {
    return r.print(os);
  }

  /*
   * Simple array class with optional bounds checking
   */
  template <typename T, int size>
  struct array {
  public:
    array() { }
    static const int length = size;

    T data[size];
    const T& operator [](int i) const {
#ifdef CHECK_BOUNDS
      assert((i >= 0) && (i < size));
#endif
      return data[i];
    }

    T& operator [](int i) {
#ifdef CHECK_BOUNDS
      assert((i >= 0) && (i < size));
#endif
      return data[i];
    }

    void clear() {
      foreach(i, size) data[i] = T();
    }

    void fill(const T& v) {
      foreach(i, size) data[i] = v;
    }
  };

  template <typename T, int size>
  struct stack {
  public:
    T data[size];
    int count;
    static const int length = size;

    void reset() { count = 0; }

    stack() { reset(); }

    const T& operator [](int i) const {
      return data[i];
    }

    T& operator [](int i) {
      return data[i];
    }

    T& push() {
      if unlikely (count >= size) abort();
      T& v = data[count++];
      return v;
    }

    T& push(const T& v) {
      T& r = push();
      r = v;
      return r;
    }

    T& pop() {
      if unlikely (!count) abort();
      T& v = data[--count];
      return v;
    }

    bool empty() const { return (count == 0); }
    bool full() const { return (count == size); }

    stack<T, size>& operator =(const stack<T, size>& stack) {
      count = stack.count;
      foreach (i, count) data[i] = stack.data[i];
      return *this;
    }

    ostream& print(ostream& os) const {
      foreach (i, count) { os << ((i) ? " " : ""), data[i]; }
      return os;
    }
  };

  template <typename T, int size>
  static inline ostream& operator <<(ostream& os, const stack<T, size>& st) {
    return st.print(os);
  }

  template <typename T, int size>
  static inline ostream& operator <<(ostream& os, const array<T, size>& v) {
	  os , endl;
    os << "Array of ", size, " elements:", endl;
    for (int i = 0; i < size; i++) {
      os << "  [", i, "]: ", v[i], endl;
    }
    return os;
  }

  /*
   * Simple STL-like dynamic array class.
   */
  template <class T>
  class dynarray {
  protected:
  public:
    T* data;
    int length;
    int reserved;
    int granularity;

  public:
    inline T& operator [](int i) { return data[i]; }
    inline T operator [](int i) const { return data[i]; }

    operator T*() const { return data; }

    // NOTE: g *must* be a power of two!
    dynarray() {
      length = reserved = 0;
      granularity = 16;
      data = NULL;
    }

    dynarray(int initcap, int g = 16) {
      length = 0;
      reserved = 0;
      granularity = g;
      data = NULL;
      reserve(initcap);
    }

    ~dynarray() {
      if (!(isprimitive(T) | ispointer(T))) {
        foreach (i, reserved) data[i].~T();
      }

      if(data) free(data);
      data = NULL;
      length = 0;
      reserved = 0;
    }

    inline int capacity() const { return reserved; }
    inline bool empty() const { return (length == 0); }
    inline void clear() { resize(0); }
    inline int size() const { return length; }
    inline int count() const { return length; }

    void push(const T& obj) {
      T& pushed = push();
      pushed = obj;
    }

    T& push() {
      reserve(length + 1);
      length++;
      return data[length-1];
    }

    T& pop() {
      length--;
      return data[length];
    }

    void resize(int newsize) {
      if likely (newsize > length) reserve(newsize);
      length = newsize;
    }

    void resize(int newsize, const T& emptyvalue) {
      int oldlength = length;
      resize(newsize);
      if unlikely (newsize <= oldlength) return;
      for (int i = oldlength; i < reserved; i++) { data[i] = emptyvalue; }
    }

    void reserve(int newsize) {
      if unlikely (newsize <= reserved) return;
      newsize = (newsize + (granularity-1)) & ~(granularity-1);
      data = renew(data, length, newsize);
      reserved = newsize;
    }

    bool remove(const T value) {
      bool move_start = false;
      foreach (i, length) {
        if(move_start){
          data[i-1] = data[i];
        }else if(data[i] == value){
          move_start = true;
        }
      }
      length--;
      return move_start;
    }

    void fill(const T value) {
      foreach (i, length) {
        data[i] = value;
      }
    }

    void clear_and_free() {
        foreach (i, length) {
            T t = data[i];
            delete t;
        }
        resize(0);
    }

    // Only works with specialization for character arrays:
    char* tokenize(char* string, const char* seplist) { abort(); }
  };

  template <class T> static inline const T& operator <<(dynarray<T>& buf, const T& v) { return buf.push(v); }
  template <class T> static inline const T& operator >>(dynarray<T>& buf, T& v) { return (v = buf.pop()); }

  template <> char* dynarray<char*>::tokenize(char* string, const char* seplist);

  template <class T>
  static inline ostream& operator <<(ostream& os, const dynarray<T>& v) {
    os << "Array of ", v.size(), " elements (", v.capacity(), " reserved): ", endl;
    for (int i = 0; i < v.size(); i++) {
      os << "  [", i, "]: ", v[i], endl;
    }
    return os;
  }

  /*
   * CRC32
   */
  struct CRC32 {
    static const W32 crctable[256];
    W32 crc;

    inline W32 update(byte value) {
      crc = crctable[byte(crc ^ value)] ^ (crc >> 8);
      return crc;
    }

    inline W32 update(byte* data, int count) {
      for (int i = 0; i < count ; i++) {
        update(data[i]);
      }
      return crc;
    }

    CRC32() {
      reset();
    }

    CRC32(W32 newcrc) {
      reset(newcrc);
    }

    inline void reset(W32 newcrc = 0xffffffff) {
      crc = newcrc;
    }

    operator W32() const {
      return crc;
    }
  };

  template <typename T>
  static inline CRC32& operator <<(CRC32& crc, const T& t) {
    crc.update((byte*)&t, sizeof(T));
    return crc;
  }

  template <class T>
  static inline CRC32& operator ,(CRC32& crc, const T& v) {
    return crc << v;
  }

  struct RandomNumberGenerator {
    W32 s1, s2, s3;

    RandomNumberGenerator(W32 seed = 123) { reseed(seed); }

    void reseed(W32 seed);
    W32 random32();
    W64 random64();
    void fill(void* p, size_t count);
  };

  //
  // selflistlink class
  // Double linked list without pointer: useful as root
  // of inheritance hierarchy for another class to save
  // space, since object pointed to is implied
  //
  class selflistlink {
  public:
    selflistlink* next;
    selflistlink* prev;
  public:
    void reset() { next = NULL; prev = NULL; }
    selflistlink() { reset(); }

    selflistlink* unlink() {
      if likely (prev) prev->next = next;
      if likely (next) next->prev = prev;
      prev = NULL;
      next = NULL;
      return this;
    }

    selflistlink* replacewith(selflistlink* newlink) {
      if likely (prev) prev->next = newlink;
      if likely (next) next->prev = newlink;
      newlink->prev = prev;
      newlink->next = next;
      return newlink;
    }

    void addto(selflistlink*& root) {
      // THIS <-> root <-> a <-> b <-> c
      this->prev = (selflistlink*)&root;
      this->next = root;
      if likely (root) root->prev = this;
      // Do not touch root->next since it might not even exist
      root = this;
    }

    bool linked() const {
      return (next || prev);
    }

    bool unlinked() const {
      return !linked();
    }
  };

  static inline ostream& operator <<(ostream& os, const selflistlink& link) {
    return os << "[prev ", link.prev, ", next ", link.next, "]";
  }

  class selfqueuelink {
  public:
    selfqueuelink* next;
    selfqueuelink* prev;
  public:
    void reset() { next = this; prev = this; }
    selfqueuelink() { }

    selfqueuelink& unlink() {
      // No effect if next = prev = this (i.e., unlinked)
		  next->prev = prev;
		  prev->next = next;
      prev = this;
      next = this;
      return *this;
    }

    void addhead(selfqueuelink& root) {
      addlink(&root, root.next);
    }

    void addhead(selfqueuelink* root) {
      addhead(*root);
    }

    void addto(selfqueuelink& root) {
      addhead(root);
    }

    void addto(selfqueuelink* root) {
      addto(*root);
    }

    void addtail(selfqueuelink& root) {
      addlink(root.prev, &root);
    }

    void addtail(selfqueuelink* root) {
      addtail(*root);
    }

    selfqueuelink* removehead() {
      if unlikely (empty()) return NULL;
      selfqueuelink* link = next;
      link->unlink();
      return link;
    }

    selfqueuelink* removetail() {
      if unlikely (empty()) return NULL;
      selfqueuelink* link = prev;
      link->unlink();
      return link;
    }

    selfqueuelink* head() const {
      return next;
    }

    selfqueuelink* tail() const {
      return prev;
    }

    bool empty() const {
      return (next == this);
    }

    bool unlinked() const {
      return ((!prev && !next) || ((prev == this) && (next == this)));
    }

    bool linked() const {
      return !unlinked();
    }

    operator bool() const { return (!empty()); }

    virtual ostream& print(ostream& os) const {return os;}
  protected:
    void addlink(selfqueuelink* prev, selfqueuelink* next) {
		  next->prev = this;
      this->next = next;
      this->prev = prev;
		  prev->next = this;
    }
  };

  //
  // Default link manager for objects in which the
  // very first member (or superclass) is selflistlink.
  //
  template <typename T>
  struct ObjectLinkManager {
    static inline T* objof(selflistlink* link) { return (T*)link; }
    static inline selflistlink* linkof(T* obj) { return (selflistlink*)obj; }
    //
    // Example:
    //
    // T* objof(selflistlink* link) {
    //   return baseof(T, hashlink, link); // a.k.a. (T*)((byte*)link) - offsetof(T, hashlink);
    // }
    //
    // selflistlink* linkof(T* obj) {
    //   return &obj->link;
    // }
    //
  };

  template <class T>
  class queuelink {
  public:
    queuelink<T>* next;
    queuelink<T>* prev;
    T* data;
	bool free;
  public:
	void reset() { next = NULL; prev = NULL; data = NULL;
		free = true; }
    queuelink() { reset(); }
    queuelink(const T& t) { reset(); data = &t; }
    queuelink(const T* t) { reset(); data = t; }
    queuelink<T>& operator ()(T* t) { reset(); data = t; return *this; }

    T& unlink() {
      // No effect if next = prev = this (i.e., unlinked)
      next->prev = prev;
      prev->next = next;
      prev = NULL;
      next = NULL;
      return *data;
    }

    void add_to_head(queuelink<T>& root) {
      addlink(&root, root.next);
    }

    void addto(queuelink<T>& root) {
      add_to_head(root);
    }

    void add_to_tail(queuelink<T>& root) {
      addlink(root.prev, &root);
    }

    queuelink<T>* remove_head() {
      queuelink<T>* link = next;
      link->unlink();
      return link;
    }

    queuelink<T>* remove_tail() {
      queuelink<T>* link = prev;
      link->unlink();
      return link;
    }

    queuelink<T>* head() const {
      return next;
    }

    queuelink<T>* tail() const {
      return prev;
    }

    bool empty() const {
      return (next == this);
    }

    bool unlinked() const {
		return ((!prev && !next));
    }

    bool linked() const {
      return !unlinked();
    }

    operator bool() const { return (!empty()); }

    T* operator->() const { return data; }
    operator T*() const { return data; }
    operator T&() const { return *data; }

  protected:
    void addlink(queuelink<T>* prev, queuelink<T>* next) {
		if(next)
			next->prev = this;
		this->next = next;
		this->prev = prev;
		if(prev)
			prev->next = this;
    }
  };

  struct FixQueueLinkObject {
	  public:
		  int idx;

		  void reset(int i) {
			  idx = i;
		  }

  };

  template <class T, int SIZE>
  class FixQueueLink {
  private:
	  int iter_count;
	  queuelink<T> *last_iter_entry;
  public:
	  queuelink<T> objects[SIZE];
	  queuelink<T> *head;
	  queuelink<T> *tail;
	  int count;
	  int size;

	  FixQueueLink() {
		  reset();
		  size = SIZE;
	  }

	  void reset() {
		  foreach(i, SIZE) {
			  objects[i].reset();
			  objects[i].data = new T();
			  objects[i].data->reset(i);
		  }
		  for(int i=0; i < SIZE; i++) {
			  objects[(i+1) % SIZE].add_to_tail(objects[i]);
		  }
		  head = tail = NULL;
		  count = 0;
		  iter_count = -1;
		  last_iter_entry = NULL;
	  }

	  T* peek() {
		  assert(head != NULL);
		  return head->data;
	  }

	  void reset_iter() {
		  iter_count = -1;
		  last_iter_entry = NULL;
	  }

	  T* iter_next() {
		  iter_count++;
		  if(iter_count >= count)
			  return NULL;

		  if(iter_count == 0) {
			  last_iter_entry = head;
		  } else {
			  last_iter_entry = last_iter_entry->next;
		  }
		  return last_iter_entry->data;
	  }

	  void add_to_tail(T* base, T* root) {
		  objects[base->idx].unlink();
		  objects[base->idx].add_to_tail(objects[root->idx]);
	  }

	  bool isFull() {
		  return count >= SIZE;
	  }

	  T* alloc() {
		  if unlikely (isFull()) return NULL;
		  if unlikely (tail == NULL) {
			  assert(head == NULL);

			  head = &objects[0];
			  tail = head;

			  head->data->init();
			  count++;
			  head->free = false;
			  return head->data;
		  }
		  tail = tail->next;
		  count++;
		  tail->free = false;
		  tail->data->init();
		  return tail->data;
	  }

	  void free(T *data) {
		  queuelink<T> *entry = &objects[data->idx];
		  entry->free = true;
		  count--;
		  if(count == 0) {
			  assert(head == tail);
			  head = NULL;
			  tail = NULL;
			  return;
		  }
		  if(head == entry) {
			  head = entry->next;
			  return;
		  }
		  if(tail == entry) {
			  tail = entry->prev;
			  return;
		  }
		  entry->unlink();
		  assert(tail != NULL);
		  entry->add_to_head(*tail);
	  }

	  void unlink(T *data) {
		  queuelink<T> *entry = &objects[data->idx];
		  count--;
		  if(count == 0) {
			  assert(head == entry);
			  assert(head == tail);
			  head = NULL;
			  tail = NULL;
			  return;
		  }
		  if(head == entry) {
				  head = entry->next;
			  return;
		  }
		  if(tail == entry) {
			  tail = entry->prev;
			  return;
		  }
		  entry->unlink();
	  }

	  void insert_before(T *data, T *root) {
		  queuelink<T> *entry = &objects[data->idx];
		  queuelink<T> *root_obj = &objects[root->idx];

		  count++;
		  entry->unlink();
		  entry->add_to_tail(*root_obj);

		  if(head == root_obj)
			  head = entry;
		  assert(head);
		  assert(tail);
	  }

	  ostream& print(ostream& os) const {
		  os << " count is ", count, " \n";
		  queuelink<T> *entry = head;
		  foreach(i, count) {
			  assert(entry != NULL);
			  entry->data->print(os);
			  entry = entry->next;
		  }
		  return os;
	  }

	  void print_all(ostream& os) {
		  queuelink<T>* entry = &objects[SIZE/2];
		  os << "Count is : ", count, endl;
		  foreach(i, SIZE) {
			  if(entry->free)
				  os << "Free[ ", entry->data->idx, "]: " ;
			  else {
				  os << "Busy: ";
				  entry->data->print(os);
			  }
			  os.flush();
			  os << "->";
			  entry = entry->next;
		  }
		  os << "::End";
	  }

	  T& operator [](size_t index) { return *(objects[index].data); }

  };

  template <class T>
  class FixQueueIter {
	  private:
		  int count;
		  queuelink<T> *last_iter_entry;
		  queuelink<T> *start_entry;
	  public:

		  FixQueueIter(queuelink<T> *entry) {
			  last_iter_entry = entry;
			  count = -1;
		  }

		  T* start() {
			  count = 0;
			  if(last_iter_entry != NULL) {
				  start_entry = last_iter_entry;
				  return last_iter_entry->data;
			  }
			  return NULL;
		  }

		  T* next() {
			  count++;
			  if(last_iter_entry->next != NULL &&
					  last_iter_entry->next != last_iter_entry) {
				  last_iter_entry = last_iter_entry->next;
				  if(last_iter_entry == start_entry) {
					  return NULL;
				  }
				  if(!last_iter_entry->free)
					  return last_iter_entry->data;
			  }
			  return NULL;

		  }
  };

#define foreach_queuelink(Q, entry, linktype) \
  linktype *entry; \
  FixQueueIter<linktype> iter(Q.head); \
  for(entry = iter.start(); \
		  entry != NULL; entry = iter.next())

  template<class T, int SIZE>
  static inline ostream& operator <<(ostream& os, const FixQueueLink<T, SIZE>& queue)
  {
	  return queue.print(os);
  }

  template <typename T, typename LM = superstl::ObjectLinkManager<T> >
  class queue: selflistlink {
  public:
    void reset() { next = this; prev = this; }
    queue() { reset(); }

    void add_to_head(selflistlink* link) { addlink(this, link, next); }
    void add_to_head(T& obj) { add_to_head(LM::linkof(&obj)); }
    void add_to_head(T* obj) { add_to_head(LM::linkof(obj)); }

    void add_to_tail(selflistlink* link) { addlink(prev, link, this); }
    void add_to_tail(T& obj) { add_to_tail(LM::linkof(&obj)); }
    void add_to_tail(T* obj) { add_to_tail(LM::linkof(obj)); }

    T* remove_head() {
      if unlikely (empty()) return NULL;
      selflistlink* link = next;
      link->unlink();
      return LM::objof(link);
    }

    T* remove_tail() {
      if unlikely (empty()) return NULL;
      selflistlink* link = prev;
      link->unlink();
      return LM::objof(link);
    }

    void enqueue(T* obj) { add_to_tail(obj); }
    T* dequeue() { return remove_head(); }

    void push(T* obj) { add_to_tail(obj); }
    void pop(T* obj) { remove_tail(); }

    T* head() const {
      return (unlikely (empty())) ? NULL : next;
    }

    T* tail() const {
      return (unlikely (empty())) ? NULL : tail;
    }

    bool empty() const { return (next == this); }

    operator bool() const { return (!empty()); }

  protected:
    void addlink(selflistlink* prev, selflistlink* link, selflistlink* next) {
      next->prev = link;
      link->next = next;
      link->prev = prev;
      prev->next = link;
    }
  };

  template <typename T, T NULLtag, int N, int Q>
  struct FixedIntegerQueueSet {
    T heads[Q];
    T tails[Q];
    T next[N];

    FixedIntegerQueueSet() { reset(); }

    void reset() {
      foreach (i, Q) { heads[i] = NULLtag; }
      foreach (i, Q) { tails[i] = NULLtag; }
      foreach (i, N) { next[i] = NULLtag; }
    }

    bool isNULL(T tag) const {
      return (tag == NULLtag);
    }

    void add(int qid, T tag) {
      T& head = heads[qid];
      T& tail = tails[qid];

      assert(isNULL(next[tag]));
      if likely (!isNULL(tail)) next[tail] = tag;
      tail = tag;
      if unlikely (isNULL(head)) head = tag;
    }

    void addhead(int qid, T tag) {
      T& head = heads[qid];
      T& tail = tails[qid];

      assert(isNULL(next[tag]));
      if unlikely (empty(qid)) {
        head = tail = tag;
        return;
      }

      next[tag] = head;
      head = tag;
    }

    T dequeue(int qid) {
      T& head = heads[qid];
      T& tail = tails[qid];

      T tag = head;
      if unlikely (isNULL(head)) return NULLtag;
      head = next[tag];
      next[tag] = NULLtag;
      if unlikely (tail == tag) tail = NULLtag;
      return tag;
    }

    void splice_into_head(int destqid, int srcqid) {
      if unlikely (empty(srcqid)) return;

      if unlikely (empty(destqid)) {
        swap(heads[destqid], heads[srcqid]);
        swap(tails[destqid], tails[srcqid]);
        return;
      }

      T& desthead = heads[destqid];
      T& desttail = tails[destqid];

      T& srchead = heads[srcqid];
      T& srctail = tails[srcqid];

      next[srctail] = desthead;
      desthead = srchead;
      desttail = desttail;
      srchead = NULLtag;
      srctail = NULLtag;
    }

    void splice_into_tail(int destqid, int srcqid) {
      if unlikely (empty(srcqid)) return;

      if unlikely (empty(destqid)) {
        swap(heads[destqid], heads[srcqid]);
        swap(tails[destqid], tails[srcqid]);
        return;
      }

      T& desthead = heads[destqid];
      T& desttail = tails[destqid];

      T& srchead = heads[srcqid];
      T& srctail = tails[srcqid];

      next[desttail] = srchead;
      desthead = desthead;
      desttail = srctail;

      srchead = NULLtag;
      srctail = NULLtag;
    }

    bool empty(int qid) const {
      return (isNULL(heads[qid]));
    }

    ostream& print(ostream& os) const {
      os << "FixedIntegerQueueSet<", sizeof(T), "-byte slots, ", N, " total slots, ", Q, " queues>:", endl;
      foreach (qid, Q) {
        if likely (empty(qid)) continue;
        os << "  Q", intstring(qid, -3), " [ head t", intstring(heads[qid], -4), " | tail t", intstring(tails[qid], -4), " ] ->";
        T tag = heads[qid];
        while (!isNULL(tag)) {
          os << " t", tag;
          tag = next[tag];
        }
        os << endl;
      }
      return os;
    }
  };

  template <typename T, T NULLtag, int N, int Q>
  ostream& operator <<(ostream& os, FixedIntegerQueueSet<T, NULLtag, N, Q> q) {
    return q.print(os);
  }

  //
  // Index References (indexrefs) work exactly like pointers but always
  // index into a specific structure. This saves considerable space and
  // can allow aliasing optimizations not possible with pointers.
  //

  template <typename T, typename P = W32, Waddr base = 0, int granularity = 1>
  struct shortptr {
    P p;

    shortptr() { }

    shortptr(const T& obj) {
      *this = obj;
    }

    shortptr(const T* obj) {
      *this = obj;
    }

    shortptr<T, P, base, granularity>& operator =(const T& obj) {
      p = (P)((((Waddr)&obj) - base) / granularity);
      return *this;
    }

    shortptr<T, P, base, granularity>& operator =(const T* obj) {
      p = (P)((((Waddr)obj) - base) / granularity);
      return *this;
    }

    T* get() const {
      return (T*)((p * granularity) + base);
    }

    T* operator ->() const {
      return get();
    }

    T& operator *() const {
      return *get();
    }

    operator T*() const { return get(); }

    shortptr<T, P, base, granularity>& operator ++() {
      (*this) = (get() + 1);
      return *this;
    }

    shortptr<T, P, base, granularity>& operator --() {
      (*this) = (get() - 1);
      return *this;
    }
  };

  template <typename T, typename P, Waddr base, int granularity>
  static inline stringbuf& operator <<(stringbuf& os, const shortptr<T, P, base, granularity>& sp) {
    return os << (T*)sp;
  }

  // NULL allowed:
  template <typename T>
  struct indexrefNULL {
    W16s index;

    indexrefNULL() { }

    indexrefNULL<T>& operator =(const T& obj) {
      index = (&obj) ? obj.index() : -1;
      return *this;
    }

    indexrefNULL<T>& operator =(const T* obj) {
      index = (obj) ? obj->index() : -1;
      return *this;
    }

    indexrefNULL<T>& operator =(int i) {
      index = i;
      return *this;
    }

    T* operator ->() const {
      return (index >= 0) ? &(get(index)) : NULL;
    }

    T& operator *() const {
      return (index >= 0) ? get(index) : *(T*)NULL;
    }

    operator T*() const { return &(get(index)); }

    T& get(int index) const;
  };

  template <typename T>
  struct indexref {
    W16s index;

    indexref() { }

    indexref<T>& operator =(const T& obj) {
      index = obj.index();
      return *this;
    }

    indexref<T>& operator =(const T* obj) {
      index = obj->index();
      return *this;
    }

    indexref<T>& operator =(int i) {
      index = i;
      return *this;
    }

    T* operator ->() const {
      return &(get(index));
    }

    T& operator *() const {
      return get(index);
    }

    operator T*() const { return &(get(index)); }

    T& get(int index) const;
  };

#define BITS_PER_WORD ((sizeof(unsigned long) == 8) ? 64 : 32)
#define BITVEC_WORDS(n) ((n) < 1 ? 0 : ((n) + BITS_PER_WORD - 1)/BITS_PER_WORD)

#ifdef __x86_64__
#define __builtin_ctzl(t) lsbindex64(t)
#define __builtin_clzl(t) msbindex64(t)
#else
#define __builtin_ctzl(t) lsbindex32(t)
#define __builtin_clzl(t) msbindex32(t)
#endif

  template<size_t N>
  struct bitvecbase {
    typedef unsigned long T;

    T w[N];

    bitvecbase() { resetop(); }

    bitvecbase(const bitvecbase<N>& vec) { foreach (i, N) w[i] = vec.w[i]; }

    bitvecbase(unsigned long long val) {
      resetop();
      w[0] = val;
    }

    static size_t wordof(size_t index) { return index / BITS_PER_WORD; }
    static size_t byteof(size_t index) { return (index % BITS_PER_WORD) / __CHAR_BIT__; }
    static size_t bitof(size_t index) { return index % BITS_PER_WORD; }
    static T maskof(size_t index) { return (static_cast<T>(1)) << bitof(index); }

    T& getword(size_t index) { return w[wordof(index)]; }
    T getword(size_t index) const { return w[wordof(index)]; }
    T& hiword() { return w[N - 1]; }
    T hiword() const { return w[N - 1]; }

    void andop(const bitvecbase<N>& x) {
      for (size_t i = 0; i < N; i++) w[i] &= x.w[i];
    }

    void orop(const bitvecbase<N>& x) {
      foreach (i, N) w[i] |= x.w[i];
    }

    void xorop(const bitvecbase<N>& x) {
      foreach (i, N) w[i] ^= x.w[i];
    }

    void shiftleftop(size_t shift) {
      if likely (shift) {
        const size_t wshift = shift / BITS_PER_WORD;
        const size_t offset = shift % BITS_PER_WORD;

        if unlikely (offset == 0) {
          for (size_t i = N - 1; i >= wshift; --i) { w[i] = w[i - wshift]; }
        } else {
          const size_t suboffset = BITS_PER_WORD - offset;
          for (size_t i = N - 1; i > wshift; --i) { w[i] = (w[i - wshift] << offset) | (w[i - wshift - 1] >> suboffset); }
          w[wshift] = w[0] << offset;
        }

        // memset(w, static_cast<T>(0), wshift);
        foreach (i, wshift) { w[i] = 0; }
      }
    }

    void shiftrightop(size_t shift) {
      if likely (shift) {
        const size_t wshift = shift / BITS_PER_WORD;
        const size_t offset = shift % BITS_PER_WORD;
        const size_t limit = N - wshift - 1;

        if unlikely (offset == 0) {
          for (size_t i = 0; i <= limit; ++i) { w[i] = w[i + wshift]; }
        } else {
          const size_t suboffset = BITS_PER_WORD - offset;
          for (size_t i = 0; i < limit; ++i) { w[i] = (w[i + wshift] >> offset) | (w[i + wshift + 1] << suboffset); }
          w[limit] = w[N-1] >> offset;
        }

        //memset(w + limit + 1, static_cast<T>(0), N - (limit + 1));
        foreach (i, N - (limit + 1)) { w[limit + 1 + i] = 0; }
      }
    }

    void maskop(size_t count) {
      T m =
        (!count) ? 0 :
        (count % BITS_PER_WORD) ? ((T(1) << bitof(count)) - T(1)) :
        T(-1);

      w[wordof(count)] &= m;

      for (size_t i = wordof(count)+1; i < N; i++) {
        w[i] = 0;
      }
    }

    void invertop() {
      foreach (i, N) w[i] = ~w[i];
    }

    void setallop() {
      foreach (i, N) w[i] = ~static_cast<T>(0);
    }

    void resetop() { memset(w, 0, N * sizeof(T)); }

    bool equalop(const bitvecbase<N>& x) const {
      T t = 0;
      foreach (i, N) { t |= (w[i] ^ x.w[i]); }
      return (t == 0);
    }

    bool nonzeroop() const {
      T t = 0;
      foreach (i, N) { t |= w[i]; }
      return (t != 0);
    }

    size_t popcountop() const {
      size_t result = 0;

      foreach (i, (int)N)
        result += popcount64(w[i]);

      return result;
    }

    unsigned long integerop() const { return w[0]; }

    void insertop(size_t i, size_t n, T v) {
      T& lw = w[wordof(i)];
      T lm = (bitmask(n) << bitof(i));
      lw = (lw & ~lm) | ((v << i) & lm);

      if unlikely ((bitof(i) + n) > BITS_PER_WORD) {
        T& hw = w[wordof(i+1)];
        T hm = (bitmask(n) >> (BITS_PER_WORD - bitof(i)));
        hw = (hw & ~hm) | ((v >> (BITS_PER_WORD - bitof(i))) & hm);
      }
    }

    void accumop(size_t i, size_t n, T v) {
      w[wordof(i)] |= (v << i);

      if unlikely ((bitof(i) + n) > BITS_PER_WORD)
        w[wordof(i+1)] |= (v >> (BITS_PER_WORD - bitof(i)));
    }

    // find index of first "1" bit starting from low end
    size_t lsbop(size_t notfound) const {
      foreach (i, N) {
        T t = w[i];
        if likely (t) return (i * BITS_PER_WORD) + __builtin_ctzl(t);
      }
      return notfound;
    }

    // find index of last "1" bit starting from high end
    size_t msbop(size_t notfound) const {
      for (int i = N-1; i >= 0; i--) {
        T t = w[i];
        if likely (t) return (i * BITS_PER_WORD) + __builtin_clzl(t);
      }
      return notfound;
    }

    // assume value is nonzero
    size_t lsbop() const {
      return lsbop(0);
    }

    // assume value is nonzero
    size_t msbop() const {
      return msbop(0);
    }

    // find the next "on" bit that follows "prev"

    size_t nextlsbop(size_t prev, size_t notfound) const {
      // make bound inclusive
      ++prev;

      // check out of bounds
      if unlikely (prev >= N * BITS_PER_WORD)
        return notfound;

      // search first word
      size_t i = wordof(prev);
      T t = w[i];

      // mask off bits below bound
      t &= (~static_cast<T>(0)) << bitof(prev);

      if likely (t != static_cast<T>(0))
        return (i * BITS_PER_WORD) + __builtin_ctzl(t);

      // check subsequent words
      i++;
      for ( ; i < N; i++ ) {
        t = w[i];
        if likely (t != static_cast<T>(0))
          return (i * BITS_PER_WORD) + __builtin_ctzl(t);
      }
      // not found, so return an indication of failure.
      return notfound;
    }
  };

  template <>
  struct bitvecbase<1> {
    typedef unsigned long T;
    T w;

    bitvecbase(void): w(0) {}
    bitvecbase(unsigned long long val): w(val) {}

    static size_t wordof(size_t index) { return index / BITS_PER_WORD; }
    static size_t byteof(size_t index) { return (index % BITS_PER_WORD) / __CHAR_BIT__; }
    static size_t bitof(size_t index) { return index % BITS_PER_WORD; }
    static T maskof(size_t index) { return (static_cast<T>(1)) << bitof(index); }

    T& getword(size_t) { return w; }
    T getword(size_t) const { return w; }
    T& hiword() { return w; }
    T hiword() const { return w; }
    void andop(const bitvecbase<1>& x) { w &= x.w; }
    void orop(const bitvecbase<1>& x)  { w |= x.w; }
    void xorop(const bitvecbase<1>& x) { w ^= x.w; }
    void shiftleftop(size_t __shift) { w <<= __shift; }
    void shiftrightop(size_t __shift) { w >>= __shift; }
    void invertop() { w = ~w; }
    void setallop() { w = ~static_cast<T>(0); }
    void resetop() { w = 0; }
    bool equalop(const bitvecbase<1>& x) const { return w == x.w; }
    bool nonzeroop() const { return (!!w); }
    size_t popcountop() const { return popcount64(w); }
    unsigned long integerop() const { return w; }
    size_t lsbop() const { return __builtin_ctzl(w); }
    size_t msbop() const { return __builtin_clzl(w); }
    size_t lsbop(size_t notfound) const { return (w) ? __builtin_ctzl(w) : notfound; }
    size_t msbop(size_t notfound) const { return (w) ? __builtin_clzl(w) : notfound; }
    void maskop(size_t count) {
      T m =
        (!count) ? 0 :
        (count < BITS_PER_WORD) ? ((T(1) << bitof(count)) - T(1)) :
        T(-1);

      w &= m;
    }

    void insertop(size_t i, size_t n, T v) {
      T m = (bitmask(n) << bitof(i));
      w = (w & ~m) | ((v << i) & m);
    }

    void accumop(size_t i, size_t n, T v) {
      w |= (v << i);
    }

    // find the next "on" bit that follows "prev"
    size_t nextlsbop(size_t __prev, size_t notfound) const {
      ++__prev;
      if unlikely (__prev >= ((size_t) BITS_PER_WORD))
        return notfound;

      T x = w >> __prev;
      if likely (x != 0)
        return __builtin_ctzl(x) + __prev;
      else
        return notfound;
    }
  };

  template <>
  struct bitvecbase<0> {
    typedef unsigned long T;

    bitvecbase() { }
    bitvecbase(unsigned long long) { }

    static size_t wordof(size_t index) { return index / BITS_PER_WORD; }
    static size_t byteof(size_t index) { return (index % BITS_PER_WORD) / __CHAR_BIT__; }
    static size_t bitof(size_t index) { return index % BITS_PER_WORD; }
    static T maskof(size_t index) { return (static_cast<T>(1)) << bitof(index); }

    T& getword(size_t) const { return *new T;  }
    T hiword() const { return 0; }
    void andop(const bitvecbase<0>&) { }
    void orop(const bitvecbase<0>&)  { }
    void xorop(const bitvecbase<0>&) { }
    void shiftleftop(size_t) { }
    void shiftrightop(size_t) { }
    void invertop() { }
    void setallop() { }
    void resetop() { }
    bool equalop(const bitvecbase<0>&) const { return true; }
    bool nonzeroop() const { return false; }
    size_t popcountop() const { return 0; }
    void maskop(size_t count) { }
    void accumop(int i, int n, T v) { }
    void insertop(int i, int n, T v) { }
    unsigned long integerop() const { return 0; }
    size_t lsbop() const { return 0; }
    size_t msbop() const { return 0; }
    size_t lsbop(size_t notfound) const { return notfound; }
    size_t msbop(size_t notfound) const { return notfound; }
    size_t nextlsbop(size_t, size_t) const { return 0; }
  };

  // Helper class to zero out the unused high-order bits in the highest word.
  template <size_t extrabits>
  struct bitvec_sanitizer {
    static void sanitize(unsigned long& val) {
      val &= ~((~static_cast<unsigned long>(0)) << extrabits);
    }
  };

  template <>
  struct bitvec_sanitizer<0> {
    static void sanitize(unsigned long) { }
  };

  template<size_t N>
  class bitvec: private bitvecbase<BITVEC_WORDS(N)> {
  private:
    typedef bitvecbase<BITVEC_WORDS(N)> base_t;
    typedef unsigned long T;

    bitvec<N>& sanitize() {
      bitvec_sanitizer<N % BITS_PER_WORD>::sanitize(this->hiword());
      return *this;
    }

  public:
    class reference {
      friend class bitvec;

      T *wp;
      T bpos;

      // left undefined
      reference();

    public:
      inline reference(bitvec& __b, size_t index) {
        wp = &__b.getword(index);
        bpos = base_t::bitof(index);
      }

      ~reference() { }

      // For b[i] = x;
      inline reference& operator =(bool x) {
        // Optimized, x86-specific way:
        if (isconst(x) & isconst(bpos)) {
          // Most efficient to just AND/OR with a constant mask:
          *wp = ((x) ? (*wp | base_t::maskof(bpos)) : (*wp & (~base_t::maskof(bpos))));
        } else {
          // Use bit set or bit reset x86 insns:
          T b1 = x86_bts(*wp, bpos);
          T b0 = x86_btr(*wp, bpos);
          *wp = (x) ? b1 : b0;
        }
        /*
        // Optimized, branch free generic way:
        *wp = (__builtin_constant_p(x)) ?
          ((x) ? (*wp | base_t::maskof(bpos)) : (*wp & (~base_t::maskof(bpos)))) :
          (((*wp) & (~base_t::maskof(bpos))) | ((static_cast<T>((x != 0))) << base_t::bitof(bpos)));
        */
        return *this;
      }

      // For b[i] = b[j];
      inline reference& operator =(const reference& j) {
        // Optimized, x86-specific way:
        // Use bit set or bit reset x86 insns:
        T b1 = x86_bts(*wp, bpos);
        T b0 = x86_btr(*wp, bpos);
        *wp = (x86_bt(*j.wp, j.bpos)) ? b1 : b0;
        /*
        // Optimized, branch free generic way:
        *wp = (__builtin_constant_p(x)) ?
          (((*(j.wp) & base_t::maskof(j.bpos))) ? (*wp | base_t::maskof(bpos)) : (*wp & (~base_t::maskof(bpos)))) :
          (((*wp) & (~base_t::maskof(bpos))) | ((static_cast<T>((((*(j.wp) & base_t::maskof(j.bpos))) != 0))) << base_t::bitof(bpos)));
        */
        return *this;
      }

      // For b[i] = 1;
      inline reference& operator++(int postfixdummy) {
        if (isconst(bpos))
          *wp |= base_t::maskof(bpos);
        else *wp = x86_bts(*wp, bpos);
        return *this;
      }

      // For b[i] = 0;
      inline reference& operator--(int postfixdummy) {
        if (isconst(bpos))
          *wp &= ~base_t::maskof(bpos);
        else *wp = x86_btr(*wp, bpos);
        return *this;
      }

      // Flips the bit
      bool operator~() const {
        //return (*(wp) & base_t::maskof(bpos)) == 0;
        return x86_btn(*wp, bpos);
      }

      // For x = b[i];
      inline operator bool() const {
        return x86_bt(*wp, bpos);
      }

      // For b[i].invert();
      inline reference& invert() {
        *wp = x86_btc(*wp, bpos);
        return *this;
      }

      bool testset() { return x86_test_bts(*wp, bpos); }
      bool testclear() { return x86_test_btr(*wp, bpos); }
      bool testinv() { return x86_test_btc(*wp, bpos); }

      bool atomicset() { return x86_locked_bts(*wp, bpos); }
      bool atomicclear() { return x86_locked_btr(*wp, bpos); }
      bool atomicinv() { return x86_locked_btc(*wp, bpos); }
    };

    friend class reference;

    bitvec() { }

    bitvec(const bitvec<N>& vec): base_t(vec) { }

    bitvec(unsigned long long val): base_t(val) { sanitize(); }

    bitvec<N>& operator&=(const bitvec<N>& rhs) {
      this->andop(rhs);
      return *this;
    }

    bitvec<N>& operator|=(const bitvec<N>& rhs) {
      this->orop(rhs);
      return *this;
    }

    bitvec<N>& operator^=(const bitvec<N>& rhs) {
      this->xorop(rhs);
      return *this;
    }

    bitvec<N>& operator <<=(int index) {
      if likely (index < (int)N) {
        this->shiftleftop(index);
        this->sanitize();
      } else this->resetop();
      return *this;
    }

    bitvec<N>& operator>>=(int index) {
      if likely (index < (int)N) {
        this->shiftrightop(index);
        this->sanitize();
      } else this->resetop();
      return *this;
    }

    bitvec<N> rotright(int index) const {
      return ((*this) >> index) | ((*this) << (N - index));
    }

    bitvec<N> rotleft(int index) const {
      return ((*this) << index) | ((*this) >> (N - index));
    }

    bitvec<N>& set(size_t index) {
      this->getword(index) |= base_t::maskof(index);
      return *this;
    }

    bitvec<N>& reset(size_t index) {
      this->getword(index) &= ~base_t::maskof(index);
      return *this;
    }

    bitvec<N>& assign(size_t index, int val) {
      if (val)
        this->getword(index) |= base_t::maskof(index);
      else
        this->getword(index) &= ~base_t::maskof(index);
      return *this;
    }

    bitvec<N>& invert(size_t index) {
      this->getword(index) ^= base_t::maskof(index);
      return *this;
    }

    bool test(size_t index) const {
      return (this->getword(index) & base_t::maskof(index)) != static_cast<T>(0);
    }

    bitvec<N>& setall() {
      this->setallop();
      this->sanitize();
      return *this;
    }

    bitvec<N>& reset() {
      this->resetop();
      return *this;
    }

    bitvec<N>& operator++(int postfixdummy) { return setall(); }
    bitvec<N>& operator--(int postfixdummy) { return reset(); }

    bitvec<N>& invert() {
      this->invertop();
      this->sanitize();
      return *this;
    }

    bitvec<N> operator ~() const { return bitvec<N>(*this).invert(); }

    reference operator [](size_t index) { return reference(*this, index); }

    bool operator [](size_t index) const { return test(index); }

    bool operator *() const { return nonzero(); }
    bool operator !() const { return iszero(); }

    unsigned long integer() const { return this->integerop(); }

    // Returns the number of bits which are set.
    size_t popcount() const { return this->popcountop(); }

    // Returns the total number of bits.
    size_t size() const { return N; }

    bool operator ==(const bitvec<N>& rhs) const { return this->equalop(rhs); }
    bool operator !=(const bitvec<N>& rhs) const { return !this->equalop(rhs); }
    bool nonzero() const { return this->nonzeroop(); }
    bool iszero() const { return !this->nonzeroop(); }
    bool allset() const { return (~(*this)).iszero(); }
    bool all() const { return allset(N); }

    bitvec<N> operator <<(size_t shift) const { return bitvec<N>(*this) <<= shift; }

    bitvec<N> operator >>(size_t shift) const { return bitvec<N>(*this) >>= shift; }

    size_t lsb() const { return this->lsbop(); }
    size_t msb() const { return this->msbop(); }
    size_t lsb(int notfound) const { return this->lsbop(notfound); }
    size_t msb(int notfound) const { return this->msbop(notfound); }
    size_t nextlsb(size_t prev, int notfound = -1) const { return this->nextlsbop(prev, notfound); }

    bitvec<N> insert(int i, int n, T v) const {
      bitvec<N> b(*this);
      b.insertop(i, n, v);
      b.sanitize();
      return b;
    }

    bitvec<N> accum(size_t i, size_t n, T v) const {
      bitvec<N> b(*this);
      b.accumop(i, n, v);
      return b;
    }

    bitvec<N> mask(size_t count) const {
      bitvec<N> b(*this);
      b.maskop(count);
      return b;
    }

    bitvec<N> operator %(size_t b) const {
      return mask(b);
    }

    bitvec<N> extract(size_t index, size_t count) const {
      return (bitvec<N>(*this) >> index) % count;
    }

    bitvec<N> operator ()(size_t index, size_t count) const {
      return extract(index, count);
    }

    bitvec<N> operator &(const bitvec<N>& y) const {
      return bitvec<N>(*this) &= y;
    }

    bitvec<N> operator |(const bitvec<N>& y) const {
      return bitvec<N>(*this) |= y;
    }

    bitvec<N> operator ^(const bitvec<N>& y) const {
      return bitvec<N>(*this) ^= y;
    }

    bitvec<N> remove(size_t index, size_t count = 1) {
      return (((*this) >> (index + count)) << index) | ((*this) % index);
    }

    template <int S> bitvec<S> subset(int i) const {
      return bitvec<S>((*this) >> i);
    }

    bitvec<N> swap(size_t i0, size_t i1) {
      bitvec<N>& v = *this;
      bool t = v[i0];
      v[i0] = v[i1];
      v[i1] = t;
      return v;
    }

    // This introduces ambiguity:
    // explicit operator unsigned long long() const { return integer(); }

    ostream& print(ostream& os) const {
      foreach (i, (int)N) {
        os << (((*this)[i]) ? '1' : '0');
      }
      return os;
    }

    stringbuf& print(stringbuf& sb) const {
      foreach (i, (int)N) {
        sb << (((*this)[i]) ? '1' : '0');
      }
      return sb;
    }

    ostream& printhl(ostream& os) const {
      for (int i = N-1; i >= 0; i--) {
        os << (((*this)[i]) ? '1' : '0');
      }
      return os;
    }

    stringbuf& printhl(stringbuf& sb) const {
      for (int i = N-1; i >= 0; i--) {
        sb << (((*this)[i]) ? '1' : '0');
      }
      return sb;
    }
  };

  //
  // Print hi-to-lo:
  //
  template <int N>
  struct hilo {
    const bitvec<N>& b;
    int bitcount;

    hilo() { }

    hilo(const bitvec<N>& b_, int bitcount_ = N): b(b_), bitcount(bitcount_) { }
  };

  template <int N>
  static inline stringbuf& operator <<(stringbuf& os, const hilo<N>& hl) {
    return hl.b.printhl(os);
  }

  template <int N>
  DeclareStringBufToStream(hilo<N>);

  template <size_t N>
  static inline ostream& operator <<(ostream& os, const bitvec<N>& v) {
    return v.print(os);
  }

  template <size_t N>
  static inline stringbuf& operator <<(stringbuf& sb, const bitvec<N>& v) {
    return v.print(sb);
  }

  template <int size, typename T>
  static inline T vec_min_index(T* list, const bitvec<size>& include) {
    int minv = limits<T>::max;
    int mini = 0;
    foreach (i, size) {
      T v = list[i];
      bool ok = (v < minv) & include[i];
      minv = (ok) ? v : minv;
      mini = (ok) ? i : mini;
    }
    return mini;
  }

  template <int size, typename T, typename I>
  static inline void vec_make_sorting_permute_map(I* permute, T* list) {
    bitvec<size> include;
    include++;

    int n = 0;
    while (*include) {
      int mini = vec_min_index<size>(list, include);
      include[mini] = 0;
      assert(n < size);
      permute[n++] = mini;
    }
  }

#undef BITVEC_WORDS
#undef BITS_PER_WORD
#undef __builtin_ctzl
#undef __builtin_clzl

  // Supports up to 262144 slots (64*64*64)
  template <int N = 262144>
  struct BitmapAllocator3Level {
    typedef bitvec<64> bitmap_t;

    bitmap_t level3[1];
    bitmap_t level2[N / 4096];
    bitmap_t level1[N / 64];
    int highest_count;

    void reset(size_t sizelimit = N) {
      assert(sizelimit >= 4096);
      assert(sizelimit <= 262144);
      // size must be a multiple of 64*64 = 4096
      assert(lowbits(sizelimit, 12) == 0);
      size_t highbits = sizelimit >> 12;
      level3[0] = (bitmap_t()++) % highbits;
      foreach (i, lengthof(level2)) { level2[i]++; }
      foreach (i, lengthof(level1)) { level1[i]++; }
      highest_count = 0;
    }

    int update_highest_count() {
      highest_count = 0;

      for (int i = lengthof(level1)-1; i >= 0; i--) {
        bitmap_t v = ~level1[i];
        if unlikely (v) {
          highest_count = (i * 64) + v.msb() + 1;
          break;
        }
      }
      return highest_count;
    }

    //
    // Note: This is not MT-safe: external locking
    // is needed around this function.
    //
    W64s alloc(W64 index) {
      //
      //     17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
      // L3: oo oo oo oo oo oo
      // L2: ss ss ss ss ss ss oo oo oo oo oo oo
      // L1: ss ss ss ss ss ss ss ss ss ss ss ss oo oo oo oo oo oo
      //

      W64 slot1 = bits(index, 6, 12);
      W64 offset1 = bits(index, 0, 6);
      //assert(level1[slot1][offset1]); // make sure it's currently free
      level1[slot1][offset1] = 0;

      W64 slot2 = bits(index, 12, 6);
      W64 offset2 = bits(index, 6, 6);
      if unlikely (!level1[slot1]) level2[slot2][offset2] = 0;

      W64 slot3 = 0; // a.k.a. bits(index, 18, 6);
      W64 offset3 = bits(index, 12, 6);
      if unlikely (!level2[slot2]) level3[slot3][offset3] = 0;

      highest_count = max(highest_count, int(index+1));

      return index;
    }

    W64s alloc() {
      bitmap_t map;

      map = level3[0];
      if unlikely (!map) return -1;
      W64 offset3 = map.lsb();

      map = level2[offset3];
      if unlikely (!map) return -1;
      W64 offset2 = map.lsb();

      map = level1[(offset3 << 6) + offset2];
      if unlikely (!map) return -1;
      W64 offset1 = map.lsb();

      W64 index = (offset3 << 12) + (offset2 << 6) + offset1;

      return alloc(index);
    }

    void free(W64 index) {
      W64 slot1 = bits(index, 6, 12);
      W64 offset1 = bits(index, 0, 6);
      level1[slot1][offset1] = 1;

      W64 slot2 = bits(index, 12, 6);
      W64 offset2 = bits(index, 6, 6);
      if unlikely (*level1[slot1]) level2[slot2][offset2] = 1;

      W64 slot3 = 0; // a.k.a. bits(index, 18, 6);
      W64 offset3 = bits(index, 12, 6);
      if unlikely (*level2[slot2]) level3[slot3][offset3] = 1;
    }

    bool isfree(W64 index) const {
      bool freebit = level1[index >> 6][index & 0x3f];
      return freebit;
    }

    bool isused(W64 index) const {
      return (!isfree(index));
    }

    ostream& print(ostream& os) const {
      os << "FreeBitmap3Level:", endl;
      os << "  L3:"; foreach (i, lengthof(level3)) os << ' ', level3[i], ((i % 4 == 3) ? '\n' : ' '); os << endl;
      os << "  L2:"; foreach (i, lengthof(level2)) os << ' ', level2[i], ((i % 4 == 3) ? '\n' : ' '); os << endl;
      os << "  L1:"; foreach (i, lengthof(level1)) os << ' ', level1[i], ((i % 4 == 3) ? '\n' : ' '); os << endl;
      return os;
    }
  };

  //
  // Convenient list iterator
  //
#define foreachlink(list, type, iter) \
  for (type* iter = (type*)((list)->first); (iter != NULL); prefetch(iter->next), iter = (type*)(iter->next)) \

  template <typename K, typename T>
  struct KeyValuePair {
    T value;
    K key;
  };

  template <typename K, int setcount>
  struct HashtableKeyManager {
    static inline int hash(const K& key);
    static inline bool equal(const K& a, const K& b);
    static inline K dup(const K& key);
    static inline void free(K& key);
  };

  template <int setcount>
  struct HashtableKeyManager<W64, setcount> {
    static inline int hash(W64 key) {
      return foldbits<log2(setcount)>(key);
    }

    static inline bool equal(W64 a, W64 b) { return (a == b); }
    static inline W64 dup(W64 key) { return key; }
    static inline void free(W64 key) { }
  };

  template <int setcount>
  struct HashtableKeyManager<const char*, setcount> {
    static inline int hash(const char* key) {
      int len = strlen(key);
      CRC32 h;
      for (int i = 0 ; i < len ; i++) { h << key[i]; }
      return h;
    }

    static inline bool equal(const char* a, const char* b) {
      return (strcmp(a, b) == 0);
    }

    static inline const char* dup(const char* key) {
      return strdup(key);
    }

    static inline void free(const char* key) {
      ::free((void*)key);
    }
  };

  template <typename T, typename K>
  struct HashtableLinkManager {
    static inline T* objof(selflistlink* link);
    static inline K& keyof(T* obj);
    static inline selflistlink* linkof(T* obj);
    //
    // Example:
    //
    // T* objof(selflistlink* link) {
    //   return baseof(T, hashlink, link); // a.k.a. *(T*)((byte*)link) - offsetof(T, hashlink);
    // }
    //
  };

  template <typename K, typename T, int setcount = 64, typename LM = ObjectLinkManager<T>, typename KM = HashtableKeyManager<K, setcount> >
  struct SelfHashtable {
  protected:
    selflistlink* sets[setcount];
  public:
    int count;

    T* get(const K& key) {
      selflistlink* tlink = sets[lowbits(KM::hash(key), log2(setcount))];
      while (tlink) {
        T* obj = LM::objof(tlink);
        if likely (KM::equal(LM::keyof(obj), key)) return obj;
        tlink = tlink->next;
      }

      return NULL;
    }

    struct Iterator {
      SelfHashtable<K, T, setcount, LM, KM>* ht;
      selflistlink* link;
      int slot;

      Iterator() { }

      Iterator(SelfHashtable<K, T, setcount, LM, KM>* ht) {
        reset(ht);
      }

      Iterator(SelfHashtable<K, T, setcount, LM, KM>& ht) {
        reset(ht);
      }

      void reset(SelfHashtable<K, T, setcount, LM, KM>* ht) {
        this->ht = ht;
        slot = 0;
        link = ht->sets[slot];
      }

      void reset(SelfHashtable<K, T, setcount, LM, KM>& ht) {
        reset(&ht);
      }

      T* next() {
        for (;;) {
          if unlikely (slot >= setcount) return NULL;

          if unlikely (!link) {
            // End of chain: advance to next chain
            slot++;
            if unlikely (slot >= setcount) return NULL;
            link = ht->sets[slot];
            continue;
          }

          T* obj = LM::objof(link);
          link = link->next;
          prefetch(link);
          return obj;
        }
      }
    };

    dynarray<T*>& getentries(dynarray<T*>& a) {
      a.resize(count);
      int n = 0;
      Iterator iter(this);
      T* t;
      while ((t = iter.next())) {
        assert(n < count);
        a[n++] = t;
      }
      return a;
    }

    SelfHashtable() {
      reset();
    }

    void reset() {
      count = 0;
      foreach (i, setcount) { sets[i] = NULL; }
    }

    void clear(bool free_after_remove = false) {
      foreach (i, setcount) {
        selflistlink* tlink = sets[i];
        while (tlink) {
          selflistlink* tnext = tlink->next;
          tlink->unlink();
          if unlikely (free_after_remove) {
            T* obj = LM::objof(tlink);
            delete obj;
          }
          tlink = tnext;
        }
        sets[i] = NULL;
      }
      count = 0;
    }

    void clear_and_free() {
      clear(true);
    }

    T* operator ()(const K& key) {
      return get(key);
    }

    T* add(T* obj) {
      T* oldobj = get(LM::keyof(obj));
      if unlikely (oldobj) {
        remove(oldobj);
      }

      if (LM::linkof(obj)->linked()) return obj;

      LM::linkof(obj)->addto(sets[lowbits(KM::hash(LM::keyof(obj)), log2(setcount))]);
      count++;
      return obj;
    }

    T& add(T& obj) {
      return *add(&obj);
    }

    T* remove(T* obj) {
      selflistlink* link = LM::linkof(obj);
      if (!link->linked()) return obj;
      link->unlink();
      count--;
      return obj;
    }

    T& remove(T& obj) {
      return *remove(&obj);
    }

    ostream& print(ostream& os) const {
      os << "Hashtable of ", setcount, " sets containing ", count, " entries:", endl;
      foreach (i, setcount) {
        selflistlink* tlink = sets[i];
        if (!tlink)
          continue;
        os << "  Set ", i, ":", endl;
        int n = 0;
        while likely (tlink) {
          T* obj = LM::objof(tlink);
          os << "    ", LM::keyof(obj), " -> ", *obj, endl;
          tlink = tlink->next;
          n++;
        }
      }
      return os;
    }
  };

  template <typename K, typename T, typename LM, int setcount, typename KM>
  static inline ostream& operator <<(ostream& os, const SelfHashtable<K, T, setcount, LM, KM>& ht) {
    return ht.print(os);
  }

  template <typename K, typename T, typename KM>
  struct ObjectHashtableEntry: public KeyValuePair<K, T> {
    typedef KeyValuePair<K, T> base_t;
    selflistlink hashlink;

    ObjectHashtableEntry() { }

    ObjectHashtableEntry(const K& key, const T& value) {
      this->value = value;
      this->key = KM::dup(key);
    }

    ~ObjectHashtableEntry() {
      hashlink.unlink();
      KM::free(this->key);
    }
  };

  template <typename K, typename T, typename KM>
  struct ObjectHashtableLinkManager {
    typedef ObjectHashtableEntry<K, T, KM> entry_t;

    static inline entry_t* objof(selflistlink* link) {
      return baseof(entry_t, hashlink, link);
    }

    static inline K& keyof(entry_t* obj) {
      return obj->key;
    }

    static inline selflistlink* linkof(entry_t* obj) {
      return &obj->hashlink;
    }
  };

  template <typename K, typename T, int setcount = 64, typename KM = HashtableKeyManager<K, setcount> >
  struct Hashtable: public SelfHashtable<K, ObjectHashtableEntry<K, T, KM>, setcount, ObjectHashtableLinkManager<K, T, KM> > {
    typedef ObjectHashtableEntry<K, T, KM> entry_t;
    typedef SelfHashtable<K, entry_t, setcount, ObjectHashtableLinkManager<K, T, KM> > base_t;

    struct Iterator: public base_t::Iterator {
      Iterator() { }

      Iterator(Hashtable<K, T, setcount, KM>* ht) {
        reset(ht);
      }

      Iterator(Hashtable<K, T, setcount, KM>& ht) {
        reset(ht);
      }

      void reset(Hashtable<K, T, setcount, KM>* ht) {
        base_t::Iterator::reset(ht);
      }

      void reset(Hashtable<K, T, setcount, KM>& ht) {
        base_t::Iterator::reset(&ht);
      }

      KeyValuePair<K, T>* next() {
        return base_t::Iterator::next();
      }
    };

    dynarray< KeyValuePair<K, T> >& getentries(dynarray< KeyValuePair<K, T> >& a) {
      a.resize(base_t::count);
      int n = 0;
      Iterator iter(this);
      KeyValuePair<K, T>* kvp;
      while ((kvp = iter.next())) {
        assert(n < base_t::count);
        a[n++] = *kvp;
      }
      return a;
    }

    T* get(const K& key) {
      entry_t* entry = base_t::get(key);
      return &entry->value;
    }

    T* operator ()(const K key) {
      return get(key);
    }

    T* add(const K& key, const T& value) {
      entry_t* entry = base_t::get(key);
      if unlikely (entry) {
        entry->value = value;
        return &entry->value;
      }

      entry = new entry_t(key, value);
      base_t::add(entry);
      return &entry->value;
    }

    bool remove(const K& key, T& value) {
      entry_t* entry = base_t::get(key);
      if unlikely (!entry) return false;

      value = entry->value;
      base_t::remove(entry);
      delete entry;
      return true;
    }

    bool remove(const K key) {
      T dummy;
      return remove(key, dummy);
    }

    ostream& print(ostream& os) {
      os << "Hashtable of ", setcount, " sets containing ", base_t::count, " entries:", endl;
      Iterator iter;
      iter.reset(this);
      KeyValuePair<K, T>* kvp;
      while (kvp = iter.next()) {
        os << "  ", kvp->key, " -> ", kvp->value, endl;
      }
      return os;
    }
  };

  template <typename K, typename T, int setcount, typename KM>
  static inline ostream& operator <<(ostream& os, const Hashtable<K, T, setcount, KM>& ht) {
    return ((Hashtable<K, T, setcount, KM>&)ht).print(os);
  }

  template <typename T, int N, int setcount>
  struct FixedValueHashtable {
    typedef int ptr_t;

    T data[N];
    ptr_t next[N];

    ptr_t sets[setcount];
    int count;

    FixedValueHashtable() {
      reset();
    }

    void reset() {
      count = 0;
      memset(sets, 0xff, sizeof(sets));
    }

    static int setof(T value) {
      return foldbits<log2(setcount)>(value);
    }

    int lookup(T value) const {
      int set = setof(value);
      ptr_t slot = sets[set];
      while (slot >= 0) {
        if unlikely (data[slot] == value) return slot;
        slot = next[slot];
      }
      return -1;
    }

    bool remaining() const { return (N - count); }
    bool full() const { return (!remaining()); }
    bool empty() const { return (!count); }

    ptr_t add(T value) {
      int set = setof(value);
      ptr_t slot = lookup(value);
      if likely (slot >= 0) return slot;

      if unlikely (slot >= N) return -1;
      slot = count++;
      data[slot] = value;
      next[slot] = sets[set];
      sets[set] = slot;
      return slot;
    }

    bool contains(T value) const {
      return (lookup(value) >= 0);
    }

    inline T& operator [](int i) { return data[i]; }
    inline T operator [](int i) const { return data[i]; }
    operator T*() const { return data; }
    operator const T*() const { return data; }

    ostream& print(ostream& os) const {
      os << "FixedValueHashtable<", sizeof(T), "-byte data, ", N, " slots, ", setcount, " sets> containing ", count, " entries:", endl;
      foreach (i, count) {
        T v = data[i];
        os << "  ", intstring(i, 4), ": ", v, endl;
      }
      return os;
    }
  };

  template <typename T, int N, int setcount>
  ostream& operator <<(ostream& os, const FixedValueHashtable<T, N, setcount>& ht) {
    return ht.print(os);
  }

  template <typename Tkey, typename Tdata, int N, int setcount>
  struct FixedKeyValueHashtable {
    typedef int ptr_t;

    Tkey keys[N];
    Tdata data[N];
    ptr_t next[N];

    ptr_t sets[setcount];
    int count;

    FixedKeyValueHashtable() {
      reset();
    }

    void reset() {
      count = 0;
      memset(sets, 0xff, sizeof(sets));
    }

    static int setof(Tkey key) {
      return foldbits<log2(setcount)>(key);
    }

    int lookup(Tkey key) const {
      int set = setof(key);
      ptr_t slot = sets[set];
      while (slot >= 0) {
        if unlikely (keys[slot] == key) return slot;
        slot = next[slot];
      }
      return -1;
    }

    bool remaining() const { return (N - count); }
    bool full() const { return (!remaining()); }
    bool empty() const { return (!count); }

    ptr_t add(Tkey key, Tdata value) {
      int set = setof(key);
      ptr_t slot = lookup(key);
      if likely (slot >= 0) return slot;

      if unlikely (slot >= N) return -1;
      slot = count++;
      keys[slot] = key;
      data[slot] = value;
      next[slot] = sets[set];
      sets[set] = slot;
      return slot;
    }

    Tdata get(Tkey key) const {
      int slot = lookup(key);
      if unlikely (slot < 0) return Tdata(0);
      return data[slot];
    }

    inline Tdata operator ()(Tkey key) { return get(key); }

    inline Tdata& operator [](int i) { return data[i]; }
    inline Tdata operator [](int i) const { return data[i]; }
    operator Tdata*() const { return data; }
    operator const Tdata*() const { return data; }

    ostream& print(ostream& os) const {
      os << "FixedKeyValueHashtable<", sizeof(Tkey), "-byte key, ", sizeof(Tdata), "-byte data, ", N, " slots, ", setcount, " sets> containing ", count, " entries:", endl;
      foreach (i, count) {
        Tkey k = keys[i];
        Tdata v = data[i];
        os << "  ", intstring(i, 4), ": ", k, " => ", v, endl;
      }
      return os;
    }
  };

  template <typename Tkey, typename Tdata, int N, int setcount>
  ostream& operator <<(ostream& os, const FixedKeyValueHashtable<Tkey, Tdata, N, setcount>& ht) {
    return ht.print(os);
  }

  template <typename T, int N>
  struct ChunkList {
    struct Chunk;

    struct Chunk {
      selflistlink link;
      bitvec<N> freemap;

      // Formula: (CHUNK_SIZE - sizeof(ChunkHeader<T>)) / sizeof(T);
      T data[N];

      Chunk() { link.reset(); freemap++; }

      bool full() const { return (!freemap); }
      bool empty() const { return freemap.allset(); }

      int add(const T& entry) {
        if unlikely (full()) return -1;
        int idx = freemap.lsb();
        freemap[idx] = 0;
        data[idx] = entry;
        return idx;
      }

      bool contains(T* entry) const {
        int idx = entry - data;
        return ((idx >= 0) & (idx < lengthof(data)));
      }

      bool remove(int idx) {
        data[idx] = 0;
        freemap[idx] = 1;

        return empty();
      }

      struct Iterator {
        Chunk* chunk;
        size_t i;

        Iterator() { }

        Iterator(Chunk* chunk_) {
          reset(chunk_);
        }

        void reset(Chunk* chunk_) {
          this->chunk = chunk_;
          i = 0;
        }

        T* next() {
          for (;;) {
            if unlikely (i >= lengthof(chunk.data)) return NULL;
            if unlikely (chunk->freemap[i]) { i++; continue; }
            return &chunk->data[i++];
          }
        }
      };

      int getentries(T* a, int limit) {
        Iterator iter(this);
        T* entry;
        int n = 0;
        while (entry = iter.next()) {
          if unlikely (n >= limit) return n;
          a[n++] = *entry;
        }

        return n;
      }
    };

    struct Locator {
      Chunk* chunk;
      int index;

      void reset() { chunk = NULL; index = 0; }
    };

    selflistlink* head;
    int elemcount;

    ChunkList() { head = NULL; elemcount = 0; }

    bool add(const T& entry, Locator& hint) {
      Chunk* chunk = (Chunk*)head;

      while (chunk) {
        prefetch(chunk->link.next);
        int index = chunk->add(entry);
        if likely (index >= 0) {
          hint.chunk = chunk;
          hint.index = index;
          elemcount++;
          return true;
        }
        chunk = (Chunk*)chunk->link.next;
      }

      Chunk* newchunk = new Chunk();
      newchunk->link.addto(head);

      int index = newchunk->add(entry);
      assert(index >= 0);

      hint.chunk = newchunk;
      hint.index = index;
      elemcount++;

      return true;
    }

    bool remove(const Locator& locator) {
      locator.chunk->remove(locator.index);
      elemcount--;

      if (locator.chunk->empty()) {
        locator.chunk->link.unlink();
        delete locator.chunk;
      }

      return empty();
    }

    void clear() {
      Chunk* chunk = (Chunk*)head;

      while (chunk) {
        Chunk* next = (Chunk*)chunk->link.next;
        prefetch(next);
        delete chunk;
        chunk = next;
      }

      elemcount = 0;
      head = NULL;
    }

    int count() { return elemcount; }

    bool empty() { return (elemcount == 0); }

    ~ChunkList() {
      clear();
    }

    struct Iterator {
      Chunk* chunk;
      Chunk* nextchunk;
      int i;

      Iterator() { }

      Iterator(ChunkList<T, N>* chunklist) {
        reset(chunklist);
      }

      void reset(ChunkList<T, N>* chunklist) {
        chunk = (Chunk*)chunklist->head;
        nextchunk = (chunk) ? (Chunk*)chunk->link.next : NULL;
        i = 0;
      }

      T* next() {
        for (;;) {
          if unlikely (!chunk) return NULL;

          if unlikely (i >= lengthof(chunk->data)) {
            chunk = nextchunk;
            if unlikely (!chunk) return NULL;
            nextchunk = (Chunk*)chunk->link.next;
            prefetch(nextchunk);
            i = 0;
          }

          if unlikely (chunk->freemap[i]) { i++; continue; }

          return &chunk->data[i++];
        }
      }
    };

    int getentries(T* a, int limit) {
      Iterator iter(this);
      T* entry;
      int n;
      while (entry = iter.next()) {
        if unlikely (n >= limit) return n;
        a[n++] = *entry;
      }

      return n;
    }
  };

  // Fast vectorized method: empty only if all slots are literally zero
  static inline bool bytes_are_all_zero(const void* ptr, size_t bytes) {
    // Fast vectorized method: empty only if all slots are literally zero
    const W64* p = (const W64*)ptr;
    W64 v = 0;
    foreach (i, (int)(bytes/8)) v |= p[i];
    if unlikely (v % 8) {
      v |= (p[(bytes/8)] & bitmask((bytes % 8)*8));
    }
    return (v == 0);
  }

  //
  // Chunk list for storing objects without tags.
  // The NULL object is considered all zeros; this
  // works for both pointers and integers.
  //
  // By default this is 64 bytes (1 cache line), enough
  // room to fit 15 shortptrs and a next pointer.
  //
  // Adds to a chunk are thread safe, and removes should
  // also be thread safe so long as only one caller
  // tries to remove the same object at any given time;
  // this is generally the case by design for most
  // applications of chunk lists.
  //
  template <typename T, int N = 15>
  struct GenericChunkList {
    T list[N];
    shortptr< GenericChunkList<T, N> > next;

    GenericChunkList() {
      setzero(*this);
    }

    // Thread safe as long as removes are appropriately serialized:
    T* get(T obj) const {
      foreach (i, lengthof(list)) {
        T& p = list[i];
        if unlikely (p == obj) return &p;
      }

      return NULL;
    }

    T* add(T obj) {
      foreach (i, lengthof(list)) {
        T& p = list[i];
        if unlikely (!p) {
          // thread safe: check for race:
          T oldp = xchg(p, obj);
          if (oldp) continue;
          return &p;
        }
      }
      return NULL;
    }

    // NOT thread safe:
    T* addunique(T obj) {
      T* np = NULL;
      for (int i = lengthof(list)-1; i >= 0; i--) {
        T& p = list[i];
        if unlikely (p == obj) return &p;
        if unlikely (!p) np = &p;
      }
      if unlikely (!np) return NULL;
      *np = obj;
      return np;
    }

    // Thread safe
    T* remove(T obj) {
      foreach (i, lengthof(list)) {
        T& p = list[i];
        if likely (p != obj) continue;
        T old = cmpxchg(p, T(0), obj);
        if (old == obj) return &p;
      }

      return NULL;
    }

    // Not thread safe: should only be called during RCU-like update
    bool empty() const {
      return bytes_are_all_zero(&list, sizeof(list));
    }

    ostream& print(ostream& os) const {
      os << "  Chunk ", this, ":", endl;
      foreach (i, lengthof(list)) {
        const T& entry = list[i];
        if unlikely (!entry) continue;
        os << "    slot ", intstring(i, 2), ": ", entry, endl;
      }

      return os;
    }

    struct Iterator {
      GenericChunkList<T>* chunk;
      int slot;

      Iterator() { }

      Iterator(GenericChunkList<T>* chunk) { reset(chunk); }
      Iterator(GenericChunkList<T>& chunk) { reset(chunk); }

      void reset(GenericChunkList<T>* chunk) {
        if likely (chunk) prefetch(chunk->next);
        this->chunk = chunk;
        slot = 0;
      }

      void reset(GenericChunkList<T>& chunk) { reset(&chunk); }

      T* next() {
        for (;;) {
          if unlikely (!chunk) return NULL;

          if unlikely (slot >= lengthof(chunk->list)) {
            reset(chunk->next);
            continue;
          }

          T& obj = chunk->list[slot++];
          if unlikely (!obj) continue;
          return &obj;
        }
      }
    };
  };

  template <typename T>
  ostream& operator <<(ostream& os, const GenericChunkList<T>& cl) {
    return cl.print(os);
  }

  //
  // Searchable chunk list, comprised of N data elements,
  // a 16-byte tag field (with N valid bytes), a 4-byte
  // next pointer, a 2-byte valid bitmap and padding.
  //
  // The low 8 bits (or some other derivation) of the
  // key of each data element are encoded into tags,
  // so we can do vectorized single-cycle lookups
  // against all N tags in a single cycle and quickly
  // identify any (probably) matching entries.
  //
  template <typename T, int bytes = 128>
  struct SearchableChunkList16Entry {
    static const int N = (bytes - (16+4+2+2)) / sizeof(T);
    vec16b tags;
    shortptr<SearchableChunkList16Entry<T>, W32, 0> next;
    W16 valid;
    W16 pad;
    T list[N];

    void reset() {
      valid = 0;
      next = NULL;
    }

    SearchableChunkList16Entry() {
      reset();
    }

    // Implemented by instantiated template:
    byte tagof(const T& target) const;

    // Implemented by instantiated template:
    bool equal(const T& target, const T& e) const;

    T* get(const T& target) {
      W32 matches = x86_sse_maskeqb(tags, tagof(target)) & valid;
      if likely (!matches) return NULL;
      while likely (matches) {
        int index = lsbindex32(matches);
        matches = x86_btr(matches, W32(index));
        T* p = &list[index];
        if likely (equal(target, *p)) return p;
      }
      return NULL;
    }

    T* add(const T& e) {
      T* p = get(e);
      if unlikely (p) return p;
      if unlikely (full()) return NULL;

      int index = lsbindex32(~valid);
      assert(index < lengthof(list));
      valid = x86_bts(W32(valid), W32(index));
      p = &list[index];
      *p = e;

      ((byte*)&tags)[index] = tagof(e);

      return p;
    }

    T* remove(const T& e) {
      T* p = get(e);
      if unlikely (!p) return NULL;

      int index = p - list;
      valid = x86_btr(W32(valid), W32(index));

      return p;
    }

    bool full() const { return (valid == bitmask(lengthof(list))); }
    bool empty() const { return (valid == 0); }

    ostream& print(ostream& os) const {
      os << "SearchableChunkList16Entry<", N, " total ", sizeof(T), "-byte entries, ", bytes, "-byte chunk:", endl;
      os << "  Tags: ", bytemaskstring((byte*)&tags, valid, N), endl;
      foreach (i, N) {
        if likely (!bit(valid, i)) continue;
        os << "  slot ", intstring(i, 2), ": ", list[i], endl;
      }
      return os;
    }

    struct Iterator {
      typedef SearchableChunkList16Entry<T, bytes> chunk_t;
      chunk_t* chunk;
      int slot;

      Iterator() { }

      Iterator(chunk_t* chunk) { reset(chunk); }
      Iterator(chunk_t& chunk) { reset(chunk); }

      void reset(chunk_t* chunk) {
        this->chunk = chunk;
        slot = 0;
      }

      void reset(chunk_t& chunk) { reset(&chunk); }

      T* next() {
        for (;;) {
          if unlikely (slot >= lengthof(chunk->list)) return NULL;
          T* entry = &list[slot++];
          if unlikely (!bit(valid, slot)) continue;
          return entry;
        }
      }
    };
  };

  template <typename T, int bytes>
  ostream& operator <<(ostream& os, const SearchableChunkList16Entry<T, bytes>& chunk) {
    return chunk.print(os);
  }

  //
  // sort - sort an array of elements
  // @p: pointer to data to sort
  // @n: number of elements
  //
  // This function does a heapsort on the given array. You may provide a
  // comparison function optimized to your element type.
  //
  // Sorting time is O(n log n) both on average and worst-case. While
  // qsort is about 20% faster on average, it suffers from exploitable
  // O(n*n) worst-case behavior and extra memory requirements that make
  // it less suitable for kernel use.
  //
  template <typename T>
  struct DefaultComparator {
    int operator ()(const T& a, const T& b) const {
      int r = (a < b) ? -1 : +1;
      if (a == b) r = 0;
      return r;
    }
  };

  template <typename T, bool backwards = 0>
  struct SortPrecomputedIndexListComparator {
    const T* v;

    SortPrecomputedIndexListComparator(const T* values): v(values) { }

    int operator ()(unsigned long a, unsigned long b) const {
      //
      // This strange construction helps the compiler do better peephole
      // optimization tricks using conditional moves when using integers.
      //
      if (backwards) {
        int r = (v[a] > v[b]) ? -1 : +1;
        if (v[a] == v[b]) r = 0;
        return r;
      } else {
        int r = (v[a] < v[b]) ? -1 : +1;
        if (v[a] == v[b]) r = 0;
        return r;
      }
    }
  };

  template <typename T, bool backwards = 0>
  struct PointerSortComparator {
    PointerSortComparator() { }

    int operator ()(T* a, T* b) const {
      //
      // This strange construction helps the compiler do better peephole
      // optimization tricks using conditional moves when using integers.
      //
      const T& aa = *a;
      const T& bb = *b;
      if (backwards) {
        int r = (aa > bb) ? -1 : +1;
        if (aa == bb) r = 0;
        return r;
      } else {
        int r = (aa < bb) ? -1 : +1;
        if (aa == bb) r = 0;
        return r;
      }
    }
  };

  template <typename T, typename Comparator>
  void sort(T* p, int n, const Comparator& compare = DefaultComparator<T>()) {
    int c;

    // heapify
    for (int i = (n/2); i >= 0; i--) {
      for (int r = i; r * 2 < n; r = c) {
        c = r * 2;
        c += ((c < (n - 1)) && (compare(p[c], p[c+1]) < 0));
        if (compare(p[r], p[c]) >= 0) break;
        swap(p[r], p[c]);
      }
    }

    // sort
    for (int i = n-1; i >= 0; i--) {
      swap(p[0], p[i]);
      for (int r = 0; r * 2 < i; r = c) {
        c = r * 2;
        c += ((c < i-1) && (compare(p[c], p[c+1]) < 0));
        if (compare(p[r], p[c]) >= 0) break;
        swap(p[r], p[c]);
      }
    }
  }

  template <typename T, typename Comparator>
  int search_sorted(const T* p, int n, const T& key, const Comparator& compare = DefaultComparator<T>()) {
    if unlikely (!n) return -1;

    int lower = 0;
    int upper = n-1;

    while (lower != upper) {
      int index = (lower + upper) / 2;
      int dir = compare(key, p[index]);
#if 1
      //
      // Branching version:
      //

      if (dir > 0) {
        // right partition
        if likely (index < n-1) lower = index+1;
      } else if (dir < 0) {
        // left partition
        if likely (index > 0) upper = index-1;
      } else {
        // match
        return index;
      }
#else
      //
      // Branch-free version (faster on some chips)
      //
      bool right = ((dir > 0) & (index < n-1));
      bool left = ((dir < 0) & (index > 0));
      lower = select(right, lower, index + 1);
      upper = select(left, upper, index - 1);
      if unlikely (!dir) return index;
#endif
    }

    return (compare(key, p[lower]) == 0) ? lower : -1;
  }

  template <typename T, typename S>
  void subtract_structures(S& d, const S& a, const S& b) {
    const T* ap = (const T*)&a;
    const T* bp = (const T*)&b;
    T* dp = (T*)&d;
    assert((sizeof(S) % sizeof(T)) == 0);
    int n = sizeof(S) / sizeof(T);
    foreach (i, n) {
      dp[i] = ap[i] - bp[i];
    }
  }

  template <typename T, typename S>
  T sum_structure_fields(const S& a) {
    assert((sizeof(S) % sizeof(T)) == 0);
    const T* ap = (const T*)&a;
    int n = sizeof(S) / sizeof(T);
    T s = 0;
    foreach (i, n) {
      s += ap[i];
    }
    return s;
  }

  //
  // Simple parameterized array comparator:
  //
  template <typename T>
  int cmpeq_arrays(const T* a, const T* b, int n) {
    foreach (i, n) {
      if unlikely (a[i] != b[i]) return i;
    }
    return n;
  }

  //
  // Detect repeated patterns in an array
  //
  template <typename T>
  bool detect_repeated_pattern(const T* list, int n, int& pattern_length, int& repeat_count, int& remaining_count) {
    pattern_length = 1;
    repeat_count = 1;
    remaining_count = 0;

    T first = list[0];
    for (int i = 1; i < n; i++) {
      if unlikely (list[i] == first) {
        pattern_length = i;
        break;
      }
    }

    if likely (pattern_length >= n) return false;

    int start = pattern_length;

    for (;;) {
      if unlikely ((n - start) < pattern_length) {
        remaining_count = n - start;
        break;
      }

      int matchlen = cmpeq_arrays(list, list + start, pattern_length);
      if unlikely (matchlen < pattern_length) {
        remaining_count = n - start;
        break;
      }

      repeat_count++;
      start += pattern_length;
    }

    // Do not allow a single "repeat" of a 1-block pattern:
    if likely ((repeat_count == 1) && (pattern_length == 1)) return false;

    return true;
  }

  static inline W64s expandword(const byte*& p, int type) {
    W64s v;

    switch (type) {
    case 0:
      return 0;
    case 1:
      v = *((W8s*)p);
      p += 1;
      return v;
    case 2:
      v = *((W16s*)p);
      p += 2;
      return v;
    case 3:
      v = *((W32s*)p);
      p += 4;
      return v;
    case 4: // signed or unsigned W64
      v = *((W64s*)p);
      p += 8;
      return v;
    case 5: // unsigned byte
      v = *((byte*)p);
      p += 1;
      return v;
    case 6: // unsigned W16
      v = *((W16*)p);
      p += 2;
      return v;
    case 7: // unsigned W32
      v = *((W32*)p);
      p += 4;
      return v;
    }

    return v;
  }

  static inline int compressword(byte*& p, W64s v) {
    int f;

    if likely (!v) {
      f = 0;
    } else if (v >= 0) {
      if (inrange(v, 0LL, 255LL)) {
        *((byte*)p) = bits(v, 0, 8);
        p += 1;
        f = 5;
      } else if (inrange(v, 0LL, 65535LL)) {
        *((W16*)p) = bits(v, 0, 16);
        p += 2;
        f = 6;
      } else if (inrange(v, 0LL, 4294967295LL)) {
        *((W32*)p) = bits(v, 0, 32);
        p += 4;
        f = 7;
      } else {
        // default to W64:
        *((W64*)p) = v;
        p += 8;
        f = 4;
      }
    } else {
      if (inrange(v, -128LL, 127LL)) {
        *((byte*)p) = bits(v, 0, 8);
        p += 1;
        f = 1;
      } else if (inrange(v, -32768LL, 32767LL)) {
        *((W16*)p) = bits(v, 0, 16);
        p += 2;
        f = 2;
      } else if (inrange(v, -2147483648LL, -2147483647LL)) {
        *((W32*)p) = bits(v, 0, 32);
        p += 4;
        f = 3;
      } else {
        // default to W64:
        *((W64*)p) = v;
        p += 8;
        f = 4;
      }
    }

    return f;
  }

  class CycleTimer {
  public:
    CycleTimer() { total = 0; tstart = 0; iterations = 0; title = "(generic)"; running = 0; }
    CycleTimer(const char* title) { total = 0; tstart = 0; iterations = 1; this->title = title; running = 0; }

    inline void start() { W64 t = rdtsc(); if (running) return; iterations++; tstart = t; running = 1; }
    inline W64 stop() {
      W64 t = rdtsc() - tstart;

      if unlikely (!running) return total;

      tstart = 0;
      total += t;
      running = 0;
      return t;
    }

    inline W64 cycles() const {
      return total;
    }

    inline double seconds() const {
      return (double)total / hz;
    }

    inline void reset() {
      stop();
      tstart = 0;
      total = 0;
    }

  public:
    W64 total;
    W64 tstart;
    int iterations;
    const char* title;
    bool running;

    static double gethz();

  protected:
    static double hz;
  };

  ostream& operator <<(ostream& os, const CycleTimer& ct);

  //
  // Automatically start cycle timer at top of block and
  // stop it when this struct leaves the scope
  //
  struct CycleTimerScope {
    CycleTimer& ct;
    CycleTimerScope(CycleTimer& ct_): ct(ct_) { ct.start(); }
    ~CycleTimerScope() { ct.stop(); }
  };

  //
  // Standard spinlock
  //

  //
  // Define this to profile spinlock contention;
  // it may increase overhead when enabled.
  //
  //#define ENABLE_SPINLOCK_PROFILING

  struct Spinlock {
    typedef byte T;
    T lock;

    Spinlock() { reset(); }

    void reset() {
      lock = 0;
    }

    W64 acquire() {
#ifdef ENABLE_SPINLOCK_PROFILING
      W64 iterations = 0;
#endif

      for (;;) {
        if unlikely (lock) {
#ifdef ENABLE_SPINLOCK_PROFILING
          iterations++;
#endif
          cpu_pause();
          barrier();
          continue;
        }

        T old = xchg(lock, T(1));
#ifdef ENABLE_SPINLOCK_PROFILING
        if likely (!old) return iterations;
#else
        if likely (!old) return 0;
#endif
      }
    }

    bool try_acquire() {
      if unlikely (lock) return false;
      T old = xchg(lock, T(1));
      return (!old);
    }

    void release() {
      lock = 0;
      barrier();
    }
  };

  //
  // Mutex with recursive locking
  //
  // acquire(vcpuid) can be called multiple times
  // with the same vcpuid, but if the vcpuid
  // does not match locking_vcpuid, the function
  // spins until the lock can be acquired.
  //
  // release(vcpuid) unlocks the mutex. The current
  // vcpuid must equal locking_vcpu.
  //
  struct RecursiveMutex {
    W16s locking_vcpuid;
    W16 counter;

    RecursiveMutex() { reset(); }

    void reset() {
      locking_vcpuid = -1;
      counter = 0;
    }

    bool acquire() {
      W16s current = current_vcpuid();
      bool acquired;
      bool recursive;

      for (;;) {
        W16s oldv = cmpxchg(locking_vcpuid, current, W16s(-1));
        barrier();
        acquired = (oldv == -1);
        recursive = (oldv == current);

        if unlikely (acquired | recursive) break;

        cpu_pause();
      }

      counter++;

      return (!recursive);
    }

    void release() {
      assert(locking_vcpuid == current_vcpuid());
      assert(counter > 0);

      counter--;
      if likely (!counter) {
        locking_vcpuid = -1;
        barrier();
      }
    }
  };

  //
  // Safe divide and remainder functions that return true iff operation did not generate an exception:
  //
  template <typename T> bool div_rem(T& quotient, T& remainder, T dividend_hi, T dividend_lo, T divisor);
  template <typename T> bool div_rem_s(T& quotient, T& remainder, T dividend_hi, T dividend_lo, T divisor);

  template <typename T>
  struct ScopedLock {
    T& lock;
    ScopedLock(T& lock_): lock(lock_) { lock.acquire(); }
    ~ScopedLock() { lock.release(); }
  };

  class TFunctor {
	  public:
		  virtual bool operator()(void* arg) = 0;
  };

  template<class T>
	  class TFunctor1 : public TFunctor {
		  private:
			  bool (T::*fpt)(void *arg);
			  T& obj;

		  public:
			  TFunctor1(T& _obj, bool (T::*_fpt)(void *arg)) :
				  obj(_obj) {
					  fpt = _fpt;
				  };

			  virtual bool operator()(void *arg) {
				  return (obj.*fpt)(arg);
			  };
	  };

  class TFunctor2 : public TFunctor {
	  private:
		  bool (*fpt)(void *arg);

	  public:
		  TFunctor2(bool (*_fpt)(void *arg)) :
			  fpt(_fpt) {}
		  virtual bool operator()(void *arg) {
			  return (*fpt)(arg);
		  }
  };

  template<class T>
	  TFunctor* signal_mem_ptr(T& _obj, bool (T::*_fpt)(void *arg)) {
		  TFunctor1<T> *t = new TFunctor1<T>(_obj, _fpt);
		  return (TFunctor*)t;
	  }

  TFunctor* signal_fun_ptr(bool (*_fpt)(void *arg));

  class Signal {
	  private:
		  stringbuf name_;
		  TFunctor* func;

	  public:
		  Signal();
		  Signal(const char* name);

          ~Signal() {}

		  bool emit(void *arg) ;
		  void connect(TFunctor* _func);
		  const char* get_name() {
			  return name_.buf;
		  }
		  void set_name(const char *name) {
			  name_ << name;
		  }
  };


}; // namespace superstl

#endif // _SUPERSTL_H_
