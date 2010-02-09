// -*- c++ -*-
//
// Data Store
//
// Copyright 2005-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <superstl.h>

#ifndef _DATASTORE_H_
#define _DATASTORE_H_

struct DataStoreNode;

typedef dynarray< KeyValuePair<const char*, DataStoreNode*> > DataStoreNodeDirectory;

struct DataStoreNode;

struct DataStoreNodePrintSettings {
  int force_sum_of_subtrees_only:1, percent_of_toplevel:1, hide_zero_branches:1, cumulative_histogram:1, show_stars_in_histogram:1;
  int maxdepth;
  int percent_digits;
  float histogram_thresh;

  DataStoreNodePrintSettings() {
    force_sum_of_subtrees_only = 0;
    maxdepth = limits<int>::max;
    percent_digits = 0;
    percent_of_toplevel = 0;
    hide_zero_branches = 0;
    histogram_thresh = 0.0001;
    cumulative_histogram = 0;
    show_stars_in_histogram = 1;
  }
};

#define DeclareOperator(name, expr) \
  struct name { \
    W64s operator ()(W64s a, W64s b) const { return (expr); } \
    double operator ()(double a, double b) const { return (expr); } \
  }

DeclareOperator(AddOperator, (a + b));
DeclareOperator(SubtractOperator, (a - b));

struct AddScaleOperator {
  typedef double context_t;
  context_t coeff;
  AddScaleOperator(double coeff_): coeff(coeff_) { };
  W64s operator ()(W64s a, W64s b) const { return (W64s)round((double)a + ((double)b * coeff)); }
  double operator ()(double a, double b) const { return a + b*coeff; }
};

#undef DeclareOperator

//
// Unary operations
//
#define DeclareOperator(name, expr) \
  struct name { \
    W64s operator ()(W64s a) const { return (expr); } \
    double operator ()(double a) const { return (expr); } \
  }

DeclareOperator(IdentityOperator, (a));
DeclareOperator(ZeroOperator, (0));

struct ScaleOperator {
  double coeff;
  ScaleOperator(double coeff_): coeff(coeff_) { };
  W64s operator ()(W64s a) const { return (W64s)round(((double)a) * coeff); }
  double operator ()(double a) const { return a * coeff; }
};

#undef DeclareOperator

struct DataStoreNodeLinkManager {
  static DataStoreNode* objof(selflistlink* link);
  static char*& keyof(DataStoreNode* obj);
  static selflistlink* linkof(DataStoreNode* obj);
};

struct DataStoreNode {
  typedef Hashtable<const char*, DataStoreNode*, 16> hash_t;
  selflistlink hashlink;
  hash_t* subnodes;
  char* name;
  DataStoreNode* sum_of_subtrees_cache;
  DataStoreNode* average_of_subtrees_cache;
  double total_sum_cache;
  char** labels;

  W16 type;
  W16 summable:1, histogramarray:1, identical_subtrees:1, dynamic:1, labeled_histogram:1;
  W32 count;

  // For nodes with an array style histogram:
  W64 histomin;       // minslot
  W64 histomax;       // maxslot
  W64 histostride;    // real units per histogram slot

  DataStoreNode* parent;

  enum NodeType { DS_NODE_TYPE_NULL, DS_NODE_TYPE_INT, DS_NODE_TYPE_FLOAT, DS_NODE_TYPE_NODE, DS_NODE_TYPE_STRING };

  union DataType {
    W64s w;
    double f;
    const char* s;
    DataStoreNode* n;
  };

  union {
    DataType* values;
    DataType value;
  };

  DataStoreNode();
  DataStoreNode(const char* name);
  DataStoreNode(const char* name, NodeType type, int count = 0);

  void init(const char* name, int type, int count = 0);
  void rename(const char* newname);

  ~DataStoreNode();
  void cleanup();

  DataStoreNode& add(DataStoreNode* node);
  DataStoreNode& add(DataStoreNode& node) { return add(&node); }

  bool remove(const char* key);

  void removeall();

  DataType getdata() const;

  void invalidate_caches();

  DataStoreNode* search(const char* key) const;

  DataStoreNode* searchpath(const char* path) const;

  DataStoreNode& get(const char* key);

  DataStoreNode& operator ()(const char* key) { return get(key); }

  DataStoreNode& operator [](const char* key) { return get(key); }

