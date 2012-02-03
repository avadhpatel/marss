"""
	Graphs.py -- a set of classes to help graph data using matplotlib and numpy
		see the main function at the bottom of this file for some examples 

	Author: Paul Rosenfeld, University of Maryland <dramninjas (at) gmail>
	
	Wishlist of features: 
		- Need better support for laying out graphs so they have proper margins
			and spacing irrespective of the number and layout of graphs
		- Need better layout support that somehow takes into account the width of labels since
			matplotlib lays out the axis box where you tell it, but the left axis labels sometimes
			push the label off the edge of the page which is obnoxious
		- Right now the only layout functionality is row-major; i.e. left to right
			then top to bottom. Might want to make a layout that allows column major
			ordering of graphs. 
"""
		


import os; 
import numpy as np;
import matplotlib
matplotlib.use("Agg"); #this gets reset later, but for now it prevents matplotlib from going into interactive mode
import matplotlib.pyplot as plt
import matplotlib.axes as ax
import matplotlib.mlab as mlab
import tempfile; 
import pdb

""" plot settings for different types of plots """
def setup_common():
	plt.rc('font', size=10)
	plt.rc('axes', grid=True, unicode_minus=False)
	plt.rc('font', serif='cmr10',family='serif') #,sans='cmss10');

def setup_latex_mode():
	plt.rc('backend=ps')
	plt.rc('text', usetex=True)
	plt.rc('xtick', labelsize=8)
	plt.rc('ytick', labelsize=8)

def setup_png_mode():
	plt.rc('backend=Agg')
	

class CompositeGraph: 
	""" A composite graph is made up of one or more SingleGraphs and is the 
		object that lays out all of the SingleGraphs and saves them to a file
	"""
	def __init__(self, w=8.5, h=11, title=None, output_mode="png", num_cols=2, num_boxes=-1):
		setup_common(); 
		if output_mode == "png":	
			self.output_mode = "png"
			setup_png_mode()
		elif output_mode == "latex":
			self.output_mode = "latex"
			setup_latex_mode()
		else:
			print "WARNING: Unknown output mode '%s'; using defaults"%output_mode
		self.w = w; 
		self.h = h; 
		self.title = title;
		self.fig = None;
		self.num_cols = num_cols;
		if num_boxes > 0:	
			self.num_rows = self.get_layout(num_boxes);
		else: 
			self.num_rows = 0;

	def get_layout(self, num_boxes):
		""" this returns the proper number of rows and columns in case the 
				number of boxes isn't divisible by the number of columns"""
		num_rows = max(num_boxes/self.num_cols,1);
		# add a row if we rounded down in the previous step
		if num_boxes%self.num_cols != 0 and num_boxes > self.num_cols:
			num_rows+=1;
		return num_rows
	def get_height_px(self, px):
		""" returns a percentage for a certain height in pixels -- useful to make
				fixed-size regions in the layout which must be specified in pixels"""
		if self.fig == None:
			return 0.0
		h_pixels = self.fig.get_figheight()*self.fig.get_dpi()
		return px/h_pixels;

	def get_width_px(self, px):
		""" returns a percentage for a certain width in pixels -- useful to make
				fixed-size regions in the layout which must be specified in pixels"""
		if self.fig == None:
			return 0.0
		w_pixels = self.fig.get_figwidth()*self.fig.get_dpi()
		return px/w_pixels;


	def rect_for_graph(self, idx):
		y_margin = self.get_height_px(80)
		x_margin = self.get_width_px(100)
		row = idx/self.num_cols;
		col = idx%self.num_cols;
		h = 1.0/float(self.num_rows)-(1.0+1.0/float(self.num_rows))*y_margin
		w = 1.0/float(self.num_cols)-(1.0+1.0/float(self.num_cols+1))*x_margin
		b = (1.0 - (row+1)*(h + y_margin))
		l = w * col + x_margin*(col+1)

	#	print "Box for idx=%d [b=%f,l=%f,w=%f,h=%f]"%(idx,b,l,w,h)
		return l,b,w,h;
	def get_num_rows(self):
		return self.num_rows
	def get_num_cols(self):
		return self.num_cols

	def draw(self,graph_arr,output_filename):
		num_boxes = len(graph_arr)
		self.num_rows = self.get_layout(num_boxes);
				
		self.fig = plt.figure(1, figsize=(self.w,self.h));
		for i,g in enumerate(graph_arr):
			rect=self.rect_for_graph(i) 
			ax = self.fig.add_axes(rect, title=g.title, xlabel="test", ylabel="ytest")
			g.draw(ax,self.output_mode); 
			
		if self.title != None:
			self.fig.suptitle(self.title, fontsize=12, fontweight='bold', x=0.515, y=1.0-self.get_height_px(20))

		if self.output_mode == "png":
			plt.savefig(output_filename);#,bbox_inches='tight'); 
		elif self.output_mode == "latex":

			tmp_out = "outl.eps"#tempfile.NamedTemporaryFile(suffix='.png',delete=False).name
			tmp_out2 = "outl.pdf";
			print "figure is %dx%d in %s"%(self.w,self.h,output_filename)
			plt.savefig(tmp_out);#,bbox_inches='tight'); 
			os.system("ps2pdf14 -d-dPDFSETTINGS=/prepress %s"%(tmp_out));
			os.system("mv %s %s"%(tmp_out2, output_filename));

		plt.clf();

