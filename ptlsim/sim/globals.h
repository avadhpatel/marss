// -*- c++ -*-
//
// Copyright 1997-2008 Matt T. Yourst <yourst@yourst.com>
//
// This program is free software; it is licensed under the
// GNU General Public License, Version 2.
//

#ifndef _GLOBALS_H_
#define _GLOBALS_H_


// #define fullsys_debug   cerr << "fullsys_debug: cycle ", sim_cycle, " in ", __FILE__, ":", __LINE__, " (", __PRETTY_FUNCTION__, ")"
// //#define USE_MSDEBUG logable(5)
// #define USE_MSDEBUG config.memory_log
// #define msdebug if(USE_MSDEBUG) ptl_logfile << " CYC ", sim_cycle, " ", __PRETTY_FUNCTION__, "(): \n"; if(USE_MSDEBUG) ptl_logfile

// #define mstest assert(0)
// #define msdebug1 if(USE_MSDEBUG) ptl_logfile

// debug segmentation fault
// #define msdebug cerr << " CYC ", sim_cycle, " ", __PRETTY_FUNCTION__, "(): \n"; cerr
// #define msdebug1  cerr


extern "C" {
#include <sys/ptrace.h>
}

#include <math.h>


#define fullsys_debug   cerr << "fullsys_debug: cycle ", sim_cycle, " in ", __FILE__, ":", __LINE__, " (", __PRETTY_FUNCTION__, ")"
#define USE_MSDEBUG (logable(5))
#define msdebug if(USE_MSDEBUG) ptl_logfile << " CYC ", sim_cycle, " ", __PRETTY_FUNCTION__, "(): \n"; if(USE_MSDEBUG) ptl_logfile
#define msdebug1 if(USE_MSDEBUG) ptl_logfile
#define ENABLE_SMT

#ifdef ENABLE_SMT
#define MAX_CONTEXTS NUM_SIM_CORES
static const int MAX_THREADS_PER_CORE = 1;
static const int NUMBER_OF_CORES = NUM_SIM_CORES;
static const int NUMBER_OF_THREAD_PER_CORE = 1;
static const int NUMBER_OF_CORES_PER_L2 = 1;


#else
static const int MAX_THREADS_PER_CORE = 1;
static const int NUMBER_OF_CORES = 1;
static const int NUMBER_OF_THREAD_PER_CORE = 1;
static const int NUMBER_OF_CORES_PER_L2 = 1;

#endif

// #define SINGLE_CORE_MEM_CONFIG
// Enable/Disable L3 cache
//#define ENABLE_L3_ACHE

typedef __SIZE_TYPE__ size_t;
typedef unsigned long long W64;
typedef signed long long W64s;
typedef unsigned int W32;
typedef signed int W32s;
typedef unsigned short W16;
typedef signed short W16s;
typedef unsigned char byte;
typedef unsigned char W8;
typedef signed char W8s;
#ifndef NULL
#define NULL 0
#endif

#ifdef __x86_64__
typedef W64 Waddr;
#else
typedef W32 Waddr;
#endif

#ifdef __cplusplus

#include <math.h>
#include <float.h>

//#define __stringify_1(x) #x
//#define stringify(x) __stringify_1(x)

#define alignto(x) __attribute__ ((aligned (x)))
#define insection(x) __attribute__ ((section (x)))
#define packedstruct __attribute__ ((packed))
#define noinline __attribute__((noinline))

#ifndef unlikely
#define unlikely(x) (__builtin_expect(!!(x), 0))
#endif
#ifndef likely
#define likely(x) (__builtin_expect(!!(x), 1))
#endif
#define isconst(x) (__builtin_constant_p(x))
#define getcaller() (__builtin_return_address(0))
#define asmlinkage extern "C"

//
// Asserts
//
#if defined __cplusplus
#  define __ASSERT_VOID_CAST static_cast<void>
#else
#  define __ASSERT_VOID_CAST (void)
#endif

void register_assert_cb(void (*fn)());

asmlinkage void assert_fail(const char *__assertion, const char *__file, unsigned int __line, const char *__function) __attribute__ ((__noreturn__));

// For embedded debugging use only:
static inline void assert_fail_trap(const char *__assertion, const char *__file, unsigned int __line, const char *__function) {
  asm("ud2a" : : "a" (__assertion), "b" (__file), "c" (__line), "d" (__function));
}

#define __CONCAT(x,y)	x ## y
#define __STRING(x)	#x

#ifdef DISABLE_ASSERT
#undef assert
#define assert(expr) ;
#else
#define assert(expr) (__ASSERT_VOID_CAST ((unlikely(expr)) ? 0 : (assert_fail (__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__), 0)))
#endif

#define nan NAN
#define inf INFINITY

