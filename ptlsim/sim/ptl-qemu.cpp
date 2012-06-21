
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
#include <ptlsim.h>

#include <cacheConstants.h>

#define __INSIDE_MARSS_QEMU__
#include <ptlcalls.h>

#include <test.h>

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
uint8_t qemu_initialized = 0;

static char *pending_command_str = NULL;
static int pending_call_type = -1;
static int pending_call_arg3 = -1;

static void save_core_dump(char* dump, W64 dump_size,
        char* app_name, W64 app_name_size, W64 signum)
{
    stringbuf  filename;
    ofstream   df;
    char      *_addr;
    char      *dmp;
    char      *app;

    dmp = (char*)qemu_malloc(sizeof(char) * dump_size);
    app = (char*)qemu_malloc((sizeof(char) * app_name_size) + 1);

    _addr = dump;
    foreach(i, (W64s)dump_size) {
        dmp[i] = (char)ldub_kernel((target_ulong)(_addr));
        _addr++;
    }

    _addr = app_name;
    foreach(i, (int)app_name_size) {
        app[i] = (char)ldub_kernel((target_ulong)(_addr));
        _addr++;
    }
    app[app_name_size] = '\0';

    filename << app << "-core";
    df.open(filename, std::ios_base::binary | std::ios_base::out);

    df.write(dmp, dump_size);
    df.flush();
    df.close();

    ptl_logfile << "Core dump received from VM is saved in ",
                filename, " with signal ", signum, endl;
}

