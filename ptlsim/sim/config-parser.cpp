//
// PTLsim: Cycle Accurate x86-64 Simulator
// Configuration Management
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <config-parser.h>
#include <ptlsim.h>

/* Use to remove escape char passed into configuration string */
static void unescape_string(stringbuf& str)
{
    stringbuf tmp(str.size() + 1);
    int j = 0;

    foreach(i, str.size()) {
        if (str.buf[i] == '\\') continue;
        tmp.buf[j] = str.buf[i];
        j++;
    }
    tmp.buf[j] = '\0';

    str = tmp;
}

ostream& ConfigurationParserBase::printusage(const void* baseptr, ostream& os) const {
  os << "Options are:", endl;
  ConfigurationOption* option = options;
  int maxlength = 0;
  while (option) {
    if likely (option->type != OPTION_TYPE_SECTION) maxlength = max(maxlength, int(strlen(option->name)));
    option = option->next;
  }

  option = options;
  while (option) {
    void* variable = (baseptr) ? ((void*)(option->offset)) : NULL;
    if (option->type == OPTION_TYPE_SECTION) {
      os << option->description, ":", endl;
      option = option->next;
      continue;
    }

    os << "  -", padstring(option->name, -maxlength), " ", option->description, " ";

	if (!variable) {
		option = option->next;
		continue;
	}

    os << "[";
    switch (option->type) {
    case OPTION_TYPE_NONE:
      break;
    case OPTION_TYPE_W64: {
      W64 v = *((W64*)(variable));
      if (v == infinity) os << "inf"; else os << v;
      break;
    }
    case OPTION_TYPE_FLOAT:
      os << *((double*)(variable));
      break;
    case OPTION_TYPE_STRING:
      os << *(stringbuf*)variable;
      break;
    case OPTION_TYPE_BOOL:
      os << ((*(bool*)variable) ? "enabled" : "disabled");
      break;
    default:
      assert(false);
    }
    os << "]", endl;
    option = option->next;
  }
  os << endl;

  return os;
}

int ConfigurationParserBase::parse(void* baseptr, int argc, char* argv[]) {
  int i = 0;

  while (i < argc) {
    if ((argv[i][0] == '-') && strlen(argv[i]) > 1) {
      char* name = &argv[i][1];
      i++;
      bool found = false;

      ConfigurationOption* option = options;
      while (option) {
        if (option->type == OPTION_TYPE_SECTION) {
          option = option->next;
          continue;
        }
        if (strequal(name, option->name)) {
          found = true;
          void* variable = (void*)(option->offset);
          if ((option->type != OPTION_TYPE_NONE) && (option->type != OPTION_TYPE_BOOL) && (i == (argc+1))) {
            cerr << "Warning: missing value for option '", argv[i-1], "'", endl;
            break;
          }
          switch (option->type) {
          case OPTION_TYPE_NONE:
            break;
          case OPTION_TYPE_W64: {
            char* p = (i < argc) ? argv[i] : NULL;
            int len = (p) ? strlen(p) : 0;
            if (!len) {
              cerr << "Warning: option ", argv[i-1], " had no argument; ignoring", endl;
              break;
            }

            char buf[256];
            strncpy(buf, p, sizeof(buf));
            p = buf;

            W64 multiplier = 1;
            char* endp = p;
            bool isinf = (strncmp(p, "inf", 3) == 0);
            if (len > 1) {
              char& c = p[len-1];
              switch (c) {
              case 'k': case 'K':
                multiplier = 1000LL; c = 0; break;
              case 'm': case 'M':
                multiplier = 1000000LL; c = 0; break;
              case 'g': case 'G':
                multiplier = 1000000000LL; c = 0; break;
              case 't': case 'T':
                multiplier = 1000000000000LL; c = 0; break;
              }
            }
            W64 v = (isinf) ? infinity : strtoull(p, &endp, 0);
            if ((!isinf) && (endp[0] != 0)) {
              cerr << "Warning: invalid value '", p, "' for option ", argv[i-1], "; ignoring", endl;
            }
            v *= multiplier;
            *((W64*)variable) = v;
            i++;

            break;
          }
          case OPTION_TYPE_FLOAT:
            if (i >= argc) {
              cerr << "Warning: option ", argv[i-1], " had no argument; ignoring", endl;
              break;
            }
            *((double*)variable) = atof(argv[i++]);
            break;
          case OPTION_TYPE_BOOL:
			*((bool*)variable) = true;
            // *((bool*)variable) = (!(*((bool*)variable)));
            break;
          case OPTION_TYPE_STRING: {
            if (i >= argc) {
              cerr << "Warning: option ", argv[i-1], " had no argument; ignoring", endl;
              break;
            }
            stringbuf& sb = *((stringbuf*)variable);
            sb.reset();
            sb << argv[i++];
            break;
          }
          default:
            assert(false);
          }
          break;
        }

        option = option->next;
      }
      if (!found) {
        cerr << "Warning: invalid option '", (inrange(i-1, 0, argc-1) ? argv[i-1] : "<missing>"), "'", endl;
        i++;
      }
    } else {
      return i; // trailing arguments, if any
    }
  }

  cerr << flush;

  // no trailing arguments
  return -1;
}

