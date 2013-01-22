
# Cache Summary Plugin

import sys
mstats = sys.modules['__main__']

class CacheSummaryFilter(mstats.Filters):
    '''Add cache summary filters to node filter options'''
    order = -0.5 # Make it to run before node filter

    def set_options(self, parser):
        pass

    def filter(self, stats, options):
        if not options.cache_summary:
            return stats

        # Here we add cache specific node filters
        l1_filter_str = "base_machine::L1_.*"
        l2_filter_str = "base_machine::L2_.*"
        l3_filter_str = "base_machine::L3_.*"

        if not options.node:
            options.node = []

        options.node.append(l1_filter_str)
        options.node.append(l2_filter_str)
        options.node.append(l3_filter_str)

        return stats

class CacheSummaryProcess(mstats.Process):
    '''Calculate and Print cache summary'''

    def set_options(self, parser):
        parser.add_option("--cache-summary", action="store_true",
                default=False, help="Print summary of cache stats")

    def calc_cache_summary(self, node):
        # Node is cache staistics.
        # Find total access, total hit access, total miss access
        # Total snoop access (if any) etc. and store back in node
        cpu_req = node['cpurequest']
        hit = cpu_req['count']['hit']
        t_hit = hit['read']['hit']
        t_hit += hit['write']['hit']
        t_hit += hit['read']['forward']
        t_hit += hit['write']['forward']

        miss = cpu_req['count']['miss']
        t_miss = miss['read'] + miss['write']

        t_access = t_hit + t_miss

        node['summary'] = {
                'total_access' : t_access,
                'total_hit' : t_hit,
                'total_miss' : t_miss,
                'hit_ratio' : (float(t_hit)/float(t_access)),
                'miss_ratio' : (float(t_miss)/float(t_access)),
                }


    def calc_summary(self, stat):
        # Iterate through all 'base_machine' and get cache nodes
        base_m = None

        if 'base_machine' in stat.keys():
            base_m = stat['base_machine']
        else:
            key = stat.keys()[0]
            base_m = stat[key]['base_machine']

        for key,val in base_m.iteritems():
            if 'L1_' in key or 'L2_' in key or 'L3_' in key:
                self.calc_cache_summary(val)

    def process(self, stats, options):
        if not options.cache_summary:
            return stats

        for stat in stats:
            self.calc_summary(stat)

        return stats

class CacheSummaryWriter(mstats.Writers):
    '''Write a nice summary of Cache hit/miss ratio'''

    def set_options(self, parser):
        parser.add_option("--cache-summary-type", default="pretty",
                help="Cache summary output type",
                choices=["pretty", "csv"])

    def write_pretty(self, stat):
        # Write a simple summary with total access and ratio
        st_name = stat.keys()[0]
        if 'base_machine' in stat.keys():
            base_m = stat['base_machine']
        else:
            key = stat.keys()[0]
            base_m = stat[key]['base_machine']

        print("%s:" % st_name)
        pfx = "  "
        for key in sorted(base_m.keys()):
            val = base_m[key]
            if 'L1_' in key or 'L2_' in key or 'L3_' in key:
                print("%s%s:" % (pfx, key))
                cpfx = "%s  " % pfx
                summary = val['summary']
                print("%sTotal Hits   : %10d" % (cpfx, summary['total_hit']))
                print("%sTotal Miss   : %10d" % (cpfx, summary['total_miss']))
                print("%sTotal Access : %10d" % (cpfx, summary['total_access']))
                print("%sHit Ratio    : %9.4f%%" % (cpfx,
                    summary['hit_ratio']*100))
                print("%sMiss Ratio   : %9.4f%%" % (cpfx,
                    summary['miss_ratio']*100))

    def write_csv(self, stat):
        # Write a simple summary with total access and ratio
        st_name = stat.keys()[0]
        if 'base_machine' in stat.keys():
            base_m = stat['base_machine']
        else:
            key = stat.keys()[0]
            base_m = stat[key]['base_machine']

        for key in sorted(base_m.keys()):
            val = base_m[key]
            if 'L1_' in key or 'L2_' in key or 'L3_' in key:
                cpfx = "%s,%s" % (st_name, key)
                summary = val['summary']
                print("%s,total_hits,%d" % (cpfx, summary['total_hit']))
                print("%s,total_miss,%d" % (cpfx, summary['total_miss']))
                print("%s,total_access,%d" % (cpfx, summary['total_access']))
                print("%s,hit_ratio,%.4f" % (cpfx,
                    summary['hit_ratio']*100))
                print("%s,miss_ratio,%.4f" % (cpfx,
                    summary['miss_ratio']*100))

    def write(self, stats, options):
        if not options.cache_summary:
            return

        for stat in stats:
            if options.cache_summary_type == "pretty":
                self.write_pretty(stat)
            elif options.cache_summary_type == "csv":
                self.write_csv(stat)
            else:
                print("Unknown output type selected for cache summary.")
