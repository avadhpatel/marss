#!/usr/bin/env python

import os
import re
import sys

depth='  '
regexs=[]
handlers=[]
stack = []
node = "root"
topnode = ""
ignore_line = False
block_end_regex = None

def register_handler(regex,handler):
    global handlers
    global regexs
    global block_end_regex
    handlers+=[handler]
    regexs+=[re.compile(regex)]
    if handler == block_end_handler:
        block_end_regex = re.compile(regex)

def rootnode_handler(matchstring):
    global node,topnode,depth,stack
    node=topnode=matchstring.group(1)
    print "%sDataStoreNodeTemplate %s(\"%s\"); {"\
            %(depth,node,node)
    depth+='  '
    if matchstring.group(2)=="summable":
        print depth+node+".summable = 1;"
    if matchstring.group(2)=="identical":
        print depth+node+".identical = 1;"

def node_handler(matchstring):
    global node,tonode,depth,stack
    prevnode=node
    stack.append(node)
    node=matchstring.group(1)
    print("%sDataStoreNodeTemplate& %s = %s(\"%s\"); {") \
            %(depth,node,prevnode,node)
    depth+='  '
    if matchstring.group(2)=="summable":
        print depth+node+".summable = 1;"
    if matchstring.group(2)=="identical":
        print depth+node+".identical = 1;"

def struct_handler(matchstring):
    global node,tonode,depth,stack
    prevnode=node
    stack.append(node)
    node=matchstring.group(1)
    print("%sDataStoreNodeTemplate& %s = %s(\"%s\"); {") \
        %(depth,node,prevnode,node)
    depth+='  '

def operator_handler(matchstring):
    global ignore_line
    ignore_line = True

def block_end_handler(matchstring):
    global stack, depth,node
    if len(stack):
        node=stack.pop()
    depth=depth[2:]
    print depth+"}"

def scalar_handler(matchstring):
    type,name=matchstring.group(1,2)
    if type=="W64":
        print("%s%s.addint(\"%s\");")\
            %(depth,node,name)
    elif type=="double":
        print("%s%s.addfloat(\"%s\");")\
            %(depth,node,name)
    else :
        print "%s%s.add(\"%s\", %s);"\
        %(depth,node,name,type)

def array_handler(matchstring):
    typedict={}
    type,name,dims=matchstring.group(1,2,3)
    typedict["W64"]="%s.addint(\"%s\", %s);"\
        %(node,name,dims)
    typedict["double"]="%s.addfloat(\"%s\", %s);" \
        %(node,name,dims)
    typedict["char"]="%s.addstring(\"%s\", %s);" \
        %(node,name,dims)
    print depth + typedict[type]

def label_handler(matchstring):
    type,name,dims,label=matchstring.group(1,2,3,4)
    if type=="W64":
        print "%s%s.histogram(\"%s\", %s, %s);"\
            %(depth,node,name,dims,label)
    else:
        raise NameError("Histograms and labeled histograms must use W64 type")

def histo_handler(matchstring):
    type,name,dims,extra=matchstring.group(1,2,3,4)
    if type=="W64":
        print "%s%s.histogram(\"%s\", %s, %s);"\
            %(depth,node,name,dims,extra)
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
print("int main(int argc, char** argv) {");

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
            handler(matchstring)
            if (depth=='  '):
                if (topnode==target):
                    print "  ofstream os(argv[1], std::ios::binary |"+\
                        "std::ios::out);"
                    print "  %s.write(os);" %target
                    print "  os.close();"
                    print "}"
                topnode=""
            break
        elif depth=='  ':
            break #if there is no rootnode don't search


infile.close()
