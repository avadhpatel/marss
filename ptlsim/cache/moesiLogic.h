
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

#ifndef MOESI_COHERENCE_LOGIC_H
#define MOESI_COHERENCE_LOGIC_H

#include <coherenceLogic.h>

#define UPDATE_MOESI_TRANS_STATS(old_state, new_state, mode) \
    if(mode) { /* kernel mode */ \
        state_transition(kernel_stats)[MOESITransTable[old_state][new_state]]++; \
    } else { \
        state_transition(user_stats)[MOESITransTable[old_state][new_state]]++; \
    }

namespace Memory {

namespace CoherentCache {

    enum MOESICacheLineState {
        MOESI_INVALID = 0, // 0 has to be invalid as its default
        MOESI_MODIFIED,
        MOESI_OWNER,
        MOESI_EXCLUSIVE,
        MOESI_SHARED,
        NUM_MOESI_STATES
    };

    enum MOESITransations {
        II=0, IM, IO, IE, IS,
        MI, MM, MO, ME, MS,
        OI, OM, OO, OE, OS,
        EI, EM, EO, EE, ES,
        SI, SM, SO, SE, SS,
        NUM_MOESI_STATE_TRANS,
    };

    static int MOESITransTable[NUM_MOESI_STATES][NUM_MOESI_STATES] = {
        {II, IM, IO, IE, IS},
        {MI, MM, MO, ME, MS},
        {OI, OM, OO, OE, OS},
        {EI, EM, EO, EE, ES},
        {SI, SM, SO, SE, SS},
    };

    static const char* MOESIStateNames[NUM_MOESI_STATES] = {
        "Invalid",
        "Modified",
        "Owner",
        "Exclusive",
        "Shared",
    };

    class MOESILogic : public CoherenceLogic
    {
        public:
            MOESILogic(CacheController *cont, Statable *parent,
                    MemoryHierarchy *mem_hierarchy)
                : CoherenceLogic("moesi", cont, parent, mem_hierarchy)
                  , state_transition("state_trans", this)
                  , miss_state("miss_state", this, MOESIStateNames)
                  , hit_state("hit_state", this, MOESIStateNames)
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

            void send_response(CacheQueueEntry *queueEntry,
                    Interconnect *sendTo);
            void send_to_cont(CacheQueueEntry *queueEntry,
                    Interconnect *sendTo, Controller *dest);
            void send_evict(CacheQueueEntry *queueEntry, W64 oldTag=-1,
                    bool with_directory=0);
            void send_update(CacheQueueEntry *queueEntry, W64 oldTag=-1);

            /* Statistics */
            StatArray<W64,NUM_MOESI_STATE_TRANS> state_transition;
            StatArray<W64, NUM_MOESI_STATES> miss_state;
            StatArray<W64, NUM_MOESI_STATES> hit_state;
    };

};

};

#endif
