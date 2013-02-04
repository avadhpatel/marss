
# Top level SConstruct for MARSSx86

import os
import config_helper

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

# We don't support python 2.4 or less
import sys
if sys.version_info < (2, 5):
    print("Please use python 2.5 or higher for MARSS")
    sys.exit(-1)


SetOption('num_jobs', num_cpus + 1)
print("running with -j%s" % GetOption('num_jobs'))

# Our build order is as following:
# 1. Configure QEMU
# 2. Build PTLsim
# 3. Build QEMU

curr_dir = os.getcwd()
qemu_dir = "%s/qemu" % curr_dir
ptl_dir = "%s/ptlsim" % curr_dir

# Colored Output of Compilation
import sys

colors = {}
colors['cyan']   = '\033[96m'
colors['purple'] = '\033[95m'
colors['blue']   = '\033[94m'
colors['green']  = '\033[92m'
colors['yellow'] = '\033[93m'
colors['red']    = '\033[91m'
colors['end']    = '\033[0m'

#If the output is not a terminal, remove the colors
if not sys.stdout.isatty():
   for key, value in colors.iteritems():
      colors[key] = ''

compile_source_message = '%sCompiling %s:: %s$SOURCE ==> $TARGET%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

create_header_message = '%sCreating %s==> %s$TARGET%s' % \
   (colors['green'], colors['purple'], colors['yellow'], colors['end'])

compile_shared_source_message = '%sCompiling shared %s==> %s$SOURCE%s' % \
   (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

link_program_message = '%sLinking Program %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_library_message = '%sLinking Static Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

ranlib_library_message = '%sRanlib Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_shared_library_message = '%sLinking Shared Library %s==> %s$TARGET%s' % \
   (colors['red'], colors['purple'], colors['yellow'], colors['end'])

pretty_printing=ARGUMENTS.get('pretty',1)

config_file = ARGUMENTS.get('config', "config")
config_debug = ARGUMENTS.get('config-debug', False)

# Base Environment used to compile Marss code (QEMU and PTLSIM both)
if int(pretty_printing) :
    base_env = Environment(
            CXXCOMSTR = compile_source_message,
            CREATECOMSTR = create_header_message,
            CCCOMSTR = compile_source_message,
            SHCCCOMSTR = compile_shared_source_message,
            SHCXXCOMSTR = compile_shared_source_message,
            ARCOMSTR = link_library_message,
            RANLIBCOMSTR = ranlib_library_message,
            SHLINKCOMSTR = link_shared_library_message,
            LINKCOMSTR = link_program_message,
            )
else:
    base_env = Environment()
# Setup the default envrionment paths from User's envrionment
base_env['ENV'] = os.environ
# To specify your c++ compiler uncomment this line and
# set the correct path to your c++ compiler
base_env['CXX'] = "g++"
base_env['CC'] = base_env['CXX']

base_env['config'] = config_helper.parse_config(config_file, debug=config_debug)

# Check the required number of cores

# 1. Configure QEMU
qemu_env = base_env.Clone()
qemu_env.Decider('MD5-timestamp')
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
ptl_env = base_env.Clone()
ptl_env.Decider('MD5-timestamp')
ptl_env.SetDefault(qemu_dir = qemu_dir)
ptl_env.SetDefault(RT_DIR = "%s" % curr_dir)

Export('ptl_env')

#print("--Compiling PTLsim--")
ptlsim_lib = SConscript(ptl_compile_script)

if ptlsim_lib == None:
    print("ERROR: PTLsim compilation error")
    exit(-1)

#print("--PTLsim Compiliation Done--")

# Get plugin modules
plugin_compile_script = "plugins/SConscript"
plugins = SConscript(plugin_compile_script)

# 3. Compile QEMU
qemu_compile_script = "%s/SConstruct" % qemu_dir
qemu_target = {}
Export('ptlsim_lib')
Export('plugins')
ptlsim_inc_dir = "%s/sim" % ptl_dir
Export('ptlsim_inc_dir')

qemu_bins = []
for target in qemu_env['targets']:
    qemu_target = target
    Export('qemu_target')
    qemu_bin = SConscript(qemu_compile_script)
    qemu_bins.append(qemu_bin)
