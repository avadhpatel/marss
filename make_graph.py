#!/usr/bin/python

import os; 
import re;
import tempfile; 
import matplotlib
matplotlib.use("Agg");
import numpy as np;
import matplotlib.pyplot as plt
import matplotlib.axes as ax
def get_layout(num_boxes):
	num_cols = 2
	if num_boxes % 2 == 1:
		num_boxes += num_cols-1;
	num_rows = max(num_boxes/num_cols,1);
	if num_boxes%num_cols != 0:
		num_rows+=1;
	return num_rows,num_cols

def rect_for_graph(idx, num_rows, num_cols):
	y_margin = 0.05
	x_margin = 0.05
	row = idx/num_cols;
	col = idx%num_cols;
	h = 1.0/float(num_rows)-(1.0+1.0/float(num_rows))*y_margin
	w = 1.0/float(num_cols)-(1.0+1.0/float(num_cols+1))*x_margin
	b = (1.0 - (row+1)*(h + y_margin))
	l = w * col + x_margin*(col+1)

#	print "Box for idx=%d [b=%f,l=%f,w=%f,h=%f]"%(idx,b,l,w,h)
	return l,b,w,h;

class CompositeGraph: 
	def draw(self,graph_arr,graph_name=None):
#		plt.rc('figure.subplot', hspace=.35, wspace=.35)
		plt.rc('font', size=10)
		plt.rc('axes', grid=True, unicode_minus=False)
		plt.rc('font', serif='cmr10',family='serif') #,sans='cmss10');

		num_boxes = len(graph_arr)
		num_rows, num_cols = get_layout(num_boxes);
				
#		fig = plt.figure(1, figsize=(5*num_cols,4*num_rows))
		fig = plt.figure(1, figsize=(14,4*num_rows))

#		plt.text(.5, .95, "sdfsf%s"%graph_name, horizontalalignment='center') 
#		fig.SubplotParams(left=0.05,bottom=0.05)
#		fig.subplots_adjust(left=0.1,bottom=0.1);
		for i,g in enumerate(graph_arr):
#			print "boxes=%d, r=%d, c=%d, i=%d" % (num_boxes,num_rows, num_cols, i)
			rect=rect_for_graph(i,num_rows,num_cols) 
			ax = fig.add_axes(rect, title="test", xlabel="xtest", ylabel="y_test")
			if (g.draw(ax)):
#				print "drawn"
				leg=ax.legend()
				leg.get_frame().set_alpha(0.5);
				plt.setp(leg.get_texts(), fontsize='small')
			
		fig.suptitle(graph_name, fontsize=12)

		outputFilename = "%s.png"%graph_name#tempfile.NamedTemporaryFile(suffix='.png',delete=False).name

#		plt.tick_params(axis='both', labelsize='small');
		plt.savefig(outputFilename);#,bbox_inches='tight'); 
		plt.clf();
		return outputFilename

class AxisDescription:
	def __init__(self,label,range_min=None,range_max=None):
		self.label = label
		self.range_min = range_min
		self.range_max = range_max

def is_string(a):
	return type(a).__name__ == 'str'

class LinePlot:
	def __init__(self,x_col,y_col,label):
		self.x_col = x_col
		self.y_col = y_col 
		self.label = label

	def get_x_data(self,file_data, col_to_num_map):
		if col_to_num_map != None and is_string(self.x_col):
			return file_data[:,col_to_num_map[self.x_col]-1]
		else:
			return file_data[:,self.x_col-1]

	def get_y_data(self,file_data, col_to_num_map):
		if col_to_num_map != None and is_string(self.y_col):
			return file_data[:,col_to_num_map[self.y_col]-1]
		else:
			return file_data[:,self.y_col-1]

	
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

	def draw(self, file_data, ax, color="#ffffff",col_to_num_map=None):
		x_data = self.get_x_data(file_data, col_to_num_map);
		y_data = self.get_y_data(file_data, col_to_num_map);

		if False:
			filtered_indices = self.filter_outliers(y_data)
		else:
			filtered_indices = range(0,len(y_data))

		filtered_y_data = y_data[filtered_indices]
		mean,std = np.mean(filtered_y_data ), np.std(filtered_y_data )
		ax.plot(x_data[filtered_indices], y_data[filtered_indices],label="%s: $\mu=%.2f,\sigma=%.2f$"%(self.label,mean,std));
	
class SingleGraph:
	def __init__(self, data_file, title, plots, x_axis_desc, y_axis_desc,isHistogram=False,col_to_num_map=None):
		if not os.path.exists(data_file):
			print "ERROR: Can't open data file %s" % data_file
			exit()
		self.isHistogram=isHistogram
		self.data_file = data_file
		self.plots = plots
		self.x_axis_desc = x_axis_desc
		self.y_axis_desc = y_axis_desc
		self.title = title
		self.col_to_num_map = col_to_num_map
#		print "COL TO NUM", self.col_to_num_map
	def draw(self,ax):
		try:
			line_data=np.genfromtxt(self.data_file, delimiter=",")
		except IOError:
			print "Stupid error from genfromtxt, skipping plot %s in file %s"%(self.title, self.data_file)
			return False
#		print line_data
		for p in self.plots:
			p.draw(line_data,ax,col_to_num_map=self.col_to_num_map);
		ax.xaxis.set_label_text(self.x_axis_desc.label)
		ax.yaxis.set_label_text(self.y_axis_desc.label)
		ax.set_title(self.title)
		
	#	plt.title("%s: $\mu=%.2f,\sigma=%.2f$"%(g.title, np.mean(line_data[:-1,y_idx-1]),np.std(line_data[:-1,y_idx-1])));
#		plt.title(self.title);
		return True

