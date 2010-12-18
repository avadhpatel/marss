
#include <basecore.h>
#include <globals.h>
#include <stats.h>

#define INSIDE_DEFCORE
#include <defcore.h>

#include <atomcore.h>

using namespace Core;

BaseCore::BaseCore(BaseCoreMachine& machine)
    : machine(machine)
{
}

void BaseCore::update_memory_hierarchy_ptr() {
    memoryHierarchy = machine.memoryHierarchyPtr;
}

BaseCoreMachine::BaseCoreMachine(const char *name)
{
    cerr << "initing the Base Machien", endl;
    machine_name = name;
    addmachine(machine_name, this);

    stringbuf stats_name;
    stats_name << "base_machine";
    update_name(stats_name.buf);

    context_used = 0;
    coreid_counter = 0;
}

BaseCoreMachine::~BaseCoreMachine()
{
    removemachine(machine_name, this);
}

void BaseCoreMachine::reset()
{
    context_used = 0;
    context_counter = 0;
    coreid_counter = 0;

    foreach(i, cores.count()) {
        BaseCore* core = cores[i];
        delete core;
    }

    cores.clear();

    if(memoryHierarchyPtr) {
        delete memoryHierarchyPtr;
        memoryHierarchyPtr = NULL;
    }
}

W8 BaseCoreMachine::get_num_cores()
{
    return cores.count();
}

bool BaseCoreMachine::init(PTLsimConfig& config)
{
    int context_idx = 0;

    // Based on the config option we create cores
    //

    if(!strcmp(config.core_config, "default")) {
        setup_default_cores();
    } else if(!strcmp(config.core_config, "ht-smt")) {
        setup_smt_mc_cores();
    } else if(config.core_config == "atom") {
        setup_atom_cores();
    } else {
        assert(0 && "Unknown type of core config");
    }

    // Make sure that all cores are initialized
    assert(context_used.allset());

    // At the end create a memory hierarchy
    memoryHierarchyPtr = new Memory::MemoryHierarchy(*this);

    foreach(i, cores.count()) {
        cores[i]->update_memory_hierarchy_ptr();
    }

    return 1;
}

int BaseCoreMachine::run(PTLsimConfig& config)
{
    if(logable(1))
        ptl_logfile << "Starting base core toplevel loop", endl, flush;

    // All VCPUs are running:
    stopped = 0;
    if unlikely (config.start_log_at_iteration &&
            iterations >= config.start_log_at_iteration &&
            !config.log_user_only) {

        if unlikely (!logenable)
            ptl_logfile << "Start logging at level ",
                        config.loglevel, " in cycle ",
                        iterations, endl, flush;

        logenable = 1;
    }

    // reset all cores for fresh start:
    foreach (cur_core, cores.count()){
        BaseCore& core =* cores[cur_core];
        if(first_run) {
            cores[cur_core]->reset();
        }
        cores[cur_core]->check_ctx_changes();
    }
    first_run = 0;

    // Run each core
    bool exiting = false;

    for (;;) {
        if unlikely ((!logenable) &&
                iterations >= config.start_log_at_iteration &&
                !config.log_user_only) {
            ptl_logfile << "Start logging at level ", config.loglevel,
                        " in cycle ", iterations, endl, flush;
            logenable = 1;
        }

        if(sim_cycle % 1000 == 0)
            update_progress();

        // limit the ptl_logfile size
        if unlikely (ptl_logfile.is_open() &&
                (ptl_logfile.tellp() > config.log_file_size))
            backup_and_reopen_logfile();

        memoryHierarchyPtr->clock();

        foreach (cur_core, cores.count()){
            BaseCore& core =* cores[cur_core];

            if(logable(4))
                ptl_logfile << "cur_core: ", cur_core, " running [core ",
                            core.get_coreid(), "]", endl;
            exiting |= core.runcycle();
        }

        global_stats.summary.cycles++;
        sim_cycle++;
        iterations++;

        if unlikely (config.wait_all_finished ||
                config.stop_at_user_insns <= total_user_insns_committed){
            ptl_logfile << "Stopping simulation loop at specified limits (", iterations, " iterations, ", total_user_insns_committed, " commits)", endl;
            exiting = 1;
            break;
        }
        if unlikely (exiting) {
            if unlikely(ret_qemu_env == NULL)
                ret_qemu_env = &contextof(0);
            break;
        }
    }

    if(logable(1))
        ptl_logfile << "Exiting out-of-order core at ", total_user_insns_committed, " commits, ", total_uops_committed, " uops and ", iterations, " iterations (cycles)", endl;

    config.dump_state_now = 0;

    return exiting;
}

void BaseCoreMachine::flush_tlb(Context& ctx)
{
    foreach(i, cores.count()) {
        BaseCore* core = cores[i];
        core->flush_tlb(ctx);
    }
}

void BaseCoreMachine::flush_tlb_virt(Context& ctx, Waddr virtaddr)
{
    foreach(i, cores.count()) {
        BaseCore* core = cores[i];
        core->flush_tlb_virt(ctx, virtaddr);
    }
}

void BaseCoreMachine::dump_state(ostream& os)
{
    foreach(i, cores.count()) {
        cores[i]->dump_state(os);
    }

    os << " MemoryHierarchy:", endl;
    memoryHierarchyPtr->dump_info(os);
}

void BaseCoreMachine::flush_all_pipelines()
{
    // TODO
}

void BaseCoreMachine::update_stats(PTLsimStats* stats)
{
    // TODO
}

Context& BaseCoreMachine::get_next_context()
{
    assert(context_counter < NUM_SIM_CORES);
    assert(context_counter < MAX_CONTEXTS);
    context_used[context_counter] = 1;
    return contextof(context_counter++);
}

W8 BaseCoreMachine::get_next_coreid()
{
    assert((coreid_counter) < MAX_CONTEXTS);
    return coreid_counter++;
}

void BaseCoreMachine::setup_default_cores()
{
    // This function set default cores for all the unused Contexts

    for(int i = 0; i < NUM_SIM_CORES; i++) {
        if(context_used[i]) {
            continue;
        }

        // From default-core create an ooo core

        DefaultCoreModel::DefaultCore *core = new DefaultCoreModel::DefaultCore(*this, 1);
        cores.push((BaseCore*)core);
    }
}

void BaseCoreMachine::setup_smt_mc_cores()
{
    // We create cores in bunch of 4 - 1 2thread smt core and 2 1thread cores

    assert(NUM_SIM_CORES % 4 == 0);

    for(int i = 0; i < NUM_SIM_CORES; i += 4) {

        DefaultCoreModel::DefaultCore *core = new DefaultCoreModel::DefaultCore(*this, 2);
        cores.push((BaseCore*)core);

        core = new DefaultCoreModel::DefaultCore(*this, 1);
        cores.push((BaseCore*)core);

        core = new DefaultCoreModel::DefaultCore(*this, 1);
        cores.push((BaseCore*)core);
    }
}

void BaseCoreMachine::setup_atom_cores()
{
    // We create single threaded atom-cores

    foreach(i, NUM_SIM_CORES) {
        AtomCoreModel::AtomCore *core = new AtomCoreModel::AtomCore(*this, 1);
        cores.push((BaseCore*)core);
    }
}

BaseCoreMachine coremodel("base");
