//
// PTLsim: Cycle Accurate x86-64 Simulator
// Statistical Analysis Tools
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <datastore.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>

struct PTLstatsConfig {
  stringbuf mode_subtree;
  stringbuf mode_histogram;
  stringbuf mode_bargraph;
  stringbuf mode_collect;
  stringbuf mode_collect_sum;
  stringbuf mode_collect_average;
  stringbuf mode_table;
  stringbuf mode_slice;
  stringbuf mode_slice_graph;

  stringbuf table_row_names;
  stringbuf table_col_names;
  stringbuf table_row_col_pattern;
  stringbuf table_type_name;
  bool use_percents;
  
  stringbuf graph_title;
  double graph_width;
  double graph_height;
  double graph_clip_percentile;
  W64 graph_logscale;
  double graph_logk;
  bool graph_stacked;

  stringbuf snapshot;
  stringbuf subtract_branch;

  bool show_sum_of_subtrees_only;
  
  W64 maxdepth;
  
  W64 table_scale_rel_to_col;
  W64 table_mark_highest_col;
  
  W64 percent_digits;
  
  double histogram_thresh;
  bool cumulative_histogram;
  bool show_stars_in_histogram;
  
  bool percent_of_toplevel;
  bool hide_zero_branches;
  bool slice_cumulative;
  
  bool invert_gains;

  bool print_datastore_info;
  bool print_template;

  void reset();
};

void PTLstatsConfig::reset() {
  mode_subtree.reset();
  mode_histogram.reset();
  mode_bargraph.reset();
  mode_collect.reset();
  mode_collect_sum.reset();
  mode_collect_average.reset();
  mode_table.reset();
  mode_slice.reset();
  mode_slice_graph.reset();

  table_row_names.reset();
  table_col_names.reset();
  table_row_col_pattern = "%row/%col.stats";
  table_type_name = "text";
  use_percents = false;

  graph_title.reset();
  graph_width = 300.0;
  graph_height = 100.0;
  graph_clip_percentile = 95.0;
  graph_logscale = 0;
  graph_logk = 100.;
  graph_stacked = 0;

  snapshot = "final";
  subtract_branch.reset();

  show_sum_of_subtrees_only = 0;
  
  maxdepth = limits<int>::max;
  
  table_scale_rel_to_col = limits<int>::max;
  table_mark_highest_col = 0;
  
  percent_digits = 1; // e.g. "66.7%"
  
  histogram_thresh = 0.0001;
  cumulative_histogram = 0;
  show_stars_in_histogram = 1;
  
  percent_of_toplevel = 1;
  hide_zero_branches = 1;
  slice_cumulative = 0;

  invert_gains = 0;

  print_datastore_info = 0;
  print_template = 0;
}

PTLstatsConfig config;
ConfigurationParser<PTLstatsConfig> configparser;

template <>
void ConfigurationParser<PTLstatsConfig>::setup() {
  section("Mode");
  add(mode_subtree,                     "subtree",                   "Subtree (specify path to node)");
  add(mode_collect,                     "collect",                   "Collect specific statistic from multiple data stores");
  add(mode_collect_sum,                 "collectsum",                "Sum of same tree in all data stores");
  add(mode_collect_average,             "collectaverage",            "Average of same tree in all data stores");
  add(mode_histogram,                   "histogram",                 "Histogram of specific node (specify path to node)");
  add(mode_bargraph,                    "bargraph",                  "Bargraph of one node across multiple data stores");
  add(mode_table,                       "table",                     "Table of one node across multiple data stores");
  add(mode_slice,                       "slice",                     "Slice of every snapshot, in list format");
  add(mode_slice_graph,                 "slice-graph",               "Slice of every snapshot, in line graph format");

  section("Table or Graph");
  add(table_row_names,                  "rows",                      "Row names (comma separated)");
  add(table_col_names,                  "cols",                      "Column names (comma separated)");
  add(table_row_col_pattern,            "table-pattern",             "Pattern to convert row (%row) and column (%col) names into stats filename");
  add(table_type_name,                  "tabletype",                 "Table type (text, latex, html)");
  add(table_scale_rel_to_col,           "scale-relative-to-col",     "Scale all other table columns relative to specified column");
  add(table_mark_highest_col,           "table-mark-highest-col",    "Mark highest column in each row");
  add(use_percents,                     "use-percents",              "Show percents (as in tree) rather than absolute values");
  add(invert_gains,                     "invert-gains",              "Invert sense of gains vs losses (i.e. 1 / x)");

  section("Statistics Range");
  add(snapshot,                         "snapshot",                  "Main snapshot (default is final snapshot)");
  add(subtract_branch,                  "subtract",                  "Snapshot to subtract from the main snapshot");

  section("Display Control");
  add(show_sum_of_subtrees_only,        "sum-subtrees-only",         "Show only the sum of subtrees in applicable nodes");
  add(maxdepth,                         "maxdepth",                  "Maximum tree depth");
  add(percent_digits,                   "percent-digits",            "Precision of percentage listings in digits");
  add(percent_of_toplevel,              "no-percent-of-toplevel",    "Show percent relative to immediate parent node rather than toplevel summable node");
  add(hide_zero_branches,               "no-hide-zero-branches",     "Display branches with a total of zero");
  add(slice_cumulative,                 "slice-cumulative",          "Show slice through each snapshot without subtracting from previous");

  section("Graph and Histogram Options");
  add(graph_title,                      "title",                     "Graph Title");
  add(graph_width,                      "width",                     "Width in SVG pixels");
  add(graph_height,                     "height",                    "Width in SVG pixels");
  add(graph_stacked,                    "graph-stacked",             "Graph with solid stacks (e.g. out of total of 100%) instead of lines");

  section("Histogram Options");
  add(graph_clip_percentile,            "percentile",                "Clip percentile");
  add(graph_logscale,                   "logscale",                  "Use log scale");
  add(graph_logk,                       "logk",                      "Log scale constant");
  add(cumulative_histogram,             "cumulative-histogram",      "Cumulative histogram");
  add(histogram_thresh,                 "histogram-thresh",          "Histogram threshold (1.0 = print nothing, 0.0 = everything)");
  add(show_stars_in_histogram,          "nostars",                   "Don't show stars (***) in histogram");

  section("Miscellaneous");
  add(print_datastore_info,             "info",                      "Print information about the data store file");
  add(print_template,                   "template",                  "Print template in C++ struct format");
};

