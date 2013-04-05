
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <machine.h>
#include <ptlsim.h>
#include <config.h>

#include <basecore.h>
#include <statsBuilder.h>
#include <memoryHierarchy.h>

#include <cstdarg>

using namespace Core;
using namespace Memory;

/* Machine Generator Functions */
MachineBuilder machineBuilder("_default_", NULL);
BaseMachine coremodel("base");


BaseMachine::BaseMachine(const char *name)
{
    machine_name = name;
    addmachine(machine_name, this);

    stringbuf stats_name;
    stats_name << "base_machine";
    update_name(stats_name.buf);

    context_used = 0;
    coreid_counter = 0;
}

BaseMachine::~BaseMachine()
{
    removemachine(machine_name, this);
}

void BaseMachine::reset()
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

void BaseMachine::shutdown()
{
	foreach (i, cores.count()) {
		BaseCore* core = cores[i];
		delete core;
	}

	cores.clear();

	foreach (i, controllers.count()) {
		Controller* cont = controllers[i];
		delete cont;
	}

	controllers.clear();

	foreach (i, interconnects.count()) {
		Interconnect* intercon = interconnects[i];
		delete intercon;
	}

	interconnects.clear();

	if (memoryHierarchyPtr) {
		delete memoryHierarchyPtr;
		memoryHierarchyPtr = NULL;
	}
}

/**
 * @brief Simulation runtime configuration is changed
 *
 * All modules are notified that configuration is changed so they can update
 * their state if needed.
 */
void BaseMachine::config_changed()
{
#define BUILDER_CONFIG_CHANGED(BuilderType, builders) \
	{ \
		Hashtable<const char*, BuilderType*, 1>::Iterator iter(BuilderType::builders); \
		KeyValuePair<const char*, BuilderType*> *kv; \
		while ((kv = iter.next())) { \
			kv->value->config_changed(); \
		} \
	}

	BUILDER_CONFIG_CHANGED(CoreBuilder, coreBuilders);
	BUILDER_CONFIG_CHANGED(ControllerBuilder, controllerBuilders);
	BUILDER_CONFIG_CHANGED(InterconnectBuilder, interconnectBuilders);
}

W8 BaseMachine::get_num_cores()
{
    return cores.count();
}

bool BaseMachine::init(PTLsimConfig& config)
{
    // At the end create a memory hierarchy
    memoryHierarchyPtr = new MemoryHierarchy(*this);

    if(config.machine_config == "") {
        ptl_logfile << "[ERROR] Please provide Machine name in config using -machine\n" << flush;
        cerr << "[ERROR] Please provide Machine name in config using -machine\n" << flush;
        return 0;
    }

    machineBuilder.setup_machine(*this, config.machine_config.buf);

    foreach(i, cores.count()) {
        cores[i]->update_memory_hierarchy_ptr();
    }

    init_qemu_io_events();

    return 1;
}

/**
 * @brief Dump Machine and all module configuration
 *
 * @param os Output stream
 *
 * This function iterates over all modules in Machine and dump each module's
 * configuration in YAML format. Configuration tree structure is like below:
 *
 * machine:
 *	name: Machine Name
 *	cpu_contexts: Total CPU Contexts that are simulated
 *	freq: Simulated Machine Core Frequency
 *	module0:
 *		module0 specific Parameters
 *	module1:
 *		module1 Specific Parameters
 *
 */
void BaseMachine::dump_configuration(ostream& os) const
{
	YAML::Emitter *config_yaml;

	os << "#\n# Simulated Machine Configuration\n#\n";

	config_yaml = new YAML::Emitter();

	*config_yaml << YAML::BeginMap;
	*config_yaml << YAML::Key << "machine";
	*config_yaml << YAML::Value << YAML::BeginMap;

	/* Some machine specific parameters */
	*config_yaml << YAML::Key << "name" << YAML::Value << config.machine_config;
	*config_yaml << YAML::Key << "cpu_contexts" << YAML::Value << NUM_SIM_CORES;
	*config_yaml << YAML::Key << "freq" << YAML::Value << config.core_freq_hz;

	/* Now go through all cores */
	foreach (i, cores.count())
		cores[i]->dump_configuration(*config_yaml);

	/* Next is all controllers/caches */
	foreach (i, controllers.count())
		controllers[i]->dump_configuration(*config_yaml);

	/* Now dump all interconnections */
	foreach (i, interconnects.count())
		interconnects[i]->dump_configuration(*config_yaml);

	/* Finalize YAML */
	*config_yaml << YAML::EndMap;
	*config_yaml << YAML::EndMap;

	os << config_yaml->c_str() << "\n";
	os << "\n# End Machine Configuration\n";

	ptl_logfile << "Dumped all machine configuration\n";
}

