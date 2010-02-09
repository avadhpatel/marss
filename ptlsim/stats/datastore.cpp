//
// Data Store
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <datastore.h>

using namespace superstl;

#include <math.h>

DataStoreNode* DataStoreNodeLinkManager::objof(selflistlink* link) {
  return baseof(DataStoreNode, hashlink, link);
}

char*& DataStoreNodeLinkManager::keyof(DataStoreNode* obj) {
  return obj->name;
}

selflistlink* DataStoreNodeLinkManager::linkof(DataStoreNode* obj) {
  return &obj->hashlink;
}

DataStoreNode::DataStoreNode() {
  init(null, DS_NODE_TYPE_NULL, 0);
}

DataStoreNode::DataStoreNode(const char* name) {
  init(name, DS_NODE_TYPE_NULL, 1);
}

DataStoreNode::DataStoreNode(const char* name, NodeType type, int count) {
  init(name, type, count);
}

void DataStoreNode::init(const char* name, int type, int count) {
  this->type = type;
  this->name = (name) ? strdup(name) : null;
  this->count = count;
  value.w = 0;
  subnodes = null;
  parent = null;
  summable = 0;
  histogramarray = 0;
  labeled_histogram = 0;
  identical_subtrees = 0;
  histomin = 0;
  histomax = 0;
  histostride = 0;
  dynamic = 0;
  sum_of_subtrees_cache = null;
  average_of_subtrees_cache = null;
  labels = null;
  total_sum_cache = -1;
}

void DataStoreNode::rename(const char* newname) {
  DataStoreNode* oldparent = parent;

  if (oldparent)
    assert(oldparent->remove(name));

  free(name);
  name = strdup(newname);

  if (oldparent) oldparent->add(this);
}

void DataStoreNode::invalidate_caches() {
  if (sum_of_subtrees_cache) delete sum_of_subtrees_cache;
  sum_of_subtrees_cache = null;
  if (average_of_subtrees_cache) delete average_of_subtrees_cache;
  average_of_subtrees_cache = null;
  total_sum_cache = -1;
  if (parent) parent->invalidate_caches();
}

void DataStoreNode::cleanup() {
  invalidate_caches();
  if (type == DS_NODE_TYPE_STRING) {
    if (count > 1) {
      foreach (i, count) {
        delete this->values[i].s;
      }
      delete[] values;
    } else {
      delete this->value.s;
    }
  } else {
    if (count > 1)
      delete[] values;
  }
  if (labels) {
    foreach (i, count) {
      delete labels[i];
    }
    delete[] labels;
  }
}

DataStoreNode::~DataStoreNode() {
  if (parent)
    assert(parent->remove(name));
  cleanup();
  removeall();
  if (subnodes)
    delete subnodes;
  free(name);
  subnodes = null;
  parent = null;
  name = null;
  type = DS_NODE_TYPE_NULL;
  count = 0;
}

DataStoreNode& DataStoreNode::add(DataStoreNode* node) {
  if (!subnodes) subnodes = new hash_t();
  node->parent = this;

  DataStoreNode** oldnode = subnodes->get(node->name);
  if (oldnode) {
    delete *oldnode;
  }

  subnodes->add(node->name, node);

  return *node;
}

bool DataStoreNode::remove(const char* key) {
  if (!subnodes)
    return false;
  return subnodes->remove(key);
}

void DataStoreNode::removeall() {
  DataStoreNodeDirectory& a = getentries();

  foreach (i, a.length) {
    delete a[i].value;
  }

  if (subnodes) assert(subnodes->count == 0);

  delete &a;
}

DataStoreNode::DataType DataStoreNode::getdata() const {
  if (!count) {
    DataType dummy;
    dummy.w = 0;
    return dummy;
  }

  return (count == 1) ? value : values[0];
}

DataStoreNode* DataStoreNode::search(const char* key) const {
  if (!subnodes)
    return null;

  if (strequal(key, "[total]")) {
    return sum_of_subtrees();
  }

  if (strequal(key, "[average]")) {
    return average_of_subtrees();
  }

  DataStoreNode** nodeptr = (*subnodes)(key);
  return (nodeptr) ? *nodeptr : null;
}

DataStoreNode* DataStoreNode::searchpath(const char* path) const {
  dynarray<char*> tokens;

  if (path[0] == '/') path++;

  char* pbase = strdup(path);
  tokens.tokenize(pbase, "/.");

  const DataStoreNode* ds = this;

  foreach (i, tokens.count()) {
    char* p = tokens[i];
    DataStoreNode* dsn = ds->search(p);

    if (!dsn) {
      delete pbase;
      return null;
    }
    ds = dsn;
  }

  free(pbase);

  return (DataStoreNode*)ds;
}

DataStoreNode& DataStoreNode::get(const char* key) {
  DataStoreNode* node = search(key);
  if (node)
    return *node;

  node = new DataStoreNode(key, DS_NODE_TYPE_NULL);
  add(node);
  return *node;
}

//
// Type: null
//

//
// Type: W64s (int)
//

DataStoreNode::DataStoreNode(const char* name, W64s value) {
  init(name, DS_NODE_TYPE_INT, 1);
  this->value.w = value;
}