def col_map_from_header(header_str):
	""" create column map that maps column names to column numbers
			from a csv string 
	"""
	col_map = {}	
	header_fields = header_str.rstrip().split(",");
	for i,field in enumerate(header_fields):
		col_map[field] = i
	return col_map

# Some convenient type checking 
def is_string(a):
	return type(a).__name__ == 'str'

def is_list(a):
	return type(a).__name__ == 'list'

def is_dict(a):
	return type(a).__name__ == 'dict'


class DataTable:
	# read the data file, convert the header information into a column map,
	# 		and use numpy to load the data in the file
	def __init__(self, data_filename, data_delimiter=",", skip_header=0):
		""" Read the speficied file, extract the header information, and
				convert it into numpy format
			The columns can be read with get_data() with either the column 
			header string or with the column number 
		""" 


		if not os.path.exists(data_filename):
			print "ERROR: Can't open data file %s" % data_filename
			exit()

		fp = open(data_filename,"r");

		# skip the first skip_header lines -- the +1 makes it so that the loop 
		# ends with the first line of interest in the buffer
		for i in range(0,skip_header+1):
			first_line = fp.readline();

		self.col_to_num_map = None;
		self.has_header = False;
		print "Header Line: ",first_line 
		if first_line[:1].isalpha():
			self.col_to_num_map = col_map_from_header(first_line)
