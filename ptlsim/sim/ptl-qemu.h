
#ifndef PTL_QEMU_H
#define PTL_QEMU_H

#include <cpu.h>

// This file is included from QEMU to call PTLsim related
// functions.  So we have to make sure that we don't add
// any C++ related code.

// ptl_machine_init 
// config_str	: contains the configuration options of PTLsim
// returns void :
// working		: Setup the global variable config (type of PTLConfig) from the
//				  configuration string passed in 'config_str' argument.  
void ptl_machine_init(const char* config_str);

// ptl_create_new_context
// returns CPUX86Context*	: a pointer to newly created CPU Context
// working					: This function will create a new CPU Context, add
//							  it to the array of contexts in ptl_machine and 
//							  returns the CPUX86Context* of that Context
CPUX86State* ptl_create_new_context();

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
bool ptl_simulate();

#endif  // PTL_QEMU_H