template <typename T> struct limits { static const T min = 0; static const T max = 0; };
#define MakeLimits(T, __min, __max) template <> struct limits<T> { static const T min = (__min); static const T max = (__max); };
MakeLimits(W8, 0, 0xff);
MakeLimits(W16, 0, 0xffff);
MakeLimits(W32, 0, 0xffffffff);
MakeLimits(W64, 0, 0xffffffffffffffffULL);
MakeLimits(W8s, 0x80, 0x7f);
MakeLimits(W16s, 0x8000, 0x7fff);
MakeLimits(W32s, 0x80000000, 0x7fffffff);
MakeLimits(W64s, 0x8000000000000000LL, 0x7fffffffffffffffLL);
#ifdef __x86_64__
MakeLimits(signed long, 0x8000000000000000LL, 0x7fffffffffffffffLL);
MakeLimits(unsigned long, 0x0000000000000000LL, 0xffffffffffffffffLL);
#else
MakeLimits(signed long, 0x80000000, 0x7fffffff);
MakeLimits(unsigned long, 0, 0xffffffff);
#endif
#undef MakeLimits

template <typename T> struct isprimitive_t { static const bool primitive = 0; };
#define MakePrimitive(T) template <> struct isprimitive_t<T> { static const bool primitive = 1; }
MakePrimitive(signed char);
MakePrimitive(unsigned char);
MakePrimitive(signed short);
MakePrimitive(unsigned short);
MakePrimitive(signed int);
MakePrimitive(unsigned int);
MakePrimitive(signed long);
MakePrimitive(unsigned long);
MakePrimitive(signed long long);
MakePrimitive(unsigned long long);
MakePrimitive(float);
MakePrimitive(double);
MakePrimitive(bool);

template<typename T> struct ispointer_t { static const bool pointer = 0; };
template <typename T> struct ispointer_t<T*> { static const bool pointer = 1; };
#define ispointer(T) (ispointer_t<T>::pointer)
#define isprimitive(T) (isprimitive_t<T>::primitive)

#ifndef offsetof_t
// Null pointer to the specified object type, for computing field offsets
template <typename T> static inline T* NULLptr() { return (T*)(Waddr)0; }
#define offsetof_t(T, field) ((Waddr)(&(NULLptr<T>()->field)) - ((Waddr)NULLptr<T>()))
#endif
#define baseof(T, field, ptr) ((T*)(((byte*)(ptr)) - offsetof_t(T, field)))
// Restricted (non-aliased) pointers:
#define noalias __restrict__

// Default placement versions of operator new.
//inline void* operator new(size_t, void* p) { return p; }
//inline void* operator new[](size_t, void* p) { return p; }
//inline void operator delete(void*, void*) { }
//inline void operator delete[](void*, void*) { }

// Base parameterless function type
typedef void (*base_function_t)();

// Add raw data auto-casts to a structured or bitfield type
#define RawDataAccessors(structtype, rawtype) \
  structtype() { } \
  structtype(rawtype rawbits) { *((rawtype*)this) = rawbits; } \
  operator rawtype() const { return *((rawtype*)this); }

// Typecasts in bizarre ways required for binary form access
union W32orFloat { W32 w; float f; };
union W64orDouble {
  W64 w;
  double d;
  struct { W32 lo; W32s hi; } hilo;
  struct { W64 mantissa:52, exponent:11, negative:1; } ieee;
  // This format makes it easier to see if a NaN is a signalling NaN.
  struct { W64 mantissa:51, qnan:1, exponent:11, negative:1; } ieeenan;
};

static inline const float W32toFloat(W32 x) { union W32orFloat c; c.w = x; return c.f; }
static inline const W32 FloatToW32(float x) { union W32orFloat c; c.f = x; return c.w; }
static inline const double W64toDouble(W64 x) { union W64orDouble c; c.w = x; return c.d; }
static inline const W64 DoubleToW64(double x) { union W64orDouble c; c.d = x; return c.w; }

//
// Functional constructor
//

template <typename T> static inline T min(const T& a, const T& b) { typeof (a) _a = a; typeof (b) _b = b; return _a > _b ? _b : _a; }
template <typename T> static inline T max(const T& a, const T& b) { typeof (a) _a = a; typeof (b) _b = b; return _a > _b ? _a : _b; }
template <typename T> static inline T clipto(const T& v, const T& minv, const T& maxv) { return min(max(v, minv), maxv); }
template <typename T> static inline bool inrange(const T& v, const T& minv, const T& maxv) { typeof (v) _v = v; return ((_v >= minv) & (_v <= maxv)); }
// template <typename T> static inline T abs(T x) { typeof (x) _x = x; return (_x < 0) ? -_x : _x; } // (built-in for gcc)

// Bit fitting
static inline bool fits_in_signed_nbit(W64s v, int b) {
  return inrange(v, W64s(-(1ULL<< (b-1))), W64s(+(1ULL << (b-1))-1));
}

static inline bool fits_in_signed_nbit_tagged(W64s v, int b) {
  return inrange(v, W64s(-(1ULL<< (b-1))+1), W64s(+(1ULL << (b-1))-1));
}

static inline bool fits_in_signed_8bit(W64s v) { return fits_in_signed_nbit(v, 8); }
static inline bool fits_in_signed_16bit(W64s v) { return fits_in_signed_nbit(v, 16); }
static inline bool fits_in_signed_32bit(W64s v) { return fits_in_signed_nbit(v, 32); }

