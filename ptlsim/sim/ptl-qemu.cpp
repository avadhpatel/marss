
// Some of the functions are referenced from 'qemu-kvm-ptlsim.c' file of project
// 'PTLsim-KVM 3.0'.
// They are modified to use QEMU CPU context by Avadh Patel
// Copyright of those functions by Matt Yourst

#include <globals.h>
#include <ptl-qemu.h>
#include <ptlhwdef.h>
#include <mm.h>

extern "C" {
#include <sysemu.h>
}

#define __INSIDE_PTLSIM_QEMU__
#include <ptlcalls.h>
//
// Physical address of the PTLsim PTLCALL hypercall page
// used to communicate with the outside world:
//
#define PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR    0x00000008fffff000ULL

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

	if(!in_simulation) {
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
				// inside_ptlsim = 1;
				// Stop the QEMU vm so user can change to simulation mode
				vm_stop(0);
				break;
			  }
			case PTLCALL_CHECKPOINT: {
				cout << "PTLCALL type PTLCALL_CHECKPOINT\n";
				cpu->regs[REG_rax] = 0;
				break;
			  }
			default : 
				cout << "PTLCALL type unknown : ", calltype, endl;
				cpu->regs[REG_rax] = -EINVAL;
		}
	} else {
		cout << "PTLCALL called while inside simulation\n";
	}
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

	ptl_mm_init(ptlsim_ram_start, ptlsim_ram_end);

	// Register PTLsim PTLCALL mmio page
	W64 ptlcall_mmio_pd = cpu_register_io_memory(0, ptlcall_mmio_read_ops, 
			ptlcall_mmio_write_ops, NULL);
	cpu_register_physical_memory(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, 4096,
			ptlcall_mmio_pd);

	cout << "ptlcall_mmio_init : Registered PTLcall MMIO page at physaddr ", 
		 PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, " descriptor ", ptlcall_mmio_pd, 
		 " io_mem_index ", ptlcall_mmio_pd >> IO_MEM_SHIFT, endl;
}
