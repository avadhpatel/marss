#!/usr/bin/python

import config
import locale
from subprocess import *
import os
import re

def rip_type(rip): 
	if rip.startswith("ffffffff"):
		return "  "
	elif rip.startswith("00007"):
		return ".."
	else:
		return "**"

def is_all_kernel(rip_strings):
	return reduce(lambda x, y: x and y.startswith("ffffffff"), rip_strings, True)

class simulation: 
	def __init__(self,num):
		locale.setlocale(locale.LC_ALL, "")
		self.num=num;
		self.isRunning=False;
		self.cycleNumber=0;
		self.cpuUsage=0.0;
		self.status="";
		self.RIPs="";

	def set_running(self,status):
		cpuUsage,stat,sim_num = status;
		if sim_num != self.num:
			return;
		self.status = stat;	
		self.cpuUsage = cpuUsage;
		self.isRunning = True;

	def get_cycle_info(self,blksize=(16*1024)):
		logfile_name = config.get_marss_dir_path("run%d.log" %self.num)
		try:
			f = open(logfile_name,"rb")
		except IOError:
			self.status="NOTHING"
			self.isRunning=False;
			return False;
		size = os.path.getsize(logfile_name)
		read_size = min(size,blksize)
		if (size > blksize):
			f.seek((-1*read_size),2) # 2 = offset from the end of the file
			
		lastLines = f.read(read_size).split("\n")[:-2]
		lastLines.reverse()
#		print lastLines
		lastLine = ""

		#find the last line to get cycle info from
		for line in lastLines:
			if "Completed " in line:
				lastLine = line; 
				break;
		if len(lastLine) == 0:
			return False	

		#figure out if there has been any user activity recently (if not, stalled)
		self.stalled=True;
		for line in lastLines:
			lineFields = filter(lambda p: len(p) > 0, line.split())
			if not is_all_kernel(lineFields[10:]):
				self.stalled=False;
				break;
		
		lastLineFields = filter(lambda p: len(p) > 0, lastLine.split())
		self.cycleNumber = int(lastLineFields[1])
		self.RIPs = " ".join(map(rip_type, lastLineFields[10:]))
#		self.RIPs = " ".join(lastLineFields[10:])
		return True;


	def __repr__(self):
		if not self.get_cycle_info():
			return "%d no log file yet"%self.num;
		cycleString = locale.format('%14d',self.cycleNumber,True)
		if not self.isRunning:
			return "%d stopped:\tcycle=%s"%(self.num,cycleString)

		if self.isRunning:
			statusString = "RUNNING"; 
		if self.stalled:
			statusString = "STALLED";
		return "%d %s:\tCPU: %f\tstatus=%s\tcycle=%s\tRIPs:%s"%(self.num,statusString,self.cpuUsage,self.status,cycleString,self.RIPs)

def get_info_from_line(line):
	sim_num_pattern = '(?:sim(\d+))|(?:hda(\d+))'
	line_arr = line.split(" ");
	line_arr = filter(lambda p: len(p) > 0, line_arr)
	#line_arr[2] = " ".join(line_arr[2:])
	#del line_arr[3:]
	sim_num = -1;

	for p in line_arr:
		m = re.search(sim_num_pattern, p)
		if m != None:
			try:
				sim_num = int(m.group(1))
			except TypeError:
				sim_num = int(m.group(2))
			finally:
				break;
	if sim_num < 0:
	#	print line_arr;
		return 0.0,"NONE",-1;

	return float(line_arr[0]),line_arr[1],sim_num

def get_status_string():
	ps_string = Popen('ps xo pcpu,stat,args'.split(), stdout=PIPE).communicate()[0]
	status_arr = {}
	return_string = ""
	for line in ps_string.split("\n"):
		if "SCREEN" in line:
			cpu,stat,sim_num = get_info_from_line(line)
			if not sim_num in status_arr:
				status_arr[sim_num] = simulation(sim_num)
	#		print "STARTED:", 	
		elif "qemu-system" in line: 
			cpu,stat,sim_num = get_info_from_line(line)
			if not sim_num in status_arr:
				status_arr[sim_num] = simulation(sim_num)
			status_arr[sim_num].set_running((cpu,stat,sim_num));

	#		print "RUNNING:", get_info_from_line(line)
	for k,s in status_arr.items():
		return_string += str(s)+"\n";
	return return_string; 

if __name__ == "__main__":
	print get_status_string(); 
