
/* 
 * PQRS : A Full System Computer-Architecture Simulator
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
 * Some of the functions are referenced from 'qemu-kvm-ptlsim.c' file of project
 * 'PTLsim-KVM 3.0'.
 * They are modified to use QEMU CPU context by Avadh Patel
 * Copyright of those functions by Matt Yourst
 *
 */


#include <globals.h>
#include <ptlhwdef.h>

extern "C" {
#include <sysemu.h>
}

#include <ptl-qemu.h>
#include <ptlsim.h>

#include <cacheConstants.h>

#define __INSIDE_PTLSIM_QEMU__
#include <ptlcalls.h>
//
// Physical address of the PTLsim PTLCALL hypercall page
// used to communicate with the outside world:
//
#define PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR    0x00000008fffff000ULL

using namespace Memory;

uint8_t in_simulation = 0;
uint8_t start_simulation = 0;

static void ptlcall_mmio_write(CPUX86State* cpu, W64 offset, W64 value, 
		int length) {
	int calltype = (int)(cpu->regs[REG_rax]);
	W64 arg1 = cpu->regs[REG_rdi];
	W64 arg2 = cpu->regs[REG_rsi];
	W64 arg3 = cpu->regs[REG_rdx];
	W64 arg4 = cpu->regs[REG_r10];
	W64 arg5 = cpu->regs[REG_r8];
	W64 arg6 = cpu->regs[REG_r9];

	cout << "ptlcall_mmio_write: calltype ", calltype, " at rip ", cpu->eip,
		 " (inside_ptlsim = ", in_simulation, " )", endl;

//	if(!in_simulation) {
		switch(calltype) {
			case PTLCALL_VERSION: {
				cout << "PTLCALL type PTLCALL_VERSION\n";
				cpu->regs[REG_rax] = 1;
				break;
			  }
			case PTLCALL_MARKER: {
				cout << "PTLCALL type PTLCALL_MARKER\n";
				cpu->regs[REG_rax] = 0;
				break;
			  }
			case PTLCALL_ENQUEUE: {
				cout << "PTLCALL type PTLCALL_ENQUEUE\n";

				// Address of the command is stored in arg1 and
				// length of the command string is in arg2

				PTLsimCommandDescriptor desc;
				desc.command = (W64)(ldq_kernel(arg1));
				desc.length = (W64)(ldq_kernel(arg1 + 8));

				char *command_str = (char*)qemu_malloc(desc.length);
				char *command_addr = (char*)(desc.command);
				int i = 0;
				for(i=0; i < desc.length; i++) {
					command_str[i] = (char)ldub_kernel(
							(target_ulong)(command_addr));
					command_addr++;
				}
				command_str[i] = '\0';

				cout << "PQRS::Command received : ", command_str, endl;
				// Stop the QEMU vm and change ptlsim configuration
				// QEMU will be automatically started in simulation mode
				vm_stop(0);
				ptl_reconfigure(command_str);
				cpu_interrupt(cpu, CPU_INTERRUPT_EXIT);
				break;
			  }
			case PTLCALL_CHECKPOINT: {
				cout << "PTLCALL type PTLCALL_CHECKPOINT\n";

				char *checkpoint_name = (char*)qemu_malloc(arg2);
				char *name_addr = (char*)(arg1);
				for(int i=0; i < arg2; i++) {
					checkpoint_name[i] = (char)ldub_kernel(
							(target_ulong)(name_addr++));
				}

				vm_stop(0);
				if(arg3 != PTLCALL_CHECKPOINT_DUMMY) {
					cout << "PQRS::Creating checkpoint ", 
						 checkpoint_name, endl;

					do_savevm(checkpoint_name);
					cout << "PQRS::Checkpoint ", checkpoint_name,
						 " created\n";

				} else {
					cout << "PQRS::Reached to Chekpoint location..\n";
					cout << "PQRS::Now you can create your checkpont..\n";
					break;
				}

				switch(arg3) {
					case PTLCALL_CHECKPOINT_AND_CONTINUE:
						vm_start();
						break;
					case PTLCALL_CHECKPOINT_AND_SHUTDOWN:
						cout << "PQRS::Shutdown requested\n";
						exit(0);
						break;
					case PTLCALL_CHECKPOINT_AND_PAUSE:
						break;
					case PTLCALL_CHECKPOINT_AND_REBOOT:
						cout << "PQRS::Rebooting system\n";
						qemu_system_reset_request();
						break;
					default:
						cout << "PQRS::Unkonw Action\n";
						break;
				}

				break;
			  }
			default : 
				cout << "PTLCALL type unknown : ", calltype, endl;
				cpu->regs[REG_rax] = -EINVAL;
		}
//	} else {
//		cout << "PTLCALL called while inside simulation\n";
//	}
}

