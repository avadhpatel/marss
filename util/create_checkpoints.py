#!/usr/bin/env python

import os
import subprocess
import sys


# Set up default variables
cwd = os.getcwd()
qemu_bin = '%s/qemu/qemu-system-x86_64' % cwd
# qemu_img = '/home/avadh/workspace/vm/spec2006_2.qcow2'
qemu_img = '/home/avadh/workspace/vm/splash_4core_orig.qcow2'
# qemu_img = '/var/work/vm/splash_4core.qcow2'
vm_memory = 4096
qemu_cmd = ''
vm_smp = 4

def add_to_cmd(opt):
    global qemu_cmd
    qemu_cmd = "%s %s" % (qemu_cmd, opt)

# Generate a common command string
add_to_cmd(qemu_bin)
add_to_cmd('-m %d' % vm_memory)
add_to_cmd('-serial pty')
#add_to_cmd('-smp %d' % vm_smp)
add_to_cmd('-vnc :20')
add_to_cmd('-cpu core2duo')

# Add Image at the end
# add_to_cmd('-hda %s' % qemu_img)
add_to_cmd('-drive file=%s,cache=unsafe' % qemu_img)

# Checkpoint list
check_list = []

# Parse with parsecmgmt
parsec_bench_list = ['blackscholes', 'bodytrack', 'ferret', 'freqmine',
        'swaptions', 'fluidanimate', 'vips', 'x264', 'canneal', 'dedup',
        'streamcluster', 'facesim', 'raytrace']
# parsec_bench_list = ['facesim', 'raytrace']

pre_setup_str = '''cd parsec-2.1 ; . env.sh
        export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
        '''
parsec_roi_list = []
for bench in parsec_bench_list:
    pre_command = "%s\nexport CHECKPOINT_NAME=\"%s\"\n" % (pre_setup_str, bench)
    parsec_cmd = "parsecmgmt -a run -c gcc-hooks -x roi -n %d -i simsmall -p %s" % (vm_smp, bench)
    bench_dict = {'name' : bench, 'command' : '%s\n%s\n' % (pre_command, parsec_cmd) }
    parsec_roi_list.append(bench_dict)