int ConfigurationParserBase::parse(void* baseptr, char* argstr) {
	dynarray<char*> argv;
	argv.tokenize(argstr, " ");
	foreach (i, argv.length) {
		// Skip comments
		if (argv[i][0] == '#') {
			argv.resize(i);
			break;
		}
		// add support for quoted strings such as -execute-after-kill "python blah.py"
		if (argv[i][0] == '"') {
			bool found_end_quoted_string = false;
			stringbuf quoted_string;
			int argv_idx;

			for (argv_idx = i; argv_idx < argv.length; argv_idx++) {
				quoted_string << argv[argv_idx] << " ";

				if (argv[argv_idx][strlen(argv[argv_idx])-1] == '"' &&
                        argv[argv_idx][strlen(argv[argv_idx])-2] != '\\') {

                    unescape_string(quoted_string);

					// copy the stringbuf removing the quotes and space at the end
					// FIXME: this probably leaks a very tiny amount of memory
					// since I'm not freeing argv[i]
					argv[i] = strndup(quoted_string.buf+1, quoted_string.size()-3);

					found_end_quoted_string = true;

					if (!argv[i]) {
						exit(-ENOMEM);
					}
					break;
				}

			}
			if (!found_end_quoted_string)
			{
				cerr << "ERROR, could not find end of quoted string starting with " << argv[i] <<"\n", flush;
				exit(-1);
			}

			// scan backwards and remove all nodes that are inside the quoted string (since they are already
			// 	merged into argv[i] )
			for ( ; argv_idx > i; argv_idx--)
			{
				argv.remove(argv[argv_idx]);
			}
		}
	}
	return parse(baseptr, argv.length, argv);
}

ostream& ConfigurationParserBase::print(const void* baseptr, ostream& os) const {
  os << "Active parameters:", endl;

  ConfigurationOption* option = options;
  while (option) {
    void* variable = (baseptr) ? ((void*)(option->offset)) : NULL;

    if (option->type == OPTION_TYPE_SECTION) {
      option = option->next;
      continue;
    }
    os << "  -", padstring(option->name, -12), " ";
    switch (option->type) {
    case OPTION_TYPE_NONE:
    case OPTION_TYPE_SECTION:
      break;
    case OPTION_TYPE_W64: {
      W64 v = *((W64*)(variable));
      if (v == 0) {
        os << 0;
      } else if (v == infinity) {
        os << "infinity";
      } else if ((v % 1000000000LL) == 0) {
        os << (v / 1000000000LL), " G";
      } else if ((v % 1000000LL) == 0) {
        os << (v / 1000000LL), " M";
      } else {
        os << v;
      }
      break;
    }
    case OPTION_TYPE_FLOAT:
      os << *((double*)(variable));
      break;
    case OPTION_TYPE_BOOL:
      os << (*((bool*)(variable)) ? "enabled" : "disabled");
      break;
    case OPTION_TYPE_STRING:
      os << *((stringbuf*)(variable));
      break;
    default:
      break;
    }
    os << endl;

    option = option->next;
  }

  return os;
}

