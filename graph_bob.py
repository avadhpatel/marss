#!/usr/bin/python
import os; 
import sys
import matplotlib
import tempfile; 
from subprocess import *
from Graphs import *
from numpy import sqrt
import config

def dump_semicolons(filename):
	output_filename2 = tempfile.NamedTemporaryFile(delete=False).name
	output_filename = tempfile.NamedTemporaryFile(delete=False).name
	os.system("sed 's/,;/,/g' %s > %s"%(filename, output_filename2))
	os.system("sed 's/;/,/g' %s > %s"%(output_filename2, output_filename))
	return output_filename

def bob_file_to_data_table(sim_name):
	bob_stats_filename = "results/BOBstats%s.txt"%(sim_name)
	print "Loading %s"%bob_stats_filename, 
	bob_stats_filename = dump_semicolons(bob_stats_filename)
	print "-> %s"%bob_stats_filename
	return DataTable(bob_stats_filename,skip_header=62)

# used by graph and send
def get_sim_desc_from_num(num):
	prefix="#SIM_DESC="
	bob_variant_prefix="#BOB_VARIANT="
	f = open(config.get_marss_dir_path("simulate%d.sh"%num));
	sim_desc=""
	bob_variant=""
	for line in f:
		if line.startswith(prefix):
			sim_desc = line[len(prefix)+1:].strip("\" \n")
		if line.startswith(bob_variant_prefix):
			bob_variant = "_"+line[len(bob_variant_prefix)+1:].strip("\" \n")
	return sim_desc+bob_variant

		
if __name__ == "__main__":

	#setup a figure 
	fig_width_pt = 498.7+150  # Get this from LaTeX using \showthe\columnwidth
	inches_per_pt = 1.0/72.27               # Convert pt to inch
	golden_mean = (sqrt(5)-1.0)/2.0         # Aesthetic ratio
	fig_width = fig_width_pt*inches_per_pt  # width in inches
	fig_width = 12
	fig_height = fig_width*golden_mean      # height in inches

	graph_title = "STREAM (8 cores)"; 

	labels =[ 'First Available', 'Round Robin']
	bob_sim_descriptions = {
			labels[0]: ('_stream_shared_F', ['-',1.5,'r'])
		,	labels[1]: ('_stream_shared_R', ["-",0.5,'b'])
	}

	outputFilename="stream_PortHeuristics.pdf"

	bob_sim_fields = {"Bandwidth":1, "Latency":2}
	data_tables = {}
	for label,sim_details in bob_sim_descriptions.iteritems():
		data_tables[label] = bob_file_to_data_table(sim_details[0])

	cg = CompositeGraph(w=fig_width, h=fig_height, output_mode="latex", title=graph_title, num_cols=1);
	bandwidth_plots = []
	latency_plots = []
	x_axis_description = AxisDescription("Time (ms)"); 

	for label in labels:
		bandwidth_plots.append(LinePlot(data_tables[label],0,bob_sim_fields["Bandwidth"],label,bob_sim_descriptions[label][1]));
		latency_plots.append(LinePlot(data_tables[label],0,bob_sim_fields["Latency"],label,bob_sim_descriptions[label][1]));
	graphs = [
			SingleGraph(bandwidth_plots, x_axis_description, AxisDescription("Bandwidth (GB/s)"), "Bandwidth", show_legend=True)
		,	SingleGraph(latency_plots, x_axis_description, AxisDescription("Latency (ns)"), "Latency", show_legend=False)
	]
	cg.draw(graphs, outputFilename);
