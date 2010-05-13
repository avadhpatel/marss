
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifndef PTL_QEMU_H
#define PTL_QEMU_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file is included from QEMU to call PTLsim related
 * functions.  So we have to make sure that we don't add
 * any C++ related code.
 */

/*
 * sim_cycle
 * type		: W64 (unsigned long long)
 * working	: This variable represents a simulation clock cycle in PTLsim and
 *              it is used by QEMU to calculate wall clock time in simulation
 *              mode
 */
typedef unsigned long long W64;
extern W64 sim_cycle;

/*
 * in_simulation
 * type		: bool
 * working	: Indicate if currently execuing in simulation or not
 */
extern uint8_t in_simulation;

/*
 * start_simulation
 * type		: bool
 * working	: Indicates that next loop should be started in simulation
 */
extern uint8_t start_simulation;

/*
 * simulation_configured
 * type     : bool
 * working  : Indicates that simuation module is configured from monitor thread
 */
extern uint8_t simulation_configured;

/*
 * inside_simulation
 * type		: bool
 * working	: Indicates if we are in simulation mode or not, implemented in
 *             ptlsim.cpp file
 */
extern uint8_t inside_ptlsim;

/*
 * ptlsim_init
 * returns void
 * working		: This function setup a MMIO region in QEMU which is used by
 *                 process in virtual machine to communicate with PTLsim, for
 *                 example, switch between simulation modes or change any
 *                 configuration option.  It also registers a chunk of memory in
 *                 RAM used by memory manager of PTLsim.
 */
void ptlsim_init(void);

/*
 * ptl_machine_configure
 * config_str	: contains the configuration options of PTLsim
 * returns void :
 * working		: Setup the global variable config (type of PTLConfig) from the
 *                 configuration string passed in 'config_str' argument.  This
 *                 function is called when QEMU gets the ptlsim configuration
 *                 and its about to change to simulation mode
 */
void ptl_machine_configure(const char* config_str);

/*
 * ptl_create_new_context
 * returns CPUX86Context*	: a pointer to newly created CPU Context
 * working					: This function will create a new CPU Context, add
 *                             it to the array of contexts in ptl_machine and
 *                             returns the CPUX86Context* of that Context
 */
struct CPUX86State;
CPUX86State* ptl_create_new_context(void);

/*
 * ptl_reconfigure
 * config_str	: string containing new configuration options for PTLsim
 * returns void :
 * working		: It will change the config variable to reflect the new
 *                 configuration options for PTLsim.
 */
void ptl_reconfigure(char* config_str);

/*
 * ptl_config_from_file
 * filename     : input file that contains simulation config options
 * returns void
 * working      : Read the file given in filename and parse the simulation
 *                configuration options
 */
void ptl_config_from_file(char *filename);

/*
 * ptl_simulate
 * returns bool : 0 indicates that simulation is completed don't return
 *                 1 indicates simulation is remaining, return to simulation
 *                 when interrupt/exception is handled.
 * working		: This function is called by QEMU everytime we switch from QEMU
 *                 to PTLsim.  First time its called, we setup the simulation
 *                 core and start simulating untill there is an
 *                 interrupt/exception is pending, as all the
 *                 interrupts/exceptions are handled by QEMU.  It will also
 *                 return when it reaches the no of instructions we specified to
 *                 simulate.
 */
uint8_t ptl_simulate(void);

/*
 * update_progress
 * returns void
 * working		: Print the progress of PTLSim to stdout
 */
void update_progress(void);

/*
 * ptl_cpuid
 * index		: requested cpuid index value
 * count		: requested count value in cpuid
 * eax			: value to return in eax
 * ebx			: value to return in ebx
 * ecx			: value to return in ecx
 * edx			: value to return in edx
 * return int	: 1 indicate if PTLsim has handled the cpuid and fill the
 *                 required values into the registers and 0 indicates its not
 *                 handled by PTLsim
 */
int ptl_cpuid(uint32_t index, uint32_t count, uint32_t *eax, uint32_t *ebx,
		uint32_t *ecx, uint32_t *edx);

/*
 * ptl_flush_bbcache
 * context_id	: ID of the context of which the BasicBlockCache will be
 *				  flushed
 * working		: Flush all the decoded cache on tlb flush
 */
void ptl_flush_bbcache(int8_t context_id);

/*
 * ptl_check_ptlcall_queue
 * returns void
 * working		: Execute any pending PTLCalls from VM
 */
void ptl_check_ptlcall_queue(void);

extern uint8_t ptl_stable_state;

/*
 * qemu_take_screenshot
 * filename     : Name of the file to store screenshot of VGA screen
 * returns void
 * working      : Takes a screenshot of VM window and save to a file
 */
void qemu_take_screenshot(char* filename);

/*
 * ptl_start_sim_rip
 * RIP location from where to switch to simulation
 */
extern uint64_t ptl_start_sim_rip;

#ifdef __cplusplus
}
#endif

#endif  // PTL_QEMU_H
