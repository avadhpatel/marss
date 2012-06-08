
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef MACHINE_H
#define MACHINE_H

#include <ptlsim.h>

#define YAML_KEY_VAL(out, key, val) \
	out << YAML::Key << key << YAML::Value << val;

#define THREAD_PAUSE_CYCLES 10000

namespace Core {
    struct BaseCore;
};

namespace Memory {
    struct Controller;
    struct Interconnect;
    struct MemoryHierarchy;
};

typedef Hashtable<const char*, bool, 1> BoolOptions;
typedef Hashtable<const char*, int, 1> IntOptions;
typedef Hashtable<const char*, stringbuf*, 1> StrOptions;

struct SingleConnection {
    stringbuf controller;
    int type;
};

struct ConnectionDef {
    stringbuf interconnect;
    stringbuf name;
    dynarray<SingleConnection*> connections;
};

struct BaseMachine: public PTLsimMachine {
    dynarray<Core::BaseCore*> cores;
    dynarray<Memory::Controller*> controllers;
    dynarray<Memory::Interconnect*> interconnects;
    dynarray<ConnectionDef*> connections;
	dynarray<Signal*> per_cycle_signals;

    Hashtable<const char*, Memory::Controller*, 1> controller_hash;
    Hashtable<const char*, BoolOptions*, 1> bool_options;
    Hashtable<const char*, IntOptions*, 1> int_options;
    Hashtable<const char*, StrOptions*, 1> str_options;

    Memory::MemoryHierarchy* memoryHierarchyPtr;

    BaseMachine(const char* name);
    virtual bool init(PTLsimConfig& config);
    virtual int run(PTLsimConfig& config);
    virtual W8 get_num_cores();
    virtual void dump_state(ostream& os);
    virtual void update_stats();
    virtual void flush_tlb(Context& ctx);
    virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
    void flush_all_pipelines();
    virtual void reset();
	virtual void dump_configuration(ostream& os) const;
	virtual void shutdown();
    virtual ~BaseMachine();

    bitvec<NUM_SIM_CORES> context_used;
    W8 context_counter;
    W8 coreid_counter;

    Context& get_next_context();
    W8 get_next_coreid();
	void config_changed();

    // Interconnect related support functions
    ConnectionDef* get_new_connection_def(const char* interconnect,
            const char* name, int id);
    void add_new_connection(ConnectionDef* conn, const char* cont, int type);
    void setup_interconnects();

    // Options related support functions
    void add_option(const char* name, const char* opt_name,
            bool value);
    void add_option(const char* name, const char* opt_name,
            int value);
    void add_option(const char* name, const char* opt_name,
            const char* value);
    void add_option(const char* core_name, int i, const char* opt_name,
            bool value);
    void add_option(const char* core_name, int i, const char* opt_name,
            int value);
    void add_option(const char* core_name, int i, const char* opt_name,
            const char* value);

    bool has_option(const char* name, const char* opt_name);

    bool get_option(const char* name, const char* opt_name, bool& value);
    bool get_option(const char* name, const char* opt_name, int& value);
    bool get_option(const char* name, const char* opt_name, stringbuf& value);
};

typedef void (*machine_gen)(BaseMachine& machine);

struct MachineBuilder {
    MachineBuilder(const char* name, machine_gen gen);
    static stringbuf& get_all_machine_names(stringbuf& names);
    void setup_machine(BaseMachine &machine, const char* name);
    static Hashtable<const char*, machine_gen, 1> *machineBuilders;
};

struct CoreBuilder {
    CoreBuilder(const char* name);
    virtual Core::BaseCore* get_new_core(BaseMachine& machine,
            const char* name) = 0;
    static Hashtable<const char*, CoreBuilder*, 1> *coreBuilders;
    static void add_new_core(BaseMachine& machine, const char* name,
            const char* core_name);
	virtual void config_changed() {}
};

struct ControllerBuilder {
    ControllerBuilder(const char* name);
    virtual Memory::Controller* get_new_controller(W8 coreid, W8 type,
            Memory::MemoryHierarchy& mem, const char* name) = 0;
    static Hashtable<const char*, ControllerBuilder*, 1> *controllerBuilders;
    static void add_new_cont(BaseMachine& machine, W8 coreid,
            const char* name, const char* cont_name, W8 type);
	virtual void config_changed() {}
};

struct InterconnectBuilder {
    InterconnectBuilder(const char* name);
    virtual Memory::Interconnect* get_new_interconnect(
            Memory::MemoryHierarchy& mem, const char* name) = 0;
    static Hashtable<const char*, InterconnectBuilder*, 1> *interconnectBuilders;
    static void create_new_int(BaseMachine& machine, W8 id,
            const char* name, const char* int_name, int count, ...);
	virtual void config_changed() {}
};

extern "C" {
void marss_add_event(Signal* signal, int delay, void* arg);
void marss_register_per_cycle_event(Signal *signal);
}

#endif // MACHINE_H