int BaseMachine::run(PTLsimConfig& config)
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

        if unlikely(sim_cycle == 0 && time_stats_file)
            StatsBuilder::get().dump_header(*time_stats_file);

        if unlikely (time_stats_file && sim_cycle > 0 &&
                sim_cycle % config.time_stats_period == 0) {
            StatsBuilder::get().dump_periodic(*time_stats_file, sim_cycle);
        }


        // limit the ptl_logfile size
        if unlikely (ptl_logfile.is_open() &&
                ((W64)ptl_logfile.tellp() > config.log_file_size))
            backup_and_reopen_logfile();

        memoryHierarchyPtr->clock();
        clock_qemu_io_events();

		foreach (i, coremodel.per_cycle_signals.size()) {
			if (logable(4))
				ptl_logfile << "Per-Cycle-Signal : " <<
					coremodel.per_cycle_signals[i]->get_name() << endl;
			exiting |= coremodel.per_cycle_signals[i]->emit(NULL);
		}

        sim_cycle++;
        iterations++;

        if unlikely (config.stop_at_insns <= total_insns_committed ||
                config.stop_at_cycle <= sim_cycle) {
            ptl_logfile << "Stopping simulation loop at specified limits (", sim_cycle, " cycles, ", total_insns_committed, " commits)", endl;
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
        ptl_logfile << "Exiting out-of-order core at ", total_insns_committed, " commits, ", total_uops_committed, " uops and ", iterations, " iterations (cycles)", endl;

    config.dump_state_now = 0;

    return exiting;
}

void BaseMachine::flush_tlb(Context& ctx)
{
    foreach(i, cores.count()) {
        BaseCore* core = cores[i];
        core->flush_tlb(ctx);
    }
}

void BaseMachine::flush_tlb_virt(Context& ctx, Waddr virtaddr)
{
    foreach(i, cores.count()) {
        BaseCore* core = cores[i];
        core->flush_tlb_virt(ctx, virtaddr);
    }
}

void BaseMachine::dump_state(ostream& os)
{
    foreach(i, cores.count()) {
        cores[i]->dump_state(os);
    }

    os << " MemoryHierarchy:", endl;
    memoryHierarchyPtr->dump_info(os);
}

void BaseMachine::flush_all_pipelines()
{
    // TODO
}

void BaseMachine::update_stats()
{
    global_stats->reset();
    *global_stats += *user_stats;
    *global_stats += *kernel_stats;

    foreach(i, cores.count()) {
        cores[i]->update_stats();
    }
}

Context& BaseMachine::get_next_context()
{
    assert(context_counter < NUM_SIM_CORES);
    assert(context_counter < MAX_CONTEXTS);
    context_used[context_counter] = 1;
    return contextof(context_counter++);
}

W8 BaseMachine::get_next_coreid()
{
    assert((coreid_counter) < MAX_CONTEXTS);
    return coreid_counter++;
}

ConnectionDef* BaseMachine::get_new_connection_def(const char* interconnect,
        const char* name, int id)
{
    ConnectionDef* conn = new ConnectionDef();
    conn->interconnect = interconnect;
    conn->name << name << id;
    connections.push(conn);
    return conn;
}

void BaseMachine::add_new_connection(ConnectionDef* conn,
        const char* cont, int type)
{
    SingleConnection* sg = new SingleConnection();
    sg->controller = cont;
    sg->type = type;

    conn->connections.push(sg);
}

