#!/usr/bin/python
import os; 
import sys
import matplotlib
import tempfile; 
from subprocess import *
matplotlib.use("ps");
import numpy as np;
import pylab
from pylab import sqrt
import matplotlib.pyplot as plt
import matplotlib.axes as ax

#from make_graph import *
#from graph_and_send import *
I_label = "I: 8/32, 4 controllers"
G_label = "G: 8/12, 4 controllers"
D_label = "D: 16/16, 2 controllers"

bob_sim_descriptions = {"PARSEC fluidanimate (8 cores)" : 
 {I_label:'shared_crazyBus3_parsec.fluidanimate_8q_4M_L2_I'
	,D_label:'shared_crazyBus3_parsec.fluidanimate_8q_4M_L2_D'
	,G_label:'shared_crazyBus3_parsec.fluidanimate_8q_4M_L2_G'
} 
}
outputFilename="fluidanimate.pdf"

if len(sys.argv) > 1:
	if sys.argv[1] == "stream":
		bob_sim_descriptions = {"STREAM (8 cores, 10 iterations, array size=2M)" : 
		 {I_label:'_stream_8core_cbus_I'
			,D_label:'_stream_8core_cbus_D'
			,G_label:'_stream_8core_cbus_G'
		} 
		}
		outputFilename="stream.pdf"

bob_sim_fields = {"Bandwidth":2, "Latency":3}

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
line_params = {I_label: ['-',1.5], G_label: ["-",0.5], D_label: [":",1.4]}


class DataTable:
	def __init__(self, data_filename, data_delimiter=","):
		if not os.path.exists(data_filename):
			print "ERROR: Can't open data file %s" % data_filename
			exit()
		
		try:
			self.file_data=np.genfromtxt(data_filename, delimiter=data_delimiter)
			print self.file_data
		except IOError:
			print "Stupid error from genfromtxt, file %s"%(data_filename)
			exit()

	def get_x_data(self, x_col, col_to_num_map):
		if col_to_num_map != None and is_string(self.x_col):
			return self.file_data[:,col_to_num_map[x_col]-1]
		else:
			return self.file_data[:,x_col-1]

	def get_y_data(self, y_col, col_to_num_map):
		if col_to_num_map != None and is_string(self.y_col):
			return self.file_data[:,col_to_num_map[self]-1]
		else:
			return self.file_data[:,y_col-1]
	def filter_outliers(self, data, num_deviations=3):
		mean,std = np.mean(data), np.std(data)
		low = max(0, mean-1.5*std);
		high = mean+1.5*std
		if std == 0.0:
			indices = np.where(True)
		else:
			indices = np.where(data < high)
		#print len(data), data, "->", len(indices), indices
		return indices
	def filter_outliers(self,data,num_deviations=3):
		return np.where(True)

	def draw(self, ax, x_col, y_col, label, color="#ffffff", col_to_num_map=None):
		print "X=",x_col,"Y=",y_col
		x_data = self.get_x_data(x_col, col_to_num_map);
		y_data = self.get_y_data(y_col, col_to_num_map);

		if False:
			filtered_indices = self.filter_outliers(y_data)
		else:
			filtered_indices = range(0,len(y_data))

		filtered_y_data = y_data[filtered_indices]
		mean,std = np.mean(filtered_y_data ), np.std(filtered_y_data )
		ax.plot(x_data[filtered_indices], y_data[filtered_indices],label="%s"%(label),c='k',linestyle=line_params[label][0], linewidth=line_params[label][1]);


def get_layout(num_boxes):
	num_cols = 2
	if num_boxes % 2 == 1:
		num_boxes += num_cols-1;
	num_rows = max(num_boxes/num_cols,1);
	if num_boxes%num_cols != 0:
		num_rows+=1;
	return num_rows,num_cols

def rect_for_graph(idx, num_rows, num_cols):
	y_margin = 0.08
	x_margin = 0.05
	row = idx/num_cols;
	col = idx%num_cols;
	h = 1.0/float(num_rows)-(1.0+1.0/float(num_rows))*y_margin
	w = 1.0/float(num_cols)-(1.0+1.0/float(num_cols+1))*x_margin
	b = (1.0 - (row+1)*(h + y_margin))
	l = w * col + x_margin*(col+1)

#	print "Box for idx=%d [b=%f,l=%f,w=%f,h=%f]"%(idx,b,l,w,h)
	return l,b,w,h;

def bob_file_to_data_table(sim_name):
	bob_stats_filename = "tmp/BOBstats%s.txt"%(sim_name)
	print "Loading %s"%bob_stats_filename, 
	bob_stats_filename = dump_semicolons(bob_stats_filename)
	bob_stats_filename,header_str2 = chop_off_head_and_tail(bob_stats_filename, 63,1)
	print "-> %s"%bob_stats_filename
	return DataTable(bob_stats_filename)

def draw_graph(ax, which, label, data_table,show_legend=True):
	data_table.draw(ax,1,bob_sim_fields[which],label)
	if show_legend:
		leg=ax.legend(loc='best', title='BOB Configurations\n(Req./Resp., \# controllers)')
#		leg.get_frame().set_alpha(0.5);
		plt.setp(leg.get_texts(), fontsize='small')

		
if __name__ == "__main__":
	plt.rc('backend=ps')
	plt.rc('font', size=10)
	plt.rc('axes', grid=True, unicode_minus=False)
	plt.rc('font', family='serif')
	plt.rc('text', usetex=True)
	plt.rc('xtick', labelsize=8)
	plt.rc('ytick', labelsize=8)

	#setup a figure 
	fig_width_pt = 498.7+150  # Get this from LaTeX using \showthe\columnwidth
	inches_per_pt = 1.0/72.27               # Convert pt to inch
	golden_mean = (sqrt(5)-1.0)/2.0         # Aesthetic ratio
	fig_width = fig_width_pt*inches_per_pt  # width in inches
	fig_width = 12
	fig_height = fig_width*golden_mean      # height in inches

	num_boxes = 2
	num_rows = 2
	num_cols = 1

	fig = plt.figure(1, figsize=(fig_width,fig_height))
	counter =0

	rect=rect_for_graph(0,num_rows,num_cols) 
	ax_b = fig.add_axes(rect, title="Bandwidth", xlabel="Time (ms)", ylabel="Bandwidth (GB/s)")
	rect=rect_for_graph(1,num_rows,num_cols) 
	ax_l = fig.add_axes(rect, title="Latency", xlabel="Time (ms)", ylabel="Latency (ns)")

	for graph_name, graph_plots in bob_sim_descriptions.iteritems():
		for label, sim_name in graph_plots.iteritems():
			for i in range(len(bob_sim_fields)):
				data_table = bob_file_to_data_table(sim_name)
				col_str = "Bandwidth"
				if counter %2 == 0:
					draw_graph(ax_b,col_str,label,data_table,show_legend=True)
				else:
					col_str = "Latency"
					draw_graph(ax_l,col_str,label,data_table,show_legend=False)
				counter = counter + 1
	 
	#figure-wide settings
	fig.suptitle(graph_name, fontsize=12, fontweight='bold', x=0.515)
	tmp_out = "outl.eps"#tempfile.NamedTemporaryFile(suffix='.png',delete=False).name
	tmp_out2 = "outl.pdf";
	print "figure is %dx%d in %s"%(fig_width,fig_height,outputFilename)
	plt.savefig(tmp_out);#,bbox_inches='tight'); 
	plt.clf();
	os.system("ps2pdf14 -d-dPDFSETTINGS=/prepress %s"%(tmp_out));
	os.system("mv %s %s"%(tmp_out2, outputFilename));


