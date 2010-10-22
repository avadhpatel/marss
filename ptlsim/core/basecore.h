
#ifndef BASE_CORE_H
#define BASE_CORE_H

#include <ptlsim.h>
#include <statsBuilder.h>
#include <memoryHierarchy.h>

namespace Core {

    struct BaseCoreMachine;

    struct BaseCore {
        BaseCore(BaseCoreMachine& machine);

        virtual void reset() = 0;
        virtual bool runcycle() = 0;
        virtual void check_ctx_changes() = 0;
        virtual void flush_tlb(Context& ctx) = 0;
        virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr) = 0;
        virtual void dump_state(ostream& os) = 0;
        virtual void update_stats(PTLsimStats* stats) = 0;
        virtual void flush_pipeline() = 0;
        virtual W8 get_coreid() = 0;

        void update_memory_hierarchy_ptr();

        BaseCoreMachine& machine;
        Memory::MemoryHierarchy* memoryHierarchy;
    };

    struct BaseCoreMachine: public PTLsimMachine {
        dynarray<BaseCore*> cores;
        Memory::MemoryHierarchy* memoryHierarchyPtr;

        BaseCoreMachine(const char* name);
        virtual bool init(PTLsimConfig& config);
        virtual int run(PTLsimConfig& config);
        virtual W8 get_num_cores();
        virtual void dump_state(ostream& os);
        virtual void update_stats(PTLsimStats* stats);
        virtual void flush_tlb(Context& ctx);
        virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
        void flush_all_pipelines();
        virtual void reset();
        ~BaseCoreMachine();

        bitvec<NUM_SIM_CORES> context_used;
        W8 context_counter;
        W8 coreid_counter;

        Context& get_next_context();
        W8 get_next_coreid();

        // Core configuration functions
        void setup_smt_mc_cores();
        void setup_default_cores();
    };
};

#endif // BASE_CORE_H