DataStoreNode::DataStoreNode(const char* name, const W64s* values, int count, bool histogram) {
  init(name, DS_NODE_TYPE_INT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) arraycopy(this->values, (DataType*)values, count);
}

DataStoreNode& DataStoreNode::histogram(const char* key, const W64* value, int count, W64s histomin, W64s histomax, W64s histostride) {
  DataStoreNode& ds = add(key, (W64s*)value, count);
  ds.histogramarray = 1;
  ds.histomin = histomin;
  ds.histomax = histomax;
  ds.histostride = histostride;

  return ds;
}

DataStoreNode& DataStoreNode::operator =(W64s data) {
  cleanup();
  this->type = DS_NODE_TYPE_INT;
  this->value.w = data;
  return *this;
}

DataStoreNode::operator W64s() const {
  switch (type) {
  case DS_NODE_TYPE_INT:
    return getdata().w; break;
  case DS_NODE_TYPE_FLOAT:
    return (W64s)getdata().f; break;
  case DS_NODE_TYPE_STRING:
    return strtoll(getdata().s, (char**)null, 10); break;
  case DS_NODE_TYPE_NULL:
    return 0;
  }
  return 0;
}

DataStoreNode::operator W64s*() const {
  assert(type == DS_NODE_TYPE_INT);
  return (!count) ? null : (count == 1) ? (W64s*)&value : (W64s*)values;
}

DataStoreNode::operator W64*() const {
  return (W64*)(W64s*)(*this);
}

//
// Type: double (float)
//

DataStoreNode::DataStoreNode(const char* name, double data) {
  init(name, DS_NODE_TYPE_FLOAT, 1);
  this->value.f = data;
}

DataStoreNode::DataStoreNode(const char* name, const double* values, int count) {
  init(name, DS_NODE_TYPE_FLOAT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) arraycopy(this->values, (DataType*)values, count);
}

DataStoreNode& DataStoreNode::operator =(double data) {
  cleanup();
  this->type = DS_NODE_TYPE_FLOAT;
  this->value.f = data;
  return *this;
}

DataStoreNode::operator double() const {
  switch (type) {
  case DS_NODE_TYPE_INT:
    return (W64s)getdata().w; break;
  case DS_NODE_TYPE_FLOAT:
    return getdata().f; break;
  case DS_NODE_TYPE_STRING:
    return atof(getdata().s); break;
  case DS_NODE_TYPE_NULL:
    return 0;
  }
  return 0;
}

DataStoreNode::operator double*() const {
  assert(type == DS_NODE_TYPE_FLOAT);
  return (!count) ? null : (count == 1) ? (double*)&value : (double*)values;
}

DataStoreNode::operator float() const {
  return (double)(*this);
}

//
// Type: const char* (string)
//

DataStoreNode::DataStoreNode(const char* name, const char* value) {
  init(name, DS_NODE_TYPE_STRING, 1);
  this->value.s = strdup(value);
}

DataStoreNode::DataStoreNode(const char* name, const char** values, int count) {
  init(name, DS_NODE_TYPE_FLOAT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) {
    foreach (i, count) {
      this->values[i].s = strdup(values[i]);
    }
  }
}

DataStoreNode& DataStoreNode::operator =(const char* data) {
  cleanup();
  this->type = DS_NODE_TYPE_FLOAT;
  this->value.s = strdup(data);
  return *this;
}

const char* DataStoreNode::string() const {
  assert(type == DS_NODE_TYPE_STRING);
  return getdata().s;
}

DataStoreNode::operator const char**() const {
  assert(type == DS_NODE_TYPE_STRING);
  return (!count) ? null : (count == 1) ? (const char**)&value : (const char**)values;
}

DataStoreNodeDirectory& DataStoreNode::getentries() const {
  DataStoreNodeDirectory& dir =* new DataStoreNodeDirectory();
  if (subnodes) subnodes->getentries(dir);
  return dir;
}

double DataStoreNode::total() const {
  if (total_sum_cache > 0) return total_sum_cache;

  double result = (double)(*this);

  DataStoreNodeDirectory& list = getentries();
  foreach (i, list.length) {
    result += list[i].value->total();
  }

  delete &list;

  ((DataStoreNode*)this)->total_sum_cache = result;

  return result;
}

double DataStoreNode::percent_of_parent() const {
  if (!parent) return 0;
  if (parent->subnodes->count == 1) return 1.0;
  return total() / parent->total();
}

double DataStoreNode::percent_of_toplevel() const {
  if (!parent) return 0;

  // Find the toplevel summable node:
  const DataStoreNode* p = this;
  while (p) {
    if (p->parent && p->parent->summable) p = p->parent; else break;
  }

  return total() / p->total();
}

DataStoreNode& DataStoreNode::histogram(const char* key, const char** names, const W64* values, int count) {
  DataStoreNode& ds = histogram(key, values, count, 0, count-1, 1);
  ds.labeled_histogram = 1;
  ds.labels = new char* [count];
  foreach (i, count) {
    ds.labels[i] = strdup(names[i]);
  }
  return ds;
}