#define sqr(x) ((x)*(x))
#define cube(x) ((x)*(x)*(x))
#define bit(x, n) (((x) >> (n)) & 1)

#define bitmask(l) (((l) == 64) ? (W64)(-1LL) : ((1LL << (l))-1LL))
#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
#define lowbits(x, l) bits(x, 0, l)
#define setbit(x,i) ((x) |= (1LL << (i)))
#define clearbit(x, i) ((x) &= (W64)(~(1LL << (i))))
#define assignbit(x, i, v) ((x) = (((x) &= (W64)(~(1LL << (i)))) | (((W64)((bool)(v))) << i)));

#define foreach(i, n) for (int i = 0; i < (n); i++)

static inline W64s signext64(W64s x, const int i) { return (x << (64-i)) >> (64-i); }
static inline W32s signext32(W32s x, const int i) { return (x << (32-i)) >> (32-i); }
static inline W16s signext16(W16s x, const int i) { return (x << (16-i)) >> (16-i); }

static inline W64s bitsext64(W64s x, const int i, const int l) { return signext64(bits(x, i, l), l); }
static inline W32s bitsext32(W32s x, const int i, const int l) { return signext32(bits(x, i, l), l); }
static inline W16s bitsext16(W16s x, const int i, const int l) { return signext16(bits(x, i, l), l); }

typedef byte v16qi __attribute__ ((vector_size(16)));
typedef v16qi vec16b;
typedef W16 v8hi __attribute__ ((vector_size(16)));
typedef v8hi vec8w;
typedef float v4sf __attribute__ ((vector_size(16)));
typedef v4sf vec4f;
typedef W32 v4si __attribute__ ((vector_size(16)));
typedef v4si vec4i;
typedef float v2df __attribute__ ((vector_size(16)));
typedef v2df vec2d;

