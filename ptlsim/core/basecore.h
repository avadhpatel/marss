
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef BASE_CORE_H
#define BASE_CORE_H

#include <ptlsim.h>
#include <ptl-qemu.h>
#include <machine.h>
#include <statsBuilder.h>
#include <memoryHierarchy.h>

namespace Core {

    class BaseCore : public Statable {
        W8 coreid;

        public:
            BaseCore(BaseMachine& machine, const char* name);
            virtual ~BaseCore() {}

            virtual void reset() = 0;
            virtual void check_ctx_changes() = 0;
            virtual void flush_tlb(Context& ctx) = 0;
            virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr) = 0;
            virtual void dump_state(ostream& os) = 0;
            virtual void update_stats() = 0;
            virtual void flush_pipeline() = 0;
		    virtual void dump_configuration(YAML::Emitter &out) const = 0;

            void update_memory_hierarchy_ptr();

            BaseMachine& machine;
            Memory::MemoryHierarchy* memoryHierarchy;

            W8 get_coreid() const {
                return coreid;
            }
    };

};

#endif // BASE_CORE_H
