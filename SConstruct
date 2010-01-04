
# Top level SConstruct for PQRS

import os

# Automatically set the -j option to No of available CPUs*2
# This options gives the best compilation time.
# user can override this by specifying -j option at runtime

num_cpus = 1
# For python 2.6+
try:
    import multiprocessing
    num_cpus = multiprocessing.cpu_count()
except (ImportError,NotImplementedError):
    pass

try:
    res = int(os.sysconf('SC_NPROCESSORS_ONLN'))
    if res > 0:
        num_cpus = res
except (AttributeError,ValueError):
    pass

SetOption('num_jobs', num_cpus * 2)
print("running with -j%s" % GetOption('num_jobs'))

# Our build order is as following:
# 1. Configure QEMU
# 2. Build PTLsim
# 3. Build QEMU

CC = "g++"

curr_dir = os.getcwd()
qemu_dir = "%s/qemu" % curr_dir
ptl_dir = "%s/ptlsim" % curr_dir

# 1. Configure QEMU
qemu_env = Environment()
qemu_env.Decider('MD5-timestamp')
qemu_env['CC'] = CC
qemu_configure_script = "%s/SConfigure" % qemu_dir
Export('qemu_env')

#print("--Configuring QEMU--")
config_success = SConscript(qemu_configure_script)

if config_success != "success":
    print("ERROR: QEMU configuration error")
    exit(-1)

#print("--Configuring QEMU Done--")

# 2. Compile PTLsim
ptl_compile_script = "%s/SConstruct" % ptl_dir
ptl_env = Environment()
ptl_env.Decider('MD5-timestamp')
ptl_env['CC'] = CC
ptl_env['qemu_dir'] = qemu_dir

Export('ptl_env')

#print("--Compiling PTLsim--")
ptlsim_lib = SConscript(ptl_compile_script)

if ptlsim_lib == None:
    print("ERROR: PTLsim compilation error")
    exit(-1)

#print("--PTLsim Compiliation Done--")

# 3. Compile QEMU
qemu_compile_script = "%s/SConstruct" % qemu_dir
qemu_target = {}
Export('ptlsim_lib')
ptlsim_inc_dir = "%s/sim" % ptl_dir
Export('ptlsim_inc_dir')

qemu_bins = []
for target in qemu_env['targets']:
    qemu_target = target
    Export('qemu_target')
    qemu_bin = SConscript(qemu_compile_script)
    qemu_bins.append(qemu_bin)

for qemu_bin in qemu_bins:
    print("Compiled PQRS binary : %s" % str(qemu_bin))
