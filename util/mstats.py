#!/usr/bin/env python

# mstats.py
#
# This is a helper script to manipulate Marss statistics (YAML, time based
# etc.). Please run --help to list all the options.
#
# This script is provided under LGPL licence.
#
# Author: Avadh Patel (avadh4all@gmail.com) Copyright 2011
#

import os
import sys
import re
import operator

from optparse import OptionParser,OptionGroup

try:
    import yaml
except (ImportError, NotImplementedError):
    path = os.path.dirname(sys.argv[0])
    a_path = os.path.abspath(path)
    sys.path.append("%s/../ptlsim/lib/python" % a_path)
    import yaml

try:
    import Graphs
    graphs_supported = True
except (ImportError, NotImplementedError):
    graphs_supported = False

try:
    from yaml import CLoader as Loader
except:
    from yaml import Loader

# Standard Logging and Error reporting functions
def log(msg):
    print(msg)

def debug(msg):
    print("[DEBUG] : %s" % msg)

def error(msg):
    print("[ERROR] : %s" % msg)
    sys.exit(-1)

# Some helper functions
def is_leaf_node(node):
    """Check if this node is leaf node or not."""
    check = lambda x,y: (type(x[y]) != list and type(x[y]) != dict)

    if type(node) == list: indexes = range(len(node))
    elif type(node) == dict: indexes = node.keys()
    else: return True

    for idx in indexes:
        if not check(node, idx):
            return False

    return True

# Base plugin Metaclass
class PluginBase(type):
    """
    A Metaclass for reader and write plugins.
    """

    def __init__(self, class_name, bases, namespace):
        if not hasattr(self, 'plugins'):
            self.plugins = []
        else:
            self.plugins.append(self)

    def __str__(self):
        return self.__name__

    def get_plugins(self, *args, **kwargs):
        return sorted(self.plugins, key=lambda x: x.order)

    def set_opt_parser(self, parser):
        for plugin in self.plugins:
            assert hasattr(plugin, 'set_options')
            p = plugin()
            p.set_options(parser)

# Reader Plugin Base Class
class Readers(object):
    """
    Base class for all Reader plugins.
    """
    __metaclass__ = PluginBase
    order = 0

    def read(options, args):
        stats = []
        for plugin in Readers.get_plugins():
            p = plugin()
            st = p.read(options, args)
            if type(st) == list:
                stats.extend(st)
            elif st:
                stats.append(st)

        return stats
    read = staticmethod(read)

# Writer Plugin Base Class
class Writers(object):
    """
    Base class for all Writer plugins.
    """
    __metaclass__ = PluginBase
    order = 0

    def write(stats, options):
        for plugin in Writers.get_plugins():
            p = plugin()
            p.write(stats, options)

        return stats
    write = staticmethod(write)

# Filter Plugin Base Class
class Filters(object):
    """
    Base class for all Filter plugins.
    """
    __metaclass__ = PluginBase
    order = 0

    def filter(stats, options):
        for plugin in Filters.get_plugins():
            p = plugin()
            stats = p.filter(stats, options)

        return stats
    filter = staticmethod(filter)

# Process Plugin Base Class
class Process(object):
    """
    Base class for Operator plugins. These plugins are run after filters and
    before writers to perform user specific operations on selected data. For
    example user can add or subtract from selected nodes.
    """
    __metaclass__ = PluginBase
    order = 0

    def process(stats, options):
        for plugin in Process.get_plugins():
            p = plugin()
            stats = p.process(stats, options)

        return stats
    process = staticmethod(process)


# YAML Stats Reader class
class YAMLReader(Readers):
    """
    Read the input file as YAML format.
    """

    def __init__(self):
        pass

    def set_options(self, parser):
        """ Add options to parser"""
        parser.add_option("-y", "--yaml", action="store_true", default=False,
                dest="yaml_file",
                help="Treat arguments as input YAML files")

    def load_yaml(self, file):
        docs = []

        for doc in yaml.load_all(file, Loader=Loader):
            doc['_file'] = file.name
            doc['_name'] = os.path.splitext(file.name)[0]
            docs.append(doc)

        return docs

    def read(self, options, args):
        """ Read yaml file if user give that option"""
        if options.yaml_file == True:
            docs = []
            for yf in args:
                l = lambda x: [ doc for doc in yaml.load_all(x, Loader=Loader)]
                with open(yf, 'r') as st_f:
                    docs += self.load_yaml(st_f)
            return docs