static uint32_t ptlcall_mmio_read(CPUX86State* cpu, W64 offset, int length) {
	cout << "PTLcall MMIO read on cpu: ", cpu->cpu_index, " (rip: ", cpu->eip,
		 " from page offset ", offset, " ,length ", length, endl;
	return 0;
}

static uint32_t ptlcall_mmio_readb(void *opaque, target_phys_addr_t addr) {
	return ptlcall_mmio_read(cpu_single_env, addr, 1);
}

static uint32_t ptlcall_mmio_readw(void *opaque, target_phys_addr_t addr) {
	return ptlcall_mmio_read(cpu_single_env, addr, 2);
}

static uint32_t ptlcall_mmio_readl(void *opaque, target_phys_addr_t addr) {
	return ptlcall_mmio_read(cpu_single_env, addr, 4);
}

static void ptlcall_mmio_writeb(void *opaque, target_phys_addr_t addr, 
		uint32_t value) {
	ptlcall_mmio_write(cpu_single_env, addr, value, 1);
}

static void ptlcall_mmio_writew(void *opaque, target_phys_addr_t addr, 
		uint32_t value) {
	ptlcall_mmio_write(cpu_single_env, addr, value, 2);
}

static void ptlcall_mmio_writel(void *opaque, target_phys_addr_t addr, 
		uint32_t value) {
	ptlcall_mmio_write(cpu_single_env, addr, value, 4);
}

static CPUReadMemoryFunc* ptlcall_mmio_read_ops[] = {
	ptlcall_mmio_readb,
	ptlcall_mmio_readw,
	ptlcall_mmio_readl,
};

static CPUWriteMemoryFunc* ptlcall_mmio_write_ops[] = {
	ptlcall_mmio_writeb,
	ptlcall_mmio_writew,
	ptlcall_mmio_writel,
};

void ptlsim_init() {

	// First allocate some memory for PTLsim for its own memory manager
	byte* ptlsim_ram_start = (byte*)qemu_malloc(PTLSIM_RAM_SIZE);
	assert(ptlsim_ram_start);
	byte* ptlsim_ram_end = ptlsim_ram_start + PTLSIM_RAM_SIZE;

	// Register PTLsim PTLCALL mmio page
	W64 ptlcall_mmio_pd = cpu_register_io_memory(0, ptlcall_mmio_read_ops, 
			ptlcall_mmio_write_ops, NULL);
	cpu_register_physical_memory(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, 4096,
			ptlcall_mmio_pd);

	cout << "ptlcall_mmio_init : Registered PTLcall MMIO page at physaddr ", 
		 PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, " descriptor ", ptlcall_mmio_pd, 
		 " io_mem_index ", ptlcall_mmio_pd >> IO_MEM_SHIFT, endl;
}