  //
  // Type: null
  //

  DataStoreNode& add(const char* key) { return add(new DataStoreNode(key)); }

  //
  // Type: W64s (int)
  //

  DataStoreNode(const char* name, W64s value);
  DataStoreNode(const char* name, const W64s* values, int count, bool histogram = false);

  DataStoreNode& add(const char* key, W64s value) { return add(new DataStoreNode(key, (W64s)value)); }

  DataStoreNode& add(const char* key, const W64s* value, int count) { return add(new DataStoreNode(key, (W64s*)value, count)); }

  DataStoreNode& histogram(const char* key, const W64* value, int count, W64s histomin, W64s histomax, W64s histostride);

  DataStoreNode& histogram(const char* key, const W64* value, int count) {
    return histogram(key, value, count, 0, count-1, 1);
  }

  DataStoreNode& histogram(const char* key, const char** names, const W64* values, int count);

  operator W64s() const;

  operator W64() const { return (W64s)(*this); }
  operator W32s() const { return (W64s)(*this); }
  operator W32() const { return (W64s)(*this); }
  operator W16s() const { return (W64s)(*this); }
  operator W16() const { return (W64s)(*this); }
  operator byte() const { return (W64s)(*this); }
  operator W8s() const { return (W64s)(*this); }

  operator W64s*() const;
  operator W64*() const;

  DataStoreNode& operator =(W64s data);

  //
  // Type: double (float)
  //

  DataStoreNode(const char* name, double value);
  DataStoreNode(const char* name, const double* values, int count);

  DataStoreNode& addfloat(const char* key, double value) { return add(new DataStoreNode(key, (double)value)); }
  DataStoreNode& addfloat(const char* key, double* value, int count) { return add(new DataStoreNode(key, (double*)value, count)); }

  operator double() const;
  operator double*() const;
  operator float() const;

  DataStoreNode& operator =(double data);

  //
  // Type: const char* (string)
  //

  DataStoreNode(const char* name, const char* value);
  DataStoreNode(const char* name, const char** values, int count);

  DataStoreNode& add(const char* key, const char* value) { return add(new DataStoreNode(key, (const char*)value)); }
  DataStoreNode& add(const char* key, const char** value, int count) { return add(new DataStoreNode(key, (const char**)value, count)); }

  DataStoreNode& operator =(const char* data);

  const char* string() const;

  operator const char**() const;

  DataStoreNodeDirectory& getentries() const;

  double total() const;
  double percent_of_parent() const;
  double percent_of_toplevel() const;

  ostream& print(ostream& os, const DataStoreNodePrintSettings& printinfo = DataStoreNodePrintSettings(), int depth = 0, double supersum = 0) const;

  DataStoreNode(ifstream& is);

  bool read(ifstream& is);

  ostream& write(ostream& os, bool bin=0) const;
  ofstream& write_bin(ofstream& os, bool bin=0) const;

  ostream& generate_structural_code(ostream& os, int level = 0) const;
  ostream& generate_reconstruction_code(ostream& os, int level = 0) const;