struct RGBAColor {
  float r;
  float g;
  float b;
  float a;
};

struct RGBA: public RGBAColor {
  RGBA() { }

  RGBA(float r, float g, float b, float a = 255) {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }

  RGBA(const RGBAColor& rgba) {
    r = rgba.r;
    g = rgba.g;
    b = rgba.b;
    a = rgba.a;
  }
};

ostream& operator <<(ostream& os, const RGBA& rgba) {
  os << '#', hexstring((byte)math::round(rgba.r), 8), hexstring((byte)math::round(rgba.g), 8), hexstring((byte)math::round(rgba.b), 8);
  return os;
}

class SVGCreator {
public:
  ostream* os;
  int idcounter;

  bool filled;
  RGBA fill;
  RGBA stroke;
  float strokewidth;
  char* fontinfo;
  float xoffs;
  float yoffs;

  float dashoffset;
  float dashon;
  float dashoff;

  SVGCreator(ostream& os, float width, float height) {
    this->os = &os;
    idcounter = 0;
    filled = 1;
    fill = RGBA(0, 0, 0, 255);
    stroke = RGBA(0, 0, 0, 255);
    strokewidth = 0.1;
    fontinfo = null;
    setoffset(0, 0);
    setdash(0, 0, 0);
    setfont("font-size:4;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:middle;writing-mode:lr-tb");

    printheader(width, height);
  }

  void setdash(float dashoffset, float dashon = 0, float dashoff = 0) {
    this->dashoffset = dashoffset;
    this->dashon = dashon;
    this->dashoff = dashoff;
  }

  void setoffset(float x, float y) {
    xoffs = x; yoffs = y;
  }

  void setfont(const char* font) {
    if (fontinfo) free(fontinfo);
    fontinfo = strdup(font);
  }

  ostream& printstyle(ostream& os) {
    os << "fill:"; if (filled) os << fill; else os << "none"; os << ";";
    os << "fill-opacity:", (fill.a / 255.0), ";";
    if (filled) os << "fill-rule:evenodd;";
    os << "stroke:"; if (strokewidth > 0) os << stroke; else os << "none"; os << ";";
    os << "stroke-width:", strokewidth, ";";
    os << "stroke-linecap:round;stroke-linejoin:miter;stroke-miterlimit:4.0;";
    os << "stroke-opacity:", (stroke.a / 255.0), ";";
    if (dashon) os << "stroke-dashoffset:", dashoffset, ";stroke-dasharray:", dashon, ",", dashoff, endl;
    return os;
  }

  ostream& printfont(ostream& os) {
    os << fontinfo, ';';
    return os;
  }

  void printheader(float width, float height) {
    *os << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>", endl;
    *os << "<svg xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns=\"http://www.w3.org/2000/svg\" id=\"svg2\" height=\"", height, "\" width=\"", width, "\" y=\"0.0\" x=\"0.0000000\" version=\"1.0\">", endl;
  }

  void newlayer(const char* name = null) {
    if (!name)
      *os << "<g id=\"", "layer", idcounter++, "\">", endl;
    else *os << "<g id=\"", name, "\">", endl;
  }

  void exitlayer() {
    *os << "</g>", endl;
  }

  void rectangle(float x, float y, float width, float height) {
    *os << "<rect id=\"rect", idcounter++, "\" style=\"";
    printstyle(*os);
    *os << "\" y=\"", (y + yoffs), "\" x=\"", (x + xoffs), "\" height=\"", height, "\" width=\"", width, "\" />", endl;
  }

  void text(const char* string, float x, float y) {
    *os << "<text xml:space=\"preserve\" id=\"text", idcounter++, "\" style=\"";
    printstyle(*os);
    printfont(*os);
    *os << "\" y=\"", y, "\" x=\"", x, "\">", endl;
    *os << "<tspan id=\"tspan", idcounter++, "\" y=\"", (y + yoffs), "\" x=\"", (x + xoffs), "\">", string, "</tspan></text>", endl;
  }

  void line(float x1, float y1, float x2, float y2) {
    *os << "<path id=\"path", idcounter++, "\" style=\"";
    printstyle(*os);
    *os << "\" d=\"M ", (x1 + xoffs), ",", (y1 + yoffs), " L ", (x2 + xoffs), ",", (y2 + yoffs), "\" />", endl;
  }

  void startpath(float x, float y, const char* name = null) {
    *os << "<path id=\"";
    if (name)
      *os << name;
    else *os << "path", idcounter;
    *os << "\" style=\"";
    printstyle(*os);
    *os << "\" d=\"M ", (x + xoffs), ",", (y + yoffs);
    idcounter++;
  }

  void nextpoint(float x, float y) {
    *os << " L ", (x + xoffs), ",", (y + yoffs);
  }

  void closepath() {
    *os << " z ", endl;
  }

  void endpath() {
    *os << "\" />", endl;
  }

