
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
 * Copyright 2012 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2012 Furat Afram <fafram1@cs.binghamton.edu>
 *
 */

#ifndef TSX_MESI_COHERENCE_LOGIC_H
#define TSX_MESI_COHERENCE_LOGIC_H

#include <coherenceLogic.h>
#include <coherentCache.h>

#ifdef UPDATE_MESI_TRANS_STATS
#undef UPDATE_MESI_TRANS_STATS
#endif

#define UPDATE_MESI_TRANS_STATS(old_state, new_state, mode) \
    if(mode) { /* kernel mode */ \
        state_transition(kernel_stats)[((old_state & 3) << 2) | (new_state & 3)]++; \
    } else { \
        state_transition(user_stats)[((old_state & 3) << 2) | (new_state & 3)]++; \
    }

namespace Memory {

namespace CoherentCache {

	enum TsxMESICacheLineState {
		TSX_MESI_INVALID = 0, // 0 has to be invalid as its default
		TSX_MESI_MODIFIED,
		TSX_MESI_EXCLUSIVE,
		TSX_MESI_SHARED,
		TSX_NO_MESI_STATES
	};

	enum TsxStatus{
		TM_READ = 1 << 3,
		TM_WRITE = 1 << 4,
		NUM_TSX_STAUS
	};

#define MESI_STATE(mix_state) (TsxMESICacheLineState(mix_state & 0x3))

	enum TsxMESITransations {
		T_II=0, T_IM, T_IE, T_IS,
		T_MI, T_MM, T_ME, T_MS,
		T_EI, T_EM, T_EE, T_ES,
		T_SI, T_SM, T_SE, T_SS,
	};

	static const char* TsxMESIStateNames[TSX_NO_MESI_STATES] = {
		"Invalid",
		"Modified",
		"Exclusive",
		"Shared",
	};

	static const char* TsxStatusNames[NUM_TSX_STAUS] = {
		"TM_Read",
		"TM_Write",
	};

	enum AbortReasons{
		ABORT_EVICT  = 0,
		ABORT_CONFLICT
	};

	class TsxCache;

	class TsxMESILogic : public CoherenceLogic
	{
		public:
			TsxMESILogic(CacheController *cont, Statable *parent,
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

			W8 get_new_state(
					CacheQueueEntry *queueEntry, bool isShared);

			void handle_local_hit_in_tsx(CacheQueueEntry *queueEntry);
			bool handle_interconn_hit_in_tsx(CacheQueueEntry *queueEntry);

			TsxMESICacheLineState get_mesi_state(CacheQueueEntry *queueEntry) {
				return TsxMESICacheLineState(
						MESI_STATE(queueEntry->line->state));
			}

			void set_mesi_state(CacheQueueEntry *queueEntry,
					int state) {
				TsxStatus tsx_state = get_tsx_state(queueEntry);
				queueEntry->line->state = tsx_state | state;
			}

			TsxStatus get_tsx_state(CacheQueueEntry *queueEntry) {
				return TsxStatus((queueEntry->line->state) &
						(TM_READ|TM_WRITE));
			}

			TsxCache *tsx_cont;

			/* Statistics */

			struct miss_state : public Statable {
				StatArray<W64, 4> cpu;
				miss_state(const char *name, Statable *parent)
					: Statable(name, parent)
					  , cpu("cpu", this, TsxMESIStateNames)
				{}
			} miss_state;

			struct hit_state : public Statable{
				StatArray<W64,4> snoop;
				StatArray<W64,4> cpu;
				hit_state (const char *name,Statable *parent)
					:Statable(name, parent)
					 ,snoop("snoop",this, TsxMESIStateNames)
					 ,cpu("cpu",this, TsxMESIStateNames)
				{ }
			} hit_state;

			StatArray<W64,16> state_transition;
	};

	class TsxCache : public CacheController
	{
		private:
			bool tsx_abort_;
			bool in_tsx_;
			W64 abort_reason_;
			Signal *abort_signal_;
			Signal *complete_signal_;
			Signal tsx_end;

        public:
			TsxCache(W8 coreid, const char *name,
					MemoryHierarchy *hierarchy, CacheType type);

            bool handle_upper_interconnect(Message &message);
            bool tsx_end_cb(void *arg);

			void enable_tsx() {
				in_tsx_ = true;
			}

            bool check_tsx_invalidated(){
				return check_cache_states_bit(TM_WRITE, TSX_MESI_INVALID);
            }

			void disable_tsx() {
				in_tsx_ = false;
				reset_cache_states_bit(TM_READ|TM_WRITE);
				marss_add_event(&tsx_end, 1, NULL);
			}

			void abort_tsx(W8 reason) {
				tsx_abort_ = 1;
				abort_reason_ = (W64)reason;
				memdebug("TSX Abort in " << get_name() << endl);

				if (abort_signal_) {
					abort_signal_->emit((void*)abort_reason_);
				}
			}

			bool in_tsx() {
				return in_tsx_;
			}
	};
};

};

#endif
