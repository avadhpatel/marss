
#
# Table Writer plugin for mstats.py
#
# Released under GPLv2
#
# Copyright 2012 Avadh Patel


import sys
mstats = sys.modules['__main__']

from copy import copy

class TableFilter(mstats.Filters):
    def set_options(self, parser):
        pass

    def filter(self, stats, options):
        if not options.table:
            return stats

        if options.node == None:
            options.node = []

        options.node.append('_name')
        return stats

class TableWriter(mstats.Writers):
    '''Output in a table format'''

    def __init__(self):
        self.c = {}
        self.rows = []
        self.max_bench_size = 20

    def set_options(self, parser):
        parser.add_option("--table", action="store_true", default=False,
                help="Output data in a table format")

    def get_machine(self, stat):
        keys = stat.keys()
        if 'base_machine' in keys:
            return stat['base_machine']

        # Stats is tagged by user
        for key in keys:
            if key[0] != '_':
                return stat[key]['base_machine']

    def get_stat_name(self, stat):
        keys = stat.keys()
        if 'base_machine' in keys:
            return stat['_name']

        for key in keys:
            if key[0] != '_':
                return key

    def setup_columns(self):
        # Here we go through each unique key and its trees
        # and create a list of column index assigend to each
        # unique tree
        columns = self.c
        self.c = []

        for key in columns.keys():

            trees = columns[key]['tree']
            common_tree = None

            for tree in trees:
                if common_tree == None:
                    common_tree = set(tree)
                else:
                    common_tree = common_tree & set(tree)

            # Once we found common tree elements, we create
            # per tree dict that contain name, inded and tree
            # for the column
            for tree in trees:
                elem = {'idx' : len(self.c) + 1}
                st = set(tree) - common_tree
                elem['name'] = "%s.%s" % ('.'.join(st), key)
                elem['tree'] = tree
                self.c.append(elem)

    def fill_columns(self, node, tree=None):

        def add_key(k):
            if not self.c.has_key(k):
                self.c[k] = {'tree' : [copy(tree)]}
                self.c[k]['idx'] = len(self.c)
            else:
                if tree not in self.c[k]['tree']:
                    self.c[k]['tree'].append(copy(tree))

        for key in sorted(node.iterkeys()):
            val = node[key]
            if tree == None:
                tree = []
            tree.append(key)
            if type(val) is dict:
                self.fill_columns(val, tree)
            elif type(node) is list:
                for el in range(len(val)):
                    add_key('%s.%d' % (key, el))
            else:
                add_key(key)
            tree.pop()

    def get_new_row(self, key):
        r = [0 for i in range(len(self.c)+1)]
        r[0] = key
        self.rows.append(r)
        self.max_bench_size = max(self.max_bench_size, len(key)+1)
        return r

    def get_val(self, stat, tree):
        st = stat
        for t in tree:
            st = st[t]
        return st

    def fill_table(self, stat, row):
        for c in self.c:
            try:
                val = self.get_val(stat, c['tree'])
                row[c['idx']] = val
            except:
                row[c['idx']] = 0


    def write(self, stats, options):
        if not options.table:
            return

        for stat in stats:
            self.fill_columns(self.get_machine(stat))

        self.setup_columns()

        for stat in stats:
            row = self.get_new_row(self.get_stat_name(stat))
            self.fill_table(self.get_machine(stat), row)

        col_str = "%-*s " % (self.max_bench_size, 'Bench')
        for c in self.c:
            col_str += "%-20s " % c['name']

        print(col_str)

        for row in self.rows:
            row_str = ""
            for elem in row:
                if row_str == "":
                    row_str = "%-*s" % (self.max_bench_size, elem)
                else:
                    row_str += "%-20s" % str(elem)
            print(row_str)
