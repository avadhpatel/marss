#!/usr/bin/python -W ignore::DeprecationWarning
  

import re
import os
import sys
import time, datetime
from subprocess import *
from send_gmail import *
from Graphs import *
import config 

from graph_bob import *

def per_core_line_plot_arr(data_table, base_key, description=None):
	if description == None: 
		description = base_key;

	return_arr = []
	if base_key in data_table.col_to_num_map:
		for i in range(16):
			test_key = "%s[%d]"%(base_key,i)
			if test_key in data_table.col_to_num_map:
				#print "Adding plot %s ..." % test_key
				return_arr.append(LinePlot(data_table,0,test_key,"%s[%d]"%(description,i)))
			else:
				break;
	#	return_arr.append(LinePlot(1,base_key,"%s[Total]"%description))
	else:
		print "%s not found"%base_key
	return return_arr;

if __name__ == "__main__":
	d = datetime.datetime.now()
	posix_time = int(time.mktime(d.timetuple()))

	run_desc=None
	string_arr=[]
	if len(sys.argv) < 2:
		print "need at least run number"
		exit();
	
	run_num = int(sys.argv[1]);
	run_desc = get_sim_desc_from_num(run_num); 
	if len(sys.argv) == 3:
		run_desc = sys.argv[2];
	
	graph_output_file = "out.png"
	log_file_base_name = config.get_marss_dir_path("run%d.log"%(run_num))

	x_axis_desc = AxisDescription("Cycle Number")
	if not os.path.exists(log_file_base_name):
		print "ERROR: Can't find log file '%s'" % log_file_base_name
		exit();
	g = []
	# eventually the memlog thing is going away, but for now, decide which stats to use
	memlog_file_name = "%s.memlog"%(log_file_base_name)
	if not os.path.exists(memlog_file_name):
		memlog_file_name = config.get_marss_dir_path("run%d.csv")%run_num

		if not os.path.exists(memlog_file_name):
			print "ERROR: Can't find a marss stats file"
			exit();
		else:
			marss_dt = DataTable(memlog_file_name); 
			g = [
				SingleGraph([
					LinePlot(marss_dt,"sim_cycle","base_machine.decoder.alu_to_mem","ALU to Memory")]
				, x_axis_desc, AxisDescription("Ops"), "ALU Ops"),

				SingleGraph([LinePlot(marss_dt,0,"base_machine.decoder.total_fast_decode","Total decoded") #, [':',2.0,'y'])
				], x_axis_desc, AxisDescription("Ops"), "ALU Ops")
]
	else:
		marss_dt = DataTable(memlog_file_name); 
		g = [
				SingleGraph(per_core_line_plot_arr(marss_dt, "l1dTotalIssued","L1D"), x_axis_desc, AxisDescription("Requests/cycle"), "Cache Request Rates")
				,SingleGraph(per_core_line_plot_arr(marss_dt, "mcTotalIssued","mC"), x_axis_desc, AxisDescription("Requests/cycle"),  "Memory Controller Request Rates")
				,SingleGraph([LinePlot(marss_dt,0,"BusNumIssued","Bus Entries Issued Per cycle")],  x_axis_desc, AxisDescription("# of requests"), "Bus Throughput")
				]
	
	bob_stats_file_name = config.get_marss_dir_path("BOBstats%s.txt"%(run_desc))
	bob_log_file_name = config.get_marss_dir_path("bobsim%s.log"%(run_desc))
	stats_file_name = config.get_marss_dir_path("run%d.stats"%(run_num))
	
	ptlstats_bin = config.get_marss_dir_path("ptlstats");

	# Files that should be saved by tar 
	archive_files = [
			os.path.basename(log_file_base_name)
		,	os.path.basename(bob_stats_file_name)
		,	os.path.basename(bob_log_file_name)
		,	os.path.basename(memlog_file_name)
		,	os.path.basename(stats_file_name)
		,	graph_output_file
	]
	# where to save those files
	archive_output_file_name = "run%d_%s_%d.tgz"%(posix_time,run_desc,run_num)

	# string_arr is the array of lines that will go into the email body

	ptlstats_cmd = [ptlstats_bin, "-snapshot","final", "-subtree","/memory/total/L2/cpurequest", stats_file_name]
	stats_str = Popen(ptlstats_cmd, stdout=PIPE).communicate()[0]
	string_arr.append(stats_str)
	stopped_str = Popen(["grep","Stopped",log_file_base_name],stdout=PIPE).communicate()[0]
	string_arr.append(stopped_str); 
	last_req_str = Popen(["tail","-1",memlog_file_name],stdout=PIPE).communicate()[0]
	string_arr.append(last_req_str)
	string_arr.insert(0, "RUN: %s"%(run_desc))
	last_req_str = Popen(["tail","-1",memlog_file_name],stdout=PIPE).communicate()[0]

	#TODO: When the ptlstats go away, this logic will have to change
	if len(stats_str) > 0:
		# the rest of the script should use absolute paths so changing directory shouldn't matter
		os.chdir(os.path.dirname(log_file_base_name))

		archive_files = filter(os.path.exists, archive_files)
		tar_cmd_list = ["tar","czf",archive_output_file_name]
		tar_cmd_list.extend(archive_files);

		Popen(tar_cmd_list,stdout=PIPE).communicate()[0]
		print "archive command:", " ".join(tar_cmd_list);
	
	if os.path.exists(bob_stats_file_name):
		bob_stats_file_name = dump_semicolons(bob_stats_file_name)
		bob_dt = DataTable(bob_stats_file_name,skip_header=62)
	else:
		bob_dt = None; 

	if bob_dt != None:
		x_axis_desc = AxisDescription("Time (ms)");
		g.append(SingleGraph([LinePlot(bob_dt,0,1,"Bandwidth")],  x_axis_desc, AxisDescription("Bandwidth (GB/s)"), "BOB Bandwidth"))
		g.append(SingleGraph([LinePlot(bob_dt,0,2,"Latency")],  x_axis_desc, AxisDescription("Latency (ns)"), "BOB Latency"))
		g.append(SingleGraph([LinePlot(bob_dt,0,3,"RW ratio")],  x_axis_desc, AxisDescription("Ratio (R:W)"), "BOB RW ratio"))
		g.append(SingleGraph([LinePlot(bob_dt,0,4,"Num Entries")],  x_axis_desc, AxisDescription("# of Requests"), "BOB pendingQueue Max"))

	cg = CompositeGraph(title=run_desc,num_cols=1,num_boxes=len(g))
	cg.w = 14.0/cg.get_num_cols()
	cg.h = 5*cg.get_num_rows()
	print "C=%d, R=%d"%(cg.get_num_cols(), cg.get_num_rows())
	cg.draw(g, graph_output_file);
	outfiles = [graph_output_file]

	authorize_and_send(None,outfiles,strings_arr=string_arr);
