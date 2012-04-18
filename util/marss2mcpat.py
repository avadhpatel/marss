#!/usr/bin/env python

try:
    import yaml
except (ImportError, NotImplementedError):
    path = os.path.dirname(sys.argv[0])
    a_path = os.path.abspath(path)
    sys.path.append("%s/../ptlsim/lib/python" % a_path)
    import yaml

try:
  from yaml import CLoader as Loader
except ImportError:
  from yaml import Loader, Dumper

import sys, os
import shutil

import xml.etree.ElementTree as ET

import argparse

warn = "\033[93m"
fatal = "\033[91m"
normal = "\033[0m"

def configError(msg):
  print warn + "Config Error: " + msg + normal
  sys.exit(-1)

def ReadFile(filename, mode):
  """ This takes the filename of a YAML file, opens it, and returns a dict """
  print "Parsing YAML...",
  with open(filename, "r") as stream:
    try:
      for segment in yaml.load_all(stream, Loader=Loader):
        if mode in segment['simulator']['tags']:
          print "done."
          return segment
      print "Specified %s as mode but wasn't in configuration" % mode
      exit(-1)
    except yaml.YAMLError, e:
      print "Malformed YAML file? Lazy error checking says: ", str(e)
      sys.exit(-1)

class XMLWriter:
  """ Serializes a prepared dictionary of mcpat stats to XML """

  tree = None
  def __init__(self):
    pass

  def findElemWithAttr(self, root, name, attrName, attrVal):
    subElemItr = root.iterfind(name)
    for i in subElemItr:
      if attrVal == i.get(attrName):
        return i
    return None

  def findParam(self, root, name):
    return self.findElemWithAttr(root, "param", "name", name)

  def findComponent(self, root, name):
    return self.findElemWithAttr(root, "component", "id", name)

  def updateStat(self, root, statName, value):
    statElem = self.findElemWithAttr(root, "stat", "name", statName)

    # If the stat is already there, just update the value
    if statElem != None:
      statElem.set("value", str(value))
    else:
      attributes = { "name" : str(statName), "value" : str(value) }
      statElem = ET.SubElement(root, "stat", attributes)

  def writeXML(self, statsDict, xml_in, out_filename):
    """ Given input xml configuration and a stats dictionary, write out XML
      suitable for mcpat input (well, hopefully) """
    # Copy the input XML to the output file so we don't clobber it
    try:
      shutil.copyfile(xml_in, out_filename)
    except Exception as e:
      print "Couldn't open copy input XML to output file: ", str(e)
      sys.exit(-1)


    # Parse the XML file and update its stats
    try:
      self.tree = ET.ElementTree()
      rootElem = self.tree.parse(out_filename)
    except Exception as e:
      print "Couldn't parse XML tree: ", str(e)

    print "XML parsed..."

    # Our XML file is structured this way
    # root
    #   system
    #     system.corei
    #       predictor
    #       itlb
    #       icache
    #       dtlb
    #       dcache
    #       BTB
    #       L1Directoryi ...
    #       L2Directoryi ...
    #       L2i
    #       L3i
    #       NOC i
    #       mem
    #       memcontroller
    #       niu
    #       pcie
    #       flashc
    #   repeat for total N cores
    system = self.findComponent(rootElem, "system")

    # XXX CHECK THAT THE CORES ARE CONFIGURED TO BE HETEROGENOUS
    # Otherwise the per-core cycle statistics are IGNORED
    homogeneous = self.findParam(system, "homogenous_cores")
    if homogeneous == None or homogeneous.get("value") == 1:
      print warn + "!!!! WARNING: Cores are homogenous! Per-core cycles are \
IGNORED!" + normal

    numCoreElem = self.findParam(system, "number_of_cores")
    if numCoreElem == None:
      configError("Number of cores not defined")
    numCores = numCoreElem.get("value")
    print "Number of cores: " + numCores

    numL2Elem = self.findParam(system, "number_of_L2s")
    if numL2Elem == None:
      configError("Number of L2 caches not defined")
    numL2 = numL2Elem.get("value")
    print "Number of L2 caches: " + numL2

    numL1DirElem = self.findParam(system, "number_of_L1Directories")
    if numL1DirElem == None:
      configError("Number of L1 directories not defined")
    numL1Dir = numL1DirElem.get("value")
    print "Number of L1 directories: " + numL1Dir

    numL2DirElem = self.findParam(system, "number_of_L2Directories")
    if numL2DirElem == None:
      configError("Number of L2 directories not defined")
    numL2Dir = numL2DirElem.get("value")
    print "Number of L2 directories: " + numL2Dir

    numL3Elem = self.findParam(system, "number_of_L3s")
    if numL3Elem == None:
      configError("Number of L3 caches not defined")
    numL3 = numL3Elem.get("value")
    print "Number of L3 caches: " + numL3

    numNoCsElem = self.findParam(system, "number_of_NoCs")
    if numNoCsElem == None:
      configError("Number of NoCs not defined")
    numNoCs = numNoCsElem.get("value")
    print "Number of NoCs: " + numNoCs


    # Then insert or edit the required stats for McPAT
    print "Updating cycle counts..."
    cycles = statsDict["base_machine"]["ooo_0_0"]["cycles"]
    idle = statsDict["base_machine"]["ooo_0_0"]["thread0"]["cycles_in_pause"]
    self.updateStat(system, "total_cycles", cycles)
    self.updateStat(system, "idle_cycles", idle)
    self.updateStat(system, "busy_cycles", cycles - idle)

    for i in range(int(numCores)):
      print "Updating core %d ..." % i
      ith_core = self.findComponent(system, "system.core%d" % i)
      if ith_core == None:
        # TODO: What I should be doing instead is duplicating the previous core
        # element, and then continuing as usual
        print "Only got %d cores, instead of %s" % (i, numCores)
        print warn + "!!!! WARNING: Assuming that the rest of the cores are \
duplicates!" + normal
        break

      oooDict = statsDict["base_machine"]["ooo_%d_%d" % (i,i)]
      tDict = oooDict["thread0"]

      print "\tUpdating core statistics..."
      # Dispatched instructions
      insn_src = oooDict["dispatch"]["opclass"]
      total_insns = sum(insn_src.itervalues())
      self.updateStat(ith_core, "total_instructions", total_insns)

      ld_insn = insn_src["ld"] + insn_src["ld.pre"]
      br_insn = insn_src["bru"] + insn_src["br.cc"] + insn_src["jmp"]
      fp_insn = 0
      for key in ['fpu', 'fp-div-sqrt', 'fp-cmp', 'fp-perm', 'fp-cvt-i2f',
                  'fp-cvt-f2i', 'fp-cvt-f2f']:
        fp_insn += insn_src[key]
      int_insn = sum(insn_src.itervalues()) - ld_insn - br_insn - fp_insn
      self.updateStat(ith_core, "int_instructions", int_insn)
      self.updateStat(ith_core, "fp_instructions", fp_insn)
      self.updateStat(ith_core, "branch_instructions", br_insn)
      self.updateStat(ith_core, "load_instructions", ld_insn)
      self.updateStat(ith_core, "store_instructions", insn_src["st"])

      mispred = tDict["branchpred"]["summary"]["mispred"]
      self.updateStat(ith_core, "branch_mispredictions", mispred)

      # Committed instructions
      cinsn_src = tDict["commit"]["opclass"]
      cld_insn = cinsn_src["ld"] + cinsn_src["ld.pre"]
      cbr_insn = cinsn_src["bru"] + cinsn_src["br.cc"] + cinsn_src["jmp"]
      cfp_insn = 0
      for key in ['fpu', 'fp-div-sqrt', 'fp-cmp', 'fp-perm', 'fp-cvt-i2f',
                  'fp-cvt-f2i', 'fp-cvt-f2f']:
        cfp_insn += cinsn_src[key]
      cint_insn = sum(cinsn_src.itervalues()) - cld_insn - cbr_insn - cfp_insn
      self.updateStat(ith_core, "committed_instructions",
          sum(cinsn_src.itervalues()))
      self.updateStat(ith_core, "committed_int_instructions", cint_insn)
      self.updateStat(ith_core, "committed_fp_instructions", cfp_insn)
      self.updateStat(ith_core, "pipeline_duty_cycle", "0.9") #TODO?

      # Cycles
      cycles = oooDict["cycles"]
      idle = tDict["cycles_in_pause"]
      self.updateStat(ith_core, "total_cycles", cycles)
      self.updateStat(ith_core, "idle_cycles", idle)
      self.updateStat(ith_core, "busy_cycles", cycles - idle)

      # ROB
      rob_reads = tDict["rob_reads"]
      rob_writes = tDict["rob_writes"]
      self.updateStat(ith_core, "ROB_reads", rob_reads)
      self.updateStat(ith_core, "ROB_writes", rob_writes)

      # Rename tables
      rename_reads = tDict["rename_table_reads"]
      rename_writes = tDict["rename_table_writes"]
      self.updateStat(ith_core, "rename_reads", rename_reads/2)
      self.updateStat(ith_core, "rename_writes", rename_writes/2)
      # NOTE: Unified counters for rename tables, so we (maybe unwisely)
      # just split them
      self.updateStat(ith_core, "fp_rename_reads", rename_reads/2)
      self.updateStat(ith_core, "fp_rename_writes", rename_writes/2)

      # Issue queue
      iq_reads = oooDict["iq_reads"]
      iq_writes = oooDict["iq_writes"]
      iq_fp_reads = oooDict["iq_fp_reads"]
      iq_fp_writes = oooDict["iq_fp_writes"]
      self.updateStat(ith_core, "inst_window_reads", iq_reads)
      self.updateStat(ith_core, "inst_window_writes", iq_writes)
      self.updateStat(ith_core, "inst_window_wakeup_accesses",
          iq_reads + iq_writes)
      self.updateStat(ith_core, "fp_inst_window_reads", iq_fp_reads)
      self.updateStat(ith_core, "fp_inst_window_writes", iq_fp_writes)
      self.updateStat(ith_core, "fp_inst_window_wakeup_accesses",
          iq_fp_reads + iq_fp_writes)

      # Regfile reads/writes
      self.updateStat(ith_core, "int_regfile_reads", tDict["reg_reads"])
      self.updateStat(ith_core, "float_regfile_reads", tDict["fp_reg_reads"])
      self.updateStat(ith_core, "int_regfile_writes", tDict["reg_writes"])
      self.updateStat(ith_core, "float_regfile_writes", tDict["fp_reg_writes"])
      self.updateStat(ith_core, "context_switches", tDict["ctx_switches"])
      self.updateStat(ith_core, "function_calls", "") # TODO?
      self.updateStat(ith_core, "ialu_accesses", "300000")
      self.updateStat(ith_core, "fpu_accesses", "100000")
      self.updateStat(ith_core, "mul_accesses", "200000")
      self.updateStat(ith_core, "cdb_alu_accesses", "300000")
      self.updateStat(ith_core, "cdb_mul_accesses", "200000")
      self.updateStat(ith_core, "cdb_fpu_accesses", "100000")

      # XXX: McPAT's XML chastises me to not change these, so for now I don't.
      self.updateStat(ith_core, "IFU_duty_cycle", "1")
      self.updateStat(ith_core, "LSU_duty_cycle", "0.5")
      self.updateStat(ith_core, "MemManU_I_duty_cycle", "1")
      self.updateStat(ith_core, "MemManU_D_duty_cycle", "0.5")
      self.updateStat(ith_core, "ALU_duty_cycle", "1")
      self.updateStat(ith_core, "MUL_duty_cycle", "0.3")
      self.updateStat(ith_core, "FPU_duty_cycle", "0.3")
      self.updateStat(ith_core, "ALU_cdb_duty_cycle", "1")
      self.updateStat(ith_core, "MUL_cdb_duty_cycle", "0.3")
      self.updateStat(ith_core, "FPU_cdb_duty_cycle", "0.3")

      print "\tUpdating branch predictor statistics..."
      ith_btb = self.findComponent(ith_core, "system.core%d.predictor" % i)
      self.updateStat(ith_btb, "predictor_accesses",
          tDict["branchpred"]["predictions"])

      print "\tUpdating ITLB statistics..."
      ith_itlb = self.findComponent(ith_core, "system.core%d.itlb" % i)
      itlbDict = tDict["dcache"]["itlb"]
      hits = itlbDict["hits"]
      misses = itlbDict["misses"]
      self.updateStat(ith_itlb, "total_accesses", hits + misses)
      self.updateStat(ith_itlb, "total_misses", hits)
      self.updateStat(ith_itlb, "conflicts", misses)

      print "\tUpdating Icache statistics..."
      ith_icache = self.findComponent(ith_core, "system.core%d.icache" % i)
      iDict = statsDict["base_machine"]["L1_I_%d" % i]["cpurequest"]
      read_miss = iDict["count"]["miss"]["read"]
      reads = sum(iDict["count"]["hit"]["read"].itervalues()) + read_miss
      self.updateStat(ith_icache, "read_accesses", reads)
      self.updateStat(ith_icache, "read_misses", read_miss)
      stalls = iDict["stall"]
      cnf = sum(stalls["read"].itervalues()) + sum(stalls["write"].itervalues())
      self.updateStat(ith_icache, "conflicts", cnf)

      print "\tUpdating DTLB statistics..."
      ith_dtlb = self.findComponent(ith_core, "system.core%d.dtlb" % i)
      dtlbDict = tDict["dcache"]["dtlb"]
      hits = dtlbDict["hits"]
      misses = dtlbDict["misses"]
      self.updateStat(ith_dtlb, "total_accesses", hits + misses)
      self.updateStat(ith_dtlb, "total_misses", hits)
      self.updateStat(ith_dtlb, "conflicts", misses)

      print "\tUpdating Dcache statistics..."
      ith_dcache = self.findComponent(ith_core, "system.core%d.dcache" % i)
      dcacheDict = statsDict["base_machine"]["L1_D_%d" % i]["cpurequest"]["count"]
      read_miss = dcacheDict["miss"]["read"]
      write_miss = dcacheDict["miss"]["write"]
      reads = sum(dcacheDict["hit"]["read"].itervalues()) + read_miss
      writes = sum(dcacheDict["hit"]["write"].itervalues()) + write_miss
      self.updateStat(ith_dcache, "read_accesses", reads)
      self.updateStat(ith_dcache, "write_accesses", writes)
      self.updateStat(ith_dcache, "read_misses", read_miss)
      self.updateStat(ith_dcache, "write_misses", write_miss)
      stalls = statsDict["base_machine"]["L1_D_%d" % i]["cpurequest"]["stall"]
      cnf = sum(stalls["read"].itervalues()) + sum(stalls["write"].itervalues())
      self.updateStat(ith_dcache, "conflicts", cnf)

      print "\tUpdating BTB statistics..."
      ith_btb = self.findComponent(ith_core, "system.core%d.BTB" % i)
      self.updateStat(ith_btb, "read_accesses", "45345")
      self.updateStat(ith_btb, "write_accesses", "0")

      print "Done updating core %d" % i


    # Caches
    print "Updating L1 directories"
    for i in range(int(numL1Dir)):
      ith_L1Dir = self.findComponent(system, "system.L1Directory%d" % i)
      self.updateStat(ith_L1Dir, "read_accesses", "")
      self.updateStat(ith_L1Dir, "write_accesses", "")
      self.updateStat(ith_L1Dir, "read_misses", "")
      self.updateStat(ith_L1Dir, "write_misses", "")
      self.updateStat(ith_L1Dir, "conflicts", "")

    print "Updating L2 cache(s)"
    for i in range(int(numL2Dir)):
      ith_L2Dir = self.findComponent(system, "system.L2Directory%d" % i)
      self.updateStat(ith_L2Dir, "read_accesses", "")
      self.updateStat(ith_L2Dir, "write_accesses", "")
      self.updateStat(ith_L2Dir, "read_misses", "")
      self.updateStat(ith_L2Dir, "write_misses", "")
      self.updateStat(ith_L2Dir, "conflicts", "")

    for i in range(int(numL2)):
      ith_L2 = self.findComponent(system, "system.L2%d" % i)
      L2Component = statsDict["base_machine"]["L2_%d" % i]["cpurequest"]
      L2_Hit = L2Component["count"]["hit"]
      L2_Miss = L2Component["count"]["miss"]
      self.updateStat(ith_L2, "read_accesses", sum(L2_Hit["read"].itervalues()))
      self.updateStat(ith_L2, "write_accesses", sum(L2_Hit["write"].itervalues()))
      self.updateStat(ith_L2, "read_misses", L2_Miss["read"])
      self.updateStat(ith_L2, "write_misses", L2_Miss["write"])

      stalls = L2Component["stall"]
      stallConflicts = sum(stalls["read"].itervalues()) + sum(stalls["write"].itervalues())
      self.updateStat(ith_L2, "conflicts", stallConflicts)
      self.updateStat(ith_L2, "duty_cycle", "1.0")

    print "Updating L3 cache"
    for i in range(int(numL3)):
      ith_L3 = self.findComponent(system, "system.L3%d" % i)
      self.updateStat(ith_L3, "read_accesses", "1824")
      self.updateStat(ith_L3, "write_accesses", "11276")
      self.updateStat(ith_L3, "read_misses", "1632")
      self.updateStat(ith_L3, "write_misses", "183")
      self.updateStat(ith_L3, "conflicts", "0")
      self.updateStat(ith_L3, "duty_cycle", "1.0")

    # Misc stuff
    print "Updating NoC"
    for i in range(int(numNoCs)):
      ith_NoC = self.findComponent(system, "system.NoC%d" % i)
      self.updateStat(ith_NoC, "total_accesses", "1000000")
      self.updateStat(ith_NoC, "duty_cycle", "1.0")

    print "Updating memory"
    memElem = self.findComponent(system, "system.mem")
    memDict = statsDict["base_machine"]["MEM_0"]
    access = sum(memDict["bank_access"])
    read = sum(memDict["bank_read"])
    write = sum(memDict["bank_write"]) + sum(memDict["bank_update"])
    self.updateStat(memElem, "memory_accesses", access)
    self.updateStat(memElem, "memory_reads", read)
    self.updateStat(memElem, "memory_writes", write)

    print "Updating memory controllers"
    # FIXME: I don't think this is right. Shouldn't this be network on chip?