#			print self.col_to_num_map
			self.has_header = True;
		fp.close()

		try:
			# names=True doesn't seem to work as advertised -- it skips the first line, but I 
			# don't see any way to reference the columns by name. So for now I'll just use
			# names=True to skip the first line and keep my own col_map to keep track of the 
			# column name mappings
			#
			# Also, please note, skiprows was renamed to skip_header in 1.5.0 
			# TODO: put a version check for 1.5.0 and adjust skiprows accordingly 

			if self.has_header:
				skip_header = skip_header + 1
			self.file_data=np.genfromtxt(data_filename, delimiter=data_delimiter, skiprows=skip_header)
		except IOError:
			print "genfromtxt couldn't read the CSV data, file %s"%(data_filename)
			exit()
	def get_column_idx(self, col):
		""" Get a column number from a name (or number), returns 0 on error (usually the first column)"""
		if is_string(col):
			if col not in self.col_to_num_map:
				print "ERROR: column name '%s' not found in data table"%(col)
				return 0;
			return self.col_to_num_map[col];
		else:
			if col >= self.file_data.shape[1]:
				print "ERROR: column number '%d' is out of bounds"%(col)
				return 0; 
				exit();
			return int(col); 

	def get_data(self, col):
		""" get a column of data out of the table; column can be referenced by name (str)
				or by number 
		"""
		return self.file_data[:,self.get_column_idx(col)]

	def add_derived_column(self, compute_fn, col_idx_arr, col_name):
		""" Use some functional python to add a new derived column -- just make sure
				that the number of arguments compute_fn() expects matches the number of 
				entries in col_idx_arr and that the entries in this array are not out of
				bounds """

		# first add a zero column to the array 
		column_height,last_column_idx = self.file_data.shape
		self.file_data = np.column_stack([self.file_data, np.zeros(column_height)])
		for i in range(column_height):
			values = map(lambda col_idx: self.file_data[i,self.get_column_idx(col_idx)], col_idx_arr)
			self.file_data[i,last_column_idx] = compute_fn(*values)
		self.col_to_num_map[col_name] = last_column_idx; 
		
			

		
	#filter out outliers that are a certain number of std devs away from the mean
	def filter_outliers(self, data, num_deviations=3):
		""" filter a column's outliers based on the number of deviations from
				the mean. Returns a list of indices that are left 
		"""
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
		""" dummy filter which just returns all indices """
		return np.where(True)

	def draw(self, ax, x_col, y_col, label, line_param_kwargs={}):
		print "X=",x_col,"Y=",y_col
		x_data = self.get_data(x_col);
		y_data = self.get_data(y_col);

		if False:
			filtered_indices = self.filter_outliers(y_data)
		else:
			#don't filter anything
			filtered_indices = range(0,len(y_data))

		filtered_y_data = y_data[filtered_indices]
		mean,std = np.mean(filtered_y_data ), np.std(filtered_y_data )
		ax.plot(x_data[filtered_indices], y_data[filtered_indices],label="%s"%(label),**(line_param_kwargs) );

class SingleGraph:
	def __init__(self, plots, x_axis_desc, y_axis_desc, title, show_legend=True, legend_kwargs={}):
		self.plots = plots
		self.x_axis_desc = x_axis_desc
		self.y_axis_desc = y_axis_desc
		self.title = title
		self.show_legend = show_legend
		self.legend_kwargs = legend_kwargs;
		self.default_line_colors = ['k','r','b','g','y','m']

	def colorize_default_lines(self): 
		for i,p in enumerate(self.plots):
			if len(p.line_params) == 0:
				idx = i%len(self.default_line_colors)
				line_color = self.default_line_colors[idx]
				print "idx=%d, color=%s"%(idx, line_color)
			#	p.set_line_style({"c":line_color})
				p.set_line_style(['-',1.0,line_color])

	def draw(self,ax,output_mode):

		self.colorize_default_lines()
		for p in self.plots:
			p.draw(ax);
		if self.show_legend:
			leg=ax.legend(**self.legend_kwargs)
			if output_mode == "png": #latex output doesn't support alpha
				leg.get_frame().set_alpha(0.5);
			plt.setp(leg.get_texts(), fontsize='small')

		ax.xaxis.set_label_text(self.x_axis_desc.label)
		ax.yaxis.set_label_text(self.y_axis_desc.label)
		ax.set_ybound(self.y_axis_desc.range_min,self.y_axis_desc.range_max)
		ax.set_xbound(self.x_axis_desc.range_min,self.x_axis_desc.range_max)

		ax.set_title(self.title)

