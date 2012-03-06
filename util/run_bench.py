#!/usr/bin/env python

#
# This script is used to run checkpointed images of MARSSx86 Simulator.
# To use this script uses 'util.cfg' configuration file to read a simulation
# run configuration. Take a look at 'util.cfg.example' to get the idea on how
# to setup a run configuration.
#
# Author  : Avadh Patel
# Contact : apatel at cs.binghamton.edu
#


import os
import subprocess
import sys
import copy
import itertools

from optparse import OptionParser
from threading import Thread, Lock

import config

# Helper functions

def get_run_config(conf, name):
    # First check if given configuration exists or not
    conf_name = "run %s" % name

    if not conf.has_section(conf_name):
        print("Unable to find configuration %s from config file." % conf_name)
        exit(-1)

    return conf_name

def get_list_from_conf(value):
    # User can specify configuration values either in comma seperated, or new
    # line seperated or mix of both.
    ret = []
    for i in value.split('\n'):
        for j in i.split(','):
            if len(j) > 0:
                ret.append(j.strip())
    return ret

# First check if user has provided directory to save all results files
opt_parser = OptionParser("Usage: %prog [options] run_config")
opt_parser.add_option("-d", "--output-dir", dest="output_dir",
        type="string", help="Name of the output directory to save all results")
opt_parser.add_option("-c", "--config",
        help="Configuration File. By default use util.cfg in util directory")
opt_parser.add_option("-e", "--email", action="store_true",
        help="Send email using 'send_gmail.py' script after completion")
opt_parser.add_option("-i", "--iterate", action="store", default=1, type=int,
        help="Run simulation N times")
opt_parser.add_option("-n", "--num-insts", dest="num_insts", default=1,
        type=int, help="Run N instance of simulations in parallel")

(options, args) = opt_parser.parse_args()

# Check if there is any configration argument provided or not
if len(args) == 0 or args[0] == None:
    print("Please provide configuration name.")
    opt_parser.print_help()
    exit(-1)

# Read configuration file
conf_parser = config.read_config(options.config)

if options.output_dir == None:
    options.output_dir = "."
else:
    print("Results files will be stored in %s directory" % options.output_dir)
    if not os.path.exists(options.output_dir):
        os.makedirs(options.output_dir)

output_dir = options.output_dir + "/"
output_dirs = [output_dir]

if options.iterate > 1:
    output_dirs = []
    for i in range(options.iterate):
        i_dir = output_dir + "run_%d/" % (i + 1)
        output_dirs.append(i_dir)
        if not os.path.exists(i_dir):
            os.makedirs(i_dir)

# Get the configuration
run_sec = get_run_config(conf_parser, args[0])

# Get qemu binary
qemu_bin = conf_parser.get(run_sec, 'qemu_bin')
if not os.path.exists(qemu_bin):
    print("Qemu binary file (%s) doesn't exists." % qemu_bin)
    exit(-1)

# Get disk images
qemu_img = conf_parser.get(run_sec, 'images')
qemu_img = get_list_from_conf(qemu_img)

for img in qemu_img:
    if not os.path.exists(img):
        print("Qemu disk image (%s) doesn't exists." % img)
        exit(-1)

# Get VM memory
vm_memory = conf_parser.get(run_sec, 'memory')

if not vm_memory:
    print("Please specify 'memory' in your config.")
    exit(-1)

if conf_parser.has_option(run_sec, 'vnc_counter'):
    vnc_counter = conf_parser.getint(run_sec, 'vnc_counter')
else:
    vnc_counter = None

# Checkpoin List
if not conf_parser.has_option(run_sec, 'suite'):
    print("Plese specify benchmark suite using 'suite' option.")
    exit(-1)

suite = "suite %s" % conf_parser.get(run_sec, 'suite')

if not conf_parser.has_section(suite):
    print("Unable to find section '%s' in your configuration." % suite)
    exit(-1)

if not conf_parser.has_option(suite, 'checkpoints'):
    print("Please specify checkpoints in section '%s'." % suite)
    exit(-1)

check_list = conf_parser.get(suite, 'checkpoints')
check_list = get_list_from_conf(check_list)
print("Checkpoints: %s" % str(check_list))

if options.iterate > 1:
    print("Will run for %d iterations." % options.iterate)

# Get the simconfig
if not conf_parser.has_option(run_sec, 'simconfig'):
    print("Please specify simconfig in section '%s'." % run_sec)
    exit(-1)

simconfig = conf_parser.get(run_sec, 'simconfig', True)
print("simconfig: %s" % simconfig)

# Get optional qemu arguments
qemu_cmd = ''
qemu_args = ''

if conf_parser.has_option(run_sec, 'qemu_args'):
    qemu_args = conf_parser.get(run_sec, 'qemu_args')

if 'snapshot' not in qemu_args:
    qemu_args = '%s -snapshot' % qemu_args

num_threads = min(int(options.num_insts), len(check_list))

# If user give argument 'out' then print the output of simulation run
# to stdout else ignore it
out_to_stdout = False

checkpoint_lock = Lock()
checkpoint_iter = itertools.product(output_dirs, check_list)

print("Chekcpoints to run: %s" % str(check_list))
print("%d parallel simulation instances will be run." % num_threads)
print("All files will be saved in: %s" % output_dir)

def pty_to_stdout(fd, untill_chr):
    chr = '1'
    while chr != untill_chr:
        chr = os.read(fd, 1)
        sys.stdout.write(chr)
    sys.stdout.flush()

