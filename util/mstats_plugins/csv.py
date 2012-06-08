
import sys
mstats = sys.modules['__main__']

# CSV Writter
class CSVWriter(mstats.Writers):
    """
    Dump output in CSV format
    """

    def set_options(self, parser):
        parser.add_option("--csv", action="store_true", default=False,
                help="Print comma separated output of given node")
        parser.add_option("--csv-list-indexed", action="store_true",
                default="False", help="Print each element of list in new row")

    def write_csv_list(self, key, val):
        """Write list elements either in one row or multiple rows"""
        if self.list_indexed == True:
            print(",%s" % key)
            idx = 0
            for v in val:
                print("%d,%s" % (idx, str(v)))
                idx += 1
        else:
            values = ""
            for v in val:
                values += "%s," % str(v)
            line = "%s,%s" % (key, values)
            print(line)

    def write_csv(self, node):
        """Simply write a simple comma-separate-value formatted output"""

        for key,val in node.items():
            if type(val) == dict:
                print("%s" % key)
                self.write_csv(val)
            elif type(val) == list:
                self.write_csv_list(key, val)
            else:
                print("%s,%s" % (key, str(val)))

    def write(self, stats, options):
        if options.csv == False:
            return

        if options.csv_list_indexed == True:
            self.list_indexed = True
        else:
            self.list_indexed = False

        for stat in stats:
            self.write_csv(stat)