class SimpleGraph:
	def __init__(self, filename):
		# First read the input file (csv format)
		self.data = mlab.csv2rec(filename)

	def draw(self, output_name, x, y_list):
		plt.autoscale(True, 'both', True)
		fig = plt.figure(figsize=(100,10), dpi=300)
		lines = []
		ax = fig.add_subplot(111)

		for y in y_list:
			ys = y.split(',')
			l = None
			if len(ys) > 1:
				l = ys[1]
			y = ys[0].replace('.','')
			line, = ax.plot(self.data[x], self.data[y], '-', label=l)
			lines.append(line)

		ax.set_xlabel(x)
		# handles, labels = ax.get_legend_handles_labels()
		ax.legend()
		plt.savefig(output_name, bbox_inches='tight')
		plt.clf()

class LinePlot:
	def __init__(self,data_table,x_col,y_col,label,line_params=None):
		""" A line plot really just corresponds to a x,y pair from the
			data table along with a label in the legend """ 
		self.x_col = x_col
		self.y_col = y_col 
		self.label = label
		self.data_table = data_table; 
		self.lineplot_kwargs = {};
		self.set_line_style(line_params); 

	def set_line_style_from_list(self, line_param_list=None):
		""" This function takes an array of 3 or fewer elements [dash style, width, color]
			and returns the corresponding dictionary that can be unpacked into the legend 
			style for matplotlib """
		params = ['-', 1.0, 'k']; # defaults that will be overwritten by the loop below (without me having to keep checking length)
		if line_param_list == None: 
			line_param_list = [];

		for i,param in enumerate(line_param_list):
			if i >= len(params):
				break;
			params[i] = param;
		return {'linestyle': params[0], 'linewidth':params[1], 'c':params[2]}
		
	def set_line_style(self, line_params):
		if line_params == None:
			line_params = []
		self.line_params = line_params; 
		if is_list(line_params):
			self.lineplot_kwargs = self.set_line_style_from_list(line_params); 
		elif is_dict(line_params): 
			self.lineplot_kwargs = line_params; 

	def draw(self,ax):
		self.data_table.draw(ax, self.x_col, self.y_col, self.label, self.lineplot_kwargs)

class AxisDescription:
	def __init__(self,label,range_min=None,range_max=None):
		self.label = label
		self.range_min = range_min
		self.range_max = range_max

def percent(a,b):
	return float(b)/(float(a)+1E-20)*100.0

if __name__ == "__main__":
	""" A small test program to create a png and latex sample graph """
	dt = DataTable("foo.csv");
	dt.add_derived_column(percent, [1,1],"test_col");
	dt.add_derived_column(lambda x,y: x+y, ["base_machine.decoder.total_fast_decode","base_machine.decoder.alu_to_mem"],"test_col2");
	default_x_axis = AxisDescription("Cycle Number"); 

	graphs = [
			SingleGraph([
				LinePlot(dt,"sim_cycle","base_machine.decoder.alu_to_mem","ALU to Memory",[':', 1.5, 'r'])
			,	LinePlot(dt,0,"base_machine.decoder.total_fast_decode","Total decoded", ["--", 1.2,'g'])
			], default_x_axis, AxisDescription("Ops"), "ALU Ops")
		,	SingleGraph([
				LinePlot(dt,0,3,"testline")
			], default_x_axis, AxisDescription("test"), "testxxx1")
		,	SingleGraph([
				LinePlot(dt,0,"test_col2","derived1")
			], default_x_axis, AxisDescription("test"), "A Derived Variable", legend_kwargs={"loc":"upper left", "title":"LEGEND TITLE"} )
		,	SingleGraph([
				LinePlot(dt,0,"test_col","should be 100 always")
			], default_x_axis, AxisDescription("test"), "test", show_legend=False)


	]	
	composite_graph = CompositeGraph(); 
	composite_graph.draw(graphs,"blah.png"); 
	
	graph_latex = CompositeGraph(output_mode="latex", title="two row graphs");
	graph_latex.draw(graphs,"blah.pdf")

	graph_latex2 = CompositeGraph(w=4, h=10, num_cols=1, output_mode="latex")
	graph_latex2.draw(graphs,"blah_4_1col.pdf")