static inline int digits(W64 v) {
  stringbuf sb;
  sb << v;
  return strlen(sb);
}

DataStoreNode* DataStoreNode::sum_of_subtrees() const {
  // We can safely override const modifier for caches:
  DataStoreNode* thisdyn = (DataStoreNode*)this;

  if (!sum_of_subtrees_cache) {
    DataStoreNodeDirectory& a = getentries();

    // Only works with this type of node
    if (!identical_subtrees) {
      thisdyn->sum_of_subtrees_cache = new DataStoreNode("invalid");
      sum_of_subtrees_cache->dynamic = 1;
    } else {
      thisdyn->sum_of_subtrees_cache = a[0].value->clone();
      sum_of_subtrees_cache->dynamic = 1;
      sum_of_subtrees_cache->rename("[total]");

      for (int i = 1; i < a.length; i++) {
        (*sum_of_subtrees_cache) += *(a[i].value);
      }
    }
  }

  return sum_of_subtrees_cache;
}


DataStoreNode* DataStoreNode::average_of_subtrees() const {
  // We can safely override const modifier for caches:
  DataStoreNode* thisdyn = (DataStoreNode*)this;

  if (!average_of_subtrees_cache) {
    DataStoreNodeDirectory& a = getentries();

    // Only works with this type of node
    if (!identical_subtrees) {
      thisdyn->average_of_subtrees_cache = new DataStoreNode("invalid");
      average_of_subtrees_cache->dynamic = 1;
    } else {
      double coeff = 1. / a.size();
      thisdyn->average_of_subtrees_cache = a[0].value->map(ScaleOperator(coeff));
      average_of_subtrees_cache->dynamic = 1;
      average_of_subtrees_cache->rename("[average]");

      for (int i = 1; i < a.length; i++) average_of_subtrees_cache->addscaled(*a[i].value, coeff);
    }
  }

  return average_of_subtrees_cache;
}

ostream& DataStoreNode::print(ostream& os, const DataStoreNodePrintSettings& printinfo, int depth, double supersum) const {
  stringbuf padding;
  foreach (i, depth) { padding << "  "; }
  os << padding;

  double selfsum = total();

  if (parent && parent->summable) {
    double p = ((printinfo.percent_of_toplevel) ? percent_of_toplevel() : percent_of_parent()) * 100.0;
    if (p >= 99.999)
      os << "[ ", padstring("100%", 4 + printinfo.percent_digits), " ] ";
    else os << "[ ", floatstring(p, 3 + printinfo.percent_digits, printinfo.percent_digits), "% ] ";
  }

  bool hide_subnodes = 0;

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    os << name;
    break;
  }
  case DS_NODE_TYPE_INT: {
    os << name;
    if (count == 1) {
      os << " = ", value.w, ";";
    } else {
      os << "[", count, "] = {";

      if (histogramarray) {
        hide_subnodes = 1;
        os << endl;

        W64 total = 0;
        W64 maxvalue = 0;
        W64 minvalue = -1ULL;
        W64 weightedsum = 0;
        foreach (i, count) {
          total += values[i].w;
          weightedsum += (values[i].w * i);
          minvalue = min((W64)values[i].w, minvalue);
          maxvalue = max((W64)values[i].w, maxvalue);
        }

        double average = double(weightedsum) / double(total);
        W64 thresh = max((W64)ceil((double)total * printinfo.histogram_thresh), (W64)1);
        W64 base = histomin;
        int width = digits(max(histomin, histomax));
        int valuewidth = digits(maxvalue);
        int w = max(width, valuewidth);

        os << padding, "  ", "Minimum:          ", intstring(minvalue, 12), endl;
        os << padding, "  ", "Maximum:          ", intstring(maxvalue, 12), endl;
        os << padding, "  ", "Average:          ", floatstring(average, 12, 3), endl;
        os << padding, "  ", "Total Sum:        ", intstring(total, 12), endl;
        os << padding, "  ", "Weighted Sum:     ", intstring(weightedsum, 12), endl;
        os << padding, "  ", "Threshold:        ", intstring(thresh, 12), endl;

        W64 accum = 0;

        foreach (i, count) {
          W64 value = (W64)values[i].w;
          accum += value;

          if (value >= thresh) {
            double percent = ((double)value / (double)total) * 100.0;
            double cumulative_percent = ((double)accum / (double)total) * 100.0;
            os << padding, "  [ ", floatstring(percent, 3 + printinfo.percent_digits, printinfo.percent_digits), "% ] ";

            if (labeled_histogram) {
              os << intstring(base, w), " ", intstring(value, w), " ", labels[i], endl;
            } else {
              if (cumulative_percent >= 99.9)
                os << "[ ", padstring("100", 3 + printinfo.percent_digits), "% ] ";
              else os << "[ ", floatstring(cumulative_percent, 3 + printinfo.percent_digits, printinfo.percent_digits), "% ] ";

              os << intstring(base, w), " ",
                intstring(base + (histostride-1), w), " ",
                intstring(value, w);

              if (printinfo.show_stars_in_histogram) {
                os << " ";
                int stars = (int)((percent / 100.0) * 50);
                foreach (i, stars) { os << '*'; }
              }
              os << endl;
            }
          }

          base += histostride;
        }

        os << padding;
      } else {
        foreach (i, count) {
          os << values[i].w;
          if (i != (count-1)) os << ", ";
        }
      }
      os << "};";
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    os << name;
    if (count == 1) {
      os << " = ", value.f, ";";
    } else {
      os << "[", count, "] = {";
      foreach (i, count) {
        os << values[i].f;
        if (i != (count-1)) os << ", ";
      }
      os << "};";
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    os << name;
    if (count == 1) {
      os << " = \"", value.s, "\"", ";";
    } else {
      os << "[", count, "] = {";
      foreach (i, count) {
        os << "\"", values[i].f, "\"";
        if (i != (count-1)) os << ", ";
      }
      os << "};";
    }
    break;
  }
  default:
    assert(false);
  }

  if (depth == printinfo.maxdepth) {
    os << " { ... }", endl;
    return os;
  }

  if ((!selfsum) && printinfo.hide_zero_branches) {
    os << " { (zero) }", endl;
    return os;
  }

  if (subnodes && (!hide_subnodes)) {
    bool isint = ((selfsum - floor(selfsum)) < 0.0001);
    if (summable) {
      os << " (total ";
      if (isint) os << (W64s)selfsum; else os << (double)selfsum; os << ")";
    }
    os << " {", endl;

    DataStoreNodeDirectory& a = getentries();

    if (identical_subtrees) {
      sum_of_subtrees()->print(os, printinfo, depth + 1, 0);

      if (!printinfo.force_sum_of_subtrees_only) {
        foreach (i, a.length) {
          a[i].value->print(os, printinfo, depth + 1, 0);
        }
      }
    } else {
      foreach (i, a.length) {
        a[i].value->print(os, printinfo, depth + 1, (summable) ? selfsum : 0);
      }
    }
    foreach (i, depth) { os << "  "; }
    os << "}";
    delete &a;
  }
  os << endl;
  return os;
}

