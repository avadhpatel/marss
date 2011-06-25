import os; 
import numpy as np;
import matplotlib
matplotlib.use("Agg");
import matplotlib.pyplot as plt
import matplotlib.axes as ax
import tempfile; 

from DataTable import * 

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
	def __init__(self, w=8.5, h=11, title=None, output_filename="out.png", output_mode="png", num_cols=2):
		setup_common(); 
		if output_mode == "png":	
			self.output_mode = "png"
			setup_png_mode()
		elif output_mode == "latex":
			self.output_mode = "latex"
			setup_latex_mode()
		else:
			print "WARNING: Unknown output mode '%s'; using defaults"%output_mode
		self.fig = plt.figure(1, figsize=(w,h));
		self.title = title;
		self.num_cols = num_cols;

	def get_layout(self, num_boxes):
		""" this returns the proper number of rows and columns in case the 
				number of boxes isn't divisible by the number of columns"""
		if num_boxes % 2 == 1:
			num_boxes += self.num_cols-1;
		num_rows = max(num_boxes/self.num_cols,1);
		if num_boxes%self.num_cols != 0:
			num_rows+=1;
		return num_rows,self.num_cols

	def rect_for_graph(self, idx, num_rows, num_cols):
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

	def draw(self,graph_arr,output_filename):
		num_boxes = len(graph_arr)
		num_rows, num_cols = self.get_layout(num_boxes);
				
		for i,g in enumerate(graph_arr):
#			print "boxes=%d, r=%d, c=%d, i=%d" % (num_boxes,num_rows, num_cols, i)
			rect=self.rect_for_graph(i,num_rows,num_cols) 
			ax = self.fig.add_axes(rect, title="test", xlabel="xtest", ylabel="y_test")
			if (g.draw(ax)):
				leg=ax.legend()
				leg.get_frame().set_alpha(0.5);
				plt.setp(leg.get_texts(), fontsize='small')
			
		if self.title != None:
			self.fig.suptitle(self.title, fontsize=12, fontweight='bold', x=0.515)

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

def is_string(a):
	return type(a).__name__ == 'str'

class DataTable:
	# read the data file, convert the header information into a column map,
	# 		and use numpy to load the data in the file
	def __init__(self, data_filename, data_delimiter=",", skip_header=0):
		""" Read the speficied file, extract the header information, and
				convert it into numpy format
			The columns can be read with get_data() with either the column 
			header string or with the column number 
		""" 


		self.col_to_num_map = None;
		if not os.path.exists(data_filename):
			print "ERROR: Can't open data file %s" % data_filename
			exit()

		fp = open(data_filename,"r");
		for i in range(0,skip_header+1):
			first_line = fp.readline();

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
			self.file_data=np.genfromtxt(data_filename, delimiter=data_delimiter, names=self.has_header, skiprows=skip_header)
		except IOError:
			print "genfromtxt couldn't read the CSV data, file %s"%(data_filename)
			exit()

	def get_data(self, col):
		""" get a column of data out of the table; column can be referenced by name (str)
				or by number 
		"""
		if self.col_to_num_map != None and is_string(col):
			return self.file_data[:,self.col_to_num_map[col]]
		else:
			return self.file_data[:,col]

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
	def __init__(self, plots, x_axis_desc, y_axis_desc, title):
		self.plots = plots
		self.x_axis_desc = x_axis_desc
		self.y_axis_desc = y_axis_desc
		self.title = title

	def draw(self,ax):
		for p in self.plots:
			p.draw(ax);
		ax.xaxis.set_label_text(self.x_axis_desc.label)
		ax.yaxis.set_label_text(self.y_axis_desc.label)
		ax.set_title(self.title)
		return True

class LinePlot:
	def __init__(self,data_table,x_col,y_col,label):
		self.x_col = x_col
		self.y_col = y_col 
		self.label = label
		self.data_table = data_table; 

	def draw(self,ax):
		self.data_table.draw(ax, self.x_col, self.y_col, self.label)

class AxisDescription:
	def __init__(self,label,range_min=None,range_max=None):
		self.label = label
		self.range_min = range_min
		self.range_max = range_max

if __name__ == "__main__":
	dt = DataTable("foo.csv");
	default_x_axis = AxisDescription("Cycle Number"); 

	graphs = [
			SingleGraph([
				LinePlot(dt,"sim_cycle","base_machine.decoder.alu_to_mem","ALU to Memory")
			,	LinePlot(dt,0,"base_machine.decoder.total_fast_decode","Total decoded")
			], default_x_axis, AxisDescription("Ops"), "ALU Ops")
		,	SingleGraph([
				LinePlot(dt,0,3,"testline")
			], default_x_axis, AxisDescription("test"), "test")
	]	
	composite_graph = CompositeGraph(); 
	composite_graph.draw(graphs,"blah.png"); 
	
	graph_latex = CompositeGraph(output_mode="latex")
	composite_graph.draw(graphs,"blah.pdf")
	
		




