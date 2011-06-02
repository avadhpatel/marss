#!/usr/bin/python -W ignore::DeprecationWarning
  

import re
import os
import sys
import time, datetime
from subprocess import *
from make_graph import *
from send_gmail import *

def chop_off_head_and_tail(filename, skip_head=3, skip_tail=1):
	output_filename = tempfile.NamedTemporaryFile(delete=False).name
	# grab the first line and return it
	header_str = Popen(["head","--lines=1",filename],stdout=PIPE).communicate()[0]

	# chop off the first and last data points since they tend to be outliers -- also remove NaN lines
	chop_cmd = "tail --lines=+%d %s | head --lines=-%d  > %s"%(skip_head,filename,skip_tail,output_filename);
	#print chop_cmd
	os.system(chop_cmd)
	return (output_filename,header_str)

def dump_semicolons(filename):
	output_filename2 = tempfile.NamedTemporaryFile(delete=False).name
	output_filename = tempfile.NamedTemporaryFile(delete=False).name
	os.system("sed 's/,;/,/g' %s > %s"%(filename, output_filename2))
	os.system("sed 's/;/,/g' %s > %s"%(output_filename2, output_filename))
	return output_filename
def get_sim_desc_from_num(num):
	prefix="#SIM_DESC="
	bob_variant_prefix="#BOB_VARIANT="
	f = open("simulate%d.sh"%num);
	sim_desc=""
	bob_variant=""
	for line in f:
		if line.startswith(prefix):
			sim_desc = line[len(prefix)+1:].strip("\" \n")
		if line.startswith(bob_variant_prefix):
			bob_variant = "_"+line[len(bob_variant_prefix)+1:].strip("\" \n")
	return sim_desc+bob_variant


def per_core_line_plot_arr(header_fields, base_key, description=None):
	if description == None: 
		description = base_key;

	return_arr = []
	if base_key in header_fields:
		for i in range(16):
			test_key = "%s[%d]"%(base_key,i)
			if test_key in header_fields:
				#print "Adding plot %s ..." % test_key
				return_arr.append(LinePlot(1,test_key,"%s[%d]"%(description,i)))
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
	log_file_base_name = "run%d.log"%(run_num)
	run_desc = get_sim_desc_from_num(run_num); 
	if len(sys.argv) == 3:
		run_desc = sys.argv[2];

	memlog_file_name = "%s.memlog"%(log_file_base_name);
	memhist_file_name = "%s.memhisto"%(log_file_base_name); 
	logfile_path = "%s/marss.dramsim/run%d.log.memlog"%(os.getenv("HOME"), run_num)

	bob_stats_file_name = "BOBstats%s.txt"%(run_desc)
	bob_log_file_name = "bobsim%s.log"%(run_desc)
	

	ptlstats_cmd = ["./ptlstats", "-snapshot","final", "-subtree","/memory/total/L2/cpurequest", "run%d.stats"%(run_num)]
	stats_str = Popen(ptlstats_cmd, stdout=PIPE).communicate()[0]
	string_arr.append(stats_str)
	stopped_str = Popen(["grep","Stopped",log_file_base_name],stdout=PIPE).communicate()[0]
	string_arr.append(stopped_str); 
	last_req_str = Popen(["tail","-1",memlog_file_name],stdout=PIPE).communicate()[0]
	string_arr.append(last_req_str)
	string_arr.insert(0, "RUN: %s"%(run_desc))
#TODO: save git diff output in the tgz
	last_req_str = Popen(["tail","-1",memlog_file_name],stdout=PIPE).communicate()[0]

	if len(stats_str) > 0:
		Popen(["tar","czf","run%d_%s_%d.tgz"%(posix_time,run_desc,run_num),memlog_file_name,memhist_file_name,log_file_base_name,bob_stats_file_name,bob_log_file_name,"run%d.stats"%(run_num),"out.png"],stdout=PIPE).communicate()[0]


	output_logfile, header_str = chop_off_head_and_tail(logfile_path)
	header_fields = header_str.rstrip().split(",");
	header_map = {}
	for i,f in enumerate(header_fields): 
		header_map[f] = i+1;
#		print f, "->", i+1
		
	
	histo_path = "%s/marss.dramsim/memhisto.log"%(os.getenv("HOME"))
	x_axis_desc = AxisDescription("Cycle Number")