void BaseMachine::setup_interconnects()
{
    foreach(i, connections.count()) {
        ConnectionDef* connDef = connections[i];

        InterconnectBuilder** builder = 
            InterconnectBuilder::interconnectBuilders->get(connDef->interconnect);

        if(!builder) {
            stringbuf err;
            err << "::ERROR::Can't find Interconnect Builder '"
                << connDef->interconnect
                << "'. Please check your config file." << endl;
            ptl_logfile << err;
            cout << err;
            assert(builder);
        }

        Interconnect* interCon = (*builder)->get_new_interconnect(
                *memoryHierarchyPtr,
                connDef->name.buf);
        interconnects.push(interCon);

        foreach(j, connDef->connections.count()) {
            SingleConnection* sg = connDef->connections[j];

            Controller** cont = controller_hash.get(
                    sg->controller);
            assert(cont);

            interCon->register_controller(*cont);
            (*cont)->register_interconnect(interCon, sg->type);
        }
    }
}

void BaseMachine::add_option(const char* name, const char* opt,
        bool value)
{
    BoolOptions** b = bool_options.get(name);
    if(!b) {
        BoolOptions* opts = new BoolOptions();
        bool_options.add(name, opts);
        b = &opts;
    }

    (*b)->add(opt, value);
}

void BaseMachine::add_option(const char* name, const char* opt,
        int value)
{
    IntOptions** b = int_options.get(name);
    if(!b) {
        IntOptions* opts = new IntOptions();
        int_options.add(name, opts);
        b = &opts;
    }

    (*b)->add(opt, value);
}

void BaseMachine::add_option(const char* name, const char* opt,
        const char* value)
{
    StrOptions** b = str_options.get(name);
    if(!b) {
        StrOptions* opts = new StrOptions();
        str_options.add(name, opts);
        b = &opts;
    }

    stringbuf* val = new stringbuf();
    *val << value;
    (*b)->add(opt, val);
}

void BaseMachine::add_option(const char* c_name, int i, const char* opt,
        bool value)
{
    stringbuf core_name;
    core_name << c_name << i;
    add_option(core_name.buf, opt, value);
}

void BaseMachine::add_option(const char* c_name, int i, const char* opt,
        int value)
{
    stringbuf core_name;
    core_name << c_name << i;
    add_option(core_name.buf, opt, value);
}

void BaseMachine::add_option(const char* c_name, int i, const char* opt,
        const char* value)
{
    stringbuf core_name;
    core_name << c_name << i;
    add_option(core_name.buf, opt, value);
}

bool BaseMachine::get_option(const char* name, const char* opt_name,
        bool& value)
{
    BoolOptions** b = bool_options.get(name);
    if(b) {
        bool* bt = (*b)->get(opt_name);
        if(bt) {
            value = *bt;
            return true;
        }
    }

    return false;
}

bool BaseMachine::get_option(const char* name, const char* opt_name,
        int& value)
{
    IntOptions** b = int_options.get(name);
    if(b) {
        int* bt = (*b)->get(opt_name);
        if(bt) {
            value = *bt;
            return true;
        }
    }

    return false;
}

bool BaseMachine::get_option(const char* name, const char* opt_name,
        stringbuf& value)
{
    StrOptions** b = str_options.get(name);
    if(b) {
        stringbuf** bt = (*b)->get(opt_name);
        if(bt) {
            value << **bt;
            return true;
        }
    }

    return false;
}

/* Machine Builder */
MachineBuilder::MachineBuilder(const char* name, machine_gen gen)
{
    if(!machineBuilders) {
        machineBuilders = new Hashtable<const char*, machine_gen, 1>();
    }
    machineBuilders->add(name, gen);
}

void MachineBuilder::setup_machine(BaseMachine &machine, const char* name)
{
    machine_gen* gen = machineBuilders->get(name);
    if(!gen) {
        stringbuf err;
        err << "::ERROR::Can't find '" << name
           << "' machine generator." << endl;
        ptl_logfile << err;
        cerr << err;
        assert(gen);
    }

    (*gen)(machine);
}

stringbuf& MachineBuilder::get_all_machine_names(stringbuf& names)
{
    dynarray< KeyValuePair<const char*, machine_gen> > machines;
    machines = machineBuilders->getentries(machines);

    foreach(i, machines.count()) {
        if(machines[i].value)
            names << machines[i].key << ", ";
    }

    return names;
}

Hashtable<const char*, machine_gen, 1> *MachineBuilder::machineBuilders = NULL;

/* CoreBuilder */