struct DataStoreNodeArrayHeader {
  W32 count;
  W32 padding;
  W64 histomin;
  W64 histomax;
  W64 histostride;
};

static inline ostream& operator <<(ostream& os, DataStoreNodeArrayHeader& ah) {
//	os.write((char*)(ah.count), sizeof(ah.count));
//	os.write((char*)(ah.padding), sizeof(ah.padding));
//	os.write((char*)(ah.histomin), sizeof(ah.histomin));
//	os.write((char*)(ah.histomax), sizeof(ah.histomax));
//	os.write((char*)(ah.histostride), sizeof(ah.histostride));
	os.write((char*)(&ah), sizeof(ah));
	return os;
}

struct DataStoreNodeHeader {
  W32 magic;
  byte type;
  byte namelength;
  W16 isarray:1, summable:1, histogramarray:1, identical_subtrees:1, labeled_histogram:1;
  W32 subcount;
  // (optional DataStoreNodeArrayInfo iff (isarray == 1)
  // (null-terminated name)
  // (count * sizeof(type) bytes)
  // (all subnodes)
};

DataStoreNode::DataStoreNode(ifstream& is) {
  if (!read(is)) {
    init("INVALID", DS_NODE_TYPE_NULL, 0);
  }
}

static inline ostream& operator <<(ostream& os, DataStoreNodeHeader& dh) {
	os.write((char*)(&dh), sizeof(dh));
	return os;
}

#define DSN_MAGIC_VER_3 0x334c5450 // 'PTL3'

bool DataStoreNode::read(ifstream& is) {
  DataStoreNodeHeader h;
  is >> h;

  // Multiple versions can be supported with different readers

  assert(is);

  if (h.magic != DSN_MAGIC_VER_3) {
    cerr << "DataStoreNode::read(): ERROR: stream does not have proper DSN version 2 header (0x",
      hexstring(h.magic, 32), ") at offset ", is.tellg(), endl, flush;
    return false;
  }

  DataStoreNodeArrayHeader ah;

  if (h.isarray) {
    is >> ah;
    count = ah.count;
    histomin = ah.histomin;
    histomax = ah.histomax;
    histostride = ah.histostride;
  }

  name = new char[h.namelength+1];
  is.read((char*)name, h.namelength+1);
  type = h.type;
  summable = h.summable;
  identical_subtrees = h.identical_subtrees;
  histogramarray = h.histogramarray;
  labeled_histogram = h.labeled_histogram;

  count = (h.isarray) ? ah.count : 1;
  subnodes = null;
  parent = null;

  if (h.isarray & h.histogramarray & h.labeled_histogram) {
    // Read the <count> histogram slot labels
    labels = new char* [count];
    foreach (i, count) {
      W16 ll;
      is >> ll;
      labels[i] = new char[ll + 1];
      is.read(labels[i], ll);
      labels[i][ll] = 0;
    }
  }

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
      is >> value.w;
    } else {
      values = new DataType[count];
      is.read((char*)(values), count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
      is >> value.f;
    } else {
      values = new DataType[count];
      is.read((char*)values, count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      W16 len;
      is >> len;
      value.s = new char[len+1];
      is.read((char*)value.s, len+1);
    } else {
      values = new DataType[count];
      foreach (i, count) {
        W16 len;
        is >> len;
        values[i].s = new char[len+1];
        is.read((char*)values[i].s, len+1);
      }
    }
    break;
  }
  default:
    assert(false);
  }

  foreach (i, h.subcount) {
    add(new DataStoreNode(is));
  }
  return is;
}

