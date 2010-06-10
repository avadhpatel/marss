
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
#include <qemu-objects.h>
#include <monitor.h>
}

#include <ptl-qemu.h>
#include <stats.h>
#include <ptlsim.h>

#include <cacheConstants.h>

#define __INSIDE_MARSS_QEMU__
#include <ptlcalls.h>

/*
 * Physical address of the PTLsim PTLCALL hypercall page
 * used to communicate with the outside world:
 */
#define PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR    0x00000008fffff000ULL

using namespace Memory;

uint8_t in_simulation = 0;
uint8_t start_simulation = 0;
uint8_t simulation_configured = 0;
uint8_t ptl_stable_state = 1;
uint64_t ptl_start_sim_rip = 0;

static char *pending_command_str = null;
static int pending_call_type = -1;
static int pending_call_arg3 = -1;

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

    switch(calltype) {
        case PTLCALL_VERSION:
            {
                cout << "PTLCALL type PTLCALL_VERSION\n";
                cpu->regs[REG_rax] = 1;
                break;
            }
        case PTLCALL_MARKER:
            {
                cout << "PTLCALL type PTLCALL_MARKER\n";
                cpu->regs[REG_rax] = 0;
                break;
            }
        case PTLCALL_ENQUEUE:
            {
                cout << "PTLCALL type PTLCALL_ENQUEUE\n";

                /*
                 * Address of the command is stored in arg1 and
                 * length of the command string is in arg2
                 */
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

                cout << "MARSSx86::Command received : ", command_str, endl;
                /*
                 * Stop the QEMU vm and change ptlsim configuration
                 * QEMU will be automatically started in simulation mode
                 */
                vm_stop(0);
                ptl_machine_configure(command_str);
                cpu_exit(cpu);
                if(in_simulation)
                    vm_start();
                simulation_configured = 1;
                break;
            }
        case PTLCALL_CHECKPOINT:
            {
                cout << "PTLCALL type PTLCALL_CHECKPOINT\n";

                char *checkpoint_name = (char*)qemu_malloc(arg2 + 1);
                char *name_addr = (char*)(arg1);
                for(int i=0; i < arg2; i++) {
                    checkpoint_name[i] = (char)ldub_kernel(
                            (target_ulong)(name_addr++));
                }
                checkpoint_name[arg2] = '\0';

                vm_stop(0);

                if (cpu_single_env)
                    cpu_exit(cpu_single_env);

                if(arg3 != PTLCALL_CHECKPOINT_DUMMY) {

                    pending_command_str = checkpoint_name;
                    pending_call_type = PTLCALL_CHECKPOINT;
                    pending_call_arg3 = arg3;

                } else {
                    cout << "MARSSx86::Reached to Chekpoint location..\n";
                    cout << "MARSSx86::Now you can create your checkpont..\n";
                    break;
                }

                switch(arg3) {
                    case PTLCALL_CHECKPOINT_AND_CONTINUE:
                        vm_start();
                        break;
                    case PTLCALL_CHECKPOINT_AND_SHUTDOWN:
                        /* Let the shutdown handled by ptl_check_ptlcall_queue */
                        vm_start();
                        simulation_configured = 1;
                        break;
                    case PTLCALL_CHECKPOINT_AND_PAUSE:
                        break;
                    case PTLCALL_CHECKPOINT_AND_REBOOT:
                        cout << "MARSSx86::Rebooting system\n";
                        qemu_system_reset_request();
                        break;
                    default:
                        cout << "MARSSx86::Unkonw Action\n";
                        break;
                }

                break;
            }
        default :
            cout << "PTLCALL type unknown : ", calltype, endl;
            cpu->regs[REG_rax] = -EINVAL;
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

void dump_all_info();
void dump_bbcache_to_logfile();

void ptlsim_init() {

    /* First allocate some memory for PTLsim for its own memory manager */
    byte* ptlsim_ram_start = (byte*)qemu_malloc(PTLSIM_RAM_SIZE);
    assert(ptlsim_ram_start);
    byte* ptlsim_ram_end = ptlsim_ram_start + PTLSIM_RAM_SIZE;

    /* Register PTLsim PTLCALL mmio page */
    W64 ptlcall_mmio_pd = cpu_register_io_memory(ptlcall_mmio_read_ops,
            ptlcall_mmio_write_ops, NULL);
    cpu_register_physical_memory(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, 4096,
            ptlcall_mmio_pd);

    cout << "ptlcall_mmio_init : Registered PTLcall MMIO page at physaddr ",
         PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, " descriptor ", ptlcall_mmio_pd,
         " io_mem_index ", ptlcall_mmio_pd >> IO_MEM_SHIFT, endl;

    /* Register ptlsim assert callback functions */
    register_assert_cb(&dump_all_info);
    register_assert_cb(&dump_bbcache_to_logfile);
}

void ptl_config_from_file(char *filename) {
    int const MAX_CMD_SIZE = 1024;
    char * line = NULL;
    size_t len = 0;
    char *cmd_line;
    int cmd_size=0;
    ssize_t read;
    FILE * fp;

    cmd_line = (char*)malloc(MAX_CMD_SIZE);
    memset(cmd_line,'\0',MAX_CMD_SIZE);
    fp = fopen(filename, "r");
    if (fp == NULL){
        fprintf(stderr, "qemu: file not found\n");
        exit(1);
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        if(line[0] != '#'){
            cmd_size+=read;
            assert(cmd_size<MAX_CMD_SIZE);
            if (line[read-1] = '\n')
                line[read-1] = ' ';
            strncat(cmd_line,line,read);
        }
    }

    if (line)
        free(line);

    printf("%s\n",cmd_line);
    ptl_machine_configure(cmd_line);
    simulation_configured = 1;
    free(cmd_line);
}

int ptl_cpuid(uint32_t index, uint32_t count, uint32_t *eax, uint32_t *ebx,
        uint32_t *ecx, uint32_t *edx) {

    /* First check if the index is same as PTLSIM_CPUID_MAGIC */
    if(index == PTLSIM_CPUID_MAGIC) {

        *eax = PTLSIM_CPUID_FOUND;
        *ebx = PTLCALL_METHOD_MMIO;
        *ecx = (W32)(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR);
        *edx = (W32)((PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR >> 32) &
                0xffff) | 0x0;

        return 1;
    }

    /*
     * Check for CPUID requesting info that can be provided by the ptlsim
     * NOTE: Look at Intel x86 maual to find out how to set the return values
     * for the cache info and others
     */

    uint32_t cores_info =
        (((NUMBER_OF_THREAD_PER_CORE - 1) << 14) & 0x3fc000) |
        (((NUMBER_OF_CORES - 1) << 26) & 0xfc00000);
    switch(index) {
        case 4:
            /*
             * Cache info from OS
             * EAX : Bits		Info
             *          4-0		0 = Null - no more cache
             *                     1 = Data cache
             *                     2 = Instruction cache
             *                     3 = Unified cache
             *                     4-31 = reserved
             *          7-5		Cache Level (starts from 1)
             *          8			Self initalizing cache
             *          9			Fully Associative cache
             *          25-14		Maximum number of IDs of logical
             *                     Processors sharing this cache
             *          31-26		Maximum number of cores in package
             *
             * EBX : Bits		Info
             *       11-0		Coherency line size
             *       21-12		Physical line partition
             *       31-22		Ways of Associativity
             *
             * ECX : Number of Sets
             * EDX : Bits		Info
             *          0			Writeback/Invalid on sharing
             *          1			Inclusive or not of lower caches
             */
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
            /* unsupported CPUID */
            return 0;
    }

    return 1;
}

void ptl_check_ptlcall_queue() {

    if(pending_call_type != -1) {

        cout << "Pending call type: ", pending_call_type, endl;
        switch(pending_call_type) {
            case PTLCALL_CHECKPOINT:
                {
                    cout << "MARSSx86::Creating checkpoint ",
                         pending_command_str, endl;

                    QDict *checkpoint_dict = qdict_new();
                    qdict_put_obj(checkpoint_dict, "name", QOBJECT(
                                qstring_from_str(pending_command_str)));
                    Monitor *mon = cur_mon;
                    do_savevm(cur_mon, checkpoint_dict);
                    cout << "MARSSx86::Checkpoint ", pending_command_str,
                         " created\n";

                    switch(pending_call_arg3) {
                        case PTLCALL_CHECKPOINT_AND_SHUTDOWN:
                            cout << "MARSSx86::Shutdown requested\n";
                            exit(0);
                            break;
                        default:
                            cout << "MARSSx86::Unkonw Action\n";
                            break;
                    }
                }
        }

        delete pending_command_str;
        pending_command_str = null;

        pending_call_type = -1;
        pending_call_arg3 = -1;
    }
}

/* QEMU Related Context functions */

RIPVirtPhys& RIPVirtPhys::update(Context& ctx, int bytes) {

    df = ((ctx.internal_eflags & FLAG_DF) != 0);
    kernel = ctx.kernel_mode;
    use64 = ctx.use64;
    padlo = 0;
    padhi = 0;
    mfnlo = 0;
    mfnhi = 0;

    int exception = 0;
    PageFaultErrorCode pfec = 0;
    int mmio = 0;

    return *this;
}

# define PHYS_ADDR_MASK 0xfffffff000LL

W64 Context::virt_to_pte_phys_addr(W64 rawvirt, byte& level) {

    W64 ptep, pte;
    W64 pde_addr, pte_addr;
    W64 ret_addr;

    assert(level > 0);

    setup_qemu_switch();

    // First check if PAE bit is enabled or not
    if(cr[4] & CR4_PAE_MASK) {
        W64 pde, pdpe;
        W64 pdpe_addr;

        // Check if we are in 32 bit mode or 64 bit
        if(hflags & HF_LMA_MASK) {

            W64 pml4e_addr, pml4e;
            pml4e_addr = ((cr[3] & ~0xfff) + (((rawvirt >> 39) & 0x1ff) << 3)) & a20_mask;
            if(level == 4) {
                ret_addr = pml4e_addr;
                goto finish;
            }

            pml4e = ldq_phys(pml4e_addr);
            if(!(pml4e & PG_PRESENT_MASK)) {
                goto dofault;
            }

            ptep = pml4e * PG_NX_MASK;

            pdpe_addr = ((pml4e & PHYS_ADDR_MASK) + (((rawvirt >> 30) & 0x1ff) << 3)) & a20_mask;

            if(level == 3) {
                ret_addr = pdpe_addr;
                goto finish;
            }

            pdpe = ldq_phys(pdpe_addr);
            if(!(pdpe & PG_PRESENT_MASK)) {
                goto dofault;
            }
            ptep &= pdpe ^ PG_NX_MASK;
        } else {

            assert(level < 4);

            pdpe_addr = ((env->cr[3] & ~0x1f) + ((rawvirt >> 27) & 0x18)) & a20_mask;

            if(level == 3) {
                ret_addr = pdpe_addr;
                goto finish;
            }

            pdpe = ldq_phys(pdpe_addr);
            if(!(pdpe & PG_PRESENT_MASK)) {
                goto dofault;
            }
            ptep = PG_NX_MASK | PG_USER_MASK | PG_RW_MASK;
        }

        pde_addr = ((pdpe & PHYS_ADDR_MASK) + (((rawvirt >> 21) & 0x1ff) << 3)) & a20_mask;

        if(level == 2) {
            ret_addr = pde_addr;
            goto finish;
        }

        pde = ldq_phys(pde_addr);
        if(!(pde & PG_PRESENT_MASK)) {
            goto dofault;
        }

        ptep &= pde ^ PG_NX_MASK;
        if(pde & PG_PSE_MASK) {
            // 2 MB Page size - no need to look up last level
            level = 0;
            ret_addr = -1;
            goto finish;
        } else {
            // 4 KB page size - make sure our level
            pte_addr = ((pde & PHYS_ADDR_MASK) + (((rawvirt >> 12) & 0x1ff) << 3)) & a20_mask;
            ret_addr = pte_addr;
            goto finish;
        }

    } else {

        W32 pde;

        assert(level < 3);

        pde_addr = ((cr[3] & ~0xfff) + ((rawvirt >> 20) & 0xffc)) & env->a20_mask;

        if(level == 2) {
            ret_addr = pde_addr;
            goto finish;
        }

        pde = ldl_phys(pde_addr);
        if (!(pde & PG_PRESENT_MASK)) {
            goto dofault;
        }

        assert(level == 1);

        if ((pde & PG_PSE_MASK) && (cr[4] & CR4_PSE_MASK)) {
            // 4 MB Page size - no need to look last level
            level = 0;
            ret_addr = -1;
            goto finish;
        } else {
            // 4 KB Page size
            pte_addr = ((pde & ~0xfff) + ((rawvirt >> 10) & 0xffc)) & a20_mask;
            ret_addr = pte_addr;
            goto finish;
        }
    }

dofault:
    ret_addr = -1;

finish:
    setup_ptlsim_switch();
    return ret_addr;
}

int Context::copy_from_user(void* target, Waddr source, int bytes, PageFaultErrorCode& pfec, Waddr& faultaddr, bool forexec) {

    if (source == 0) {
        return -1;
    }

    int n = 0 ;
    pfec = 0;

    setup_qemu_switch_all_ctx(*this);

    if(logable(10))
        ptl_logfile << "Copying from userspace ", bytes, " bytes from ",
                    source, endl;

    int exception = 0;
    int mmio = 0;
    Waddr physaddr = check_and_translate(source, 0, 0, 0, exception,
            mmio, pfec, forexec);
    if (exception) {
        int old_exception = exception_index;
        int mmu_index = cpu_mmu_index((CPUState*)this);
        int fail = cpu_x86_handle_mmu_fault((CPUX86State*)this,
                source, 2, mmu_index, 1);
        if(logable(10))
            ptl_logfile << "page fault while reading code fault:", fail,
                        " source_addr:", (void*)(source),
                        " eip:", (void*)(eip), " fail: ", fail, endl;
        if (fail != 0) {
            if(logable(10))
                ptl_logfile << "Unable to read code from ",
                            hexstring(source, 64), endl;
            setup_ptlsim_switch_all_ctx(*this);
            /*
             * restore the exception index as it will be
             * restore when we try to commit this entry from ROB
             */
            exception_index = old_exception;
            if likely (forexec)
                exec_fault_addr = source;
            faultaddr = source;
            pfec = 1;
            return 0;
        }
    }

    int bytes_frm_first_page = min(4096 - lowbits(source, 12), (Waddr)bytes);
    target_ulong source_b = source;
    byte* target_b = (byte*)(target);
    while(n < bytes_frm_first_page) {
        char data;
        if(forexec) data = ldub_code(source_b);
        else if(kernel_mode) data = ldub_kernel(source_b);
        else data = ldub_user(source_b);
        if(logable(109)) {
            ptl_logfile << "[", hexstring((W8)(data), 8),
                        "-", hexstring((W8)(ldub_code(source_b)), 8),
                        "@", (void*)(source_b), "] ";
        }
        target_b[n] = data;
        source_b++;
        n++;
    }

    if(n == bytes) return n;

    // If we need to access second page, check if its present in TLB or PTE
    exception = 0;
    mmio = 0;

    physaddr = check_and_translate(source + n, 0, 0, 0, exception,
            mmio, pfec, forexec);
    if (exception) {
        int old_exception = exception_index;
        int mmu_index = cpu_mmu_index((CPUState*)this);
        int fail = cpu_x86_handle_mmu_fault((CPUX86State*)this,
                source + n, 2, mmu_index, 1);
        if(logable(10))
            ptl_logfile << "page fault while reading code fault:", fail,
                        " source_addr:", (void*)(source + n),
                        " eip:", (void*)(eip), endl;
        if (fail != 0) {
            if(logable(10))
                ptl_logfile << "Unable to read code from ",
                            hexstring(source + n, 64), endl;
            setup_ptlsim_switch_all_ctx(*this);
            /*
             * restore the exception index as it will be
             * restore when we try to commit this entry from ROB
             */
            exception_index = old_exception;
            if likely (forexec)
                exec_fault_addr = source + n;
            faultaddr = source + n;
            pfec = 1;
            return n;
        }
    }

    while(n < bytes) {
        char data;
        if(forexec) data = ldub_code(source_b);
        else if(kernel_mode) data = ldub_kernel(source_b);
        else data = ldub_user(source_b);
        if(logable(109)) {
            ptl_logfile << "[", hexstring((W8)(data), 8),
                        "-", hexstring((W8)(ldub_code(source_b)), 8),
                        "@", (void*)(source_b), "] ";
        }
        target_b[n] = data;
        source_b++;
        n++;
    }

    if(logable(109)) ptl_logfile << endl;

    setup_ptlsim_switch_all_ctx(*this);

    if(logable(10))
        ptl_logfile << "Copy done..\n";

    return n;
}

void Context::update_mode_count() {
    W64 prev_cycles = cycles_at_last_mode_switch;
    W64 prev_insns = insns_at_last_mode_switch;
    W64 delta_cycles = sim_cycle - cycles_at_last_mode_switch;
    W64 delta_insns = total_user_insns_committed -
        insns_at_last_mode_switch;

    cycles_at_last_mode_switch = sim_cycle;
    insns_at_last_mode_switch = total_user_insns_committed;

    if likely (use64) {
        if(kernel_mode) {
            per_core_event_update(cpu_index, cycles_in_mode.kernel64 += delta_cycles);
            per_core_event_update(cpu_index, insns_in_mode.kernel64 += delta_insns);
        } else {
            per_core_event_update(cpu_index, cycles_in_mode.user64 += delta_cycles);
            per_core_event_update(cpu_index, insns_in_mode.user64 += delta_insns);
        }
    } else {
        if(kernel_mode) {
            per_core_event_update(cpu_index, cycles_in_mode.kernel32 += delta_cycles);
            per_core_event_update(cpu_index, insns_in_mode.kernel32 += delta_insns);
        } else {
            per_core_event_update(cpu_index, cycles_in_mode.user32 += delta_cycles);
            per_core_event_update(cpu_index, insns_in_mode.user32 += delta_insns);
        }
    }
}

void Context::update_mode(bool is_kernel) {
    update_mode_count();
    kernel_mode = is_kernel;
    if(config.log_user_only) {
        if(kernel_mode)
            logenable = 0;
        else
            logenable = 1;
    }
}


Waddr Context::check_and_translate(Waddr virtaddr, int sizeshift, bool store, bool internal, int& exception, int& mmio, PageFaultErrorCode& pfec, bool is_code) {

    exception = 0;
    pfec = 0;

    if unlikely (internal) {
        /*
         *  Directly mapped to PTL space (microcode load/store)
         *  We need to patch in PTLSIM_VIRT_BASE since in 32-bit
         *  mode, ctx.virt_addr_mask will chop off these bits.
         *  Now in QEMU we dont have any special mapping of this
         *  memory region, so virtualaddress is where we
         *  will store the internal data
         */
        return virtaddr;
    }

    bool page_not_present;
    bool page_read_only;
    bool page_kernel_only;

    int mmu_index = cpu_mmu_index((CPUState*)this);
    int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    W64 tlb_addr;
redo:
    if likely (!store) {
        if likely (!is_code) {
            tlb_addr = tlb_table[mmu_index][index].addr_read;
        } else {
            tlb_addr = tlb_table[mmu_index][index].addr_code;
        }
    } else {
        tlb_addr = tlb_table[mmu_index][index].addr_write;
    }

    if(logable(10)) {
        ptl_logfile << "mmu_index:", mmu_index, " index:", index,
                    " virtaddr:", hexstring(virtaddr, 64),
                    " tlb_addr:", hexstring(tlb_addr, 64),
                    " virtpage:", hexstring(
                            (virtaddr & TARGET_PAGE_MASK), 64),
                    " tlbpage:", hexstring(
                            (tlb_addr & TARGET_PAGE_MASK), 64),
                    " addend:", hexstring(
                            tlb_table[mmu_index][index].addend, 64),
                    endl;
    }
    if likely ((virtaddr & TARGET_PAGE_MASK) ==
            (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        /* Check if its not MMIO address */
        if(tlb_addr & ~TARGET_PAGE_MASK) {
            mmio = 1;
            return (Waddr)(virtaddr + iotlb[mmu_index][index]);
        }

        /* we find valid TLB entry, return the physical address for it */
        mmio = 0;
        return (Waddr)(virtaddr + tlb_table[mmu_index][index].addend);
    }

    mmio = 0;

    /* Can't find valid TLB entry, its an exception */
    exception = (store) ? EXCEPTION_PageFaultOnWrite : EXCEPTION_PageFaultOnRead;
    pfec.rw = store;
    pfec.us = (!kernel_mode);

    return INVALID_PHYSADDR;
}

bool Context::is_mmio_addr(Waddr virtaddr, bool store) {

    int mmu_index = cpu_mmu_index((CPUState*)this);
    int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    W64 tlb_addr;

    if likely (!store) {
        tlb_addr = tlb_table[mmu_index][index].addr_read;
    } else {
        tlb_addr = tlb_table[mmu_index][index].addr_write;
    }

    if(logable(10)) {
        ptl_logfile << "mmio mmu_index:", mmu_index, " index:", index,
                    " virtaddr:", hexstring(virtaddr, 64),
                    " tlb_addr:", hexstring(tlb_addr, 64),
                    " virtpage:", hexstring(
                            (virtaddr & TARGET_PAGE_MASK), 64),
                    " tlbpage:", hexstring(
                            (tlb_addr & TARGET_PAGE_MASK), 64),
                    " addend:", hexstring(
                            tlb_table[mmu_index][index].addend, 64),
                    endl;
    }
    if likely ((virtaddr & TARGET_PAGE_MASK) ==
            (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        // Check if its not MMIO address
        if(tlb_addr & ~TARGET_PAGE_MASK) {
            return true;
        }
    }

    return false;

}

bool Context::has_page_fault(Waddr virtaddr, int store) {
    int mmu_index = cpu_mmu_index((CPUState*)this);
    int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    W64 tlb_addr;

    if likely (!store) {
        tlb_addr = tlb_table[mmu_index][index].addr_read;
    } else {
        tlb_addr = tlb_table[mmu_index][index].addr_write;
    }

    if likely ((virtaddr & TARGET_PAGE_MASK) ==
            (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        return false;
    }

    return true;
}

int copy_from_user_phys_prechecked(void* target, Waddr source, int bytes, Waddr& faultaddr) {

    int n = 0 ;

    int exception;
    PageFaultErrorCode pfec;
    int mmio;
    Waddr source_paddr = contextof(0).check_and_translate(source, 0, false, false,
            exception, mmio, pfec);

    if(exception > 0 || pfec > 0) {
        faultaddr = source;
        return 0;
    }
    n = min(4096 - lowbits(source, 12), (Waddr)bytes);
    memcpy(target, (void*)source_paddr, n);

    /* Check if all the bytes are read in first page or not */
    if likely (n == bytes) return n;

    source_paddr = contextof(0).check_and_translate(source + n, 0, false, false,
            exception, mmio, pfec);
    if(exception > 0 || pfec > 0) {
        faultaddr = source + n;
        return 0;
    }

    Waddr next_page_addr = ((source >> TARGET_PAGE_BITS) + 1) << TARGET_PAGE_BITS;

    memcpy((byte*)target + n, (void*)next_page_addr, bytes - n);
    n = bytes;
    return n;
}

void Context::propagate_x86_exception(byte exception, W32 errorcode , Waddr virtaddr ) {
    if(logable(2))
        ptl_logfile << "Propagating exception from simulation at eip: ",
                    this->eip, " cycle: ", sim_cycle, endl;
    setup_qemu_switch_all_ctx(*this);
    ptl_stable_state = 1;
    handle_interrupt = 1;
    if(errorcode) {
        raise_exception_err((int)exception, (int)errorcode);
    } else {
        raise_exception((int)exception);
    }
    ptl_stable_state = 0;
    setup_ptlsim_switch_all_ctx(*this);
}

W64 Context::loadvirt(Waddr virtaddr, int sizeshift) {
    Waddr addr = virtaddr;
    assert(virtaddr > 0xffff);
    setup_qemu_switch_all_ctx(*this);
    W64 data = 0;

    bool mmio = is_mmio_addr(virtaddr, 0);

    if likely (!kernel_mode && !mmio) {
        switch(sizeshift) {
            case 0: {
                        W8 d = ldub_user(virtaddr);
                        data = (W64)d;
                        break;
                    }
            case 1: {
                        W16 d = lduw_user(virtaddr);
                        data = (W64)d;
                        break;
                    }
            case 2: {
                        W32 d = ldl_user(virtaddr);
                        data = (W64)d;
                        break;
                    }
            default:
                    data = ldq_user(virtaddr);
        }
    } else {
        switch(sizeshift) {
            case 0: {
                        W8 d = ldub_kernel(virtaddr);
                        data = (W64)d;
                        break;
                    }
            case 1: {
                        W16 d = lduw_kernel(virtaddr);
                        data = (W64)d;
                        break;
                    }
            case 2: {
                        W32 d = ldl_kernel(virtaddr);
                        data = (W64)d;
                        break;
                    }
            default:
                    data = ldq_kernel(virtaddr);
        }
    }

    if(logable(10) && mmio)
        ptl_logfile << "MMIO READ addr: ", hexstring(virtaddr, 64),
                    " data: ", hexstring(data, 64), " size: ",
                    sizeshift, endl;
    else if(logable(10))
        ptl_logfile << "Context::loadvirt addr[", hexstring(addr, 64),
                    "] data[", hexstring(data, 64), "] origaddr[",
                    hexstring(virtaddr, 64), "]\n";

    setup_ptlsim_switch_all_ctx(*this);

    return data;
}

W64 Context::loadphys(Waddr addr, bool internal, int sizeshift) {
    /*
     * Currently we check sizeshift only for internal data
     * for data on RAM or IO we load data at 64 bit boundry
     */
    if(internal) {
        W64 data = 0;
        switch(sizeshift) {
            case 0: { // load one byte
                        byte b = byte(*(byte*)(addr));
                        data = b;
                        break;
                    }
            case 1: { // load a word
                        W16 w = W16(*(W16*)(addr));
                        data = w;
                        break;
                    }
            case 2: { // load double word
                        W32 dw = W32(*(W32*)(addr));
                        data = dw;
                        break;
                    }
            case 3: // load quad word
            default: {
                         data = W64(*(W64*)(addr));
                     }
        }

        if(logable(10))
            ptl_logfile << "Context::internal_loadphys addr[",
                        hexstring(addr, 64), "] data[",
                        hexstring(data, 64), "] sizeshift[", sizeshift,
                        "] ", endl;
        return data;
    }

    W64 data = 0;
    Waddr orig_addr = addr;
    addr = floor(addr, 8);
    setup_qemu_switch_all_ctx(*this);
    data = ldq_raw((uint8_t*)addr);

    if(logable(10))
        ptl_logfile << "Context::loadphys addr[", hexstring(addr, 64),
                    "] data[", hexstring(data, 64), "] origaddr[",
                    hexstring(orig_addr, 64), "]\n";
    setup_ptlsim_switch_all_ctx(*this);
    return data;
}

W64 Context::storemask_virt(Waddr virtaddr, W64 data, byte bytemask, int sizeshift) {
    W64 old_data = 0;
    setup_qemu_switch_all_ctx(*this);
    Waddr paddr = floor(virtaddr, 8);

    if(logable(10))
        ptl_logfile << "Trying to write to addr: ", hexstring(paddr, 64),
                    " with bytemask ", bytemask, " data: ", hexstring(
                            data, 64), endl;

    if(is_mmio_addr(virtaddr, 1)) {
        switch(sizeshift) {
            case 0: {
                        stb_kernel(virtaddr, (W8)data);
                        break;
                    }
            case 1: {
                        stw_kernel(virtaddr, (W16)data);
                        break;
                    }
            case 2: {
                        stl_kernel(virtaddr, (W32)data);
                        break;
                    }
            default:
                    stq_kernel(virtaddr, data);
        }
        if(logable(10))
            ptl_logfile << "MMIO WRITE addr: ", hexstring(virtaddr, 64),
                        " data: ", hexstring(data, 64), " size: ",
                        sizeshift, endl;
        return data;
    }

    switch(sizeshift) {
        case 0: // byte write
            (kernel_mode) ? stb_kernel(virtaddr, data) :
                stb_user(virtaddr, data);
            break;
        case 1: // word write
            (kernel_mode) ? stw_kernel(virtaddr, data) :
                stw_user(virtaddr, data);
            break;
        case 2: // double word write
            (kernel_mode) ? stl_kernel(virtaddr, data) :
                stl_user(virtaddr, data);
            break;
        case 3: // quad word write
        default:
            (kernel_mode) ? stq_kernel(virtaddr, data) :
                stq_user(virtaddr, data);
            break;
    }
    if(logable(10))
        ptl_logfile << "Context::storemask addr[", hexstring(paddr, 64),
                    "] data[", hexstring(data, 64), "]\n";
    //#define CHECK_STORE
#ifdef CHECK_STORE
    W64 data_r = 0;
    switch(sizeshift) {
        case 0: // byte write
            (kernel_mode) ? data_r = (W64)ldub_kernel(virtaddr) :
                data_r = (W64)ldub_user(virtaddr);
            break;
        case 1: // word write
            (kernel_mode) ? data_r = (W64)lduw_kernel(virtaddr) :
                data_r = (W64)lduw_user(virtaddr);
            break;
        case 2: // double word write
            (kernel_mode) ? data_r = (W64)ldul_kernel(virtaddr) :
                data_r = (W64)ldul_user(virtaddr);
            break;
        case 3: // quad word write
        default:
            (kernel_mode) ? data_r = (W64)ldq_kernel(virtaddr) :
                data_r = (W64)ldq_user(virtaddr);
            break;
    }
    if((W64)data != data_r) {
        ptl_logfile << "Stored data does not match..\n";
        ptl_logfile << "Data: ", (void*)data, " Data_r: ", (void*)data_r, endl, flush;
        assert_fail(__STRING(0), __FILE__, __LINE__,
                __PRETTY_FUNCTION__);
    }

#endif
    return data;
}

void Context::check_store_virt(Waddr virtaddr, W64 data, byte bytemask, int sizeshift) {
    W64 data_r = 0;
    W64 mask = 0;
    switch(sizeshift) {
        case 0: // byte write
            (kernel_mode) ? data_r = (W64)ldub_kernel(virtaddr) :
                data_r = (W64)ldub_user(virtaddr);
	    mask = 0xff;
            //data = signext64(data, 8);
            break;
        case 1: // word write
            (kernel_mode) ? data_r = (W64)lduw_kernel(virtaddr) :
                data_r = signext64((W64)lduw_user(virtaddr), 16);
	    mask = 0xffff;
            //data = signext64(data, 16);
            break;
        case 2: // double word write
            (kernel_mode) ? data_r = (W64)ldul_kernel(virtaddr) :
                data_r = signext64((W64)ldul_user(virtaddr), 32);
	    mask = 0xffffffff;
            //data = signext64(data, 32);
            break;
        case 3: // quad word write
        default:
            (kernel_mode) ? data_r = (W64)ldq_kernel(virtaddr) :
                data_r = (W64)ldq_user(virtaddr);
	    mask = (W64)-1;
            break;
    }
    if((data & mask) != (data_r & mask)) {
        ptl_logfile << "Stored data does not match..\n";
        ptl_logfile << "Data: ", (void*)data, " Data_r: ", (void*)data_r, endl, flush;
        assert_fail(__STRING(0), __FILE__, __LINE__,
                __PRETTY_FUNCTION__);
    }
}

W64 Context::store_internal(Waddr addr, W64 data, byte bytemask) {
    W64 old_data = W64(*(W64*)(addr));
    W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask],
            old_data, data);
    if(logable(10))
        ptl_logfile << "Context::store_internal addr[",
                    hexstring(addr, 64), "] old_data[",
                    hexstring(old_data, 64), "] new_data[",
                    hexstring(merged_data, 64), "]\n";
    *(W64*)(addr) = merged_data;
    return merged_data;
}

W64 Context::storemask(Waddr paddr, W64 data, byte bytemask) {
    W64 old_data = 0;
    setup_qemu_switch_all_ctx(*this);
    if(logable(10))
        ptl_logfile << "Trying to write to addr: ", hexstring(paddr, 64),
                    " with bytemask ", bytemask, " data: ", hexstring(
                            data, 64), endl;
    old_data = ldq_raw((uint8_t*)paddr);
    W64 merged_data = mux64(expand_8bit_to_64bit_lut[bytemask], old_data, data);
    if(logable(10))
        ptl_logfile << "Context::storemask addr[", hexstring(paddr, 64),
                    "] data[", hexstring(merged_data, 64), "]\n";
    stq_raw((uint8_t*)paddr, merged_data);
#ifdef CHECK_STORE
    W64 new_data = 0;
    new_data = ldq_raw((uint8_t*)paddr);
    ptl_logfile << "Context::storemask store-check: addr[",
                hexstring(paddr, 64), "] data[", hexstring(new_data,
                        64), "]\n";
    assert(new_data == merged_data);
#endif
    return data;
}

void Context::handle_page_fault(Waddr virtaddr, int is_write) {
    setup_qemu_switch_all_ctx(*this);

    if(kernel_mode) {
        if(logable(5))
            ptl_logfile << "Page fault in kernel mode...", endl, flush;
    }

    if(logable(5)) {
        ptl_logfile << "Context before page fault handling:\n", *this, endl;
    }

    exception_is_int = 0;
    ptl_stable_state = 1;
    handle_interrupt = 1;
    int mmu_index = cpu_mmu_index((CPUState*)this);
    tlb_fill(virtaddr, is_write, mmu_index, null);
    ptl_stable_state = 0;

    if(kernel_mode) {
        if(logable(5))
            ptl_logfile << "Page fault in kernel mode...handled", endl,
                        flush;
    }

    setup_ptlsim_switch_all_ctx(*this);
    return;
}

bool Context::try_handle_fault(Waddr virtaddr, bool store) {

    setup_qemu_switch_all_ctx(*this);

    if(logable(10))
        ptl_logfile << "Trying to fill tlb for addr: ", (void*)virtaddr, endl;

    int mmu_index = cpu_mmu_index((CPUState*)this);
    int fault = cpu_x86_handle_mmu_fault((CPUState*)this, virtaddr, store, mmu_index, 1);

    setup_ptlsim_switch_all_ctx(*this);
    if(fault) {
        if(logable(10))
            ptl_logfile << "Fault for addr: ", (void*)virtaddr, endl, flush;

        error_code = 0;
        exception_index = -1;

        return false;
    }

    if(logable(10))
        ptl_logfile << "Tlb fill for addr: ", (void*)virtaddr, endl, flush;

    return true;
}