#	print "HEADERMAP:",header_map
	g = [
			SingleGraph(output_logfile,  "Cache Request Rates",per_core_line_plot_arr(header_fields, "l1dTotalIssued","L1D"), x_axis_desc, AxisDescription("Requests/cycle"), col_to_num_map=header_map)
			,SingleGraph(output_logfile,  "Memory Controller Request Rates",per_core_line_plot_arr(header_fields, "mcTotalIssued","mC"), x_axis_desc, AxisDescription("Requests/cycle"), col_to_num_map=header_map)
			# latencies
#			,SingleGraph(output_logfile, "Full latency",[LinePlot(1,"RoundtripLatency","L2 to Bus")],  x_axis_desc, AxisDescription("Latency (cycles)"), col_to_num_map=header_map)
	#		,SingleGraph(output_logfile, "Bus to MC",[LinePlot(1,"bus_mc_lat","Bus to MC")],  x_axis_desc, AxisDescription("Latency (cycles)"), col_to_num_map=header_map)
#			,SingleGraph(output_logfile, "MC to Bus",[LinePlot(1,"mc_bus_lat","MC to Bus")],  x_axis_desc, AxisDescription("Latency (cycles)"), col_to_num_map=header_map)
#			,SingleGraph(output_logfile, "Bus to L2",[LinePlot(1,"bus_L2_lat","Bus to L2")],  x_axis_desc, AxisDescription("Latency (cycles)"), col_to_num_map=header_map)
			,SingleGraph(output_logfile, "Bus Throughput", [LinePlot(1,"BusNumIssued","Bus Entries Issued Per cycle")],  x_axis_desc, AxisDescription("# of requests"), col_to_num_map=header_map)
# Signs of congestion
#			,SingleGraph(output_logfile,  "Cache Failures",[LinePlot(1,"cacheToBusFail","cache to bus"),LinePlot(1,"cacheUpFail","cache up fail"), LinePlot(1,"cacheDownFail","cache down fail"), LinePlot(1,"cpuCacheAccessRetry", "Cache retries from CPU")], x_axis_desc, AxisDescription("Intercache send fail (per 100k)"), col_to_num_map=header_map)
#			,SingleGraph(output_logfile,  "Cache Failures 2",[LinePlot(1,"l2_cache_full","L2 Cache Full"),LinePlot(1,"l1_cache_full","L1D full"),], x_axis_desc, AxisDescription("Intercache send fail (per 100k)"), col_to_num_map=header_map)
			
#			,SingleGraph(output_logfile,  "MC Factors",[LinePlot(1,"mcFull","MC Full")
#			,LinePlot(1,"busReqUnbroadcastable","BusReqUnbroadcastable")
#					,LinePlot(1,"busDataBroadcastFail","BusDataBroadcastFail")
#					], x_axis_desc, AxisDescription("Bus Broadcast Fail (per 100k)"), col_to_num_map=header_map)
#			,SingleGraph(output_logfile,  "Bus Factors",[LinePlot(1,"busQueueCount","Bus Queue Count")], x_axis_desc, AxisDescription("Queue Count (per 100k)"), col_to_num_map=header_map)
#			,SingleGraph(output_logfile,  "Bus Factors 2",[LinePlot(1,"busNumArbitrations","Bus Arbitrations")], x_axis_desc, AxisDescription("Arbitration Count (per 100k)"), col_to_num_map=header_map)
			]

	if os.path.exists(bob_stats_file_name):
		print "Found bob stats file = %s"%(bob_stats_file_name)
		bob_stats_file_name = dump_semicolons(bob_stats_file_name)
		bob_stats_file_name,header_str2 = chop_off_head_and_tail(bob_stats_file_name, 63,1)
		x_axis_desc = AxisDescription("Time (ms)");
		print "Reading file ",bob_stats_file_name;
		g.append(SingleGraph(bob_stats_file_name, "BOB Bandwidth",[LinePlot(1,2,"Bandwidth")],  x_axis_desc, AxisDescription("Bandwidth (GB/s)")))
		g.append(SingleGraph(bob_stats_file_name, "BOB Latency",[LinePlot(1,3,"Latency")],  x_axis_desc, AxisDescription("Latency (ns)")))
		g.append(SingleGraph(bob_stats_file_name, "BOB RW ratio",[LinePlot(1,4,"RW ratio")],  x_axis_desc, AxisDescription("Ratio (R:W)")))
		g.append(SingleGraph(bob_stats_file_name, "BOB pendingQueue Max",[LinePlot(1,5,"Num Entries")],  x_axis_desc, AxisDescription("# of Requests")))
	else:
		print "Can't find a BOB stats file with name %s"%bob_stats_file_name
	outfile = CompositeGraph().draw(g, run_desc);
	outfiles = [outfile]

#	authorize_and_send(None,outfiles,strings_arr=string_arr);