ofstream& DataStoreNode::write_bin(ofstream& os, bool bin) const {
  DataStoreNodeHeader h;
  DataStoreNodeArrayHeader ah;

  int namelen = strlen(name);
  assert(namelen < 256);

  h.magic = DSN_MAGIC_VER_3;
  h.type = type;
  h.namelength = (byte)namelen;
  h.histogramarray = histogramarray;
  h.summable = summable;
  h.identical_subtrees = identical_subtrees;
  h.labeled_histogram = labeled_histogram;

  h.isarray = (count > 1);
  if (count > 1) {
    ah.count = count;
    ah.histomin = histomin;
    ah.histomax = histomax;
    ah.histostride = histostride;
  }

  h.subcount = (subnodes) ? subnodes->count : 0;

  os << h;
  if (h.isarray) os << ah;

  os.write(name, h.namelength + 1);

  if (h.isarray & h.histogramarray & h.labeled_histogram) {
    // Write the <count> histogram slot labels
    foreach (i, count) {
      W16 ll = strlen(labels[i]);
      if(bin)
          os << ll;
      else
          os.write((char*)(&ll), sizeof(ll));
      os.write(labels[i], ll);
    }
  }

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
        if(bin)
            os << value.w;
        else
            os.write((char*)(&value.w), sizeof(value.w));
    } else {
      os.write(reinterpret_cast<const char*>(values), count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
        if(bin)
            os << value.f;
        else
            os.write((char*)(&value.f), sizeof(value.f));
    } else {
      os.write(reinterpret_cast<const char*>(values), count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      int len = strlen(value.s);
      assert(len < 65536);
      if(bin)
          os << (W16)len;
      else
          os.write((char*)(&len), sizeof(W16));
      os.write(value.s, len+1);
    } else {
      foreach (i, count) {
        int len = strlen(values[i].s);
        assert(len < 65536);
        if(bin)
            os << (W16)len;
        else
            os.write((char*)(&len), sizeof(W16));
        os.write(reinterpret_cast<const char*>(values[i].s), len+1);
      }
    }
    break;
  }
  default:
    assert(false);
  }

  if (subnodes) {
    DataStoreNodeDirectory& a = getentries();
    foreach (i, a.length) {
      a[i].value->write(os);
    }
    delete &a;
  }
  return os;
}

ostream& DataStoreNode::write(ostream& os, bool bin) const {
  DataStoreNodeHeader h;
  DataStoreNodeArrayHeader ah;

  int namelen = strlen(name);
  assert(namelen < 256);

  h.magic = DSN_MAGIC_VER_3;
  h.type = type;
  h.namelength = (byte)namelen;
  h.histogramarray = histogramarray;
  h.summable = summable;
  h.identical_subtrees = identical_subtrees;
  h.labeled_histogram = labeled_histogram;

  h.isarray = (count > 1);
  if (count > 1) {
    ah.count = count;
    ah.histomin = histomin;
    ah.histomax = histomax;
    ah.histostride = histostride;
  }

  h.subcount = (subnodes) ? subnodes->count : 0;

  os << h;
  if (h.isarray) os << ah;

  os.write(name, h.namelength + 1);

  if (h.isarray & h.histogramarray & h.labeled_histogram) {
    // Write the <count> histogram slot labels
    foreach (i, count) {
      W16 ll = strlen(labels[i]);
      if(bin)
          os << ll;
      else
          os.write((char*)(&ll), sizeof(ll));
      os.write(labels[i], ll);
    }
  }

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
        if(bin)
            os << value.w;
        else
            os.write((char*)(&value.w), sizeof(value.w));
    } else {
      os.write(reinterpret_cast<const char*>(values), count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
        if(bin)
            os << value.f;
        else
            os.write((char*)(&value.f), sizeof(value.f));
    } else {
      os.write(reinterpret_cast<const char*>(values), count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      int len = strlen(value.s);
      assert(len < 65536);
      if(bin)
          os << (W16)len;
      else
          os.write((char*)(&len), sizeof(W16));
      os.write(value.s, len+1);
    } else {
      foreach (i, count) {
        int len = strlen(values[i].s);
        assert(len < 65536);
        if(bin)
            os << (W16)len;
        else
            os.write((char*)(&len), sizeof(W16));
        os.write(reinterpret_cast<const char*>(values[i].s), len+1);
      }
    }
    break;
  }
  default:
    assert(false);
  }

  if (subnodes) {
    DataStoreNodeDirectory& a = getentries();
    foreach (i, a.length) {
      a[i].value->write(os);
    }
    delete &a;
  }
  return os;
}

