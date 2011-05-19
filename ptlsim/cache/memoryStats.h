
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifndef MEMORY_STATS_H
#define MEMORY_STATS_H

#include <ptlsim.h>
#include <statsBuilder.h>
#include <cacheConstants.h>

//#include <dcache.h>

#define SETUP_STATS(type) \
   userStats_ = &(per_core_cache_stats_ref_with_stats(user_stats, coreid).type); \
   totalUserStats_ = &(user_stats.memory.total.type); \
   kernelStats_ = &(per_core_cache_stats_ref_with_stats(kernel_stats, coreid).type); \
   totalKernelStats_ = &(kernel_stats.memory.total.type);


#define STAT_UPDATE(expr, mode) 0

#define N_STAT_UPDATE(counter, expr, mode) { \
    if(mode) { /* kernel mode */ \
        counter(n_kernel_stats)expr; \
    } else { \
        counter(n_user_stats)expr; \
    } \
}

struct stall_sub { // rootnode:summable
    W64 dependency;
    W64 cache_port;
    W64 buffer_full;

    stall_sub& operator+=(const stall_sub &rhs) { // operator
        dependency += rhs.dependency;
        cache_port += rhs.cache_port;
        buffer_full += rhs.buffer_full;
        return *this;
    }
};

namespace Memory {

	struct CacheStats{ // rootnode:

		struct cpurequest {
			struct count{ //node: summable
				struct hit {
					struct read{  //node: summable
						struct hit { //node: summable
							W64 hit;
							W64 forward;
						} hit;
					} read;

					struct write{  //node: summable
						struct hit { //node: summable
							W64 hit;
							W64 forward;
						} hit;
					} write;
				} hit;

				struct miss { //node: summable
					W64 read;
					W64 write;
				} miss;

                count& operator+=(const count &rhs) { // operator
                    hit.read.hit.hit += rhs.hit.read.hit.hit;
                    hit.read.hit.forward += rhs.hit.read.hit.forward;
                    hit.write.hit.hit += rhs.hit.write.hit.hit;
                    hit.write.hit.forward += rhs.hit.write.hit.forward;
                    miss.read += rhs.miss.read;
                    miss.write += rhs.miss.write;
                    return *this;
                }
			} count;

			struct stall{ //node: summable
                stall_sub read;
                stall_sub write;
			} stall;

			W64 redirects;

            cpurequest& operator+=(const cpurequest &rhs) { // operator
                count += rhs.count;
                stall.read += rhs.stall.read;
                stall.write += rhs.stall.write;
                redirects += rhs.redirects;
                return *this;
            }
		} cpurequest ;

		struct snooprequest { //node: summable
			W64 hit;
			W64 miss;
		} snooprequest;

		W64 annul;
		W64 queueFull;

		struct latency{ //node: summable
			W64 IF;
			W64 load;
			W64 store;
		} latency;

		struct lat_count{  //node: summable
			W64 IF;
			W64 load;
			W64 store;
		} lat_count;

        struct mesi_stats {

            struct hit_state { //node: summable
                W64 snoop[5]; //histo: 0, 4, 1
                W64 cpu[5]; //histo: 0, 4, 1
            } hit_state;

            W64 state_transition[17]; //histo: 0, 16, 1

            mesi_stats& operator+=(const mesi_stats &rhs) { // operator
                foreach(i, 5)
                    hit_state.snoop[i] += rhs.hit_state.snoop[i];
                foreach(i, 5)
                    hit_state.cpu[i] += rhs.hit_state.cpu[i];
                foreach(i, 17)
                    state_transition[i] += rhs.state_transition[i];
                return *this;
            }
        } mesi_stats;

        CacheStats& operator+=(const CacheStats &rhs) { // operator
            cpurequest += rhs.cpurequest;
            snooprequest.hit += rhs.snooprequest.hit;
            snooprequest.miss += rhs.snooprequest.miss;
            annul += rhs.annul;
            queueFull += rhs.queueFull;
            latency.IF += rhs.latency.IF;
            latency.load += rhs.latency.load;
            latency.store += rhs.latency.store;
            lat_count.IF += rhs.lat_count.IF;
            lat_count.load += rhs.lat_count.load;
            lat_count.store += rhs.lat_count.store;
            mesi_stats += rhs.mesi_stats;
            return *this;
        }
	};

	struct PerCoreCacheStats { // rootnode:
		CacheStats CPUController;
		CacheStats L1I;
		CacheStats L1D;
		CacheStats L2;
		CacheStats L3;

        PerCoreCacheStats& operator+=(const PerCoreCacheStats &rhs) { // operator
            CPUController += rhs.CPUController;
            L1I += rhs.L1I;
            L1D += rhs.L1D;
            L2 += rhs.L2;
            L3 += rhs.L3;
            return *this;
        }
	};

    struct BusStats { // rootnode:

        struct broadcasts { //node: summable
            W64 read;
            W64 write;
            W64 update;
        } broadcasts;

        struct broadcast_cycles { //node: summable
            W64 read;
            W64 write;
            W64 update;
        } broadcast_cycles;

        W64 addr_bus_cycles;
        W64 data_bus_cycles;