int ptl_cpuid(uint32_t index, uint32_t count, uint32_t *eax, uint32_t *ebx,
		uint32_t *ecx, uint32_t *edx) {

	// First check if the index is same as PTLSIM_CPUID_MAGIC
	if(index == PTLSIM_CPUID_MAGIC) {

		*eax = PTLSIM_CPUID_FOUND;
		*ebx = PTLCALL_METHOD_MMIO;
		*ecx = (W32)(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR);
		*edx = (W32)((PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR >> 32) & 
				0xffff) | 0x0;

		return 1;
	}

	// Check for CPUID requesting info that can be provided by the ptlsim
	// NOTE: Look at Intel x86 maual to find out how to set the return values
	// for the cache info and others
	
	uint32_t cores_info = 
		(((NUMBER_OF_THREAD_PER_CORE - 1) << 14) & 0x3fc000) |
		(((NUMBER_OF_CORES - 1) << 26) & 0xfc00000);
	switch(index) {
		case 4:
			// Cache info from OS
			// EAX : Bits		Info
			//		 4-0		0 = Null - no more cache
			//					1 = Data cache
			//					2 = Instruction cache
			//					3 = Unified cache
			//					4-31 = reserved
			//		 7-5		Cache Level (starts from 1)
			//		 8			Self initalizing cache
			//		 9			Fully Associative cache
			//		 25-14		Maximum number of IDs of logical 
			//					Processors sharing this cache
			//		 31-26		Maximum number of cores in package
			//
			// EBX : Bits		Info 
			//       11-0		Coherency line size
			//       21-12		Physical line partition
			//       31-22		Ways of Associativity
			//
			// ECX : Number of Sets
			// EDX : Bits		Info
			//		 0			Writeback/Invalid on sharing
			//		 1			Inclusive or not of lower caches
			switch(count) {
				case 0: { // L1-D cache info
					*eax = 0x121 | cores_info;
					*ebx = (L1D_LINE_SIZE & 0xfff |
							(L1D_LINE_SIZE << 12) & 0x3ff000 |
							(L1D_WAY_COUNT << 22) & 0xffc00000 );
					*ecx = L1D_SET_COUNT;
					*edx = 0x1;
					break;
				}
				case 1: { // L1-I cache info
					*eax = 0x122 | cores_info;
					*ebx = (L1I_LINE_SIZE & 0xfff |
							(L1I_LINE_SIZE << 12) & 0x3ff000 |
							(L1I_WAY_COUNT << 22) & 0xffc00000 );
					*ecx = L1I_SET_COUNT;
					*edx = 0x1;
					break;
				}
				case 2: { // L2 cache info
					uint32_t l2_core_info = 
						(((NUMBER_OF_CORES_PER_L2 - 1) << 14) &
						 0x3fc000);
					l2_core_info |= ((NUMBER_OF_CORES - 1) << 26) &
					   	0xfc00000;
					*eax = 0x143 | l2_core_info;
					*ebx = (L2_LINE_SIZE & 0xfff |
							(L2_LINE_SIZE << 12) & 0x3ff000 |
							(L2_WAY_COUNT << 22) & 0xffc00000 );
					*ecx = L2_SET_COUNT;
					*edx = 0x1;
					break;
				}
#ifdef ENABLE_L3_CACHE
				case 3: { // L3 cache info
					uint32_t l3_core_info = 
						(((NUMBER_OF_CORES - 1) << 14) &
						 0x3fc000);
					l3_core_info |= ((NUMBER_OF_CORES - 1) << 26) &
					   	0xfc00000;
					*eax = 0x163 | l3_core_info;
					*ebx = (L3_LINE_SIZE & 0xfff |
							(L3_LINE_SIZE << 12) & 0x3ff000 |
							(L3_WAY_COUNT << 22) & 0xffc00000 );
					*ecx = L3_SET_COUNT;
					*edx = 0x1;
					break;
				}
#endif
				default: {
					*eax = 0;
					*ebx = 0;
					*ecx = 0;
					*edx = 0;
				}
			}
			break;
		default:
			// unsupported CPUID
			return 0;
	}

	return 1;
}