class TimeGraphRead(Readers):
    """
    Generate a graph from periodic stats dump file
    """
    def __init__(self):
        global graphs_supported
        self.enabled = graphs_supported

    def set_options(self, parser):
        if self.enabled == False:
            return
        parser.add_option("--time-stats", action="store_true", default=False,
                help="Input time stats file")

    def read(self, options, args):
        if self.enabled and options.time_stats == True:
            assert(len(args) == 1)
            options.sg = Graphs.SimpleGraph(args[0])
        else:
            options.sg = None

class TimeGraphGen(Writers):
    def __init__(self):
        global graphs_supported
        self.enabled = graphs_supported

    def set_options(self, parser):
        if not self.enabled:
            return

        parser.add_option("--time-col", type="string", action="append",
                help="Label of column for creating graph, to add separate \
                        title name use COL_NAME,TITLE_NAME format")
        parser.add_option("--time-graph", type="string", default="time.png",
                help="Output file name")
        parser.add_option("--time-list-idx", action="store_true",
                default=False, help="Print the column title and its index")

    def write(self, stats, options):
        if self.enabled and options.sg and options.time_list_idx == True:
            print("Id\tTitle")
            i = 0
            for dt in options.sg.data.dtype.names:
                print("%d\t%s" % (i, dt))
                i += 1
        elif options.sg:
            options.sg.draw(options.time_graph, "sim_cycle", options.time_col)

class TagFilter(Filters):
    """
    Filter the stats based on tags.
    '-t' or '--tags=TAGS' option which works as must have tags.
    The stats must have all of the given tags in '-t' otherwise it will be
    filtered out.
    """
    order = -1 # Make sure its run before all default plugins

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("-t", "--tags", type="string", action="append",
                help="Specify tag search pattern. (Ex. astar|gcc to match \
                either one)")

    def check_required(self, re_tags, tags):
        count = 0
        matched_tags = []
        for r in re_tags:
            for t in tags:
                if type(t) != str: t = str(t)
                if r.match(t):
                    count += 1
                    matched_tags.append(t)
                    break
        # Check if all the tags has hit
        return len(re_tags) == count, matched_tags

    def filter(self, stats, options):
        """Filter stats based on given tags option"""
        if not options.tags:
            return stats

        # First we create regular expression for each tag search
        re_tags = []
        for tag in options.tags:
            re_tags.append(re.compile(tag))

        filtered = []
        for stat in stats:
            if not stat['simulator']['tags']:
                pass

            res = self.check_required(re_tags, stat['simulator']['tags'])
            if res[0] == True:
                st = {}
                st['.'.join([stat['_name']] + res[1])] = stat
                filtered.append(st)

        return filtered