  void finalize() {
    *os << "</svg>", endl;
  }

  ~SVGCreator() {
    finalize();
  }
};

static inline double logscale(double x) {
  return log(1 + (x*config.graph_logk)) / log(1 + config.graph_logk);
}

static inline double invlogscale(double x) {
  return (exp(x*log(1 + config.graph_logk)) - 1) / config.graph_logk;
}

const RGBA graph_background(225, 207, 255);

struct LineAttributes {
  bool enabled;
  bool stacked;
  RGBAColor stroke;
  float width;
  float dashoffset;
  float dashon;
  float dashoff;
  bool filled;
  RGBAColor fill;
};

const LineAttributes black_linetype = {1, 0, {0,   0,   0, 255}, 0.10, 0.00, 0.00, 0.00, 0, {0,   0,   0,   255}};

void create_svg_of_histogram_percent_bargraph(ostream& os, W64s* histogram, int count, const char* title = null, double imagewidth = 300.0, double imageheight = 100.0) {
  double leftpad = 10.0;
  double toppad = 5.0;
  double rightpad = 4.0;
  double bottompad = 5.0;

  if (title) toppad += 16;

  int maxwidth = 0;

  W64 total = 0;
  foreach (i, count) { total += histogram[i]; }

  double cum = 0;
  foreach (i, count) { 
    cum += ((double)histogram[i] / (double)total);
    maxwidth++;
    if (cum >= (config.graph_clip_percentile / 100.0)) break;
  }

  double maxheight = 0;
  foreach (i, maxwidth+1) { maxheight = max(maxheight, (double)histogram[i] / (double)total); }

  double xscale = imagewidth / ((double)maxwidth + 1);

  SVGCreator svg(os, imagewidth + leftpad + rightpad, imageheight + toppad + bottompad);

  svg.newlayer();

  svg.strokewidth = 0.0;
  svg.stroke = RGBA(255, 255, 255);
  svg.filled = 0;
  svg.rectangle(0, 0, imagewidth + leftpad + rightpad, imageheight + toppad + bottompad);

  svg.setoffset(leftpad, toppad);

  if (title) {
    svg.fill = RGBA(0, 0, 0);
    svg.filled = 1;
    svg.setfont("font-size:8;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:middle;writing-mode:lr-tb");
    svg.text(title, imagewidth / 2, -6);
  }

  svg.stroke = RGBA(0, 0, 0);
  svg.strokewidth = 0.0;
  svg.filled = 1;
  svg.fill = graph_background;
  svg.rectangle(0, 0, (maxwidth+1) * xscale, imageheight);

  svg.strokewidth = 0.0;

  svg.fill = RGBA(64, 0, 255);

  foreach (i, maxwidth+1) {
    double x = ((double)histogram[i] / (double)total) / maxheight;
    if (config.graph_logscale) x = logscale(x);
    double barsize = x * imageheight;

    if (barsize >= 0.1) svg.rectangle(i*xscale, imageheight - barsize, xscale, barsize);
  }

  svg.fill = RGBA(0, 0, 0);

  svg.setfont("font-size:4;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:middle;writing-mode:lr-tb");

  for (double i = 0; i <= 1.0; i += 0.1) {
    stringbuf sb;
    sb << floatstring(i * maxwidth, 0, 0);
    svg.text(sb, i * imagewidth, imageheight + 3.0);
  }

  svg.setfont("font-size:4;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:end;writing-mode:lr-tb");

  for (double i = 0; i <= 1.0; i += 0.2) {
    stringbuf sb;
    double value = (config.graph_logscale) ? (invlogscale(i) * maxheight * 100.0) : (i * maxheight * 100.0);
    double y = ((1.0 - i)*imageheight);
    sb << floatstring(value, 0, 0), "%";
    svg.text(sb, -0.2, y - 0.2);

    svg.strokewidth = 0.1;
    svg.stroke = RGBA(170, 156, 192);
    svg.line(-6, y, (maxwidth+1) * xscale, y);
    svg.strokewidth = 0;
  }

  for (double x = 0; x <= 1.0; x += 0.05) {
    svg.strokewidth = 0.1;
    svg.stroke = RGBA(170, 156, 192);
    svg.line(x * imagewidth, 0, x * imagewidth, imageheight);
    svg.strokewidth = 0;
  }

  svg.exitlayer();
}

