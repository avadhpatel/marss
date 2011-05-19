
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