class NodeFilter(Filters):
    """
    Filter stats based on Node selection option. All other nodes from stats
    will be removed. Users can specify regular expression for node name.
    """
    order = 1 # Make sure it runs at the end

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("-n", "--node", action="append",
                help="Select only given node, format: nodeA::nodeB::nodeC. \
                You can also use Regular expressions to select multiple nodes at same level.")

    def get_pattern(self, node):
        ret = []
        for n in node.split('::'):
            try:
                ret.append(re.compile("^" + n + "$"))
            except Exception as e:
                print("Invalid Node Search Pattern: %s" % str(n))
                print("In Node option: %s" % str(node))
                exit(-1)
        return ret

    def search_nodes(self, nr, nd):
        ret = []
        for key in nd.keys():
            if nr.match(key):
                ret.append(key)
        return ret

    def find_node(self, tree, node_re):
        """
        Find the node of a tree that matches the requested pattern and search
        recursively untill we run out of search patterns.
        """
        if len(node_re) == 0:
            return dict()

        ret = dict()
        keys_found = self.search_nodes(node_re[0], tree)

        for key in keys_found:
            node = None

            if len(node_re) == 1:
                # This node is last in search list so return full subnode
                node = tree[key]
            elif type(tree[key]) == dict:
                node = self.find_node(tree[key], node_re[1:])

            if node != None:
                ret[key] = node
        return ret

    def merge_tree(self, tree_a, tree_b):
        dst = tree_a.copy()
        stack = [(dst, tree_b)]

        while stack:
            c_dst, c_src = stack.pop()
            for key in c_src:
                if key not in c_dst:
                    c_dst[key] = c_src[key]
                else:
                    if type(c_src) == dict and type(c_dst) == dict:
                        stack.append((c_dst[key], c_src[key]))
                    else:
                        c_dst[key] = c_src[key]
        return dst

    def filter(self, stats, options):
        if not options.node:
            return stats

        filter_stats = []

        if options.tags != None:
            # When tag based filter is used, we add tags at the top
            # of the filtered stats. So we need to update the node
            # search.
            options.node = [".*::%s" % n for n in options.node]

        for stat in stats:
            filter_stat = {}
            for node in options.node:
                node_re = self.get_pattern(node)
                found = self.find_node(stat, node_re)
                filter_stat = self.merge_tree(filter_stat, found)

            if filter_stat:
                filter_stats.append(filter_stat)

        return filter_stats

class Summation(Process):
    """
    Sum all the nodes of filtered stats
    """
    order = 2

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("--sum", action="store_true", default=False,
                dest="sum", help="Sum of all selected nodes")
        parser.add_option("--sum-all", type="string" , default="",
                dest="sum_all", help="Sum of all stats")

    def do_sum(self, node, value = 0.0):
        for key,val in node.items():
            if type(val) == dict:
                value = self.do_sum(val, value)
            elif type(val) == list:
                if len(val) == 0:
                    continue
                if type(val[0]) == str:
                    continue
                if value == 0.0:
                    value = []
                value = map(sum, zip(val, value))
            elif type(val) == int or type(val) == float:
                value += val
        return value

    def sum(self, stats):
        summed = []

        for stat in stats:
            sum_stat = {}
            sum_val = self.do_sum(stat)
            key = stat.keys()[0]
            sum_stat[key] = sum_val
            summed.append(sum_stat)

        return summed

    def do_sum_merge(self, node, merge_node):
        for key,val in node.items():
            if type(val) == dict:
                self.do_sum_merge(val, merge_node[key])
            elif type(val) == list:
                if len(val) == 0:
                    continue
                if type(val[0]) == str:
                    continue # TODO Merge strings
                merge_node[key] = map(sum, zip(val, merge_node[key]))
            elif type(val) == int or type(val) == float:
                merge_node[key] += val

    def sum_all(self, stats, name):
        summed = { name : {}}

        for stat in stats:
            if len(summed[name]) == 0:
                summed[name] = stat[stat.keys()[0]] # Copy the first stat
            else:
                self.do_sum_merge(stat[stat.keys()[0]], summed[name])

        return [summed]

    def process(self, stats, options):
        if options.sum_all != "":
            stats = self.sum_all(stats, options.sum_all)
        if options.sum == True:
            stats = self.sum(stats)
        return stats


# YAML based output generation
class YAMLWriter(Writers):
    """
    Print output in YAML format
    """

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("--yaml-out", action="store_true", default=False,
                dest="yaml_out",
                help="Print output in YAML format")

    def write(self, stats, options):
        if options.yaml_out == True:
            yaml.dump_all(stats, stream=sys.stdout)

# Flatten the output
class FlattenWriter(Writers):
    """
    Dump result in flattened format 'nodeX::nodeY::nodeZ : Value'
    """

    def set_options(self, parser):
        parser.add_option("--flatten", action="store_true", default=False,
                help="Print result in flattened format")
        parser.add_option("--flatten-sep", default=":", dest="flatten_sep",
                help="Print result in flattened format")

    def flatten_dict(self, node, str_pfx=None):
        for key,val in node.items():
            if str_pfx:
                str_pfx1 = str_pfx + self.sep + str(key)
            else:
                str_pfx1 = str(key)
            if type(val) == dict:
                self.flatten_dict(val, str_pfx1)
            else:
                print("%s%s%s" % (str_pfx1, self.sep, str(val)))

    def write(self, stats, options):
        if options.flatten == True:
            self.sep = options.flatten_sep
            for stat in stats:
                self.flatten_dict(stat)