void DataStoreNodeTemplate::init(const char* name, int type, int count, const char** labels) {
  magic = DataStoreNodeTemplateBase::MAGIC;
  length = sizeof(DataStoreNodeTemplateBase);
  this->name = strdup(name);
  this->type = type;
  this->count = count;

  parent = null;

  subcount = 0;
  summable = 0;
  histogramarray = 0;
  identical_subtrees = 0;
  labeled_histogram = 0;
  this->labels = null;

  histomin = 0;
  histomax = 0;
  histostride = 0;
  limit = 0;

  if (labels) {
    labeled_histogram = 1;
    this->labels = new char*[count];
    foreach (i, count) {
      this->labels[i] = strdup(labels[i]);
    }
  }
}

DataStoreNodeTemplate::DataStoreNodeTemplate(const char* name, int type, int count, const char** labels) {
  init(name, type, count, labels);
}

DataStoreNodeTemplate::DataStoreNodeTemplate(const DataStoreNodeTemplate& base, const char* newname) {
  init((newname) ? newname : base.name, base.type, base.count, (const char**)base.labels);
  summable = base.summable;
  histogramarray = base.histogramarray;
  identical_subtrees = base.identical_subtrees;
  // labeled_histogram inherited automatically
  histomin = base.histomin;
  histomax = base.histomax;
  histostride = base.histostride;
  limit = base.limit;

  foreach (i, base.subnodes.length) {
    add(new DataStoreNodeTemplate(*base.subnodes[i], (const char*)base.subnodes[i]->name));
  }
}

DataStoreNodeTemplate& DataStoreNodeTemplate::add(const char* key, DataStoreNodeTemplate& base) {
  DataStoreNodeTemplate& t =* new DataStoreNodeTemplate(base, key);
  add(t);
  return t;
}

DataStoreNodeTemplate::~DataStoreNodeTemplate() {
  foreach (i, subnodes.length) {
    delete subnodes[i];
  }
  subnodes.clear();
  subcount = 0;
  if (labels) {
    foreach (i, count) { delete[] labels[i]; }
    delete[] labels;
  }
  if (name) delete[] name;
}

//
// Generate a C struct definition for the tree
//
ostream& DataStoreNodeTemplate::generate_struct_def(ostream& os, int depth) const {
  foreach (i, depth) os << "  ";

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    os << "struct ", name, " {";
    if (summable | identical_subtrees) os << " // node:";
    if (summable) os << " summable";
    if (identical_subtrees) os << " identical";
    os << endl;
    break;
  }
  case DS_NODE_TYPE_INT: {
    os << "W64 ", name;
    if (count > 1) os << "[", count, "]";
    os << ";";
    if (labeled_histogram) {
      os << " // label:";
      foreach (i, count) { os << " ", labels[i]; }
    } else if (histogramarray) {
      os << " // histo: ", histomin, " ", histomax, " ", histostride;
    }

    os << endl;
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    os << "double ", name;
    if (count > 1) os << "[", count, "]";
    os << ";", endl;
    break;
  }
  case DS_NODE_TYPE_STRING: {
    os << "char ", name;
    if (count > 1) os << "[", count, "]";
    os << "[", limit, "]";
    os << ";", endl;
    break;
  }
  default:
    assert(false);
  }

  foreach (i, subnodes.length) {
    subnodes[i]->generate_struct_def(os, depth + 1);
  }

  if (type == DS_NODE_TYPE_NULL) {
    foreach (i, depth) os << "  ";
    if (depth) {
      os << "} ", (name ?: "UNKNOWN"), ";", endl;
    } else {
      os << "};", endl;
    }
  }

  return os;
}

//
// Write structural definition in binary format for use by ptlstats:
//
ofstream& DataStoreNodeTemplate::write(ofstream& os) const {
  W16 n;

  os.write((char*)((DataStoreNodeTemplateBase*)this), sizeof(DataStoreNodeTemplateBase));

  n = strlen(name);
  os.write((char*)(&n), sizeof(W16));
  os.write(name, n);

  if (labeled_histogram) {
    foreach (i, count) {
      n = strlen(labels[i]);
      os.write((char*)(&n), sizeof(W16));
//      os << n;
      os.write(labels[i], n);
    }
  }

  assert(subcount == subnodes.length);

  foreach (i, subcount) {
    subnodes[i]->write(os);
  }

  return os;
}

//
// Read structural definition in binary format for use by ptlstats:
//
DataStoreNodeTemplate::DataStoreNodeTemplate(ifstream& is) {
  // Fill in all static fields:
  is.read((char*)(this), sizeof(DataStoreNodeTemplateBase));
  assert(magic == DataStoreNodeTemplateBase::MAGIC);
  assert(length == sizeof(DataStoreNodeTemplateBase));

  parent = null;

  W16 n;
  is >> n; name = new char[n+1]; is.read(name, n); name[n] = 0;

  labels = null;
  if (labeled_histogram) {
    labels = new char*[count];
    foreach (i, count) {
      is >> n; labels[i] = new char[n+1]; is.read(labels[i], n); labels[i][n] = 0;
    }
  }

  subnodes.resize(subcount);

  foreach (i, subcount) {
    subnodes[i] = new DataStoreNodeTemplate(is);
  }
}

