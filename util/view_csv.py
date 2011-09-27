#!/usr/bin/python
""" 
provides a way to look at csv files that come out of the time based stats in a
column format. Otherwise, it's hard to line up what number belongs to what
column header
"""

import sys,re

def print_and_get_field_length(field, remove_prefix):
	if remove_prefix: 
		field = field.replace(remove_prefix,"",1)
	field = field.strip()+" "
	print field,
	return len(field)

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print """
Usage: view_csv.py csv_file [remove_prefix]"
Example: view_csv.py blah.csv base_machine.node."
         will strip 'base_machine.node.' from all "
		 the headers that have it """
		exit()

	if len(sys.argv) == 3:
		remove_prefix = sys.argv[2]
	else:
		remove_prefix = None

	csv_filename = sys.argv[1];
	csv_file = open(csv_filename, "r");
	header_field_lengths = []
	header_line = csv_file.readline()
	line = header_line
	for i,header_field in enumerate(header_line.split(",")):
		header_field_lengths.append(print_and_get_field_length(header_field, remove_prefix))
	print "" # newline
	while line:
		line = csv_file.readline()
		if re.match(r'\d+,(0,)+0$',line):
			#don't bother with all zero epochs
			continue; 

		for i,field in enumerate(line.split(",")):
			field = field.strip() + (" "*(header_field_lengths[i]-len(field)))
			print field,
		print "" #newline