        BusStats& operator+=(const BusStats &rhs) { // operator
            broadcasts.read += rhs.broadcasts.read;
            broadcasts.write += rhs.broadcasts.write;
            broadcasts.update += rhs.broadcasts.update;

            broadcast_cycles.read += rhs.broadcast_cycles.read;
            broadcast_cycles.write += rhs.broadcast_cycles.write;
            broadcast_cycles.update += rhs.broadcast_cycles.update;

            addr_bus_cycles += rhs.addr_bus_cycles;
            data_bus_cycles += rhs.data_bus_cycles;

            return *this;
        }
    };


// New memory stats
struct BaseCacheStats : public Statable
{
    struct cpurequest : public Statable
    {
        struct count : public Statable
        {
            struct hit : public Statable
            {
                struct hit_sub : public Statable
                {
                    StatObj<W64> hit;
                    StatObj<W64> forward;

                    hit_sub(const char *name, Statable *parent)
                        : Statable(name, parent)
                          , hit("hit", this)
                          , forward("forward", this)
                    {}
                };

                hit_sub read;
                hit_sub write;

                hit(Statable *parent)
                    : Statable("hit", parent)
                      , read("read", this)
                      , write("write", this)
                {}
            } hit;

            struct miss : public Statable
            {
                StatObj<W64> read;
                StatObj<W64> write;

                miss(Statable *parent)
                    : Statable("miss", parent)
                      , read("read", this)
                      , write("write", this)
                {}
            } miss;

            count(Statable *parent)
                : Statable("count", parent)
                  , hit(this)
                  , miss(this)
            {}
        } count;

        struct stall : public Statable
        {
            struct stall_sub : public Statable
            {
                StatObj<W64> dependency;
                StatObj<W64> cache_port;
                StatObj<W64> buffer_full;

                stall_sub(const char *name, Statable *parent)
                    : Statable(name, parent)
                      , dependency("dependency", this)
                      , cache_port("cache_port", this)
                      , buffer_full("buffer_full", this)
                {}
            };

            stall_sub read;
            stall_sub write;

            stall(Statable *parent)
                : Statable("stall", parent)
                  , read("read", this)
                  , write("write", this)
            {}
        } stall;

        StatObj<W64> redirects;

        cpurequest(Statable *parent)
            : Statable("cpurequest", parent)
              , count(this)
              , stall(this)
              , redirects("redirects", this)
        {}
    } cpurequest;

    StatObj<W64> annul;
    StatObj<W64> queueFull;

    BaseCacheStats(const char *name, Statable *parent=NULL)
        : Statable(name, parent)
          , cpurequest(this)
          , annul("annul", this)
          , queueFull("queueFull", this)
    {}
};

static const char* mesi_state_names[4] = {
    "Modified", "Exclusive", "Shared", "Invalid"
};

struct MESIStats : public BaseCacheStats {

    struct miss_state : public Statable {
        StatArray<W64, 5> cpu;
        miss_state(const char *name, Statable *parent)
            : Statable(name, parent)
              , cpu("cpu", this)
        {}
    } miss_state;

    struct hit_state : public Statable{
        StatArray<W64,4> snoop;
        StatArray<W64,4> cpu;
        hit_state (const char *name,Statable *parent)
            :Statable(name, parent)
             ,snoop("snoop",this, mesi_state_names)
             ,cpu("cpu",this, mesi_state_names)
        {
        }
    } hit_state;

    StatArray<W64,16> state_transition;

    MESIStats(const char *name, Statable *parent=NULL)
        :BaseCacheStats(name, parent)
         ,hit_state("hit_state",this)
         ,miss_state("miss_state",this)
         ,state_transition("state_transition",this)
    {}
};

struct NewBusStats : public Statable {

    struct broadcasts : public Statable {
        StatObj<W64> read;
        StatObj<W64> write;
        StatObj<W64> update;

        broadcasts(Statable *parent)
            : Statable("broadcast", parent)
            , read("read", this)
            , write("write", this)
            , update("update", this)
        {}
    } broadcasts;

    struct broadcast_cycles : public Statable {
        StatObj<W64> read;
        StatObj<W64> write;
        StatObj<W64> update;

        broadcast_cycles(Statable *parent)
            : Statable("broadcast_cycles", parent)
            , read("read", this)
            , write("write", this)
            , update("update", this)
        {}
    } broadcast_cycles;

    StatObj<W64> addr_bus_cycles;
    StatObj<W64> data_bus_cycles;
    StatObj<W64> bus_not_ready;

    NewBusStats(const char* name, Statable *parent)
        : Statable(name, parent)
          , broadcasts(this)
          , broadcast_cycles(this)
          , addr_bus_cycles("addr_bus_cycles", this)
          , data_bus_cycles("data_bus_cycles", this)
          , bus_not_ready("bus_not_ready", this)
    {}
};

struct RAMStats : public Statable {

    StatArray<W64, MEM_BANKS> bank_access;
    StatArray<W64, MEM_BANKS> bank_read;
    StatArray<W64, MEM_BANKS> bank_write;
    StatArray<W64, MEM_BANKS> bank_update;

    RAMStats(const char* name, Statable *parent)
        : Statable(name, parent)
          , bank_access("bank_access", this)
          , bank_read("bank_read", this)
          , bank_write("bank_write", this)
          , bank_update("bank_update", this)
    {}
};

};

#endif // MEMORY_STATS_H