void create_svg_of_percentage_line_graph(ostream& os, double* xpoints, int xcount, double** ypoints, int ycount, char** ynames,
                                         double imagewidth, double imageheight, const LineAttributes* linetype, const RGBA& background, bool stacked) {
  double leftpad = 10.0;
  double toppad = 5.0;
  double rightpad = 4.0;
  double bottompad = 5.0;

  double xmax = 0;

  foreach (i, xcount) {
    xmax = max(xmax, xpoints[i]);
  }

  double xscale = imagewidth / xmax;

  double yscale = imageheight / 100.0;

  SVGCreator svg(os, imagewidth + leftpad + rightpad, imageheight + toppad + bottompad);
  svg.setoffset(leftpad, toppad);

  svg.newlayer();

  svg.stroke = RGBA(0, 0, 0);
  svg.strokewidth = 0.1;
  svg.filled = 1;
  svg.fill = background;
  svg.rectangle(0, 0, imagewidth, imageheight);

  svg.strokewidth = 0.1;
  svg.stroke = RGBA(0, 0, 0);
  svg.strokewidth = 0.1;
  svg.filled = 0;
  svg.fill = RGBA(0, 0, 0);

  double* stackbase = new double[xcount];
  foreach (i, xcount) stackbase[i] = 0;
  /*
  foreach (x, xcount) {
    cout << "[", x, "]";
    foreach (y, ycount) {
      cout << ' ', ypoints[y][x];
    }
    cout << endl;
  }
  */
  foreach (col, ycount) {
    const LineAttributes& line = (linetype) ? linetype[col] : black_linetype;

    if (!line.enabled)
      continue;
    if (!stacked)
      continue;

    foreach (sample, xcount) {
      ypoints[col][sample] += stackbase[sample];
      stackbase[sample] = ypoints[col][sample];
    }
  }
#if 0
  foreach (x, xcount) {
    cout << "[", x, "]";
    foreach (y, ycount) {
      cout << ' ', ypoints[y][x];
    }
    cout << endl;
  }
#endif

  delete[] stackbase;

  /*
  for (int layer = 1; layer >= 0; layer--) {
    for (int j = ycount-1; j >= 0; j--) {
      const LineAttributes& line = (linetype) ? linetype[j] : black_linetype;
      svg.strokewidth = line.width;
      svg.stroke = line.stroke;
      svg.setdash(line.dashoffset, line.dashon, line.dashoff);
      svg.filled = line.filled;
      svg.fill = line.fill;

      if (!line.enabled)
        continue;

      if (stacked != layer)
        continue;

      foreach (i, xcount) {
        double yy = ypoints[j][i];
        double x = xpoints[i] * xscale;
        double y = imageheight - (yy * yscale);
        if (i == 0) x = 0; else if (i == xcount-1) x = imagewidth;
        y = clipto(y, 0.0, imageheight - 1);
        if (i == 0) {
          char* pathname = null;
          stringbuf sb;
          if (ynames) {
            sb << "graph_", ynames[j];
            pathname = sb;
          }
          if (line.filled)
            svg.startpath(0, imageheight, pathname);
          else svg.startpath(x, y, pathname);
        }
        svg.nextpoint(x, y);
      }

      if (line.filled) svg.nextpoint(imagewidth, imageheight);
      svg.endpath();
    }
  }
  */

  for (int col = ycount-1; col >= 0; col--) {
    const LineAttributes& line = (linetype) ? linetype[col] : black_linetype;
    svg.strokewidth = line.width;
    svg.stroke = line.stroke;
    svg.setdash(line.dashoffset, line.dashon, line.dashoff);
    svg.filled = line.filled;
    svg.fill = line.fill;
    
    if (!line.enabled)
      continue;
    
    foreach (sample, xcount) {
      double yy = ypoints[col][sample];
      double xp = xpoints[sample] * xscale;
      double yp = imageheight - (yy * yscale);
      if (sample == 0) xp = 0; else if (sample == xcount-1) xp = imagewidth;
      yp = clipto(yp, 0.0, imageheight - 1);
      if (sample == 0) {
        char* pathname = null;
        stringbuf sb;
        if (ynames) {
          sb << "graph_", ynames[col];
          pathname = sb;
        }
        if (stacked)
          svg.startpath(0, imageheight, pathname);
        else svg.startpath(xp, yp, pathname);
      }
      svg.nextpoint(xp, yp);
    }
    
    if (stacked) {
      svg.nextpoint(imagewidth, imageheight);
      svg.nextpoint(0, imageheight);
      svg.closepath();
    }
    svg.endpath();
  }

  svg.filled = 1;
  svg.fill = RGBA(0, 0, 0);
  svg.strokewidth = 0;
  svg.setdash(0);

  svg.setfont("font-size:4;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:middle;writing-mode:lr-tb");

  for (double i = 0; i <= 1.0; i += 0.1) {
    stringbuf sb;
    sb << floatstring(i * xmax, 0, 0);
    svg.text(sb, i * imagewidth, imageheight + 4.0);
  }

  svg.setfont("font-size:4;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial;text-anchor:end;writing-mode:lr-tb");

  for (double i = 0; i <= 1.0; i += 0.1) {
    stringbuf sb;
    double value = i * 100.0;

    double y = ((1.0 - i)*imageheight);

    sb << floatstring(value, 0, 0), "%";
    svg.text(sb, -0.3, y - 0.3);

    svg.strokewidth = 0.1;
    svg.stroke = RGBA(170, 156, 192);
    svg.line(-6, y, imagewidth, y);
    svg.strokewidth = 0;
  }

  for (double x = 0; x <= 1.0; x += 0.05) {
    svg.strokewidth = 0.1;
    svg.stroke = RGBA(170, 156, 192);
    svg.line(x * imagewidth, 0, x * imagewidth, imageheight);
    svg.strokewidth = 0;
  }

  svg.exitlayer();
}

#define NOLINE {0, 0, {0, 0, 0, 0}, 0.00, 0.00, 0.00, 0.00, 0, {0, 0, 0, 0}}

/*

struct LineAttributes {
  bool enabled;
  bool stacked;
  RGBAColor stroke;
  float width;
  float dashoffset;
  float dashon;
  float dashoff;
  bool filled;
  RGBAColor fill;
};

*/

void printbanner() {
  cerr << "//  ", endl;
  cerr << "//  PTLstats: PTLsim statistics data store analysis tool", endl;
  cerr << "//  Copyright 1999-2006 Matt T. Yourst <yourst@yourst.com>", endl;
  cerr << "//  ", endl;
  cerr << endl;
}

