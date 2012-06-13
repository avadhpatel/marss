Plugins Folder for MARSS
========================

This folder is used as MARSS to detect all plugins and compile them into
binary.  All plugins are exptected to be in Shared-Library format and must
have '.so' extension.  If there is an .so file in this folder, MARSS will
link binary aginst it!!

Creating Plugins
----------------

In order to build/create a plugin please refer to wiki.


Installing Plugins
------------------

Simply place an .so file of plugin in this folder and you'r good to go!!

MARSS compilation process support 3 different methods to compile your plugin
source code automatically:

1) Single .cpp file plugin:

   If Plugin module is small and implemented in just one .cpp file then simply
place that file into this folder and compilation process will create .so file
and link against it. No need to change anything or add any 'makefile' or
'SConscript' file for this method!!

2) Folder with SConscript file:

  If Plugin module has multiple files in a folder and it contains 'SConscript'
file then MARSS compilation process will invoke this SConscript with fully
setup environment containing all CPPFLAGS and include directories.  Its
'SConscript's responsibility to copy final .so object into 'plugins' folder.

3) Folder with Makefile:

  If plugin folder has 'Makefile'/'makefile' present, then MARSS compilation
process will invoke 'make' into that folder and it must create and copy final
.so object into this plugins folder.