#    mcElem = self.findComponent(system, "system.mc")
#    ctrlComponent = statsDict["base_machine"]["core_0_cont"]["cpurequest"]
#    mc_read = ctrlComponent["read"]
#    mc_write = ctrlComponent["write"] + ctrlComponent["update"]
#    mc_access = mc_write + mc_read
#    self.updateStat(mcElem, "memory_accesses", mc_access)
#    self.updateStat(mcElem, "memory_reads", mc_read)
#    self.updateStat(mcElem, "memory_writes", mc_write)

    # XXX: The following is not provided by MARSS when I wrote this, so I put
    # these here only for setting sane defaults
    print "Updating NIU"
    niuElem = self.findComponent(system, "system.niu")
    self.updateStat(niuElem, "duty_cycle", "0.9")
    self.updateStat(niuElem, "total_load_perc", "0.8")

    print "Updating PCI-e"
    pcieElem = self.findComponent(system, "system.pcie")
    self.updateStat(pcieElem, "duty_cycle", "0.9")
    self.updateStat(pcieElem, "total_load_perc", "0.8")

    print "Updating flash controller"
    flashcElem = self.findComponent(system, "system.flashc")
    self.updateStat(flashcElem, "duty_cycle", "0.9")
    self.updateStat(flashcElem, "total_load_perc", "0.8")

    # Close the file
    self.tree.write(out_filename)

def processOptions():

  argparser = argparse.ArgumentParser(description= \
      "Parse Marss results to mcpat input")

  input_group = argparser.add_argument_group('Input')
  input_group.add_argument('--marss', required=True, metavar='FILE',
      help='Statistics output by a MARSS simulation run')
  input_group.add_argument('--xml_in', required=True, metavar='FILE',
      help='McPAT configuration for a processor')
  input_group.add_argument('--cpu_mode', required=True, metavar='MODE',
      help='Mode for stats {user, kernel, total}',
      choices=['user', 'kernel', 'total'])

  output_group = argparser.add_argument_group('Output')
  output_group.add_argument('-o', default='mcpat.xml', metavar='FILE',
      help='Destination for XML')
  args = argparser.parse_args()
  return args

if __name__ == "__main__":

  args = processOptions()
  statsDict = ReadFile(args.marss, args.cpu_mode)
  w = XMLWriter();
  w.writeXML(statsDict, args.xml_in, args.o)
  print "Done."