# Histogram Writter
class HistogramWriter(Writers):
    """
    Dump Histogram information of given node. A node must contain a list.
    """

    def set_options(self, parser):
        parser.add_option("--hist", action="store_true", default=False,
                help="Print Histogram of given node")

    def get_hist_of_node(self, node, pad):
        avg = 0.0
        total = 0
        maxval = 0
        minval = 0
        maxwidth = 0
        maxvwidth = 0
        weightedsum = 0

        is_dict = True if type(node) == dict else False
        if is_dict:
            sorted_t = sorted(node.iteritems(), key=operator.itemgetter(1),
                    reverse=True)
            indexes = [ x for x,v in sorted_t]
        else:
            indexes = range(len(node))

        for idx in indexes:
            value = node[idx]
            total += value
            minval = min(value, minval)
            maxval = max(value, maxval)
            maxwidth = max(len(str(idx)), maxwidth)
            maxvwidth = max(len(str(value)), maxvwidth)

            if not is_dict:
                weightedsum += (value * idx)

        if is_dict:
            avg = float(total)/float(len(node))
        else:
            avg = float(weightedsum)/float(total)


        output =  pad + "Minimum:         %d\n" % minval
        output += pad + "Maximum:         %d\n" % maxval
        output += pad + "Average:         %.2f\n" % avg
        output += pad + "Total Sum:       %d\n" % total
        if not is_dict:
            output += pad + "Weighted Sum:    %d\n" % weightedsum

        output += "\n"

        accum = 0

        for idx in indexes:
            value = node[idx]
            accum += value
            percent = float(value)/float(total) * 100.0;
            cumulative_p = float(accum)/float(total) * 100.0;

            n_stars = int((percent / 100.0) * 50);
            stars = ""
            for i in range(n_stars):
                stars += "*"

            if is_dict:
                output += pad + "%-*s [%5.1f] [%5.1f]: %*d %s\n" % (
                        maxwidth, idx, percent, cumulative_p, maxvwidth,
                        value, stars)
            else:
                output += pad + "%-*d [%5.1f] [%5.1f]: %*d %s\n" % (
                        maxwidth, idx, percent, cumulative_p, maxvwidth,
                        value, stars)
        return output

    def histogram_of_node(self, node, pad):
        for key,val in node.items():

            if type(val) not in [dict, list]:
                continue

            print("%s%s {" % (pad, key))
            pad += "  "

            if is_leaf_node(val) and len(val) > 1:
                print(self.get_hist_of_node(val, pad))
            else:
                self.histogram_of_node(val, pad)

            print("%s}" % pad[:-2])


    def write(self, stats, options):
        if options.hist == True:
            for stat in stats:
                self.histogram_of_node(stat,"")

############ Simpoints Merg Support Plugins  ############

class SPWeight(Readers):
    """
    Read the simpoint weights file
    """
    order = 1

    def __init__(self):
        pass

    def set_options(self, parser):
        """Set option parsers for cmdline options"""
        parser.add_option("--sp-weights", type="string", dest="sp_weights",
                help="Provide Simpoint Weight file")

    def read(self, options, args):
        if not options.sp_weights:
            return

        # Check if given file exists or not
        if not os.path.exists(options.sp_weights):
            error("Given simpoint weights file (%s) doesn't exists." %
                    options.sp_weights)

        with open(options.sp_weights, 'r') as weights:
            w = {}
            for line in weights.readlines():
                sp = line.strip().split(' ')
                assert(len(sp) == 2)
                weight = float(sp[0])
                id = int(sp[1])
                w[id] = weight

            # Replace 'sp_weights' in options with weights we have read in
            options.sp_weights = w

