#!/usr/bin/env python

import os
import re
import sys

depth=''
regexs=[]
handlers=[]
stack = []
node = "root"
topnode = ""
target = ''
ignore_line = False
block_end_regex = None

def get_full_stack_name():
    global node
    ret_str = 'stats->'
    if len(stack) > 0:
        for i in stack[1:]:
            ret_str += i + "."
        ret_str += node + "."
    return ret_str

def print_add_attr_func():
    global node,topnode,depth,stack
    print("%svoid add_bson_obj_attr_bool(bson_buffer *bb, const char *key, int value) {" % depth)
    depth += '  '
    print("%sbson_append_bool(bb, key, (bson_bool_t)(value));" % depth)
    print("%sreturn;" % depth)
    depth = depth[2:]
    print("%s}\n" % depth)

def register_handler(regex,handler):
    global handlers
    global regexs
    global block_end_regex
    handlers+=[handler]
    regexs+=[re.compile(regex)]
    if handler == block_end_handler:
        block_end_regex = re.compile(regex)

def rootnode_handler(matchstring):
    global node,topnode,depth,stack,target
    node=topnode=matchstring.group(1)
    print("%svoid add_bson_%s(%s *stats, bson_buffer *bb, const char* obj_name) {"\
            %(depth, node, node))
    depth+='  '
    print("%sbson_buffer *obj = bson_append_start_object(bb, obj_name);" %
            depth)
    if matchstring.group(2)=="summable":
        print("%sadd_bson_obj_attr_bool(obj, \"summable\", 1);" % depth)
    if matchstring.group(2)=="identical":
        print("%sadd_bson_obj_attr_bool(obj, \"identical\", 1);" % depth)

    print("")

def node_handler(matchstring):
    global node,tonode,depth,stack
    prevnode=node
    stack.append(node)
    node=matchstring.group(1)

    print("%s{" % depth)
    depth += '  '
    print("%sconst char *node_name = \"%s\";" % (depth, node))
    print("%sbson_buffer *obj = bson_append_start_object(bb, node_name);" %
            depth)

    if matchstring.group(2)=="summable":
        print("%sadd_bson_obj_attr_bool(obj, \"summable\", 1);" % depth)
    if matchstring.group(2)=="identical":
        print("%sadd_bson_obj_attr_bool(obj, \"identical\", 1);" % depth)

def struct_handler(matchstring):
    global node,tonode,depth,stack
    prevnode=node
    stack.append(node)
    node=matchstring.group(1)
    print("%s{" % depth)
    depth += '  '
    print("%sconst char *node_name = \"%s\";" % (depth, node))
    print("%sbson_buffer *obj = bson_append_start_object(bb, node_name);" %
            depth)

def operator_handler(matchstring):
    global ignore_line
    ignore_line = True

def block_end_handler(matchstring):
    global stack, depth,node
    if len(stack):
        node=stack.pop()
    print("%sbson_append_finish_object(bb);" % depth)
    depth=depth[2:]
    print("%s}" % depth)
    print("")

def scalar_handler(matchstring):
    type,name=matchstring.group(1,2)
    value_name = "%s%s" % (get_full_stack_name(), name)
    if type=="W64":
        print("%sbson_append_long(bb, \"%s\", %s);" % (depth, name, value_name))
        # print("%s%s.addint(\"%s\");")\
            # %(depth,node,name)
    elif type=="double":
        print("%sbson_append_long(bb, \"%s\", %s);" % (depth, name, value_name))
        # print("%s%s.addfloat(\"%s\");")\
            # %(depth,node,name)
    else :
        print("%sadd_bson_%s(&%s, bb, \"%s\");" % (depth, type, value_name, name))
        # print "%s%s.add(\"%s\", %s);"\
        # %(depth,node,name,type)

def array_handler(matchstring):
    global depth
    typedict={}
    type,name,dims=matchstring.group(1,2,3)

    bson_type = ''
    if type == "W64":
        bson_type = 'long'
    elif type == "double":
        bson_type = 'double'
    elif type == "char":
        bson_type = 'string'
    else:
        sys.error("Unknown array type")
        sys.exit(1)

    value_name = "%s%s" % (get_full_stack_name(), name)

    if bson_type == 'string':
        print("%sbson_append_string(bb, \"%s\", %s);" % (depth, name, value_name))
        return

    print("%s{" % depth)
    depth += '  '
    print("%schar numstr[8];" % depth)
    print("%sbson_buffer *arr = bson_append_start_array(bb, \"%s\");" %
            (depth, name))
    print("%sfor(int i = 0; i < %s; i++) {" % (depth, dims))
    depth += '  '
    print("%sbson_numstr(numstr, i);" % depth)
    print("%sbson_append_%s(arr, numstr, %s[i]);" % (depth, bson_type,
        value_name))
    depth = depth[2:]
    print("%s}" % depth)
    print("%sbson_append_finish_object(arr);" % depth)
    depth = depth[2:]
    print("%s}" % depth)
    print("")