static void ptlcall_mmio_write(CPUX86State* cpu, W64 offset, W64 value,
        int length) {
    int calltype = (int)(cpu->regs[REG_rax]);
    W64 arg1 = cpu->regs[REG_rdi];
    W64 arg2 = cpu->regs[REG_rsi];
    W64 arg3 = cpu->regs[REG_rdx];
    W64 arg4 = cpu->regs[REG_r10];
    W64 arg5 = cpu->regs[REG_r8];

    switch(calltype) {
        case PTLCALL_VERSION:
            {
                if (!config.quiet) cout << "PTLCALL type PTLCALL_VERSION\n";
                cpu->regs[REG_rax] = 1;
                break;
            }
        case PTLCALL_MARKER:
            {
                if (!config.quiet) cout << "PTLCALL type PTLCALL_MARKER\n";
                cpu->regs[REG_rax] = 0;
                break;
            }
        case PTLCALL_ENQUEUE:
            {
                if (!config.quiet) cout << "PTLCALL type PTLCALL_ENQUEUE\n";

                /*
                 * arg1: Start of descriptor
                 * arg2: Total number of descriptors
                 */
                int num_desc = (W64)arg2;
                W64 desc_ptr = arg1;
                stringbuf command;

                foreach (i, num_desc) {
                    PTLsimCommandDescriptor desc;
                    desc.command = (W64)(ldq_kernel(desc_ptr));
                    desc.length = (W64)(ldq_kernel(desc_ptr + 8));

                    desc_ptr += 16;

                    char *command_str = (char*)qemu_malloc(desc.length + 1);
                    char *command_addr = (char*)(desc.command);
                    foreach (i, (W64s)desc.length) {
                        command_str[i] = (char)ldub_kernel(
                                (target_ulong)(command_addr));
                        command_addr++;
                    }
                    command_str[desc.length] = '\0';

                    command << command_str << " ";

                    qemu_free(command_str);
                }


                ptl_logfile << "Command received: " << command << endl << flush;
                pending_command_str = (char*)qemu_malloc(command.size() * sizeof(char));
                strcpy(pending_command_str, command.buf);
                pending_call_type = PTLCALL_ENQUEUE;
                simulation_configured = 1;

                /*
                 * Stop the QEMU vm and change ptlsim configuration
                 * QEMU will be automatically started in simulation mode
                 */
                cpu_exit(cpu);
                break;
            }
        case PTLCALL_CHECKPOINT:
            {
                if (!config.quiet) cout << "PTLCALL type PTLCALL_CHECKPOINT\n";

                char *checkpoint_name = (char*)qemu_malloc(arg2 + 1);
                char *name_addr = (char*)(arg1);
                foreach (i, (W64s)arg2) {
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
        case PTLCALL_CORE_DUMP:
            {
                /* User space application has crashed so save its core-dump
                 * into file '%appname-core-dump' */
                save_core_dump((char*)arg1, arg2, (char*)arg3, arg4,
                        arg5);
                break;
            }
        case PTLCALL_LOG:
            {
                char* log_ptr = (char*)arg1;
                W64 size = arg2;
                stringbuf vm_log(size+1);
                char tmp;

                foreach (i, (int)size) {
                    tmp = (char)ldub_kernel((target_ulong)(log_ptr));
                    vm_log.buf[i] = tmp;
                    log_ptr++;
                }
                vm_log.buf[size] = '\0';

                ptl_logfile << "[VM @" << sim_cycle << "] " << vm_log;
                break;
            }
        default :
            cout << "PTLCALL type unknown : ", calltype, endl;
            cpu->regs[REG_rax] = -EINVAL;
    }
}

static uint32_t ptlcall_mmio_read(CPUX86State* cpu, W64 offset, int length) {
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

    /* Register PTLsim PTLCALL mmio page */
    W64 ptlcall_mmio_pd = cpu_register_io_memory(ptlcall_mmio_read_ops,
            ptlcall_mmio_write_ops, NULL, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(PTLSIM_PTLCALL_MMIO_PAGE_PHYSADDR, 4096,
            ptlcall_mmio_pd);

    /* Register ptlsim assert callback functions */
    register_assert_cb(&dump_all_info);
    register_assert_cb(&dump_bbcache_to_logfile);
}

static const char COMMENT_CHAR = '#';
static bool is_commented(stringbuf& line)
{
    stringbuf opt;

    opt = line.strip();

    if (opt.buf[0] == COMMENT_CHAR) {
        return true;
    }

    return false;
}

void ptl_config_from_file(const char *filename) {
    stringbuf line;
    stringbuf cmd_line;
    char split_char[2] = {COMMENT_CHAR, '\0'};
    dynarray<stringbuf*> *cmds;

    ifstream cmd_file(filename);
    if (!cmd_file) {
        fprintf(stderr, "qemu: simconfig file '%s' not found\n",
                filename);
        exit(1);
    }

    for (;;) {

        line.reset();

		std::string temp;
		std::getline(cmd_file, temp);
		line << temp.c_str();

        if (!cmd_file)
            break;

        if (is_commented(line))
            continue;

        cmds = new dynarray<stringbuf*>();
        line.split(*cmds, split_char);

        if (!cmds->size())
            continue;

        stringbuf *cmd = (*cmds)[0];
        cmd_line << *cmd << " ";

        while (cmds->size()) {
            stringbuf *c = cmds->pop();
            delete c;
        }

        delete cmds;
    }

    ptl_machine_configure(cmd_line);
    simulation_configured = 1;
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

    PTLsimMachine* machine = PTLsimMachine::getmachine(config.core_name);
    if (machine && machine->handle_cpuid)
        return machine->handle_cpuid(index, count, eax, ebx, ecx, edx);

    return 0;
}

void create_checkpoint(const char* chk_name)
{
    if (!config.quiet)
        cout << "MARSSx86::Creating checkpoint ",
             chk_name, endl;

    QDict *checkpoint_dict = qdict_new();
    qdict_put_obj(checkpoint_dict, "name", QOBJECT(
                qstring_from_str(chk_name)));
    do_savevm(cur_mon, checkpoint_dict);

    if (!config.quiet)
        cout << "MARSSx86::Checkpoint ", chk_name,
             " created\n";
}

void ptl_check_ptlcall_queue() {

    if(pending_call_type != -1) {

        switch(pending_call_type) {
            case PTLCALL_ENQUEUE:
                {
                    if (!config.quiet) cout << "MARSSx86::Command received : ",
                         pending_command_str, endl;
                    ptl_machine_configure(pending_command_str);
                    break;
                }
            case PTLCALL_CHECKPOINT:
                {
                    create_checkpoint(pending_command_str);

                    switch(pending_call_arg3) {
                        case PTLCALL_CHECKPOINT_AND_SHUTDOWN:
                            if (!config.quiet) cout << "MARSSx86::Shutdown requested\n";
                            ptl_quit();
                            break;
                        default:
                            if (!config.quiet) cout << "MARSSx86::Unkonw Action\n";
                            break;
                    }
                    break;
                }
        }

        delete pending_command_str;
        pending_command_str = NULL;

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

    return *this;
}

# define PHYS_ADDR_MASK 0xfffffff000LL

W64 Context::virt_to_pte_phys_addr(W64 rawvirt, byte& level) {

    W64 ptep;
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

int Context::copy_from_vm(void* target, Waddr source, int bytes, PageFaultErrorCode& pfec, Waddr& faultaddr, bool forexec) {

    if (source == 0) {
        return -1;
    }

    W64 cr2 = -1;
    int n = 0 ;
    pfec = 0;

    setup_qemu_switch_all_ctx(*this);

    if(logable(10))
        ptl_logfile << "Copying from userspace ", bytes, " bytes from ",
                    source, endl;

    int exception = 0;
    int mmio = 0;
    check_and_translate(source, 0, 0, 0, exception,
            mmio, pfec, forexec);
    if (exception) {
        cr2 = cr[2];
        int old_exception = exception_index;
        int mmu_index = cpu_mmu_index((CPUState*)this);
        int fail = cpu_x86_handle_mmu_fault((CPUX86State*)this,
                source, 2, mmu_index, 1);
        cr[2] = cr2;
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

    check_and_translate(source + n, 0, 0, 0, exception,
            mmio, pfec, forexec);
    if (exception) {
        cr2 = cr[2];
        int old_exception = exception_index;
        int mmu_index = cpu_mmu_index((CPUState*)this);
        int fail = cpu_x86_handle_mmu_fault((CPUX86State*)this,
                source + n, 2, mmu_index, 1);
        cr[2] = cr2;
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
}

void Context::update_mode(bool is_kernel) {
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

    Waddr paddr;

    int mmu_index = cpu_mmu_index((CPUState*)this);
    int index = (virtaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    W64 tlb_addr;

    if likely (!store) {
        if likely (!is_code) {
            tlb_addr = tlb_table[mmu_index][index].addr_read;
        } else {
            tlb_addr = tlb_table[mmu_index][index].addr_code;
        }
    } else {
        tlb_addr = tlb_table[mmu_index][index].addr_write;
    }
	Waddr host_virtaddr;
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

        host_virtaddr = (Waddr)(virtaddr + tlb_table[mmu_index][index].addend);
        if (unlikely(get_phys_memory_address(host_virtaddr, paddr) < 0))
        {
            // Since the entry is in the TLB, it should also have a mapping, so this case should never arise
            printf("ERROR: Cannot find mapping for host virtual addr %lx\n", (unsigned long)virtaddr);
            assert(0);
        }
        if (paddr > qemu_ram_size) {
            if (qemu_ram_size < 0xe0000000 ) {
                printf("ERROR: guest physical address 0x%llx is out of bounds\n", paddr);
            } else {
                // we have a split memory from 0 to 3.5G and from 4G+
                if (paddr < 0x100000000ULL && paddr > 0xe0000000) {
                    // It seems that qemu won't allocate paddrs in this range so warn about it
                    printf("ERROR: guest physical address 0x%llx is between 3.5GB and 4.0GB\n", paddr);
                } else if (paddr >= 0x100000000ULL && paddr - (0x100000000ULL-0xe0000000) > qemu_ram_size) {
                    printf("ERROR: guest physical address 0x%llx is out of bounds\n", paddr);
                }
            }
        }
        return paddr;
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


    // this function copies from source_paddr which is invalid since now it is a guest physical
    // address. I don't think this function is ever called, but to make sure. If it does get
    // called, the guest physical address will have to be converted into a host virtual address
    // before being able to use this function again
    assert(0);
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
        // ptl_logfile << "Checker ctx\n", *checker_context, endl;
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
    W64 cr2 = cr[2];
    tlb_fill(virtaddr, is_write, mmu_index, NULL);
    ptl_stable_state = 0;

    if(kernel_mode) {
        if(logable(5))
            ptl_logfile << "Page fault in kernel mode...handled", endl,
                        flush;
    }

    setup_ptlsim_switch_all_ctx(*this);
    cr[2] = cr2;
    return;
}

bool Context::try_handle_fault(Waddr virtaddr, int store) {

    setup_qemu_switch_all_ctx(*this);

    if(logable(10))
        ptl_logfile << "Trying to fill tlb for addr: ", (void*)virtaddr, endl;

    W64 cr2 = cr[2];
    int mmu_index = cpu_mmu_index((CPUState*)this);
    int fault = cpu_x86_handle_mmu_fault((CPUState*)this, virtaddr, store, mmu_index, 1);

    cr[2] = cr2;

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

extern "C" void ptl_add_phys_memory_mapping(int8_t cpu_index, uint64_t host_vaddr, uint64_t guest_paddr)
{
  contextof(cpu_index).hvirt_gphys_map[(Waddr)host_vaddr] = (Waddr)guest_paddr;
}

void ptl_quit()
{
    in_simulation = 0;
    no_shutdown = 0;
    qemu_system_shutdown_request();
}

uint64_t get_sim_cpu_freq() {
    return config.core_freq_hz;
}

/* Simpoint Support */

struct Simpoint
{
    int label;
    int interval;

    Simpoint(int _interval, int _label)
        : label(_label), interval(_interval)
    { }

    bool operator < (const Simpoint& t) const
    {
        return (interval < t.interval);
    }

    bool operator == (const Simpoint& t) const
    {
        return (interval == t.interval);
    }

    bool operator > (const Simpoint& t) const
    {
        return (interval > t.interval);
    }
};

static dynarray<Simpoint*> simpoints;
static W64 total_simpoint_inst_complted = 0;
static int simpoint_ctr = -1;
static int simpoint_enabled = 0;

void add_simpoint(int point, int label)
{
    Simpoint* t = new Simpoint(point, label);
    simpoints.push(t);
}

int get_simpoint(int id)
{
    return simpoints[id]->interval;
}

int get_simpoint_label(int id)
{
    return simpoints[id]->label;
}

void clear_simpoints(void)
{
    foreach (i, simpoints.size()) {
        Simpoint* t = simpoints.pop();
        delete t;
    }

    simpoints.clear();
    simpoint_ctr = -1;
    total_simpoint_inst_complted = 0;
}

void read_simpoint_file()
{
    assert(simpoint_ctr == -1);
    int id;
    int point;
    stringbuf line;
    char split_char[2] = {' ', '\0'};
    ifstream is(config.simpoint_file);

    assert(simpoint_ctr == -1);
    if (!is) {
        cerr << "Error: Unable to read simpoint file: " <<
            config.simpoint_file << endl;
        ptl_quit();
    }

    assert(simpoint_ctr == -1);
    while (1) {
        dynarray<stringbuf*> split;
        line.reset();
        is.getline(line.buf, line.length);
        if (!is) break;

        line.split(split, split_char);

        point = atoi(split[0]->buf);
        id = atoi(split[1]->buf);

        // Here we assume that simpoints will be written in
        // assending order
        add_simpoint(point, id);

        foreach(i, split.size()) {
            stringbuf* tmp = split.pop();
            delete tmp;
        }
    }

    is.close();

    sort(simpoints.data, simpoints.size(), PointerSortComparator<Simpoint>());
}

void set_next_simpoint(CPUX86State* ctx)
{
    W64 point;

    simpoint_ctr++;

    if (simpoint_ctr >= simpoints.size()) {
        simpoint_enabled = 0;
        ctx->simpoint_decr = 0;
        return;
    }

    point = get_simpoint(simpoint_ctr) * config.simpoint_interval;
    ctx->simpoint_decr = (point - total_simpoint_inst_complted);
    total_simpoint_inst_complted = point;
    tb_flush(ctx);

    if (ctx->simpoint_decr == 0 && get_simpoint(simpoint_ctr) == 0) {
        ptl_simpoint_reached(ctx->cpu_index);
    }
}

stringbuf* get_simpoint_chk_name()
{
    stringbuf* name = new stringbuf();

    (*name) << config.simpoint_chk_name << "_sp_" <<
        get_simpoint_label(simpoint_ctr);

    return name;
}

void init_simpoints()
{
    /* First check if we are simulating only one core or not */
    if (NUM_SIM_CORES != 1) {
        cerr << "ERROR: Marss doesnt support simpoints with more than " <<
            "one CPU Context.  Please simulate with only one CPU.\n";
        ptl_quit();
    }

    read_simpoint_file();
    simpoint_enabled = 1;
}

/**
 * @brief Flag to indicate if simulation is waiting for fast-fwd to complete
 *
 * Flag vlaue 1 means count all instructions
 * Flag value 2 means count only user level instructions
 */
uint8_t ptl_fast_fwd_enabled = 0;

uint8_t sim_update_clock_offset = 1;

/**
 * @brief Set CPU's simpoint_decr count to fast-forward simulation mode
 */
void set_cpu_fast_fwd()
{
    W64 fwd_insns;

    if (config.fast_fwd_insns == 0 && config.fast_fwd_user_insns == 0)
        return;

    if (config.fast_fwd_insns > 0) {
        ptl_fast_fwd_enabled = 1;
        fwd_insns = config.fast_fwd_insns;
    } else if (config.fast_fwd_user_insns > 0) {
        ptl_fast_fwd_enabled = 2;
        fwd_insns = config.fast_fwd_user_insns;
    }

    /* Set each CPU's counter specified from config.fast_fwd_insns */
    W64 per_cpu_fast_fwd = fwd_insns / NUM_SIM_CORES;

    ptl_logfile << "All CPU context will be fast-forwared to " <<
        per_cpu_fast_fwd << " instructions.\n";

    foreach (i, NUM_SIM_CORES) {
        Context& ctx = contextof(i);
        ctx.simpoint_decr = per_cpu_fast_fwd;
        tb_flush(&ctx);
    }
}

/**
 * @brief Allocate part of remaining instructions to specified CPU
 *
 * @param ctx CPU Context in which instructions will be allocated
 */
static void adjust_fwd_insts(Context& ctx)
{
    W64 min_remaining = (W64)-1;
    Context* min_ctx = NULL;

    foreach (i, NUM_SIM_CORES) {
        if (i == ctx.cpu_index)
            continue;

        Context& t_ctx = contextof(i);

		if (t_ctx.halted && !qemu_cpu_has_work(&t_ctx) &&
				!t_ctx.stopped && t_ctx.simpoint_decr > 0) {
            min_remaining = min((W64)t_ctx.simpoint_decr, min_remaining);
            if (min_remaining == t_ctx.simpoint_decr)
                min_ctx = &contextof(i);
        }
    }

    /* Maximum allocation is 10million instructions */
    min_remaining = min(min_remaining, (W64)10000000);

    if (min_remaining && min_ctx) {
        min_ctx->simpoint_decr -= min_remaining;
        ctx.simpoint_decr = min_remaining;

        if (logable(2)) {
            ptl_logfile << "Min ctx " << int(min_ctx->cpu_index) <<
                " allocated " << min_remaining << " instructions to " <<
                int(ctx.cpu_index) << " CPU\n";
        }
    } else {
        /* We can't find any CPU Context with remaining
         * instructions so we will force switch to simulation */
        ctx.simpoint_decr = 0;
    }
}

/**
 * @brief CPU has emulated fast-fwd instructions, check if simulation point has
 * reached or not
 *
 * @param ctx CPU Context that finished emulating its allocated instructions
 */
static void cpu_fast_fwded(Context& ctx)
{
    bool all_halted_or_stopped = true;
    bool others_halted = false;
    W64 insns_remaining = 0;

    /* Stop this CPU and check if all CPU are stopped or not */
    ctx.stopped = 1;

    /* Check if all other cpus are stopped or not */
    foreach (i, NUM_SIM_CORES) {
        Context& t_ctx = contextof(i);
        insns_remaining += t_ctx.simpoint_decr;

        if (i != ctx.cpu_index)
            others_halted |= (t_ctx.halted);

		all_halted_or_stopped &= (t_ctx.stopped || t_ctx.halted ||
				!qemu_cpu_has_work(&t_ctx));
    }

    if (others_halted && insns_remaining > 100) {
        /* This happens because all other CPUs are either halted
         * or stopped and if we still have some instructions
         * remaining then we allocate more instructions to this core
         * and let it run in emulation mode untill our instruciton
         * count reaches to near zero. */
        adjust_fwd_insts(ctx);

        /* If we found some more instructions to emulate then return */
        if (ctx.simpoint_decr) {
            if (logable(2)) {
                ptl_logfile << "Cpu " << int(ctx.cpu_index) << " will emulate " <<
                    ctx.simpoint_decr << " instructions\n";
            }
            ctx.stopped = 0;
            return;
        }
    }

    /* If all CPU's are stopped then issue -run to start simulation */
    if (all_halted_or_stopped) {

        /* If we still have any instrucitons remaining then print message
         * to logfile indicating that we are switching to simulation
         * earlier than expected. */
        if (insns_remaining > 1000) {
			/* Restore this counter for debugging only */
			ctx.simpoint_decr = insns_remaining;

            ptl_logfile << "WARNING: Early switching to simulation mode. ";
            ptl_logfile << "Instrucitons remaining in each CPU context to fast-forward are:\n";

            foreach (i, NUM_SIM_CORES) {
                ptl_logfile << "\tCPU " << (int)i << ": " <<
                    (int)(contextof(i).simpoint_decr) << "\n";
                contextof(i).simpoint_decr = 0;
            }
        }

        ptl_fast_fwd_enabled = 0;

        foreach (i, NUM_SIM_CORES) {
            contextof(i).stopped = 0;
            tb_flush(&contextof(i));
        }

        if (config.fast_fwd_checkpoint.size() > 0) {
            create_checkpoint(config.fast_fwd_checkpoint.buf);
            ptl_quit();
        } else {
            start_simulation = 1;
        }
    }
}

/**
 * @brief CPU has reached to specified point in emulation mode
 *
 * @param cpuid ID of CPU that reached to the specified point
 *
 * This is a callback function called when emulating CPU reaches to a
 * pre-specific point in emulation such as number of instructions
 * executed/emulated.
 */
void ptl_simpoint_reached(int cpuid)
{
    Context& ctx = contextof(cpuid);

    if (simpoint_enabled) {

        stringbuf* chk_name = get_simpoint_chk_name();
        create_checkpoint(chk_name->buf);

        set_next_simpoint(&ctx);

        delete chk_name;
    }

    if (config.fast_fwd_insns > 0 || config.fast_fwd_user_insns > 0) {
        cpu_fast_fwded(ctx);
    }
}

/**
 * @brief Initialize simulation specific structures after QEMU finish its
 * initialization
 */
void ptl_qemu_initialized(void)
{
    qemu_initialized = 1;

    // If config.run_tests is enabled, then run testcases
    if(config.run_tests) {
        run_tests();
    }

    if (simpoint_enabled) {
        set_next_simpoint(&contextof(0));
    }

    set_cpu_fast_fwd();

    if (config.run) {
        /* If we are going to run simulations immediately then we set
         * simulation clock offset before QEMU updates offset with
         * current clock values. */
        cpu_set_sim_ticks();
    }
}