parsec_list = [
        {'name' : 'blackscholes',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/blackscholes/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint blackscholes; /root/parsec-2.1/pkgs/apps/blackscholes/inst/amd64-linux.gcc-hooks/bin/blackscholes %d in_4K.txt prices.txt; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'bodytrack',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/bodytrack/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint bodytrack; /root/parsec-2.1/pkgs/apps/bodytrack/inst/amd64-linux.gcc-hooks/bin/bodytrack sequenceB_1 4 1 1000 5 0 %d; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'ferret',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/ferret/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint ferret; /root/parsec-2.1/pkgs/apps/ferret/inst/amd64-linux.gcc-hooks/bin/ferret corel lsh queries 10 20 %d output.txt; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'freqmine',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/freqmine/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint freqmine; /root/parsec-2.1/pkgs/apps/freqmine/inst/amd64-linux.gcc-hooks/bin/freqmine kosarak_250k.dat 220; ~/stop_sim
         '''
         },
        {'name' : 'swaptions',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/swaptions/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint swaptions; /root/parsec-2.1/pkgs/apps/swaptions/inst/amd64-linux.gcc-hooks/bin/swaptions -ns 16 -sm 5000 -nt %d; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'fluidanimate',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/fluidanimate/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint fluidanimate; /root/parsec-2.1/pkgs/apps/fluidanimate/inst/amd64-linux.gcc-hooks/bin/fluidanimate %d 5 in_35K.fluid out.fluid; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'vips',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/vips/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/libxml2/inst/amd64-linux.gcc-hooks/lib:$LD_LIBRARY_PATH
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint vips; /root/parsec-2.1/pkgs/apps/vips/inst/amd64-linux.gcc-hooks/bin/vips im_benchmark pomegranate_1600x1200.v output.v; ~/stop_sim
         '''
         },
        {'name' : 'x264',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/apps/x264/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint x264; /root/parsec-2.1/pkgs/apps/x264/inst/amd64-linux.gcc-hooks/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads %d -o eledream.264 eledream_640x360_8.y4m; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'canneal',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/kernels/canneal/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint canneal; /root/parsec-2.1/pkgs/kernels/canneal/inst/amd64-linux.gcc-hooks/bin/canneal %d 10000 2000 100000.nets 32; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'dedup',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/kernels/dedup/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint dedup; /root/parsec-2.1/pkgs/kernels/dedup/inst/amd64-linux.gcc-hooks/bin/dedup -c -p -f -t %d -i media.dat -o output.dat.ddp; ~/stop_sim
         ''' % vm_smp
         },
        {'name' : 'streamcluster',
         'command' :
         '''cd parsec-2.1
         source env.sh
         cd /root/parsec-2.1/pkgs/kernels/streamcluster/run
         export LD_LIBRARY_PATH=/root/parsec-2.1/pkgs/libs/hooks/inst/amd64-linux.gcc-hooks/lib
         export PARSEC_CPU_NUM=`grep processor /proc/cpuinfo | wc -l`; echo $PARSEC_CPU_NUM
         ~/create_checkpoint streamcluster; /root/parsec-2.1/pkgs/kernels/streamcluster/inst/amd64-linux.gcc-hooks/bin/streamcluster 2 5 1 10 10 5 none output.txt %d; ~/stop_sim
         ''' % vm_smp
         },
        ]

# Splash2 Benchmarks
# Directory structure :
# $HOME/splash2/codes/[apps/kernels]/[benchmark]/[executables]
splash_dir = "/root/splash2/codes"

# bench_specific_structure : ['bench_name', 'apps/kernels', 'benchmark_dir', 'binary_name', 'benchmark_options']
splash_bench_specifics = [
        ['barnes', 'apps', 'barnes', 'BARNES', ' < input.%d'],
        ['fmm', 'apps', 'fmm', 'FMM', ' < inputs/input.16384.%d'],
        ['ocean_c', 'apps', 'ocean/contiguous_partitions', 'OCEAN', ' -n258 -p%d -r20000 -t2880'],
        ['ocean_nc', 'apps', 'ocean/non_contiguous_partitions', 'OCEAN', ' -n258 -p%d -r20000 -t2880'],
        ['water_n', 'apps', 'water-nsquared', 'WATER-NSQUARED', ' < input.%d'],
        ['water_s', 'apps', 'water-spatial', 'WATER-SPATIAL', ' < input.%d'],
        ['cholesky', 'kernels', 'cholesky', 'CHOLESKY', ' -p%d -B32 -C16384 inputs/tk14.O'],
        ['fft', 'kernels', 'fft', 'FFT', ' -m10 -p%d -n65536 -l4'],
        ['lu_c', 'kernels', 'lu/contiguous_blocks', 'LU', ' -n512 -p%d -b16'],
        ['lu_nc', 'kernels', 'lu/non_contiguous_blocks', 'LU', ' -n512 -p%d -b16'],
        ['radix', 'kernels', 'radix', 'RADIX', ' -p%d -n262144 -r1024 -m524288'],
        ]
splash_bench_tilera_paper_specifics = [
        ['barnes', 'apps', 'barnes', 'BARNES', ' < input.%d'],
        ['fmm', 'apps', 'fmm', 'FMM', ' < inputs/input.16384.%d'],
        ['ocean_c', 'apps', 'ocean/contiguous_partitions', 'OCEAN', ' -n1024 -p%d -r20000 -t2880'],
        ['ocean_nc', 'apps', 'ocean/non_contiguous_partitions', 'OCEAN', '-n1024 -p%d -r20000 -t2880'],
        ['water_n', 'apps', 'water-nsquared', 'WATER-NSQUARED', ' < input.%d'],
        ['water_s', 'apps', 'water-spatial', 'WATER-SPATIAL', ' < input.%d'],
        ['cholesky', 'kernels', 'cholesky', 'CHOLESKY', ' -p%d -B32 -C16384 inputs/tk17.O'],
        ['fft', 'kernels', 'fft', 'FFT', ' -m20 -p%d -n65536 -l4'],
        ['lu_c', 'kernels', 'lu/contiguous_blocks', 'LU', ' -n1024 -p%d -b16'],
        ['lu_nc', 'kernels', 'lu/non_contiguous_blocks', 'LU', ' -n1024 -p%d -b16'],
        ['radix', 'kernels', 'radix', 'RADIX', ' -p%d -n2097152 -r1024 -m524288'],
        ]
splash_list = []
for bench in splash_bench_tilera_paper_specifics:
  bench_ = {'name' : bench[0],
    'command' : 
        '''cd %s/%s/%s
        export NUM_CPUS=`grep processor /proc/cpuinfo | wc -l`; echo $NUM_CPUS
        ~/create_checkpoint %s; ./%s %s ; ~/stop_sim
        ''' % (splash_dir, bench[1], bench[2], bench[0], bench[3], (bench[4] %
            vm_smp))
        }
  splash_list.append(bench_)

# SPEC 2006 Benchmarks
# Directory structure :
# $HOME/spec2006/[compile_config]/[bench_name]/[executables]

spec_list = [
        {'name' : 'perl', # 0
         'command' :
         '''cd spec2006bin/icc.fast/400.perlbench/
         ~/create_checkpoint perl; ./perlbench_base.marss.icc.fast -I. -I./lib pack.pl ; ~/stop_sim
         '''
         },
        {'name' : 'bzip', # 1
         'command' :
         '''cd spec2006bin/icc/401.bzip2/
         ~/create_checkpoint bzip; ./bzip2_base.marss.icc.core2 input.program 2 ; ~/stop_sim
         '''
         },
        {'name' : 'gcc', # 2
         'command' :
         '''cd spec2006bin/icc/403.gcc/
         ~/create_checkpoint gcc; ./gcc_base.marss.icc.core2 cccp.i -o cccp.s ; ~/stop_sim
         '''
         },
        {'name' : 'bwaves', # 3
         'command' :
         '''ulimit -s unlimited; cd spec2006bin/icc/410.bwaves/
         ~/create_checkpoint bwaves; ./bwaves_base.marss.icc.core2 ; ~/stop_sim
         '''
         },
        {'name' : 'gamess', # 4
         'command' :
         '''cd spec2006bin/icc/416.gamess/
         ~/create_checkpoint gamess; ./gamess_base.marss.icc.core2 < exam29.config ; ~/stop_sim
         '''
         },
        {'name' : 'mcf', # 5
         'command' :
         '''cd spec2006bin/icc/429.mcf/
         ~/create_checkpoint mcf; ./mcf_base.marss.icc.core2 inp.in ; ~/stop_sim
         '''
         },
        {'name' : 'milc', # 6
         'command' :
         '''cd spec2006bin/gcc/433.milc/
         ~/create_checkpoint milc; ./milc_base.marss.gcc.core2 < su3imp.in ; ~/stop_sim
         '''
         },
        {'name' : 'zeusmp', # 7
         'command' :
         '''ulimit -s unlimited; cd spec2006bin/icc/434.zeusmp/
         ~/create_checkpoint zeusmp; ./zeusmp_base.marss.icc.core2 ; ~/stop_sim
         '''
         },
        {'name' : 'gromacs', # 8
         'command' :
         '''cd spec2006bin/gcc/435.gromacs/
         ~/create_checkpoint gromacs ; ./gromacs_base.marss.gcc.core2 -silent -deffnm gromacs -nice 0 ; ~/stop_sim
         '''
         },
        {'name' : 'cactusADM', # 9
         'command' :
         '''cd spec2006bin/gcc/436.cactusADM/
         ~/create_checkpoint cactusADM; ./cactusADM_base.marss.gcc.core2 benchADM.par ; ~/stop_sim
         '''
         },
        {'name' : 'leslie3d', #10
         'command' :
         '''ulimit -s unlimited; cd spec2006bin/icc/437.leslie3d/
         ~/create_checkpoint leslie3d; ./leslie3d_base.marss.icc.core2 < leslie3d.in ; ~/stop_sim
         '''
         },
        {'name' : 'namd', #11
         'command' :
         '''cd spec2006bin/icc/445.namd/
         ~/create_checkpoint namd; ./namd_base.marss.icc.core2  --input namd.input --iterations 1 --output namd.out ; ~/stop_sim
         '''
         },
        {'name' : 'gobmk', #12
         'command' :
         '''cd spec2006bin/icc/445.gobmk/
         ~/create_checkpoint gobmk; ./gobmk_base.marss.icc.core2 --quiet --mode gtp < capture.tst ; ~/stop_sim
         '''
         },
        {'name' : 'deal2', #13
         'command' :
         '''cd spec2006bin/gcc/447.dealII/
         ~/create_checkpoint deal2; ./dealII_base.marss.gcc.core2 2 ; ~/stop_sim
         '''
         },
        {'name' : 'soplex', #14
         'command' :
         '''cd spec2006bin/icc/450.soplex/
         ~/create_checkpoint soplex; ./soplex_base.marss.icc.core2 test.mps ; ~/stop_sim
         '''
         },
        {'name' : 'povray', #15
         'command' :
         '''cd spec2006bin/icc/453.povray/
         ~/create_checkpoint povray; ./povray_base.marss.icc.core2 SPEC-benchmark-test.ini ; ~/stop_sim
         '''
         },
        {'name' : 'calculix', #16
         'command' :
         '''cd spec2006bin/gcc/454.calculix/
         ~/create_checkpoint calculix; ./calculix_base.marss.gcc.core2 beampic ; ~/stop_sim
         '''
         },
        {'name' : 'hmm', #17
         'command' :
         '''cd spec2006bin/icc/456.hmmer/
         ~/create_checkpoint hmm; ./hmmer_base.marss.icc.core2 --fixed 0 --mean 325 --num 4500 --sd 200 --seed 0 bombesin.hmm ; ~/stop_sim
         '''
         },
        {'name' : 'sjeng', #18
         'command' :
         '''cd spec2006bin/icc/458.sjeng/
         ~/create_checkpoint sjeng; ./sjeng_base.marss.icc.core2 test.txt ; ~/stop_sim
         '''
         },
        {'name' : 'GemsFDTD', #19
         'command' :
         '''ulimit -s unlimited; cd spec2006bin/icc/459.GemsFDTD/
         ~/create_checkpoint GemsFDTD; ./GemsFDTD_base.marss.icc.core2 ; ~/stop_sim
         '''
         },
        {'name' : 'quantum', #20
         'command' :
         '''cd spec2006bin/icc/462.libquantum/
         ~/create_checkpoint quantum; ./libquantum_base.marss.icc.core2 33 5 ; ~/stop_sim
         '''
         },
        {'name' : 'h264', #21
         'command' :
         '''cd spec2006bin/icc/464.h264ref/
         ~/create_checkpoint h264; ./h264ref_base.marss.icc.core2 -d foreman_test_encoder_baseline.cfg ; ~/stop_sim
         '''
         },
        {'name' : 'tonto', #22
         'command' :
         '''cd spec2006bin/icc/465.tonto/
         ~/create_checkpoint tonto; ./tonto_base.marss.icc.core2 ; ~/stop_sim
         '''
         },
        {'name' : 'lbm', #23
         'command' :
         '''cd spec2006bin/icc/470.lbm/
         ~/create_checkpoint lbm; ./lbm_base.marss.icc.core2 20 reference.dat 0 1 100_100_130_cf_a.of ; ~/stop_sim
         '''
         },
        {'name' : 'omnetpp', #24
         'command' :
         '''cd spec2006bin/icc/471.omnetpp/
         ~/create_checkpoint omnetpp; ./omnetpp_base.marss.icc.core2 omnetpp.ini ; ~/stop_sim
         '''
         },
        {'name' : 'astar', #25
         'command' :
         '''cd spec2006bin/icc/473.astar/
         ~/create_checkpoint astar; ./astar_base.marss.icc.core2 lake.cfg ; ~/stop_sim
         '''
         },
        {'name' : 'sphinx', #26
         'command' :
         '''cd spec2006bin/icc/482.sphinx3/
         ~/create_checkpoint sphinx; ./sphinx_livepretend_base.marss.icc.core2 ctlfile . args.an4 ; ~/stop_sim
         '''
         },
        {'name' : 'xalanc', #27
         'command' :
         '''cd spec2006bin/gcc/483.xalancbmk/
         ~/create_checkpoint xalanc; ./Xalan_base.marss.gcc.core2 -v test.xml xalanc.xsl ; ~/stop_sim
         '''
         },
        ]

# To create single checkpoint
#check_list.append(parsec_list[6])
#check_list.append(parsec_roi_list[6])
check_list = splash_list

# To create all spec checkpoints
# check_list = splash_list

print("Execution command: %s" % qemu_cmd)
print("Number of Chekcpoints to create: %d" % len(check_list))

login_cmds = ["root\n", "root\n"]

def pty_to_stdout(fd, untill_chr):
    chr = '1'
    while chr != untill_chr:
        chr = os.read(fd, 1)
        sys.stdout.write(chr)
    sys.stdout.flush()

def pty_login(fd):
    os.write(fd, login_cmds[0])
    pty_to_stdout(fd, ':')
    os.write(fd, login_cmds[1])

# Start simulation from checkpoints
pty_prefix = 'char device redirected to '
for checkpoint in check_list:

    print("Starting QEMU for checkpoint: %s" % checkpoint['name'])

    p = subprocess.Popen(qemu_cmd.split(), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=0)

    pty_term = None

    while p.poll() is None:
        line = p.stdout.readline()
        sys.stdout.write(line)
        if line.startswith(pty_prefix):
            dev_name = line[len(pty_prefix):].strip()

            # Open the device terminal and send simulation command
            pty_term = os.open(dev_name, os.O_RDWR)

            break

    if pty_term == None:
        print("ERROR: While connecting with pty terminal")
        continue

    pty_to_stdout(pty_term, ':')

    # Now send the login commands to the termianl and wait
    # untill some response text
    pty_login(pty_term)

    pty_to_stdout(pty_term, '#')

    # At this point we assume that we have successfully logged in
    # Now give the command to create checkpoint
    os.write(pty_term, checkpoint['command'])

    pty_to_stdout(pty_term, '#')

    sys.stdout.write('\n')
    for line in p.stdout:
        sys.stdout.write(line)

    # Wait for simulation to complete
    p.wait()