def label_handler(matchstring):
    global depth
    type,name,dims,label=matchstring.group(1,2,3,4)
    value_name = "%s%s" % (get_full_stack_name(), name)
    if type=="W64":
        print("%s{" % depth)
        depth += '  '
        print("%schar numstr[8];" % depth)
        print("%sbson_buffer *arr = bson_append_start_array(bb, \"%s\");" %
                (depth, name))
        print("%s{" % depth)
        depth += '  '
        print("%sbson_buffer *label_arr = bson_append_start_array(bb, \"label\");" %
                (depth))
        print("%sfor(int i = 0; i < %s; i++) {" % (depth, dims))
        depth += '  '
        print("%sbson_numstr(numstr, i);" % depth)
        print("%sbson_append_string(label_arr, numstr, %s[i]);" % (depth, label))
        depth = depth[2:]
        print("%s}" % depth)
        print("%sbson_append_finish_object(arr);" % depth)
        depth = depth[2:]
        print("%s}" % depth)
        print("")
        print("%sfor(int i = 0; i < %s; i++) {" % (depth, dims))
        depth += '  '
        print("%sbson_numstr(numstr, i);" % depth)
        print("%sbson_append_long(arr, numstr, %s[i]);" % (depth, value_name))
        depth = depth[2:]
        print("%s}" % depth)
        print("%sbson_append_finish_object(arr);" % depth)
        depth = depth[2:]
        print("%s}" % depth)

    else:
        raise NameError("Histograms and labeled histograms must use W64 type")

def histo_handler(matchstring):
    global depth
    type,name,dims,extra=matchstring.group(1,2,3,4)
    value_name = "%s%s" % (get_full_stack_name(), name)
    if type=="W64":
        print("%s{" % depth)
        depth += '  '
        print("%schar numstr[8];" % depth)
        print("%sbson_buffer *arr = bson_append_start_array(bb, \"%s\");" %
                (depth, name))
        print("%sbson_append_bool(bb, \"histogram\", (bson_bool_t)1);" % depth)
        print("%sfor(int i = 0; i < %s; i++) {" % (depth, dims))
        depth += '  '
        print("%sbson_numstr(numstr, i);" % depth)
        print("%sbson_append_long(arr, numstr, %s[i]);" % (depth, value_name))
        depth = depth[2:]
        print("%s}" % depth)
        print("%sbson_append_finish_object(arr);" % depth)
        depth = depth[2:]
        print("%s}" % depth)
        print("")

    else:
        raise NameError("Histograms and labeled histograms must use W64 type")

def commit_handler(matchstring):
    pass

def preprocessor_handler(matchstring):
    pass

def blank_handler(matchstring):
    pass

def unrecognized_line(matchstring):
    print >> sys.stderr, "error in line: %s" % (matchstring.string)
    raise NameError("error:")

infile = open(sys.argv[1],'r')
target =  sys.argv[2]
#must be the first handler to register
register_handler("\s*struct\s+(\w+)\s*\{\s*\/\/\s*rootnode\:\s*(.*)",rootnode_handler)
register_handler("^\s*struct\s+(\w+)\s*\{\s*\/\/\s*node:\s*(.*)",node_handler)
register_handler("^\s*struct\s+(\w+)\s*\{",struct_handler)
register_handler("^\s*[\s\w\+-=\(\)&]+\{\s*\/\/\s*operator\s*(.*)",operator_handler)
register_handler("^\s*\}",block_end_handler)
register_handler("^\s*(\w+)\s+(\w+)\s*\;",scalar_handler)
register_handler("^\s*(\w+)\s+(\w+)\s*\[(.+)\]\s*\;\s*$",array_handler)
register_handler("^\s*(\w+)\s+(\w+)\s*\[(.+)\]\s*\;\s*\/\/\s*label:\s+(.+)$",label_handler)
register_handler("^\s*(\w+)\s+(\w+)\s*\[(.+)\]\s*\;\s*\/\/\s*histo:\s+(.+)$",histo_handler)
register_handler("^\s*\/\/",commit_handler)
register_handler("^\s*\#",preprocessor_handler)
register_handler("^$/",blank_handler)
register_handler(".",unrecognized_line) # must be the last handler to register
f=infile.readlines()

print("#include <bson/bson.h>\n")

print_add_attr_func()

for line in f:
    line=line.rstrip()
    # Ignore the lines if ignore_line flag is enabled
    if ignore_line == True:
        matchstring = block_end_regex.search(line)
        if (matchstring) :
            ignore_line = False
        continue
    for regex, handler in zip(regexs,handlers):
        matchstring=regex.search(line)
        if(matchstring):
            # print("Found handler %s" % str(handler.__name__))
            handler(matchstring)
            if (depth==''):
                # if (topnode==target):
                    # print "  ofstream os(argv[1], std::ios::binary |"+\
                        # "std::ios::out);"
                    # print "  %s.write(os);" %target
                    # print "  os.close();"
                    # print "}"
                topnode=""
            break
        elif depth=='':
            break #if there is no rootnode don't search


infile.close()