//
// Reconstruct a stats tree from its template and an array of words
// representing the tree in depth first traversal order, in a format
// identical to the C struct generated by generate_struct_def()
//
DataStoreNode* DataStoreNodeTemplate::reconstruct(const W64*& p) const {
  DataStoreNode* ds;

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    ds = new DataStoreNode(name);
    ds->summable = summable;
    ds->identical_subtrees = identical_subtrees;
    foreach (i, subnodes.length) {
      ds->add(subnodes[i]->reconstruct(p));
    }
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count > 1) {
      ds = new DataStoreNode(name, (W64s*)p, count);
      ds->histogramarray = histogramarray;
      ds->histomin = histomin;
      ds->histomax = histomax;
      ds->histostride = histostride;
      ds->labeled_histogram = labeled_histogram;

      if (labeled_histogram) {
        ds->labels = new char* [count];
        foreach (i, count) {
          ds->labels[i] = strdup(labels[i]);
          DataStoreNode* subds = new DataStoreNode(labels[i], W64s(p[i]));
          ds->add(subds);
        }
      }

      p += count;
    } else {
      ds = new DataStoreNode(name, *(W64s*)p);
      p++;
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count > 1) {
      ds = new DataStoreNode(name, (double*)p, count);
      p += count;
    } else {
      ds = new DataStoreNode(name, *(double*)p);
      p++;
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count > 1) {
      assert(false); // not supported
      const char** strings = new const char* [count];
      foreach (i, count) {
        strings[i] = (const char*)p;
        p += limit / 8;
      }
      ds = new DataStoreNode(name, strings, count);
    } else {
      ds = new DataStoreNode(name, (const char*)p);
      p = (W64*)(((Waddr)p) + limit);
    }

    break;
  }
  default:
    assert(false);
  }

  return ds;
}

//
// Subtract two arrays of words representing the tree in depth first
// traversal order, in a format identical to the C struct generated
// by generate_struct_def(). The subtraction takes into account
// the actual type (int, double, string) represented by each word in
// the raw data. Subtraction is only done on W64 and double types.
//
void DataStoreNodeTemplate::subtract(W64*& p, W64*& psub) const {
  switch (type) {
  case DS_NODE_TYPE_NULL: {
    foreach (i, subnodes.length) {
      subnodes[i]->subtract(p, psub);
    }
    break;
  }
  case DS_NODE_TYPE_INT: {
    foreach (i, count) p[i] -= psub[i];
    p += count;
    psub += count;
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    foreach (i, count) ((double*)p)[i] -= ((double*)psub)[i];
    p += count;
    psub += count;
    break;
  }
  case DS_NODE_TYPE_STRING: {
    assert(count == 1);
    assert((limit % 8) == 0);
    p += (limit / 8);
    psub += (limit / 8);
    break;
  }
  default:
    assert(false);
  }
}

//
// StatsFileWriter
//
void StatsFileWriter::open(const char* filename, const void* dst, size_t dstsize, int record_size) {
  close();
//  os.open(filename, std::ofstream::binary | std::ofstream::out);
  os.open(filename, std::ios_base::binary | std::ios_base::out);

  namelist = null;

  header.magic = StatsFileHeader::MAGIC;
  header.template_offset = sizeof(StatsFileHeader);
  header.template_size = dstsize;
  header.record_offset = ceil(header.template_offset + header.template_size, PAGE_SIZE);
  header.record_size = record_size;
  header.record_count = 0; // filled in later
  header.index_offset = 0; // filled in later
  header.index_count = 0; // filled in later
  os << header;

  os.seekp(header.template_offset);
  os.write((char*)(dst), dstsize);

  os.seekp(header.record_offset);
}

void StatsFileWriter::write(const void* record, const char* name) {
  if (!os.is_open()) return;

  if (name) {
    StatsIndexRecordLink* link = new StatsIndexRecordLink(next_uuid(), name);
    link->addto((selflistlink*&)namelist);
    header.index_count++;
  }

//  os.write(reinterpret_cast<const char*>(record), header.record_size);
  os.write((char*)(record), header.record_size);
  header.record_count++;
}

void StatsFileWriter::flush() {
  if (!os.is_open()) return;

  header.index_offset = (int)(os.tellp());
  if(header.index_offset == -1) {
	  cerr << "Error in file operation: ", errno, endl;
  }
  assert(header.index_offset == (header.record_offset + (header.record_count * header.record_size)));

  StatsIndexRecordLink* namelink = namelist;
  int n = 0;

  while (namelink) {
//    os << (W64)(namelink->uuid);
	os.write((char*)(&namelink->uuid), sizeof(W64));
    W16 namelen = strlen(namelink->name) + 1;
//    os << namelen;
    os.write((char*)(&namelen), sizeof(W16));
    os.write(namelink->name, namelen);
    namelink = (StatsIndexRecordLink*)namelink->next;
    n++;
  }

  assert(n == header.index_count);

  os.seekp(0);
  os << header;

  assert(os.good());

  os.seekp(header.record_offset + (header.record_count * header.record_size));
}