CoreBuilder::CoreBuilder(const char* name)
{
    if(!coreBuilders) {
        coreBuilders = new Hashtable<const char*, CoreBuilder*, 1>();
    }
    coreBuilders->add(name, this);
}

Hashtable<const char*, CoreBuilder*, 1> *CoreBuilder::coreBuilders = NULL;

void CoreBuilder::add_new_core(BaseMachine& machine,
        const char* name, const char* core_name)
{
    stringbuf core_name_t;
    ptl_logfile << name;
    core_name_t << name << machine.coreid_counter;
    CoreBuilder** builder = coreBuilders->get(core_name);

    if(!builder) {
        stringbuf err;
        err << "::ERROR::Can't find Core Builder '" << core_name
            << "'. Please check your config file." << endl;
        ptl_logfile << err;
        cout << err;
        assert(builder);
    }

    BaseCore* core = (*builder)->get_new_core(machine, core_name_t.buf);
    machine.cores.push(core);
}

/* Cache Controller Builders */

ControllerBuilder::ControllerBuilder(const char* name)
{
    if(!controllerBuilders) {
        controllerBuilders = new Hashtable<const char*, ControllerBuilder*, 1>();
    }
    controllerBuilders->add(name, this);
}

Hashtable<const char*, ControllerBuilder*, 1>
    *ControllerBuilder::controllerBuilders = NULL;

void ControllerBuilder::add_new_cont(BaseMachine& machine, W8 coreid,
        const char* name, const char* cont_name, W8 type)
{
    stringbuf cont_name_t;
    cont_name_t << name << coreid;
    ControllerBuilder** builder = ControllerBuilder::controllerBuilders->get(cont_name);

    if(!builder) {
        stringbuf err;
        err << "::ERROR::Can't find Controller Builder '" << cont_name
            << "'. Please check your config file." << endl;
        ptl_logfile << err;
        cout << err;
        assert(builder);
    }

    Controller* cont = (*builder)->get_new_controller(coreid, type,
            *machine.memoryHierarchyPtr, cont_name_t.buf);
    machine.controllers.push(cont);
    machine.controller_hash.add(cont_name_t, cont);
}

/* Cache Interconnect Builders */

InterconnectBuilder::InterconnectBuilder(const char* name)
{
    if(!interconnectBuilders) {
        interconnectBuilders = new Hashtable<const char*,
            InterconnectBuilder*, 1>();
    }
    interconnectBuilders->add(name, this);
}

Hashtable<const char*, InterconnectBuilder*, 1>
    *InterconnectBuilder::interconnectBuilders = NULL;

void InterconnectBuilder::create_new_int(BaseMachine& machine, W8 id,
        const char* name, const char* int_name, int count, ...)
{
    va_list ap;
    char* controller_name;
    int conn_type;
    stringbuf int_name_t;

    int_name_t << name << id;
    InterconnectBuilder** builder = 
        InterconnectBuilder::interconnectBuilders->get(int_name);
    assert(builder);

    Interconnect* interCon = (*builder)->get_new_interconnect(
            *machine.memoryHierarchyPtr, int_name_t.buf);
    machine.interconnects.push(interCon);

    va_start(ap, count);
    foreach(i, count*2) {
        controller_name = va_arg(ap, char*); 
        assert(controller_name);

        Controller** cont = machine.controller_hash.get(
                controller_name);
        assert(cont);

        conn_type = va_arg(ap, int);

        interCon->register_controller(*cont);
        (*cont)->register_interconnect(interCon, conn_type);
    }
    va_end(ap);
}

extern "C" {

/**
 * @brief Add an Event to simulate after specific cycles
 *
 * @param signal Call signal's callback when event is simualted
 * @param delay Number of cycles to delay the event
 * @param arg Argument passed to callback function
 */
void marss_add_event(Signal* signal, int delay, void* arg)
{
	coremodel.memoryHierarchyPtr->add_event(signal, delay, arg);
}

/**
 * @brief Register Signal to call at each cycle
 *
 * @param signal Signal object to register
 *
 * Use this registration function to add an event that will be executed at each
 * simulation cycle.
 */
void marss_register_per_cycle_event(Signal *signal)
{
	coremodel.per_cycle_signals.push(signal);
}

} // extern "C"
