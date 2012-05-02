
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
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef MESI_COHERENCE_LOGIC_H
#define MESI_COHERENCE_LOGIC_H

#include <coherenceLogic.h>

#define UPDATE_MESI_TRANS_STATS(old_state, new_state, mode) \
    if(mode) { /* kernel mode */ \
        state_transition(kernel_stats)[(old_state << 2) | new_state]++; \
    } else { \
        state_transition(user_stats)[(old_state << 2) | new_state]++; \
    }

namespace Memory {

namespace CoherentCache {

    enum MESICacheLineState {
        MESI_INVALID = 0, // 0 has to be invalid as its default
        MESI_MODIFIED,
        MESI_EXCLUSIVE,
        MESI_SHARED,
        NO_MESI_STATES
    };

    enum MESITransations {
        II=0, IM, IE, IS,
        MI, MM, ME, MS,
        EI, EM, EE, ES,
        SI, SM, SE, SS,
    };

    static const char* MESIStateNames[NO_MESI_STATES] = {
        "Invalid",
        "Modified",
        "Exclusive",
        "Shared",
    };

    class MESILogic : public CoherenceLogic
    {
        public:
            MESILogic(CacheController *cont, Statable *parent,
                    MemoryHierarchy *mem_hierarchy)
                : CoherenceLogic("mesi", cont, parent, mem_hierarchy)
                  , miss_state("miss_state", this)
                  , hit_state("hit_state", this)
                  , state_transition("state_transition", this)
            {}

            void handle_local_hit(CacheQueueEntry *queueEntry);
            void handle_local_miss(CacheQueueEntry *queueEntry);
            void handle_interconn_hit(CacheQueueEntry *queueEntry);
            void handle_interconn_miss(CacheQueueEntry *queueEntry);
            void handle_cache_insert(CacheQueueEntry *queueEntry, W64 oldTag);
            void handle_cache_evict(CacheQueueEntry *entry);
            void complete_request(CacheQueueEntry *queueEntry,
                    Message &message);
            void handle_response(CacheQueueEntry *entry,
                    Message &message);
            bool is_line_valid(CacheLine *line);
            void invalidate_line(CacheLine *line);
			void dump_configuration(YAML::Emitter &out) const;

            MESICacheLineState get_new_state(CacheQueueEntry *queueEntry, bool isShared);

            /* Statistics */

            struct miss_state : public Statable {
                StatArray<W64, 4> cpu;
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
                     ,snoop("snoop",this, MESIStateNames)
                     ,cpu("cpu",this, MESIStateNames)
                { }
            } hit_state;

            StatArray<W64,16> state_transition;
    };
};

};

#endif