DataStoreNode* collect_into_supernode(int argc, char** argv, char* path, const char* deltastart = null, const char* deltaend = "final") {
  DataStoreNode* supernode = new DataStoreNode("super");

  StatsFileReader reader;

  foreach (i, argc) {
    char* filename = argv[i];
    DataStoreNode* endbase;
    DataStoreNode* end;
    DataStoreNode* startbase;
    DataStoreNode* start;

    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return null;
    }

    // Can't have slashes in tree pathnames
    int filenamelen = strlen(filename);
    foreach (i, filenamelen) { if (filename[i] == '/') filename[i] = ':'; }

    DataStoreNode* dsbase = (deltastart) ? reader.getdelta(deltaend, deltastart) : reader.get(deltaend);

    if (!dsbase) {
      cerr << "ptlstats: Error: cannot find ending snapshot '", deltaend, "' or starting snapshot '", deltastart, "'", endl;
      reader.close();
      return null;
    }

    DataStoreNode* ds = null;

    if (!(ds = dsbase->searchpath(path))) {
      cerr << "ptlstats: Error: cannot find subtree '", path, "'", endl;
      delete dsbase;
      reader.close();
      return null;
    }

    ds->rename(filename);
    supernode->add(ds);
  }

  return supernode;
}

class TableCreator {
public:
  ostream& os;
  dynarray<char*>& rownames;
  dynarray<char*>& colnames;
  int row_name_width;
public:
  TableCreator(ostream& os_, dynarray<char*>& rownames_, dynarray<char*>& colnames_):
    os(os_), rownames(rownames_), colnames(colnames_) {
    row_name_width = 0;
    foreach (i, rownames.size()) row_name_width = max(row_name_width, (int)strlen(rownames[i]));
  }

  virtual void start_header_row() {
    os << padstring("", row_name_width);
  }

  virtual void print_header(int col) {
    os << "  ", padstring(colnames[col], 8);
  }

  virtual void end_header_row() {
    os << endl;
  }

  virtual void start_row(int row) {
    os << padstring(rownames[row], row_name_width);
  }

  virtual void print_data(double value, int row, int column) {
    bool isint = ((value - math::floor(value)) < 0.0000001);
    int width = max((int)strlen(colnames[column]), 8) + 2;
    if (isint) os << intstring((W64s)value, width); else os << floatstring(value, width, 3);
  }

  virtual void end_row() {
    os << endl;
  }

  virtual void start_special_row(const char* title) {
    os << padstring(title, row_name_width);
  }

  virtual void end_table() { }
};

class LaTeXTableCreator: public TableCreator {
public:
  LaTeXTableCreator(ostream& os_, dynarray<char*>& rownames_, dynarray<char*>& colnames_):
    TableCreator(os_, rownames_, colnames_) {
    os << "\\documentclass{article}", endl;
    os << "\\makeatletter", endl;
    os << "\\providecommand{\\tabularnewline}{\\\\}", endl;
    os << "\\makeatother", endl;
    os << "\\begin{document}", endl;

    os << "\\begin{tabular}{|c|";
    foreach (i, colnames.size()) { os << "|c"; }
    os << "|}", endl;
    os << "\\hline", endl;
  }

  virtual void start_header_row() { }

  virtual void print_header(int col) {
    os << "&", colnames[col];
  }

  virtual void end_header_row() {
    os << "\\tabularnewline\\hline\\hline", endl;
  }

  virtual void start_row(int row) {
    os << rownames[row];
  }

  virtual void start_special_row(const char* title) {
    os << title;
  }

  virtual void print_data(double value, int row, int column) {
    os << "&";
    bool isint = ((value - math::floor(value)) < 0.0000001);
    if (isint) os << (W64s)value; else os << floatstring(value, 0, 1);
  }

  virtual void end_row() {
    os << "\\tabularnewline\\hline", endl;
  }

  virtual void end_table() {
    os << "\\end{tabular}", endl;
    os << "\\end{document}", endl;
  }
};

enum { TABLE_TYPE_TEXT, TABLE_TYPE_LATEX, TABLE_TYPE_HTML };

bool capture_table(dynarray<char*>& rowlist, dynarray<char*>& collist, dynarray<double>& sum_of_all_rows, dynarray< dynarray<double> >& data,
                   char* statname, char* rownames, char* colnames, char* row_col_pattern) {
  rowlist.tokenize(rownames, ",");
  collist.tokenize(colnames, ",");

  sum_of_all_rows.resize(collist.size());
  sum_of_all_rows.fill(0);

  data.resize(rowlist.size());

  //
  // Collect data
  //
  const char* findarray[2] = {"%row", "%col"};

  StatsFileReader reader;

  for (int row = 0; row < rowlist.size(); row++) {
    data[row].resize(collist.size());

    for (int col = 0; col < collist.size(); col++) {
      stringbuf filename;

      const char* replarray[2];
      replarray[0] = rowlist[row];
      replarray[1] = collist[col];
      stringsubst(filename, config.table_row_col_pattern, findarray, replarray, 2);

      if (!reader.open(filename)) {
        cerr << "ptlstats: Cannot open '", filename, "' for row ", row, ", col ", col, endl, endl, flush;
        return false;
      }

      DataStoreNode* ds = reader.get(config.snapshot);
      if (!ds) {
        cerr << "ptlstats: Cannot open snapshot '", config.snapshot, "' in '", filename, "' for row ", row, ", col ", col, endl, endl, flush;
        reader.close();
        return false;
      }

      ds = ds->searchpath(statname);

      double value;
      if (ds) {
        value = *ds;
        sum_of_all_rows[col] += value;
      } else { 
        cerr << "ptlstats: Warning: cannot find subtree '", statname, "' for row ", row, ", col ", col, endl;
        value = 0;
      }

      data[row][col] = value;

      reader.close();
    }
  }

  return true;
}

