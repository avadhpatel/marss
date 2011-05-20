
#include <basecore.h>
#include <globals.h>
#include <stats.h>

#define INSIDE_DEFCORE
#include <defcore.h>

#include <atomcore.h>

using namespace Core;

BaseCore::BaseCore(BaseMachine& machine)
    : machine(machine)
{
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
