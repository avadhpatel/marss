
# config_helper.py
#
# Provide support to SCons to parse/setup Marss configuration via config files

# First check if system has yaml installed then use that

try:
    import yaml
except (ImportError, NotImplementedError):
    import sys
    sys.path.append("ptlsim/lib/python")
    import yaml

try:
    from yaml import CLoader as Loader
except:
    from yaml import Loader

import os
import sys
import copy

# Module local variables and functions
_required_cache_params = [
        'SIZE', 'LINE_SIZE', 'ASSOC', 'LATENCY', 'READ_PORTS', 'WRITE_PORTS']
_required_mem_params = ['LATENCY']
_required_keys = {
        'config': ['core', 'cache', 'machine', 'memory'],
        'core': ['base'],
        'cache': ['base', { 'params' : _required_cache_params } ],
        'memory': ['base'],
        'machine': ['description', 'min_contexts',
            'cores', 'caches', 'interconnects'],
        }
_debug_t = False
_top_conf_file = ""

_files_parsed = []

def parse_config(config_file="config", debug=False):
    '''Parse provided configuration file and return a dict with all
    configuration parameters read from the file.
    '''
    global _debug_t,_top_conf_file
    _debug_t = debug
    _top_conf_file = config_file

    if os.path.isfile(config_file):
        config = _parse_file(config_file)
    elif os.path.isdir(config_file):
        config = _parse_dir(config_file)
    _check_required_key(_required_keys, 'config', config, config_file)
    _merge_params(config)
    _debug(yaml.dump(config))

    return config

def save_config(config_filename, config):
    '''Save given configuration into dump_file as yaml format'''
    if not os.path.exists(os.path.dirname(config_filename)):
        os.mkdir(os.path.dirname(config_filename))
    with open(config_filename, 'w') as config_file:
        config_file.write(yaml.dump(config))

    _debug("Written configuration in : %s" % config_filename)

def _error(string, flname=None):
    if not flname: flname = _top_conf_file
    string = "[ERROR] [CONFIG_PARSER] [File:%s] %s" % (flname, string)
    print(string)
    sys.exit(-1)

def _debug(string):
    if _debug_t:
        string = "[CONFIG_PARSER] %s" % string
        print(string)

def _check_config(config):
    '''Check config if it has all required parameters like
    core, cache and machine configuration'''
    for key in _required_keys['config']:
        if not config[key]:
            _error("Unable to find any '%s' configuration. Aborting.." %
                    key)

def _check_required_key(req_objs, obj_key, obj, filename, objName=None):
    req_obj = req_objs[obj_key]
    for key in req_obj:
        if type(key) == dict:
            for key1,val1 in key.items():
                _check_required_key(key, key1, obj[key1], filename, objName)
        elif not obj.has_key(key):
            errmsg = "\nERROR:\n"
            errmsg += "\tFile: %s\n" % filename
            errmsg += "\tConfiguration: %s\n" % obj_key
            if objName:
                errmsg += "\tObject: %s\n" % objName
            errmsg += "\tis Missing required key: %s" % key
            _error(errmsg)
        elif not obj[key]:
            errmsg = "\nERROR:\n"
            errmsg += "\tFile: %s\n" % filename
            errmsg += "\tConfiguration: %s\n" % obj_key
            if objName:
                errmsg += "\tObject: %s\n" % objName
            errmsg += "\tis Missing required value for key: %s" % key
            _error(errmsg)

def _parse_file(filename,
        config={'core': {}, 'cache': {}, 'machine': {}, 'memory': {}}):
    '''Parse given YAML file.'''

    if filename in _files_parsed:
        return config
    else:
        _files_parsed.append(filename)

    _debug("Reading config file %s" % filename)
    try:
        with open(filename, 'r') as fl:
            for doc in yaml.load_all(fl):
                if doc.has_key('import'):
                    [_parse_file(_full_filename(filename, import_file), config)
                            for import_file in doc['import']]
                _merge_docs(config, doc, filename)
    except IOError, e:
        _error("Unable to read config file: %s, Exception %s" % (
            str(filename), str(e)))

    return config

def _parse_dir(dirname,
        config={'core': {}, 'cache': {}, 'machine': {}, 'memory': {}}):
    _debug("Traversing config directory %s" % dirname)

    dirList=os.listdir(dirname)
    for fname in dirList:
        ext = os.path.splitext(fname)
        if len(ext) > 1 and ext[1] == ".conf" and \
                fname not in _files_parsed:
            config = _parse_file("%s/%s" % (dirname, fname), config)
    return config

def _full_filename(basefile, filename):
    if os.path.isabs(filename):
        return filename
    return os.path.join(os.path.dirname(basefile), filename)

def _merge_docs(base, new, filename):
    '''Merge contents of new into base'''
    for key in _required_keys['config']:
        _debug("Key : %s" % key)
        if new.has_key(key):
            for name,obj in new[key].items():

                if base[key].has_key(name):
                    err_st = "Found 2nd defination of module %s type %s" % (
                            name, key)
                    err_st += "\nFirst Defination in file: %s" % (
                            base[key][name]['_file'])
                    err_st += "\nSecond Definatio in file: %s" % (
                            os.path.abspath(filename))
                    _error(err_st)

                if obj.has_key('base'):
                    _merge_obj_parms(new[key], obj)

                _check_required_key(_required_keys, key, obj, filename, name)
                obj['_file'] = os.path.abspath(filename)
                base[key][name] = obj

def _merge_params(conf):
    '''Merge all the params of 'core' and 'caches'.'''
    for key in ['core', 'cache', 'memory']:
        for objName, obj in conf[key].items():
            _merge_obj_parms(conf[key], obj)

def _merge_obj_parms(keyObj, obj):
    '''Merge params of given object under 'key'.'''

    if obj.has_key('_params_merged'):
        _debug("Found Obj with params merged: %s" % str(obj))
        return

    base_name = obj['base']
    base = _get_base_obj(keyObj, base_name)
    if base and base != obj:
        if not base.has_key('_params_merged'):
            _merge_obj_parms(keyObj, base)
        params = copy.copy(base['params'])
        params.update(obj['params'])
        obj['params'] = params
        obj['base'] = base['base']

    obj['_params_merged'] = True

def _get_base_obj(store, base_name):
    '''Return a base object from the list if present'''
    try:
        return store[base_name]
    except:
        return None

if __name__ == "__main__":
    if len(sys.argv) == 2:
        config = parse_config(sys.argv[1], debug=True)
    else:
        config = parse_config(debug=True)
