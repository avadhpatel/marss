// -*- c++ -*-
//
// Copyright 2005-2008 Matt T. Yourst <yourst@yourst.com>
//
// This program is free software; it is licensed under the
// GNU General Public License, Version 2.
//
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <globals.h>
#include <superstl.h>
#include <stdarg.h>

static const W64 infinity = limits<W64s>::max;

struct ConfigurationOption {
  const char* name;
  const char* description;
  int type;
  int fieldsize;
  Waddr offset;

  ConfigurationOption* next;

  ConfigurationOption() { }

  ConfigurationOption(const char* name, const char* description, int type, Waddr offset, int fieldsize = 0) {
    this->name = name;
    this->description = description;
    this->type = type;
    this->offset = offset;
    this->fieldsize = fieldsize;
    this->next = null;
  }
};

enum {
  OPTION_TYPE_NONE    = 0,
  OPTION_TYPE_W64     = 1,
  OPTION_TYPE_FLOAT   = 2,
  OPTION_TYPE_STRING  = 3,
  OPTION_TYPE_TRAILER = 4,
  OPTION_TYPE_BOOL    = 5,
  OPTION_TYPE_SECTION = -1
};

struct ConfigurationParserBase {
  ConfigurationOption* options;
  ConfigurationOption* lastoption;

  void addentry(void* baseptr, void* field, int type, const char* name, const char* description) {
    Waddr offset = ((Waddr)field) - ((Waddr)baseptr);
    ConfigurationOption* option = new ConfigurationOption(name, description, type, offset);
    if (lastoption) lastoption->next = option;
    if (!options) options = option;
    lastoption = option;
  }

  ConfigurationParserBase() { options = null; lastoption = null; }

  int parse(void* baseptr, int argc, char* argv[]);
  int parse(void* baseptr, char* argstr);
  ostream& printusage(const void* baseptr, ostream& os) const;
  ostream& print(const void* baseptr, ostream& os) const;
};

template <typename T>
struct ConfigurationParser: public T {
  ConfigurationParserBase options;

  ConfigurationParser() { }

  void add(W64& field, const char* name, const char* description) {
    options.addentry(this, &field, OPTION_TYPE_W64, name, description);
  }

  void add(double& field, const char* name, const char* description) {
    options.addentry(this, &field, OPTION_TYPE_FLOAT, name, description);
  }

  void add(bool& field, const char* name, const char* description) {
    options.addentry(this, &field, OPTION_TYPE_BOOL, name, description);
  }

  void add(stringbuf& field, const char* name, const char* description) {
    options.addentry(this, field, OPTION_TYPE_STRING, name, description);
  }

  void section(const char* name) {
    options.addentry(this, null, OPTION_TYPE_SECTION, name, name);
  }

  int parse(T& config, int argc, char* argv[]) {
    return options.parse(&config, argc, argv);
  }

  int parse(T& config, char* argstr) {
    return options.parse(&config, argstr);
  }

  ostream& print(ostream& os, const T& config) {
    return options.print(&config, os);
  }

  ostream& printusage(ostream& os, const T& config) {
    return options.printusage(&config, os);
  }

  // Provided by user:
  void setup();
};

void expand_command_list(dynarray<char*>& list, int argc, char** argv, int depth = 0);
void expand_command_list(dynarray<char*>& list, char* args, int depth = 0);
void free_command_list(dynarray<char*>& list);

#endif // _CONFIG_H_
