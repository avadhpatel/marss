
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <basecore.h>
#include <globals.h>
#include <decode.h>

using namespace Core;

BaseCore::BaseCore(BaseMachine& machine, const char* name)
    : Statable(name, &machine)
      , machine(machine)
{
    coreid = machine.get_next_coreid();
}

void BaseCore::update_memory_hierarchy_ptr() {
    memoryHierarchy = machine.memoryHierarchyPtr;
}

extern "C" void ptl_flush_bbcache(int8_t context_id) {
    if(in_simulation) {
      foreach(i, NUM_SIM_CORES) {
        bbcache[i].flush(context_id);
        // Get the current ptlsim machine and call its flush tlb
        PTLsimMachine* machine = PTLsimMachine::getcurrent();

        if(machine) {
            Context& ctx = machine->contextof(context_id);
            machine->flush_tlb(ctx);
        }
      }
    }
}