void expand_command_list(dynarray<char*>& list, int argc, char** argv, int depth) {
  dynarray<char*> includes;
  stringbuf line;

  if (depth >= 1024) {
    cerr << "Warning: excessive depth (infinite recursion?) while expanding command list", endl;
    return;
  }

  foreach (i, argc) {
    char* arg = argv[i];
    if (arg[0] == '@') {
      includes.push(arg+1);
    } else if (arg[0] == ':') {
      list.push(strdup(line));
      line.reset();
    } else {
      line << argv[i];
      if (i != (argc-1)) line << " ";
    }
  }

  if (strlen(line)) {
    list.push(strdup(line));
  }

  foreach (i, includes.length) {
    char* listfile = includes[i];
    ifstream is(listfile);
    if (!is) {
      cerr << "Warning: cannot open command list file '", listfile, "'", endl;
      continue;
    }

    for (;;) {
      line.reset();
      is >> line;
      if (!is) break;

      char* p = strchr(line, '#');
      if (p) *p = 0;
      int length = strlen(line);
      bool empty = 1;
      foreach (j, length) {
        empty &= ((line[j] == ' ') | (line[j] == '\t'));
      }
      if (empty) continue;

      expand_command_list(list, line, depth + 1);
    }
  }
}

void expand_command_list(dynarray<char*>& list, char* args, int depth) {
  dynarray<char*> argv;
  char* temp = argv.tokenize(strdup(args), " ");
  expand_command_list(list, argv.length, argv, depth);
  delete temp;
}

void free_command_list(dynarray<char*>& list) {
  foreach (i, list.length) {
    delete list[i];
    list[i] = NULL;
  }
  list.resize(0);
}

struct PTLsimConfig;
ConfigurationParser<PTLsimConfig> config;
void setup_defualt_config() __attribute__((constructor(65535)));

/**
 * @brief Initialize Configuration structure
 *
 * DO NOT CALL THIS FUNCTION. This function is called before 'main'.
 */
void setup_defualt_config()
{
	config.setup();
	config.reset();
}

extern "C" {

/**
 * @brief Add new configuration Section
 *
 * @param name Name of configuration Section
 *
 * Allows modules to add configuration Section before adding any configuration
 * options.  By adding sections it makes easier for users to find specific
 * configuration options quickly.
 */
void marss_add_config_section(const char* name)
{
	config.section(name);
}

/**
 * @brief Add Configuration option of type 'W64'
 *
 * @param field Variable used to hold option value
 * @param name Name of configuration option
 * @param description Description of option used in -help
 */
void marss_add_config_W64(W64& field, const char* name, const char* description)
{
	config.add(field, name, description);
}

/**
 * @brief Add Configuration option of type 'double'
 *
 * @param field Variable used to hold option value
 * @param name Name of configuration option
 * @param description Description of option used in -help
 */
void marss_add_config_double(double& field, const char* name, const char* description)
{
	config.add(field, name, description);
}

/**
 * @brief Add Configuration option of type 'bool'
 *
 * @param field Variable used to hold option value
 * @param name Name of configuration option
 * @param description Description of option used in -help
 */
void marss_add_config_bool(bool& field, const char* name, const char* description)
{
	config.add(field, name, description);
}

/**
 * @brief Add Configuration option of type 'stringbuf'
 *
 * @param field Variable used to hold option value
 * @param name Name of configuration option
 * @param description Description of option used in -help
 */
void marss_add_config_str(stringbuf& field, const char* name, const char* description)
{
	config.add(field, name, description);
}

} // extern "C"
