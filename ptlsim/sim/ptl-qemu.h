
#ifndef PTL_QEMU_H
#define PTL_QEMU_H

//#ifdef __cplusplus
//#include <globals.h>
//extern "C" {
//#endif
//#include <cpu.h>
//#ifdef __cplusplus
//}
//#endif

#ifdef __cplusplus
extern "C" {
#endif

// This file is included from QEMU to call PTLsim related
// functions.  So we have to make sure that we don't add
// any C++ related code.

// sim_cycle
// type		: W64 (unsigned long long)
// working	: This variable represents a simulation clock cycle in PTLsim and
//			  it is used by QEMU to calculate wall clock time in simulation
//			  mode
typedef unsigned long long W64;
extern W64 sim_cycle;

// in_simulation
// type		: bool
// working	: Indicate if currently execuing in simulation or not
extern uint8_t in_simulation;

// start_simulation
// type		: bool
// working	: Indicates that next loop should be started in simulation
extern uint8_t start_simulation;

// inside_simulation
// type		: bool
// working	: Indicates if we are in simulation mode or not, implemented in
//			  ptlsim.cpp file
extern uint8_t inside_ptlsim;

// ptlsim_init
// returns void
// working		: This function setup a MMIO region in QEMU which is used by
//				  process in virtual machine to communicate with PTLsim, for
//				  example, switch between simulation modes or change any
//				  configuration option.  It also registers a chunk of memory in
//				  RAM used by memory manager of PTLsim.
//#ifdef __cplusplus
//extern "C" 
//#endif
void ptlsim_init(void);

// ptl_machine_init 
// config_str	: contains the configuration options of PTLsim
// returns void :
// working		: Setup the global variable config (type of PTLConfig) from the
//				  configuration string passed in 'config_str' argument.  This
//				  function is called when QEMU gets the ptlsim configuration
//				  and its about to change to simulation mode 
//#ifdef __cplusplus
//extern "C" 
//#endif
void ptl_machine_init(char* config_str);

// ptl_create_new_context
// returns CPUX86Context*	: a pointer to newly created CPU Context
// working					: This function will create a new CPU Context, add
//							  it to the array of contexts in ptl_machine and 
//							  returns the CPUX86Context* of that Context
struct CPUX86State;
CPUX86State* ptl_create_new_context(void);

// ptl_reconfigure
// config_str	: string containing new configuration options for PTLsim
// returns void :
// working		: It will change the config variable to reflect the new
//				  configuration options for PTLsim.
void ptl_reconfigure(char* config_str);

// ptl_simulate
// returns bool : 0 indicates that simulation is completed don't return
//				  1 indicates simulation is remaining, return to simulation
//				  when interrupt/exception is handled.
// working		: This function is called by QEMU everytime we switch from QEMU
//				  to PTLsim.  First time its called, we setup the simulation
//				  core and start simulating untill there is an
//				  interrupt/exception is pending, as all the
//				  interrupts/exceptions are handled by QEMU.  It will also
//				  return when it reaches the no of instructions we specified to
//				  simulate.
uint8_t ptl_simulate(void);

// update_progress
// returns void
// working		: Print the progress of PTLSim to stdout
void update_progress(void);

#ifdef __cplusplus
}
#endif

#endif  // PTL_QEMU_H