void StatsFileWriter::close() {
  if (!os.is_open()) return;

  flush();

  StatsIndexRecordLink* namelink = namelist;
  int n = 0;

  while (namelink) {
    StatsIndexRecordLink* next = (StatsIndexRecordLink*)namelink->next;
    namelink->unlink();
    delete namelink->name;
    delete namelink;
    namelink = next;
    n++;
  }

  assert(n == header.index_count);
  namelist = null;

  os.flush();
  os.close();
}

//
// StatsFileReader
//

bool StatsFileReader::open(const char* filename) {
  close();
  is.open(filename, std::ifstream::in | std::ifstream::binary);

  if (!is) {
    cerr << "StatsFileReader: cannot open ", filename, endl;
    return false;
  }

  is >> header;

  if (!is) {
    cerr << "StatsFileReader: error reading header", endl;
    close();
    return false;
  }

  if (header.magic != StatsFileHeader::MAGIC) {
    cerr << "StatsFileReader: header magic or version mismatch", endl;
    close();
    return false;
  }

  buf = new byte[header.record_size];
  bufsub = new byte[header.record_size];

  is.seekg(header.template_offset);
  dst = new DataStoreNodeTemplate(is);

  if ((!is) | (!dst)) {
    cerr << "StatsFileReader: error while reading and parsing template", endl;
    close();
    return false;
  }

  //
  // Read in the index
  //
  is.seekg(header.index_offset);

  foreach (i, header.index_count) {
    W64 uuid = 0;
    is >> uuid;
    assert(is.is_open());
    W16 namelen;
    is >> namelen;
    assert(is.is_open());
    if (namelen) {
      char* name = new char[namelen];
      is.read(name, namelen);
      name_to_uuid.add(name, uuid);
      delete[] name;
    }

    assert(is.is_open());
  }

  return true;
}

DataStoreNode* StatsFileReader::get(W64 uuid) {
  if unlikely (uuid >= header.record_count) return null;
  W64 offset = header.record_offset + (header.record_size * uuid);

  is.seekg(offset);
  is.read((char*)(buf), header.record_size);
  assert(is.good());
  if unlikely (is.gcount() != header.record_size) return null;

  const W64* p = (const W64*)buf;
  DataStoreNode* dsn = dst->reconstruct(p);

  return dsn;
}

DataStoreNode* StatsFileReader::getdelta(W64 uuid, W64 uuidsub) {
  if unlikely (uuid >= header.record_count) return null;
  if unlikely (uuidsub >= header.record_count) return null;
  W64 offset = header.record_offset + (header.record_size * uuid);
  W64 offsetsub = header.record_offset + (header.record_size * uuidsub);

  is.seekg(offset);
  is.read((char*)(buf), header.record_size);
  int size = strlen((char*)(buf));
  if unlikely (size != header.record_size) return null;

  is.seekg(offsetsub);
  is.read((char*)(bufsub), header.record_size);
  size = strlen((char*)(buf));
  if unlikely (size != header.record_size) return null;

  const W64* p = (const W64*)buf;
  W64* porig = (W64*)p;
  W64* psub = (W64*)bufsub;

  dst->subtract(porig, psub);

  DataStoreNode* dsn = dst->reconstruct(p);

  return dsn;
}

W64s StatsFileReader::uuid_of_name(const char* name) {
  bool all_nums = 1;
  W64 id = 0;
  foreach (i, strlen(name)) {
    all_nums &= inrange(name[i], '0', '9');
    id = (id * 10) + (int)(name[i] - '0');
  }

  if unlikely (all_nums) {
    return id;
  }

  W64* uuidp = name_to_uuid(name);
  if unlikely (!uuidp) return -1;

  W64 uuid = *uuidp;

  return uuid;
}

DataStoreNode* StatsFileReader::get(const char* name) {
  W64s uuid = uuid_of_name(name);
  if unlikely (uuid < 0) return null;
  return get(uuid);
}

DataStoreNode* StatsFileReader::getdelta(const char* name, const char* namesub) {
  W64s uuid = uuid_of_name(name);
  W64s uuidsub = uuid_of_name(namesub);
  if unlikely ((uuid < 0) || (uuidsub < 0)) return null;
  return getdelta(uuid, uuidsub);
}

void StatsFileReader::close() {
  if (dst) { delete dst; dst = null; }
  if (buf) { delete[] buf; buf = null; }
  if (bufsub) { delete[] bufsub; bufsub = null; }

  name_to_uuid.clear();

  if (is) is.close();
}

ostream& StatsFileReader::print(ostream& os) const {
  if unlikely (!is.is_open()) {
    os << "Data store is not open", endl;
    return os;
  }

  char magic[9];
  *((W64*)&magic) = header.magic;
  magic[8] = 0;

  os << "Data store header version '", magic, "'", endl;
  os << "  Template at:  ", intstring(header.template_offset, 16), ", ", intstring(header.template_size, 16), " bytes", endl;
  os << "  Records at:   ", intstring(header.record_offset, 16), ", ", intstring(header.record_size, 16), " bytes", endl;
  os << "  Index at:     ", intstring(header.index_offset, 16), ", ", intstring(header.index_count, 16), " entries", endl;
  os << "  Record count: ", intstring(header.record_count, 16), " records", endl;
  os << endl;
  os << "Index:", endl;
  os << name_to_uuid;
  os << endl;

  return os;
}
