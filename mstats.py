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

from optparse import OptionParser,OptionGroup

try:
    import yaml
except (ImportError, NotImplementedError):
    path = os.path.dirname(sys.argv[0])
    a_path = os.path.abspath(path)
    sys.path.append("%s/../ptlsim/lib/python" % a_path)
    import yaml

try:
    from yaml import CLoader as Loader
except:
    from yaml import Loader


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

# Operators Plugin Base Class
class Operators(object):
    """
    Base class for Operator plugins. These plugins are run after filters and
    before writers to perform user specific operations on selected data. For
    example user can add or subtract from selected nodes.
    """
    __metaclass__ = PluginBase
    order = 0

    def operate(stats, options):
        for plugin in Operators.get_plugins():
            p = plugin()
            stats = p.operate(stats, options)

        return stats
    operate = staticmethod(operate)


# YAML Stats Reader class
class YAMLReader(Readers):
    """
    Read the input file as YAML format.
    """

    def __init__(self):
        pass

    def set_options(self, parser):
        """ Add options to parser"""
        parser.add_option("-y", "--yaml", action="store_true", default="False",
                dest="yaml_file",
                help="Treat arguments as input YAML files")

    def load_yaml(self, file):
        docs = []

        for doc in yaml.load_all(file, Loader=Loader):
            doc['_file'] = file.name
            doc['_name'] = file.name.split('.')[0]
            docs.append(doc)

        return docs

    def read(self, options, args):
        """ Read yaml file if user give that option"""
        if options.yaml_file:
            docs = []
            for yf in args:
                l = lambda x: [ doc for doc in yaml.load_all(x, Loader=Loader)]
                with open(yf, 'r') as st_f:
                    docs += self.load_yaml(st_f)
            return docs

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
            if not stat['sim_stats']['tags']:
                pass

            res = self.check_required(re_tags, stat['sim_stats']['tags'])
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

    def search_node(self, nr, nd):
        for key in nd.keys():
            if nr.match(key):
                return key, nd[key]
        return None, None

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
                # First convert node name to list
                node = node.split('::')
                node_re = [re.compile("^" + n + "$") for n in node]

                # We start with top node
                st_keys = stat.keys()
                nd = stat
                fs = filter_stat
                k = None
                for nr in node_re:
                    k,nd = self.search_node(nr, nd)
                    if nd == None:
                        break
                    if not fs.has_key(k):
                        fs[k] = {}

                    if node_re[-1] == nr and nd != None:
                        fs[k] = nd
                    else:
                        if not fs.has_key(k):
                            fs[k] = {}
                        fs = fs[k]

            filter_stats.append(filter_stat)

        return filter_stats

# YAML based output generation
class YAMLWriter(Writers):
    """
    Print output in YAML format
    """

    def __init__(self):
        pass

    def set_options(self, parser):
        parser.add_option("--yaml-out", action="store_true", default="False",
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
        parser.add_option("--flatten", action="store_true", default="False",
                help="Print result in flattened format")

    def flatten_dict(self, node, str_pfx=None):
        for key,val in node.items():
            if str_pfx:
                str_pfx1 = str_pfx + "::" + str(key)
            else:
                str_pfx1 = str(key)
            if type(val) == dict:
                self.flatten_dict(val, str_pfx1)
            else:
                print("%s : %s" % (str_pfx1, str(val)))

    def write(self, stats, options):
        if options.flatten == True:
            for stat in stats:
                self.flatten_dict(stat)

def setup_options():
    opt = OptionParser("usage: %prog [options] args")

    opt_setup = lambda x,y: y.set_opt_parser(x) or opt.add_option_group(x)

    read_opt = OptionGroup(opt, "Input Options")
    opt_setup(read_opt, Readers)

    filter_opt = OptionGroup(opt, "Stats Filtering Options")
    opt_setup(filter_opt, Filters)

    write_opt = OptionGroup(opt, "Output Options")
    opt_setup(write_opt, Writers)

    return opt

def execute(options, args):
    """ Run this script with given options to generate user specific output"""

    # First read in all the stats
    stats = Readers.read(options, args)

    stats = Filters.filter(stats, options)

    Writers.write(stats, options)

if __name__ == "__main__":
    opt = setup_options()
    (options, args) = opt.parse_args()

    if args == None or args == []:
        opt.print_help()
        sys.exit(-1)

    execute(options, args)