class SPPrefix(Readers):
    """
    Set the tag filter based on simpoint prefix
    """
    order = 2  # Set it to run after 'SPWeight' has run

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("--sp-pfx", type="string", dest="sp_pfx",
                help="Simpoint prefix used to filter stats")

    def read(self, options, args):
        if not options.sp_pfx:
            if options.sp_weights:
                error("Please provide simpoint prefix for filtering " + \
                      "benchmarks using --sp-pfx option")
            return

        # generate tag filter based on prefix provided
        sp_filter_pattern = "%s_sp_[0-9]+" % options.sp_pfx

        if options.tags == None:
            options.tags = [sp_filter_pattern]
        elif type(options.tags) == list:
            options.tags.append(sp_filter_pattern)
        else:
            error("Tag type is : %s" % type(options.tags))

class SPMerge(Process):
    """
    Merge the stats using Simpoint Weights
    """
    order = 0

    def __init__(self):
        pass

    def set_options(self, parser):
        # We dont set any option. This plugin will run when sp-weights is set
        # by SPWeight plugin
        pass

    def get_sp_id(self, st_name):
        sp = st_name.split('.')

        # we always ignore the first name because its a file name
        for n in sp[1:]:
            if 'sp_' in n:
                return int(n.split('_')[-1])

    def apply_weight(self, node, weight, merge_node):
        for key,val in node.items():
            if type(val) == dict:
                if not merge_node.has_key(key):
                    merge_node[key] = {}
                self.apply_weight(val, weight, merge_node[key])
            elif type(val) == list:
                if not merge_node.has_key(key):
                    merge_node[key] = [x * weight for x in val]
                else:
                    merge_node[key] = [y + (x * weight) for x,y in zip(val,
                        merge_node[key])]
            elif type(val) == int:
                if not merge_node.has_key(key):
                    merge_node[key] = 0
                merge_node[key] += (val * weight)
            elif type(val) == float:
                if not merge_node.has_key(key):
                    merge_node[key] = 0.0
                merge_node[key] += (val * weight)

    def process(self, stats, options):
        if options.sp_weights == None:
            return stats

        weights = options.sp_weights
        name = "%s_sp_merged" % options.sp_pfx
        merged_stat = { name : {} }

        # Iterate through all the stats and apply the weight
        for stat in stats:
            sp_id = self.get_sp_id(stat.keys()[0])
            weight = weights[sp_id]
            self.apply_weight(stat[stat.keys()[0]], weight, merged_stat[name])

        return [merged_stat]


def setup_options():
    opt = OptionParser("usage: %prog [options] args")

    opt_setup = lambda x,y: y.set_opt_parser(x) or opt.add_option_group(x)

    read_opt = OptionGroup(opt, "Input Options")
    opt_setup(read_opt, Readers)

    filter_opt = OptionGroup(opt, "Stats Filtering Options")
    opt_setup(filter_opt, Filters)

    process_opt = OptionGroup(opt, "PostProcess Options")
    opt_setup(process_opt, Process)

    write_opt = OptionGroup(opt, "Output Options")
    opt_setup(write_opt, Writers)

    return opt

def execute(options, args):
    """ Run this script with given options to generate user specific output"""

    # First read in all the stats
    stats = Readers.read(options, args)

    stats = Filters.filter(stats, options)

    stats = Process.process(stats, options)

    Writers.write(stats, options)

def load_plugins():
    exec_dir = os.path.dirname(os.path.realpath(sys.argv[0]))
    sys.path.append(exec_dir)
    path = "%s/mstats_plugins" % (exec_dir)
    for root, dirs, files in os.walk(path):
        for name in files:
            if name.endswith(".py") and not name.startswith("__"):
                path = os.path.join("mstats_plugins", name)
                path = path [1:] if path[0] == '/' else path
                plugin_name = path.rsplit('.',1)[0].replace('/','.')
                try:
                    __import__(plugin_name)
                except Exception as e:
                    debug("Unable to load plugin: %s" % plugin_name)
                    debug("Exception %s" % str(e))
                    pass

if __name__ == "__main__":
    load_plugins()

    opt = setup_options()
    (options, args) = opt.parse_args()

    if args == None or args == []:
        opt.print_help()
        sys.exit(-1)

    execute(options, args)