void create_table(ostream& os, int tabletype, char* statname, char* rownames, char* colnames, char* row_col_pattern, int scale_relative_to_col) {
  dynarray<char*> rowlist;
  dynarray<char*> collist;
  dynarray<double> sum_of_all_rows;
  dynarray< dynarray<double> > data;

  if (!capture_table(rowlist, collist, sum_of_all_rows, data, statname, rownames, colnames, row_col_pattern)) return;

  TableCreator* creator;
  switch (tabletype) {
  case TABLE_TYPE_TEXT:
    creator = new TableCreator(os, rowlist, collist); break;
  case TABLE_TYPE_LATEX:
    creator = new LaTeXTableCreator(os, rowlist, collist); break;
  case TABLE_TYPE_HTML:
    assert(false);
  }

  //
  // Print data
  //
  creator->start_header_row();
  for (int col = 0; col < collist.size(); col++) creator->print_header(col);
  creator->end_header_row();

  for (int row = 0; row < rowlist.size(); row++) {
    double relative_base = 0;
    creator->start_row(row);
    for (int col = 0; col < collist.size(); col++) {
      double value = data[row][col];

      if (scale_relative_to_col < collist.size()) {
        if (col != scale_relative_to_col) {
          value = ((data[row][scale_relative_to_col] / value) - 1.0) * 100.0;
        }
      }

      creator->print_data(value, row, col);
    }
    creator->end_row();
  }

  {
    double relative_base = 0;
    creator->start_special_row("Total");
    for (int col = 0; col < collist.size(); col++) {
      double value = sum_of_all_rows[col];

      if (scale_relative_to_col < collist.size()) {
        if (col == scale_relative_to_col) {
          relative_base = value;
        } else {
          value = ((relative_base / value) - 1.0) * 100.0;
        }
      }

      creator->print_data(value, rowlist.size()+0, col);
    }
    creator->end_row();
  }

  creator->end_table();
}

//
// In SVG standard, 1.25 points = 1.0 px
//
inline double px_to_pt(double px) { return px * 1.25; }
inline double pt_to_px(double pt) { return pt / 1.25; }

void create_grouped_bargraph(ostream& os, char* statname, char* rownames, char* colnames, char* row_col_pattern,
                             int scale_relative_to_col, const char* title = null, double imagewidth = 300.0, double imageheight = 100.0) {

  static const bool show_row_labels_in_group = false;
  static const bool show_average = false;

  dynarray<char*> rowlist;
  dynarray<char*> collist;
  dynarray<double> sum_of_all_rows;
  dynarray< dynarray<double> > data;

  if (!capture_table(rowlist, collist, sum_of_all_rows, data, statname, rownames, colnames, row_col_pattern)) return;

  if (show_average) {
    dynarray<double>& avg_of_all_rows =* new dynarray<double>();
    avg_of_all_rows.resize(collist.size());

    foreach (i, collist.size()) {
      avg_of_all_rows[i] = sum_of_all_rows[i] / rowlist.size();
    }

    rowlist.push(strdup("Avg"));
    data.push(avg_of_all_rows);
  }

  double leftpad = 10.0;
  double toppad = 5.0;
  double rightpad = 4.0;
  double bottompad = (show_row_labels_in_group) ? 24.0 : 12.0;

  if (title) toppad += 16;

  double maxheight = 0;

  if (scale_relative_to_col < collist.size()) {
    foreach (i, rowlist.size()) {
      double first = data[i][scale_relative_to_col];
      foreach (j, collist.size()) {
        data[i][j] /= first;
        if (config.invert_gains) data[i][j] = 1.0 / data[i][j];
        maxheight = max(maxheight, data[i][j]);
      }
    }
  } else {
    foreach (i, rowlist.size()) {
      foreach (j, collist.size()) {
        //data[i][j] = data[i][j] - 1.0;
        maxheight = max(maxheight, data[i][j]);
      }
    }
  }

  static const double relative_barwidth = 1.0;
  static const double relative_gapwidth = 1.0;

  double rawwidth = (relative_gapwidth * (rowlist.count()-1)) + (relative_barwidth * rowlist.count() * collist.count());
  double xscale = imagewidth / rawwidth;

  double barwidth = relative_gapwidth * xscale;
  double gapwidth = relative_barwidth * xscale;

  /*
  cerr << "maxheight = ", maxheight, endl;
  cerr << "imagewidth = ", imagewidth, endl;
  cerr << "imageheight = ", imageheight, endl;
  cerr << "rawwidth = ", rawwidth, endl;
  cerr << "xscale = ", xscale, endl;
  cerr << "barwidth = ", barwidth, endl;
  cerr << "gapwidth = ", gapwidth, endl;
  */

  SVGCreator svg(os, imagewidth + leftpad + rightpad, imageheight + toppad + bottompad);

  svg.newlayer();

  svg.setoffset(leftpad, toppad);

  //
  // Make horizontal bars and labels
  //
  for (double i = 0; i <= maxheight; i += 0.2) {
    double value = i;
    double y = imageheight - ((i / maxheight)*imageheight);

    svg.strokewidth = 0;
    svg.fill = RGBA(0, 0, 0);
    svg.filled = 1;
    svg.setfont("font-size:6;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial Narrow;text-anchor:right;writing-mode:lr-tb");

    stringbuf sb;
    sb << floatstring(value, 0, 1);
    svg.text(sb, -8.0, y - 1);

    svg.filled = 0;
    svg.strokewidth = 0.5;
    svg.stroke = RGBA(128, 128, 128);
    svg.line(-8.0, y, imagewidth, y);
    svg.strokewidth = 0;
  }

#define CCC 128
  /*
  static const RGBAColor barcolors[] = {
    RGBA(CCC, CCC, CCC, 255), // gray
    RGBA(255, 255, CCC, 255), // yellow
    RGBA(CCC, 255, CCC, 255), // green
    RGBA(CCC, CCC, 255, 255), // blue
    RGBA(255, CCC, CCC, 255), // red
    RGBA(255, CCC, 255, 255), // pink
  };
  */
  static const RGBAColor barcolors[] = {
    RGBA(CCC, CCC, CCC, 255), // gray
    RGBA(255, 128, 128, 255), // yellow
    RGBA(64,  192, 64,  255), // green
    RGBA(64,  64,  128, 255), // blue
  };

#undef CCC
  double x = 0;
  foreach (row, rowlist.count()) {
    svg.strokewidth = 0;
    svg.fill = RGBA(0, 0, 0);
    svg.filled = 1;
    svg.setfont("font-size:8;font-style:bold;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial Narrow;text-anchor:middle;writing-mode:lr-tb");
    svg.text(rowlist[row], x + (barwidth * collist.count())/2, imageheight + ((show_row_labels_in_group) ? 16 : 8));

    foreach (col, collist.count()) {
      svg.strokewidth = 0.5;
      svg.stroke = RGBA(0, 0, 0);
      svg.fill = barcolors[col % lengthof(barcolors)];

      double barheight = (data[row][col] / maxheight) * imageheight;
      svg.rectangle(x, imageheight - barheight, barwidth, barheight);

      if (show_row_labels_in_group) {
        svg.strokewidth = 0;
        svg.fill = RGBA(0, 0, 0);
        svg.filled = 1;
        svg.setfont("font-size:6;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial Narrow;text-anchor:middle;writing-mode:lr-tb");
        svg.text(collist[col], x + barwidth/2, imageheight + 8);
      }

      svg.setfont("font-size:10;font-style:bold;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial Narrow;text-anchor:middle;writing-mode:lr-tb");
      stringbuf label;
      if (false) { // (scale_relative_to_col < collist.count()) {
        svg.strokewidth = 0;
        svg.fill = RGBA(0, 0, 0);
        svg.filled = 1;
        svg.setfont("font-size:6;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;font-family:Arial Narrow;text-anchor:middle;writing-mode:lr-tb");
        label << floatstring(data[row][col], 0, 2);
        svg.text(label, x + barwidth/2, imageheight - barheight - 2.0);
      }
      x += barwidth;
    }
    x += gapwidth;
  }

  svg.exitlayer();
}