inline vec16b x86_sse_pcmpeqb(vec16b a, vec16b b) { asm("pcmpeqb %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec8w x86_sse_pcmpeqw(vec8w a, vec8w b) { asm("pcmpeqw %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec4i x86_sse_pcmpeqd(vec4i a, vec4i b) { asm("pcmpeqd %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec16b x86_sse_psubusb(vec16b a, vec16b b) { asm("psubusb %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec16b x86_sse_paddusb(vec16b a, vec16b b) { asm("paddusb %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec16b x86_sse_pandb(vec16b a, vec16b b) { asm("pand %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec8w x86_sse_psubusw(vec8w a, vec8w b) { asm("psubusb %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec8w x86_sse_paddusw(vec8w a, vec8w b) { asm("paddsub %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec8w x86_sse_pandw(vec8w a, vec8w b) { asm("pand %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
inline vec16b x86_sse_packsswb(vec8w a, vec8w b) { asm("packsswb %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return (vec16b)a; }
inline W32 x86_sse_pmovmskb(vec16b vec) { W32 mask; asm("pmovmskb %[vec],%[mask]" : [mask] "=r" (mask) : [vec] "x" (vec)); return mask; }
inline W32 x86_sse_pmovmskw(vec8w vec) { return x86_sse_pmovmskb(x86_sse_packsswb(vec, vec)) & 0xff; }
inline vec16b x86_sse_psadbw(vec16b a, vec16b b) { asm("psadbw %[b],%[a]" : [a] "+x" (a) : [b] "xg" (b)); return a; }
template <int i> inline W16 x86_sse_pextrw(vec16b a) { W32 rd; asm("pextrw %[i],%[a],%[rd]" : [rd] "=r" (rd) : [a] "x" (a), [i] "N" (i)); return rd; }

inline vec16b x86_sse_zerob() { vec16b rd = {0}; asm("pxor %[rd],%[rd]" : [rd] "+x" (rd)); return rd; }
inline vec16b x86_sse_onesb() { vec16b rd = {0}; asm("pcmpeqb %[rd],%[rd]" : [rd] "+x" (rd)); return rd; }
inline vec8w x86_sse_zerow() { vec8w rd = {0}; asm("pxor %[rd],%[rd]" : [rd] "+x" (rd)); return rd; }
inline vec8w x86_sse_onesw() { vec8w rd = {0}; asm("pcmpeqw %[rd],%[rd]" : [rd] "+x" (rd)); return rd; }

// If lddqu is available (SSE3: Athlon 64 (some cores, like X2), Pentium 4 Prescott), use that instead. It may be faster.

extern const byte byte_to_vec16b[256][16];
extern const byte index_bytes_vec16b[16][16];
extern const byte index_bytes_plus1_vec16b[16][16];

inline vec16b x86_sse_dupb(const byte b) {
  return *((vec16b*)&byte_to_vec16b[b]);
}

inline vec8w x86_sse_dupw(const W16 b) {
  union {
      vec8w v;
      W32 wp[4];
  } val;

  W32 w = (b << 16) | b;
  foreach(i, 4) { val.wp[i] = w; }
  return val.v;
}

inline void x86_set_mxcsr(W32 value) { asm volatile("ldmxcsr %[value]" : : [value] "m" (value)); }
inline W32 x86_get_mxcsr() { W32 value; asm volatile("stmxcsr %[value]" : [value] "=m" (value)); return value; }
union MXCSR {
  struct { W32 ie:1, de:1, ze:1, oe:1, ue:1, pe:1, daz:1, im:1, dm:1, zm:1, om:1, um:1, pm:1, rc:2, fz:1; } fields;
  W32 data;

  MXCSR() { }
  MXCSR(W32 v) { data = v; }
  operator W32() const { return data; }
};
enum { MXCSR_ROUND_NEAREST, MXCSR_ROUND_DOWN, MXCSR_ROUND_UP, MXCSR_ROUND_TOWARDS_ZERO };
#define MXCSR_EXCEPTION_DISABLE_MASK 0x1f80 // OR this into mxcsr to disable all exceptions
#define MXCSR_DEFAULT 0x1f80 // default settings (no exceptions, defaults for rounding and denormals)

inline W32 x86_bsf32(W32 b) { W32 r = 0; asm("bsf %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }
inline W64 x86_bsf64(W64 b) { W64 r = 0; asm("bsf %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }
inline W32 x86_bsr32(W32 b) { W32 r = 0; asm("bsr %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }
inline W64 x86_bsr64(W64 b) { W64 r = 0; asm("bsr %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }

template <typename T> inline bool x86_bt(T r, T b) { byte c; asm("bt %[b],%[r]; setc %[c]" : [c] "=r" (c) : [r] "r" (r), [b] "r" (b)); return c; }
template <typename T> inline bool x86_btn(T r, T b) { byte c; asm("bt %[b],%[r]; setnc %[c]" : [c] "=r" (c) : [r] "r" (r), [b] "r" (b)); return c; }

// Return the updated data; ignore the old value
template <typename T> inline W64 x86_bts(T r, T b) { asm("bts %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }
template <typename T> inline W64 x86_btr(T r, T b) { asm("btr %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }
template <typename T> inline W64 x86_btc(T r, T b) { asm("btc %[b],%[r]" : [r] "+r" (r) : [b] "r" (b)); return r; }

// Return the old value of the bit, but still update the data
template <typename T> inline bool x86_test_bts(T& r, T b) { byte c; asm("bts %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+r" (r) : [b] "r" (b)); return c; }
template <typename T> inline bool x86_test_btr(T& r, T b) { byte c; asm("btr %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+r" (r) : [b] "r" (b)); return c; }
template <typename T> inline bool x86_test_btc(T& r, T b) { byte c; asm("btc %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+r" (r) : [b] "r" (b)); return c; }

// Full SMP-aware locking with test-and-[set|reset|complement] in memory
template <typename T> inline bool x86_locked_bts(T& r, T b) { byte c; asm volatile("lock bts %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+m" (r) : [b] "r" (b) : "memory"); return c; }
template <typename T> inline bool x86_locked_btr(T& r, T b) { byte c; asm volatile("lock btr %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+m" (r) : [b] "r" (b) : "memory"); return c; }
template <typename T> inline bool x86_locked_btc(T& r, T b) { byte c; asm volatile("lock btc %[b],%[r]; setc %[c]" : [c] "=r" (c), [r] "+m" (r) : [b] "r" (b) : "memory"); return c; }

template <typename T> inline T bswap(T r) { asm("bswap %[r]" : [r] "+r" (r)); return r; }

// Return v with groups of N bits swapped
template <typename T, int N>
static inline T bitswap(T v) {
  T m =
    (N == 1) ? T(0x5555555555555555ULL) :
    (N == 2) ? T(0x3333333333333333ULL) :
    (N == 4) ? T(0x0f0f0f0f0f0f0f0fULL) : 0;

  return ((v & m) << N) | ((v & (~m)) >> N);
}

template <typename T>
T reversebits(T v) {
  v = bitswap<T, 1>(v);
  v = bitswap<T, 2>(v);
  v = bitswap<T, 4>(v);
  v = bswap(v);
  return v;
}

static inline W16 x86_sse_maskeqb(const vec16b v, byte target) { return x86_sse_pmovmskb(x86_sse_pcmpeqb(v, x86_sse_dupb(target))); }

// This is a barrier for the compiler only, NOT the processor!
#define barrier() asm volatile("": : :"memory")

// Denote parallel sections for the compiler
#define parallel

template <typename T>
static inline T xchg(T& v, T newv) {
	switch (sizeof(T)) {
  case 1: asm volatile("lock xchgb %[newv],%[v]" : [v] "+m" (v), [newv] "+r" (newv) : : "memory"); break;
  case 2: asm volatile("lock xchgw %[newv],%[v]" : [v] "+m" (v), [newv] "+r" (newv) : : "memory"); break;
  case 4: asm volatile("lock xchgl %[newv],%[v]" : [v] "+m" (v), [newv] "+r" (newv) : : "memory"); break;
  case 8: asm volatile("lock xchgq %[newv],%[v]" : [v] "+m" (v), [newv] "+r" (newv) : : "memory"); break;
	}
	return newv;
}

template <typename T>
static inline T xadd(T& v, T incr) {
	switch (sizeof(T)) {
  case 1: asm volatile("lock xaddb %[incr],%[v]" : [v] "+m" (v), [incr] "+r" (incr) : : "memory"); break;
  case 2: asm volatile("lock xaddw %[incr],%[v]" : [v] "+m" (v), [incr] "+r" (incr) : : "memory"); break;
  case 4: asm volatile("lock xaddl %[incr],%[v]" : [v] "+m" (v), [incr] "+r" (incr) : : "memory"); break;
  case 8: asm volatile("lock xaddq %[incr],%[v]" : [v] "+m" (v), [incr] "+r" (incr) : : "memory"); break;
	}
  return incr;
}

template <typename T>
static inline T cmpxchg(T& mem, T newv, T cmpv) {
	switch (sizeof(T)) {
  case 1: asm volatile("lock cmpxchgb %[newv],%[mem]" : [mem] "+m" (mem), [cmpv] "+a" (cmpv), [newv] "+r" (newv) : : "memory"); break;
  case 2: asm volatile("lock cmpxchgw %[newv],%[mem]" : [mem] "+m" (mem), [cmpv] "+a" (cmpv), [newv] "+r" (newv) : : "memory"); break;
  case 4: asm volatile("lock cmpxchgl %[newv],%[mem]" : [mem] "+m" (mem), [cmpv] "+a" (cmpv), [newv] "+r" (newv) : : "memory"); break;
  case 8: asm volatile("lock cmpxchgq %[newv],%[mem]" : [mem] "+m" (mem), [cmpv] "+a" (cmpv), [newv] "+r" (newv) : : "memory"); break;
	}

  // Return the old value in the slot (so we can check if it matches newv)
  return cmpv;
}

static inline void cpu_pause() { asm volatile("pause" : : : "memory"); }

static inline void prefetch(const void* x) { asm volatile("prefetcht0 (%0)" : : "r" (x)); }

static inline void cpuid(int op, W32& eax, W32& ebx, W32& ecx, W32& edx) {
	asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "0" (op));
}

static inline W64 rdtsc() {
  W32 lo, hi;
  asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((W64)lo) | (((W64)hi) << 32);
}

template <typename T>
static inline T x86_ror(T r, int n) { asm("ror %%cl,%[r]" : [r] "+q" (r) : [n] "c" ((byte)n)); return r; }

template <typename T>
static inline T x86_rol(T r, int n) { asm("rol %%cl,%[r]" : [r] "+q" (r) : [n] "c" ((byte)n)); return r; }

#ifndef __x86_64__
// Need to emulate this on 32-bit x86
template <>
static inline W64 x86_ror(W64 r, int n) {
  return (r >> n) | (r << (64 - n));
}
#endif

template <typename T>
static inline T dupb(const byte b) { return T(b) * T(0x0101010101010101ULL); }

/**
 * @brief Get Host Machine's CPU Frequency
 *
 * @return Frequency of CPU in HZ
 */
W64 get_native_core_freq_hz();

/**
 * @brief Convert 'ticks' to Host seconds
 *
 * @param ticks Ticks/Cycles counter used for conversion
 *
 * @return Seconds of Host Machine
 */
static inline double ticks_to_native_seconds(W64 ticks) {
  return (double)ticks / (double)get_native_core_freq_hz();
}

/**
 * @brief Convert seconds to Host 'ticks'
 *
 * @param seconds value used for conversion
 *
 * @return Ticks/Cycles based on Host Machine's Frequency
 */
static inline W64 seconds_to_native_ticks(double seconds) {
  return (W64)(seconds * (double)get_native_core_freq_hz());
}

template <int n> struct lg { static const int value = 1 + lg<n/2>::value; };
template <> struct lg<1> { static const int value = 0; };
#define log2(v) (lg<(v)>::value)

template <int n> struct lg10 { static const int value = 1 + lg10<n/10>::value; };
template <> struct lg10<1> { static const int value = 0; };
template <> struct lg10<0> { static const int value = 0; };
#define log10(v) (lg10<(v)>::value)

template <int N, typename T>
static inline T foldbits(T a) {
  if (N == 0) return 0;

  const int B = (sizeof(T) * 8);
  const int S = (B / N) + ((B % N) ? 1 : 0);

  T z = 0;
  foreach (i, S) {
    z ^= a;
    a >>= N;
  }

  return lowbits(z, N);
}


// For specifying easy to read arrays
#define _ (0)

template <bool b00,     bool b01 = 0, bool b02 = 0, bool b03 = 0,
          bool b04 = 0, bool b05 = 0, bool b06 = 0, bool b07 = 0,
          bool b08 = 0, bool b09 = 0, bool b10 = 0, bool b11 = 0,
          bool b12 = 0, bool b13 = 0, bool b14 = 0, bool b15 = 0,
          bool b16 = 0, bool b17 = 0, bool b18 = 0, bool b19 = 0,
          bool b20 = 0, bool b21 = 0, bool b22 = 0, bool b23 = 0,
          bool b24 = 0, bool b25 = 0, bool b26 = 0, bool b27 = 0,
          bool b28 = 0, bool b29 = 0, bool b30 = 0, bool b31 = 0,
          bool b32 = 0, bool b33 = 0, bool b34 = 0, bool b35 = 0,
          bool b36 = 0, bool b37 = 0, bool b38 = 0, bool b39 = 0,
          bool b40 = 0, bool b41 = 0, bool b42 = 0, bool b43 = 0,
          bool b44 = 0, bool b45 = 0, bool b46 = 0, bool b47 = 0,
          bool b48 = 0, bool b49 = 0, bool b50 = 0, bool b51 = 0,
          bool b52 = 0, bool b53 = 0, bool b54 = 0, bool b55 = 0,
          bool b56 = 0, bool b57 = 0, bool b58 = 0, bool b59 = 0,
          bool b60 = 0, bool b61 = 0, bool b62 = 0, bool b63 = 0>
struct constbits {
  static const W64 value =
  (((W64)b00)  <<  0) +
  (((W64)b01)  <<  1) +
  (((W64)b02)  <<  2) +
  (((W64)b03)  <<  3) +
  (((W64)b04)  <<  4) +
  (((W64)b05)  <<  5) +
  (((W64)b06)  <<  6) +
  (((W64)b07)  <<  7) +
  (((W64)b08)  <<  8) +
  (((W64)b09)  <<  9) +
  (((W64)b10)  << 10) +
  (((W64)b11)  << 11) +
  (((W64)b12)  << 12) +
  (((W64)b13)  << 13) +
  (((W64)b14)  << 14) +
  (((W64)b15)  << 15) +
  (((W64)b16)  << 16) +
  (((W64)b17)  << 17) +
  (((W64)b18)  << 18) +
  (((W64)b19)  << 19) +
  (((W64)b20)  << 20) +
  (((W64)b21)  << 21) +
  (((W64)b22)  << 22) +
  (((W64)b23)  << 23) +
  (((W64)b24)  << 24) +
  (((W64)b25)  << 25) +
  (((W64)b26)  << 26) +
  (((W64)b27)  << 27) +
  (((W64)b28)  << 28) +
  (((W64)b29)  << 29) +
  (((W64)b30)  << 30) +
  (((W64)b31)  << 31) +
  (((W64)b32)  << 32) +
  (((W64)b33)  << 33) +
  (((W64)b34)  << 34) +
  (((W64)b35)  << 35) +
  (((W64)b36)  << 36) +
  (((W64)b37)  << 37) +
  (((W64)b38)  << 38) +
  (((W64)b39)  << 39) +
  (((W64)b40)  << 40) +
  (((W64)b41)  << 41) +
  (((W64)b42)  << 42) +
  (((W64)b43)  << 43) +
  (((W64)b44)  << 44) +
  (((W64)b45)  << 45) +
  (((W64)b46)  << 46) +
  (((W64)b47)  << 47) +
  (((W64)b48)  << 48) +
  (((W64)b49)  << 49) +
  (((W64)b50)  << 50) +
  (((W64)b51)  << 51) +
  (((W64)b52)  << 52) +
  (((W64)b53)  << 53) +
  (((W64)b54)  << 54) +
  (((W64)b55)  << 55) +
  (((W64)b56)  << 56) +
  (((W64)b57)  << 57) +
  (((W64)b58)  << 58) +
  (((W64)b59)  << 59) +
  (((W64)b60)  << 60) +
  (((W64)b61)  << 61) +
  (((W64)b62)  << 62) +
  (((W64)b63)  << 63);

  operator W64() const { return value; }
};

asmlinkage {
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/user.h>
};

#include <stdarg.h>

#ifdef PAGE_SIZE
#undef PAGE_SIZE
// We're on x86 or x86-64, so pages are always 4096 bytes:
#define PAGE_SIZE 4096
#endif

// e.g., head (a, b, c) => a
// e.g., if list = (a, b, c), head list => a
//#define head(h, ...) (h)
//#define tail(h, ...) __VA_ARGS__

#define TOLERANCE 0.00001

/*
 * Sometimes floating point numbers do strange things. Like the fact
 * that -0 and +0 are in fact not bit-for-bit equal even though the
 * math says they are. Similar issues come up when dealing with numbers
 * computed from infinities, etc. This function is to make sure we
 * really follow the math, not the IEEE FP standard's idea of "equal".
 */
static inline bool fcmpeqtol(float a, float b) {
  return (a == b) || (fabs(a-b) <= TOLERANCE);
}

/*
 * Make these math functions available even inside of member functions with the same name:
 */
static inline float fsqrt(float v) { return (float)sqrt(v); }
static inline void freemem(void* p) { free(p); }

template <typename T> static inline void setzero(T& x) { memset(&x, 0, sizeof(T)); }
template <typename T> static inline void fillwith(T& x, byte v) { memset(&x, v, sizeof(T)); }

#define HI32(x) (W32)((x) >> 32LL)
#define LO32(x) (W32)((x) & 0xffffffffLL)
#define CONCAT64(hi, lo) ((((W64)(hi)) << 32) + (((W64)(lo)) & 0xffffffffLL))

template <typename T, typename A> static inline T floor(T x, A a) { return (T)(((T)x) & ~((T)(a-1))); }
template <typename T, typename A> static inline T trunc(T x, A a) { return (T)(((T)x) & ~((T)(a-1))); }
template <typename T, typename A> static inline T ceil(T x, A a) { return (T)((((T)x) + ((T)(a-1))) & ~((T)(a-1))); }
template <typename T, typename A> static inline T mask(T x, A a) { return (T)(((T)x) & ((T)(a-1))); }

template <typename T, typename A> static inline T* floorptr(T* x, A a) { return (T*)floor((Waddr)x, a); }
template <typename T, typename A> static inline T* ceilptr(T* x, A a) { return (T*)ceil((Waddr)x, a); }
template <typename T, typename A> static inline T* maskptr(T* x, A a) { return (T*)mask((Waddr)x, a); }
static inline W64 mux64(W64 sel, W64 v0, W64 v1) { return (sel & v1) | ((~sel) & v0); }
template <typename T> static inline T mux(T sel, T v1, T v0) { return (sel & v1) | ((~sel) & v0); }

template <typename T> void swap(T& a, T& b) { T t = a;  a = b; b = t; }

// #define noinline __attribute__((noinline))

//
// Force the compiler to use branchless forms:
//
template <typename T, typename K>
T select(K cond, T if0, T if1) {
  T z = if0;
  asm("test %[cond],%[cond]; cmovnz %[if1],%[z]" : [z] "+r" (z) : [cond] "r" (cond), [if1] "rm" (if1) : "flags");
  return z;
}

template <typename T, typename K>
void condmove(K cond, T& v, T newv) {
  asm("test %[cond],%[cond]; cmovnz %[newv],%[v]" : [v] "+r" (v) : [cond] "r" (cond), [newv] "rm" (newv) : "flags");
}

#define typeof __typeof__
#define ptralign(ptr, bytes) ((typeof(ptr))((unsigned long)(ptr) & ~((bytes)-1)))
#define ptrmask(ptr, bytes) ((typeof(ptr))((unsigned long)(ptr) & ((bytes)-1)))

template <typename T>
inline void arraycopy(T* dest, const T* source, int count) { memcpy(dest, source, count * sizeof(T)); }

template <typename T, typename V>
inline void rawcopy(T& dest, const V& source) { memcpy(&dest, &source, sizeof(T)); }

// static inline float randfloat() { return ((float)rand() / RAND_MAX); }

static inline bool aligned(W64 address, int size) {
  return ((address & (W64)(size-1)) == 0);
}

inline bool strequal(const char* a, const char* b) {
  return (strcmp(a, b) == 0);
}

template <typename T, int size> int lengthof(T (&)[size]) { return size; }

extern const byte popcountlut8bit[];
extern const byte lsbindexlut8bit[];

static inline int popcount8bit(byte x) {
  return popcountlut8bit[x];
}

static inline int lsbindex8bit(byte x) {
  return lsbindexlut8bit[x];
}

static inline int popcount(W32 x) {
  return (popcount8bit(x >> 0) + popcount8bit(x >> 8) + popcount8bit(x >> 16) + popcount8bit(x >> 24));
}

static inline int popcount64(W64 x) {
  return popcount(LO32(x)) + popcount(HI32(x));
}


extern const W64 expand_8bit_to_64bit_lut[256];

// LSB index:

// Operand must be non-zero or result is undefined:
inline unsigned int lsbindex32(W32 n) { return x86_bsf32(n); }

inline int lsbindexi32(W32 n) {
  int r = lsbindex32(n);
  return (n ? r : -1);
}

#ifdef __x86_64__
inline unsigned int lsbindex64(W64 n) { return x86_bsf64(n); }
#else
inline unsigned int lsbindex64(W64 n) {
  unsigned int z;
  W32 lo = LO32(n);
  W32 hi = HI32(n);

  int ilo = lsbindex32(lo);
  int ihi = lsbindex32(hi) + 32;

  return (lo) ? ilo : ihi;
}
#endif

inline unsigned int lsbindexi64(W64 n) {
  int r = lsbindex64(n);
  return (n ? r : -1);
}

// static inline unsigned int lsbindex(W32 n) { return lsbindex32(n); }
inline unsigned int lsbindex(W64 n) { return lsbindex64(n); }

// MSB index:

// Operand must be non-zero or result is undefined:
inline unsigned int msbindex32(W32 n) { return x86_bsr32(n); }

inline int msbindexi32(W32 n) {
  int r = msbindex32(n);
  return (n ? r : -1);
}

#ifdef __x86_64__
inline unsigned int msbindex64(W64 n) { return x86_bsr64(n); }
#else
inline unsigned int msbindex64(W64 n) {
  unsigned int z;
  W32 lo = LO32(n);
  W32 hi = HI32(n);

  int ilo = msbindex32(lo);
  int ihi = msbindex32(hi) + 32;

  return (hi) ? ihi : ilo;
}
#endif

inline unsigned int msbindexi64(W64 n) {
  int r = msbindex64(n);
  return (n ? r : -1);
}

// static inline unsigned int msbindex(W32 n) { return msbindex32(n); }
inline unsigned int msbindex(W64 n) { return msbindex64(n); }

#define percent(x, total) (100.0 * ((float)(x)) / ((float)(total)))

inline int modulo_span(int lower, int upper, int modulus) {
  int result = (upper - lower);
  if (upper < lower) result += modulus;
  return result;
}

inline int add_index_modulo(int index, int increment, int bufsize) {
  // Only if power of 2: return (index + increment) & (bufsize-1);
  index += increment;
  if (index < 0) index += bufsize;
  if (index >= bufsize) index -= bufsize;
  return index;
}

/*
//
// (for making the lookup table used in modulo_ranges_intersect():
//
static bool makelut(int x) {
  //
  // There are only four cases where the spans DO NOT intersect:
  //
  // [a0 a1 b0 b1] ...Aaaaa........ no
  //               .........Bbbb...
  //
  // [b0 b1 a0 a1] .........Aaaa... no
  //               ...Bbbbb........
  //
  // [b1 a0 a1 b0] ...Aaaaa........ no
  //               bb.......Bbbbbbb
  //
  // [a1 b0 b1 a0] aa.......Aaaaaaa no
  //               ...Bbbbb........
  //
  // AND (a0 != b0) & (a0 != b1) & (a1 != b0) & (a1 != b1);
  //
  // All other cases intersect.
  //

  bool le_a0a1 = bit(x, 0);
  bool le_a1b0 = bit(x, 1);
  bool le_b0b1 = bit(x, 2);
  bool le_b1a0 = bit(x, 3);
  bool ne_a0b0 = bit(x, 4);
  bool ne_a0b1 = bit(x, 5);
  bool ne_a1b0 = bit(x, 6);
  bool ne_a1b1 = bit(x, 7);

  bool separate1 =
    (le_a0a1 & le_a1b0 & le_b0b1) |
    (le_b0b1 & le_b1a0 & le_a0a1) |
    (le_b1a0 & le_a0a1 & le_a1b0) |
    (le_a1b0 & le_b0b1 & le_b1a0);

  bool separate2 = ne_a0b0 & ne_a0b1 & ne_a1b0 & ne_a1b1;

  return !(separate1 & separate2);
}
*/

inline bool modulo_ranges_intersect(int a0, int a1, int b0, int b1, int size) {

  int idx =
    ((a0 <= a1) << 0) |
    ((a1 <= b0) << 1) |
    ((b0 <= b1) << 2) |
    ((b1 <= a0) << 3) |
    ((a0 != b0) << 4) |
    ((a0 != b1) << 5) |
    ((a1 != b0) << 6) |
    ((a1 != b1) << 7);

  static const byte lut[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0
  };

  return lut[idx];
}

//
// Express bitstring constant as octal but convert
// to binary for easy Verilog-style expressions:
//
#define bitseq(x) \
  (bit((W64)x,  0*3) <<  0) + \
  (bit((W64)x,  1*3) <<  1) + \
  (bit((W64)x,  2*3) <<  2) + \
  (bit((W64)x,  3*3) <<  3) + \
  (bit((W64)x,  4*3) <<  4) + \
  (bit((W64)x,  5*3) <<  5) + \
  (bit((W64)x,  6*3) <<  6) + \
  (bit((W64)x,  7*3) <<  7) + \
  (bit((W64)x,  8*3) <<  8) + \
  (bit((W64)x,  9*3) <<  9) + \
  (bit((W64)x, 10*3) << 10) + \
  (bit((W64)x, 11*3) << 11) + \
  (bit((W64)x, 12*3) << 12) + \
  (bit((W64)x, 13*3) << 13) + \
  (bit((W64)x, 14*3) << 14) + \
  (bit((W64)x, 15*3) << 15) + \
  (bit((W64)x, 16*3) << 16) + \
  (bit((W64)x, 17*3) << 17) + \
  (bit((W64)x, 18*3) << 18) + \
  (bit((W64)x, 19*3) << 19) + \
  (bit((W64)x, 20*3) << 20) + \
  (bit((W64)x, 21*3) << 21)

#include <superstl.h>

using namespace superstl;

ostream& operator <<(ostream& os, const vec16b& v);
ostream& operator ,(ostream& os, const vec16b& v);
ostream& operator <<(ostream& os, const vec8w& v);
ostream& operator ,(ostream& os, const vec8w& v);

#endif // __cplusplus

#endif // _GLOBALS_H_
