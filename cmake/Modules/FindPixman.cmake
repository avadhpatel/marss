# - Try to find Pixman
# Once done, this will define
#
#  PIXMAN_FOUND - system has Pixman
#  PIXMAN_INCLUDE_DIRS - the Pixman include directories
#  PIXMAN_LIBRARIES - link these to use Pixman
#
# Copyright (C) 2016 Tyler J. Stachecki <stachecki.tyler@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)
pkg_check_modules(PC_PIXMAN QUIET pixman-1)

find_path(PIXMAN_INCLUDE_DIRS
  NAMES pixman-version.h
  HINTS ${PC_PIXMAN_INCLUDEDIR}
        ${PC_PIXMAN_INCLUDE_DIRS}
  PATH_SUFFIXES pixman-1
)

find_library(PIXMAN_LIBRARIES
  NAMES pixman-1 libpixman-1
  HINTS ${PC_PIXMAN_LIBDIR}
        ${PC_PIXMAN_LIBRARY_DIRS}
)

# Version detection
if (NOT ${PIXMAN_INCLUDE_DIRS} MATCHES "PIXMAN_INCLUDE_DIRS-NOTFOUND")
  file(READ "${PIXMAN_INCLUDE_DIRS}/pixman-version.h" PIXMAN_VERSION_H_CONTENTS)
  string(REGEX MATCH "#define PIXMAN_VERSION_MAJOR ([0-9]+)" _dummy "${PIXMAN_VERSION_H_CONTENTS}")
  set(PIXMAN_VERSION_MAJOR "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define PIXMAN_VERSION_MINOR ([0-9]+)" _dummy "${PIXMAN_VERSION_H_CONTENTS}")
  set(PIXMAN_VERSION_MINOR "${CMAKE_MATCH_1}")
  string(REGEX MATCH "#define PIXMAN_VERSION_MICRO ([0-9]+)" _dummy "${PIXMAN_VERSION_H_CONTENTS}")
  set(PIXMAN_VERSION_MICRO "${CMAKE_MATCH_1}")
  set(PIXMAN_VERSION "${PIXMAN_VERSION_MAJOR}.${PIXMAN_VERSION_MINOR}.${PIXMAN_VERSION_MICRO}")
endif (NOT ${PIXMAN_INCLUDE_DIRS} MATCHES "PIXMAN_INCLUDE_DIRS-NOTFOUND")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Pixman REQUIRED_VARS PIXMAN_INCLUDE_DIRS PIXMAN_LIBRARIES
                                         VERSION_VAR   PIXMAN_VERSION)

mark_as_advanced(
  ${PIXMAN_INCLUDE_DIRS}
  ${PIXMAN_LIBRARIES}
)
