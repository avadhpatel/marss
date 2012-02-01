
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
        counter(kernel_stats)expr; \
    } else { \
        counter(user_stats)expr; \
    } \
}

namespace Memory {

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

struct CPUControllerStats : public BaseCacheStats
{
    StatArray<W64, 200> icache_latency;
    StatArray<W64, 200> dcache_latency;

    CPUControllerStats(const char *name, Statable *parent)
        : BaseCacheStats(name, parent)
          , icache_latency("icache_latency", this)
          , dcache_latency("dcache_latency", this)
    { }
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
         ,miss_state("miss_state",this)
         ,hit_state("hit_state",this)
         ,state_transition("state_transition",this)
    {}
};

struct BusStats : public Statable {

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

    BusStats(const char* name, Statable *parent)
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