def gen_simconfig(args):
    global simconfig
    gen_cfg = simconfig
    recursive_count = 0
    while '%' in gen_cfg or recursive_count > 10:
        gen_cfg = gen_cfg % args
        recursive_count += 1
    return gen_cfg

# Thread class that will store the output on the serial port of qemu to file
class SerialOut(Thread):

    def __init__(self, out_filename, out_devname):
        # global output_dir
        super(SerialOut, self).__init__()
        self.out_filename = out_filename
        self.out_devname = out_devname

    def run(self):
        # Open the serial port and a file
        out_file = open(self.out_filename, 'w')
        out_dev_file = os.open(self.out_devname, os.O_RDONLY)

        try:
            while True:
                line = os.read(out_dev_file, 1)
                out_file.write(line)
                if len(line) == 0:
                    break
        except OSError:
            pass

        print("Writing to output file completed")
        out_file.close()
        os.close(out_dev_file)

# Thread class that will store the output on the serial port of qemu to file
class StdOut(Thread):

    def __init__(self, out_obj_):
        super(StdOut, self).__init__()
        self.out_obj = out_obj_

    def run(self):
        # Open the serial port and a file
        global out_to_stdout
        try:
            while True:
                line = self.out_obj.read(1)
                if len(line) == 0:
                    break
                if out_to_stdout:
                    sys.stdout.write(line)
        except OSError:
            pass

        print("Writing to stdout completed")


class RunSim(Thread):

    def __init__(self, qemu_img_name):
        super(RunSim, self).__init__()
        self.qemu_img = qemu_img_name

    def add_to_cmd(self, opt):
        self.qemu_cmd = "%s %s" % (self.qemu_cmd, opt)

    def run(self):
        global checkpoint_lock
        global checkpoint_iter
        global sim_cmd_generic
        global vnc_counter
        global output_dirs
        global simconfig

        print("Running thread with img: %s" % self.qemu_img)

        # Start simulation from checkpoints
        pty_prefix = 'char device redirected to '
        # for checkpoint in check_list:
        while True:
            checkpoint = None
            self.qemu_cmd = ''

            try:
                checkpoint_lock.acquire()
                checkpoint = checkpoint_iter.next()
                self.vnc_counter = vnc_counter
                if vnc_counter != None:
                    vnc_counter += 1
            except:
                checkpoint = None
            finally:
                checkpoint_lock.release()

            print("Checkpoint %s" % str(checkpoint))
            if not checkpoint:
                break

            output_dir = checkpoint[0]
            checkpoint = checkpoint[1]

            sim_file_cmd_name = "/tmp/%s.simconfig" % checkpoint
            sim_file_cmd = open(sim_file_cmd_name, "w")
            config_args = copy.copy(conf_parser.defaults())
            config_args['out_dir'] = os.path.realpath(output_dir)
            config_args['bench'] = checkpoint
            t_simconfig = gen_simconfig(config_args)
            print("simconfig: %s" % t_simconfig)
            sim_file_cmd.write(t_simconfig)
            sim_file_cmd.write("\n")
            sim_file_cmd.close()
            print("Config file written")

            # Generate a common command string
            self.add_to_cmd(qemu_bin)
            self.add_to_cmd('-m %s' % str(vm_memory))
            self.add_to_cmd('-serial pty')
            if self.vnc_counter:
                self.add_to_cmd('-vnc :%d' % self.vnc_counter)
            else:
                self.add_to_cmd('-nographic')

            # Add Image at the end
            self.add_to_cmd('-drive cache=unsafe,file=%s' % self.qemu_img)
            self.add_to_cmd('-simconfig %s' % sim_file_cmd_name)
            self.add_to_cmd('-loadvm %s' % checkpoint)
            self.add_to_cmd(qemu_args)

            print("Starting Checkpoint: %s" % checkpoint)
            print("Command: %s" % self.qemu_cmd)

            p = subprocess.Popen(self.qemu_cmd.split(), stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, stdin=subprocess.PIPE, bufsize=0)

            monitor_pty = None
            serial_pty = None

            while p.poll() is None:
                line = p.stdout.readline()
                sys.stdout.write(line)
                if line.startswith(pty_prefix):
                    dev_name = line[len(pty_prefix):].strip()

                    # Open the device terminal and send simulation command
                    # pty_term = os.open(dev_name, os.O_RDWR)
                    pty_term = dev_name

                    serial_pty = pty_term

                    if serial_pty != None:
                        break

            # Redirect output of serial terminal to file
            serial_thread = SerialOut('%s%s.out' % (output_dir, checkpoint), serial_pty)

            # os.dup2(serial_pty, sys.stdout.fileno())

            stdout_thread = StdOut(p.stdout)
            stdout_thread.start()
            serial_thread.start()

            # Wait for simulation to complete
            p.wait()

            serial_thread.join()
            stdout_thread.join()


# Now start RunSim threads
threads = []

for i in range(num_threads):
    th = RunSim(qemu_img[0])
    threads.append(th)
    th.start()

print("All Threads are started")

for th in threads:
    th.join()

# Send email to notify run completion
if options.email:
    email_script = "%s/send_gmail.py" % os.path.dirname(os.path.realpath(__file__))
    subprocess.call([email_script, "-m", "Completed simulation runs in %s" %
        str(options.output_dir)])

print("Completed all simulation runs.")