int main(int argc, char* argv[]) {
  configparser.setup();
  config.reset();

  argc--; argv++;

  if (!argc) {
    printbanner();
    cerr << "Syntax is:", endl;
    cerr << "  ptlstats [-options] statsfile", endl, endl;
    configparser.printusage(cerr, config);
    return 1;
  }

  int n = configparser.parse(config, argc, argv);

  bool no_args_needed = config.mode_table.set() || config.mode_bargraph.set();

  if ((n < 0) & (!no_args_needed)) {
    printbanner();
    cerr << "ptlstats: Error: no statistics data store filename given", endl, endl;
    cerr << "Syntax is:", endl;
    cerr << "  ptlstats [-options] statsfile", endl, endl;
    configparser.printusage(cerr, config);
    return 1;
  }

  char* filename = (no_args_needed) ? null : argv[n];

  DataStoreNodePrintSettings printinfo;
  printinfo.force_sum_of_subtrees_only = config.show_sum_of_subtrees_only;
  printinfo.maxdepth = config.maxdepth;
  printinfo.percent_digits = config.percent_digits;
  printinfo.percent_of_toplevel = config.percent_of_toplevel;
  printinfo.hide_zero_branches = config.hide_zero_branches;
  printinfo.histogram_thresh = config.histogram_thresh;
  printinfo.cumulative_histogram = config.cumulative_histogram;
  printinfo.show_stars_in_histogram = config.show_stars_in_histogram;

  char* subtract_branch = (config.subtract_branch.set()) ? (char*)config.subtract_branch : null;
  char* snapshot = (config.snapshot.set()) ? (char*)config.snapshot : null;

  StatsFileReader reader;

  if (config.print_datastore_info) {
    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return 2;
    }
    reader.print(cout);
    reader.close();
  } else if (config.print_template) {
    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return 2;
    }
    reader.dst->generate_struct_def(cout);
    reader.close();
  } else if (config.mode_histogram.set()) {
    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return 2;
    }

    DataStoreNode* ds = reader.get(config.snapshot);
    if (!ds) {
      cerr << "ptlstats: Cannot find snapshot '", config.snapshot, "'", endl;
      return 2;
    }

    ds = ds->searchpath(config.mode_histogram);
    
    if (!ds) {
      cerr << "ptlstats: Error: cannot find subtree '", config.mode_histogram, "'", endl;
      return 1;
    }

    if (!ds->histogramarray) {
      cerr << "ptlstats: Error: subtree '", config.mode_histogram, "' is not a histogram array node", endl;
      return 1;
    }
    create_svg_of_histogram_percent_bargraph(cout, *ds, ds->count, config.graph_title, config.graph_width, config.graph_height);
    delete ds;
  } else if (config.mode_collect.set()) {
    argv += n; argc -= n;

    DataStoreNode* supernode = collect_into_supernode(argc, argv, config.mode_collect, subtract_branch, snapshot);
    if (!supernode) return -1;
    supernode->identical_subtrees = 0;
    supernode->print(cout, printinfo);
    delete supernode;
  } else if (config.mode_collect_sum.set()) {
    argv += n; argc -= n;
    DataStoreNode* supernode = collect_into_supernode(argc, argv, config.mode_collect_sum, subtract_branch, snapshot);
    if (!supernode) return -1;
    supernode->identical_subtrees = 1;
    DataStoreNode* sumnode = supernode->sum_of_subtrees();
    sumnode->rename(config.mode_collect_sum);
    sumnode->print(cout, printinfo);
    delete supernode;
  } else if (config.mode_collect_average.set()) {
    argv += n; argc -= n;
    DataStoreNode* supernode = collect_into_supernode(argc, argv, config.mode_collect_average, subtract_branch, snapshot);
    if (!supernode) return -1;
    supernode->identical_subtrees = 1;
    DataStoreNode* avgnode = supernode->average_of_subtrees();
    avgnode->summable = 1;
    avgnode->rename(config.mode_collect_average);
    avgnode->print(cout, printinfo);
    delete supernode;
  } else if (config.mode_table.set()) {
    if ((!config.table_row_names.set()) | (!config.table_col_names.set())) {
      cerr << "ptlstats: Error: must specify both -rows and -cols options for the table mode", endl;
      return 1;
    }

    int tabletype = TABLE_TYPE_TEXT;
    if (strequal(config.table_type_name, "text"))
      tabletype = TABLE_TYPE_TEXT;
    else if (strequal(config.table_type_name, "latex"))
      tabletype = TABLE_TYPE_LATEX;
    else if (strequal(config.table_type_name, "html"))
      tabletype = TABLE_TYPE_HTML;
    else {
      cerr << "ptlstats: Error: unknown table type '", config.table_type_name, "'", endl;
      return 1;
    }

    create_table(cout, tabletype, config.mode_table, config.table_row_names, config.table_col_names,
                 config.table_row_col_pattern, config.table_scale_rel_to_col);
  } else if (config.mode_bargraph.set()) {
    if ((!config.table_row_names) | (!config.table_col_names)) {
      cerr << "ptlstats: Error: must specify both -rows and -cols options for the table mode", endl;
      return 1;
    }

    create_grouped_bargraph(cout, config.mode_bargraph, config.table_row_names, config.table_col_names,
                            config.table_row_col_pattern, config.table_scale_rel_to_col, config.graph_title,
                            config.graph_width, config.graph_height);
  } else if (config.mode_slice.set() || config.mode_slice_graph.set()) {
    bool graphing = config.mode_slice_graph.set();

    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return 2;
    }

    dynarray<char*> colnames;
    colnames.tokenize((graphing) ? config.mode_slice_graph : config.mode_slice, ",");

    if (!graphing) {
      cout << padstring("Snapshot", 16);
      foreach (i, colnames.length) {
        cout << ' ', padstring(colnames[i], 16);
      }
      cout << endl;
    }

    double* xpoints = new double[reader.header.record_count];
    double** ypoints = new double*[colnames.length];

    foreach (col, colnames.length) {
      ypoints[col] = new double[reader.header.record_count];
    }

    W64 basecycle = 0;

    foreach (i, reader.header.record_count) {
      bool do_subtract = ((!config.slice_cumulative) && (i > 0));
      DataStoreNode* dsroot = (do_subtract) ? reader.getdelta(i, i-1) : reader.get(i);

      if (!graphing) cout << intstring(i, 16);

      xpoints[i] = i;

      double sum = 0;

      foreach (col, colnames.length) {
        DataStoreNode* ds = dsroot->searchpath(colnames[col]);

        if (!ds) {
          cerr << "ptlstats: Error: cannot find subtree '", colnames[col], "' in column ", col, endl;
          break;
        }
        
        if (ds->type != DataStoreNode::DS_NODE_TYPE_INT) {
          cerr << "ptlstats: Error: slice '", colnames[col], "' cannot be taken for this node time", endl;
          break;
        }
        
        W64 rawvalue = W64(*ds);
        double value = rawvalue;
        if (isnan(value)) value = 0;
        sum += value;

        /*
        if (graphing) {
          // graph it later
        } else {
          cout << ' ', intstring(rawvalue, 16);
        }
        */
        /*
        if (config.use_percents) {
          if (config.percent_of_toplevel)
            value = (ds->percent_of_toplevel() * 100);
          else value = (ds->percent_of_parent() * 100);
        }
        */
        ypoints[col][i] = value;
      }

      foreach (col, colnames.length) {
        double& value = ypoints[col][i];
        if (config.use_percents) {
          if (sum) value = 100 * (value / sum);
        }
        if (graphing) {
          // graph it later
        } else {
          cout << ' ', floatstring(value, 16, 1);
        }
      }

      if (!graphing) cout << endl;

      DataStoreNode* dscycle = dsroot->searchpath("summary.cycles");
      assert(dscycle);
      basecycle += W64(*dscycle);
      delete dsroot;
    }

    if (graphing) {
      create_svg_of_percentage_line_graph(cout, xpoints, reader.header.record_count, ypoints, colnames.length, colnames,
                                          config.graph_width, config.graph_height, null, graph_background, config.graph_stacked);
    }

    foreach (j, colnames.length) {
      delete[] ypoints[j];
    }

    delete[] xpoints;
  } else {
    if (!reader.open(filename)) {
      cerr << "ptlstats: Cannot open '", filename, "'", endl, endl;
      return 2;
    }

    DataStoreNode* ds = reader.get(snapshot);
    if (!ds) {
      cerr << "ptlstats: Cannot get snapshot '", snapshot, "'", endl, endl;
      reader.close();
      return 1;
    }

    if (config.mode_subtree) {
      ds = ds->searchpath(config.mode_subtree);

      if (!ds) {
        cerr << "ptlstats: Error: cannot find subtree '", config.mode_subtree, "'", endl;
        return 1;
      }
    }

    ds->print(cout, printinfo);
    delete ds;
  }
}