  template <class F>
  DataStoreNode* map(const F& func) const {
    DataStoreNode* newnode = null;

    switch (type) {
    case DataStoreNode::DS_NODE_TYPE_NULL: {
      newnode = new DataStoreNode(name);
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_INT: {
      if (count == 1) {
        newnode = new DataStoreNode(name, func(value.w));
      } else {
        W64s* destdata = new W64s[count];
        foreach (i, count) destdata[i] = func(values[i].w);
        newnode = new DataStoreNode(name, destdata, count);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_FLOAT: {
      if (count == 1) {
        newnode = new DataStoreNode(name, func(value.f));
      } else {
        double* destdata = new double[count];
        foreach (i, count) destdata[i] = func(values[i].f);
        newnode = new DataStoreNode(name, destdata, count);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_STRING: {
      if (count == 1) {
        newnode = new DataStoreNode(name, string());
      } else {
        newnode = new DataStoreNode(name, (const char**)(*this), count);
      }
      break;
    }
    default:
      assert(false);
    }

    newnode->summable = summable;
    newnode->histogramarray = histogramarray;
    newnode->identical_subtrees = identical_subtrees;

    newnode->histomin = histomin;
    newnode->histomax = histomax;
    newnode->histostride = histostride;

    DataStoreNodeDirectory& list = getentries();

    foreach (i, list.length) {
      newnode->add(list[i].value->map(func));
    }

    delete &list;

    return newnode;
  }

  DataStoreNode* clone() const { return map(IdentityOperator()); }
  DataStoreNode* zero() const { return map(ZeroOperator()); }

  // NOTE: These results cannot be freed: dynamic cached subtree only
  DataStoreNode* sum_of_subtrees() const;
  DataStoreNode* average_of_subtrees() const;

  template <class F>
  static DataStoreNode* apply(const F& func, const DataStoreNode& a, const DataStoreNode& b) {
    DataStoreNode* newnode = null;

    if (!((a.type == b.type) & (a.count == b.count))) {
      cerr << "DataStoreNode::apply(", a.name, ", ", b.name, "): mismatch types (", a.type, " vs ", b.type, "), count (", a.count, " vs ", b.count, ")", endl, flush;
      assert(false);
    }

    switch (a.type) {
    case DataStoreNode::DS_NODE_TYPE_NULL: {
      newnode = new DataStoreNode(a.name);
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_INT: {
      if (a.count == 1) {
        newnode = new DataStoreNode(a.name, func(a.value.w, b.value.w));
      } else {
        W64s* destdata = new W64s[a.count];
        W64s* adata = a;
        W64s* bdata = b;
        foreach (i, a.count) destdata[i] = func(adata[i], bdata[i]);
        newnode = new DataStoreNode(a.name, destdata, a.count);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_FLOAT: {
      if (a.count == 1) {
        newnode = new DataStoreNode(a.name, func(a.value.f, b.value.f));
      } else {
        double* destdata = new double[a.count];
        double* adata = a;
        double* bdata = b;
        foreach (i, a.count) destdata[i] = func(adata[i], bdata[i]);
        newnode = new DataStoreNode(a.name, destdata, a.count);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_STRING: {
      if (a.count == 1) {
        newnode = new DataStoreNode(a.name, b.string());
      } else {
        newnode = new DataStoreNode(a.name, (const char**)b, b.count);
      }
      break;
    }
    default:
      assert(false);
    }

    newnode->summable = a.summable;
    newnode->histogramarray = a.histogramarray;
    newnode->identical_subtrees = a.identical_subtrees;

    newnode->histomin = a.histomin;
    newnode->histomax = a.histomax;
    newnode->histostride = a.histostride;

    DataStoreNodeDirectory& alist = a.getentries();
    DataStoreNodeDirectory& blist = b.getentries();

    if (alist.length != blist.length) {
      cerr << "DataStoreNode::apply(", a.name, ", ", b.name, "): mismatch in subnode list length (", alist.length, " vs ", blist.length, ")", endl, flush;
      assert(alist.length == blist.length);
    }

    foreach (i, alist.length) {
      DataStoreNode& anode = *a.search(alist[i].key);
      DataStoreNode& bnode = *b.search(blist[i].key);
      assert(&anode); assert(&bnode);
      newnode->add(apply(func, anode, bnode));
    }

    delete &alist;
    delete &blist;

    return newnode;
  }

  template <class F>
  DataStoreNode& apply(const F& func, const DataStoreNode& b) {
    if (!((type == b.type) & (count == b.count))) {
      cerr << "DataStoreNode::apply(", name, ", ", b.name, "): mismatch types (", type, " vs ", b.type, "), count (", count, " vs ", b.count, ")", endl, flush;
      assert(false);
    }

    switch (type) {
    case DataStoreNode::DS_NODE_TYPE_NULL: {
      // No action
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_INT: {
      if (count == 1) {
        value.w = func(value.w, b.value.w);
      } else {
        foreach (i, count) values[i].w = func(values[i].w, b.values[i].w);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_FLOAT: {
      if (count == 1) {
        value.f = func(value.f, b.value.f);
      } else {
        foreach (i, count) values[i].f = func(values[i].f, b.values[i].f);
      }
      break;
    }
    case DataStoreNode::DS_NODE_TYPE_STRING: {
      // Leave strings alone
      break;
    }
    default:
      assert(false);
    }

    DataStoreNodeDirectory& alist = getentries();
    DataStoreNodeDirectory& blist = b.getentries();

    if (alist.length != blist.length) {
      cerr << "DataStoreNode::apply(", name, ", ", b.name, "): mismatch in subnode list length (", alist.length, " vs ", blist.length, ")", endl, flush;
      assert(alist.length == blist.length);
    }

    foreach (i, alist.length) {
      DataStoreNode& anode = *search(alist[i].key);
      DataStoreNode& bnode = *b.search(blist[i].key);
      assert(&anode); assert(&bnode);
      anode.apply(func, bnode);
    }

    delete &alist;
    delete &blist;

    return *this;
  }

  DataStoreNode* operator +(const DataStoreNode& b) const {
    return apply(AddOperator(), *this, b);
  }

  DataStoreNode* operator -(const DataStoreNode& b) const {
    return apply(SubtractOperator(), *this, b);
  }

  DataStoreNode& operator +=(const DataStoreNode& b) {
    return apply(AddOperator(), b);
  }

  DataStoreNode& addscaled(const DataStoreNode& b, double scale) {
    return apply(AddScaleOperator(scale), b);
  }

  DataStoreNode& operator -=(const DataStoreNode& b) {
    return apply(SubtractOperator(), b);
  }
};

//inline ofstream& operator <<(ofstream& os, const DataStoreNode& node) {
//  return node.write_bin(os, 1);
//}

inline ostream& operator <<(ostream& os, const DataStoreNode& node) {
  return node.write(os, 1);
}

//
// Data store node templates provide a means for storing
// and retrieving symbolic type information about an opaque
// structure representing a tree.
//
// When writing a data store tree to a file, it's preferable
// to store the template only once, then write the C structure
// representing the entire tree in one operation.
//
// This is much more suitable for embedded systems use, since
// the template can be "pre-compiled" and written out verbatim
// at the start of every stream. Then, the caller can simply
// update a normal C structure previously generated by
// generate_struct_def() while knowing nothing about the
// various DataStoreNode classes.
//

struct DataStoreNodeTemplateBase {
  static const W32 MAGIC = 0x31545344; // 'DST1'

  W32 magic; // node descriptor magic number and version
  W16 length; // length of this structure
  W16 type; // node type
  W32 histogramarray:1, labeled_histogram:1, summable:1, identical_subtrees:1;
  W32 subcount; // number of subnodes in tree
  W32 count; // element count in this node
  W32 limit; // length limit in bytes, for char strings
  // For nodes with an array or histogram:
  W64 histomin;       // minslot
  W64 histomax;       // maxslot
  W64 histostride;    // real units per histogram slot
};

struct DataStoreNodeTemplate: public DataStoreNodeTemplateBase {
  char* name;
  dynarray<DataStoreNodeTemplate*> subnodes;
  char** labels;
  DataStoreNodeTemplate* parent;

  enum NodeType { DS_NODE_TYPE_NULL, DS_NODE_TYPE_INT, DS_NODE_TYPE_FLOAT, DS_NODE_TYPE_NODE, DS_NODE_TYPE_STRING, DS_NODE_TYPE_LABELED_HISTOGRAM };

  void init(const char* name, int type = DS_NODE_TYPE_NULL, int count = 1, const char** labels = null);

  DataStoreNodeTemplate() { name = null; labels = null; parent = null; }

  DataStoreNodeTemplate(const char* name, int type = DS_NODE_TYPE_NULL, int count = 1, const char** labels = null);

  DataStoreNodeTemplate(const DataStoreNodeTemplate& base, const char* name = null);

  ~DataStoreNodeTemplate();

  DataStoreNodeTemplate& add(DataStoreNodeTemplate* node) {
    node->parent = this;
    subnodes.push(node);
    subcount++;
    return *node;
  }

  DataStoreNodeTemplate& add(DataStoreNodeTemplate& node) { return add(&node); }

  // Simple null container node
  DataStoreNodeTemplate& add(const char* key) { return add(new DataStoreNodeTemplate(key, DS_NODE_TYPE_NULL)); }
  DataStoreNodeTemplate& operator ()(const char* key) { return add(key); }

  // Cloned template node
  DataStoreNodeTemplate& add(const char* key, DataStoreNodeTemplate& base);

  // Integer
  DataStoreNodeTemplate& addint(const char* key, int count = 1) { return add(new DataStoreNodeTemplate(key, DS_NODE_TYPE_INT, count)); }

  // Integer histogram
  DataStoreNodeTemplate& histogram(const char* key, int count, W64 histomin, W64 histomax, W64 histostride, const char** names = null) {
    DataStoreNodeTemplate& dsn = add(new DataStoreNodeTemplate(key, DS_NODE_TYPE_INT, count, names));
    dsn.histogramarray = 1;
    dsn.histomin = histomin;
    dsn.histomax = histomax;
    dsn.histostride = histostride;
    return dsn;
  }

  // Integer histogram, simplified
  DataStoreNodeTemplate& histogram(const char* key, int count, const char** names = null) {
    return histogram(key, count, 0, count-1, 1, names);
  }

  // Float
  DataStoreNodeTemplate& addfloat(const char* key, int count = 1) { return add(new DataStoreNodeTemplate(key, DS_NODE_TYPE_FLOAT, count)); }

  // String
  DataStoreNodeTemplate& addstring(const char* key, int limit, int count = 1) {
    DataStoreNodeTemplate& dsn = add(new DataStoreNodeTemplate(key, DS_NODE_TYPE_STRING));
    // Round up to next largest size to keep everything aligned to 64-bit boundaries
    dsn.limit = ceil(limit, 8);
    return dsn;
  }

  //
  // Generate a C struct definition for the tree
  //
  ostream& generate_struct_def(ostream& os, int depth = 0) const;

  //
  // Write structural definition in binary format for use by ptlstats:
  //
  ofstream& write(ofstream& os) const;

  //
  // Read structural definition in binary format for use by ptlstats:
  //
  DataStoreNodeTemplate(ifstream& is);

  //
  // Reconstruct a stats tree from its template and an array of words
  // representing the tree in depth first traversal order, in a format
  // identical to the C struct generated by generate_struct_def()
  //
  DataStoreNode* reconstruct(const W64*& p) const;

  //
  // Subtract two arrays of words representing the tree in depth first
  // traversal order, in a format identical to the C struct generated
  // by generate_struct_def(). The subtraction takes into account
  // the actual type (int, double, string) represented by each word in
  // the raw data. Subtraction is only done on W64 and double types.
  //
  void subtract(W64*& p, W64*& psub) const;
};

//static inline ofstream& operator <<(ofstream& os, const DataStoreNodeTemplate& node) {
//  return node.write(os);
//}

static inline ostream& operator <<(ostream& os, const DataStoreNodeTemplate& node) {
  return node.generate_struct_def(os);
}

struct StatsFileHeader {
  W64 magic;
  W64 template_offset;
  W64 template_size;
  W64 record_offset;
  W64 record_size;
  W64 record_count;
  W64 index_offset;
  W64 index_count;

  static const W64 MAGIC = 0x31307473644c5450ULL; // 'PTLdst01'
};

static inline ostream& operator <<(ostream& os, StatsFileHeader& sh) {
	return os.write((char*)(&sh), sizeof(sh));
}

struct StatsIndexRecordLink: public selflistlink {
  W64 uuid;
  char* name;

  StatsIndexRecordLink() { }

  StatsIndexRecordLink(W64 uuid, const char* name) {
    this->uuid = uuid;
    this->name = strdup(name);
  }
};

struct StatsFileWriter {
  ofstream os;
  StatsFileHeader header;
  StatsIndexRecordLink* namelist;

  StatsFileWriter() { }

  void open(const char* filename, const void* dst, size_t dstsize, int record_size);

  operator bool() const { return os.is_open(); }
  W64 next_uuid() const { return header.record_count; }

  void write(const void* record, const char* name = null);
  void flush();
  void close();
};

struct StatsFileReader {
  ifstream is;
  StatsFileHeader header;
  byte* buf;
  byte* bufsub;
  DataStoreNodeTemplate* dst;
  Hashtable<const char*, W64, 256> name_to_uuid;

  StatsFileReader() { dst = null; buf = null; bufsub = null; }

  bool open(const char* filename);

  void close();

  W64s uuid_of_name(const char* name);

  DataStoreNode* get(W64 uuid);
  DataStoreNode* getdelta(W64 uuid, W64 uuidsub);

  DataStoreNode* get(const char* name);
  DataStoreNode* getdelta(const char* name, const char* namesub);

  ostream& print(ostream& os) const;
};

static inline ostream& print(ostream& os, const StatsFileReader& reader) {
  return reader.print(os);
}

#endif // _DATASTORE_H_
