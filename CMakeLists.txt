#
# MARSS: Micro ARchitectural and System Simulator
#
# This file is subject to the terms and conditions defined in
# 'LICENSE', which is part of this source code package.
#

cmake_minimum_required(VERSION 2.8)
project(marss)

include(ExternalProject)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules ${CMAKE_MODULE_PATH})
set(CURSES_NEED_NCURSES TRUE)

find_package(Curses REQUIRED)
find_package(GLIB REQUIRED)
find_package(LibFdt REQUIRED)
find_package(Pixman REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

#
# Compile time build options.
#
set(NUM_SIM_CORES 2 CACHE STRING "Number of cores in simulated processor")

#
# Optional packages; enable support if present.
#
find_package(SDL)

if (DEFINED SDL_FOUND AND ${SDL_FOUND})
  SET(SDL_DEFAULT_ENABLE ON)
else ()
  SET(SEL_DEFAULT_ENABLE OFF)
endif ()

option(SDL_SUPPORT "Build qemu with SDL support" ${SDL_DEFAULT_ENABLE})

if (SDL_SUPPORT STREQUAL ON)
  if (NOT DEFINED SDL_FOUND)
    message(FATAL_ERROR "SDL support requested, but SDL not found!")
  endif ()

  list (APPEND QEMU_CONFIGURE_SWITCHES "--enable-sdl")
else ()
  list (APPEND QEMU_CONFIGURE_SWITCHES "--disable-sdl")
endif ()

#
# Fetch, configure, and build the MARSS-variant of qemu.
#
ExternalProject_Add(
  qemu
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/qemu
  CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/qemu/configure
    --prefix=${CMAKE_BINARY_DIR}
    --target-list=x86_64-softmmu
    --cc=${CMAKE_C_COMPILER}
    --cxx=${CMAKE_CXX_COMPILER}
    --extra-cflags=-DNEED_CPU_H
    --extra-cflags=-DMARSS_QEMU
    --extra-cflags=-DNUM_SIM_CORES=${NUM_SIM_CORES}
    --extra-cflags=-I${CMAKE_SOURCE_DIR}/qemu/fpu
    --extra-cflags=-I${CMAKE_SOURCE_DIR}/qemu/target-i386
    --extra-cflags=-I${CMAKE_BINARY_DIR}/qemu-prefix/src/qemu-build//x86_64-softmmu
    --extra-cflags=-I${CMAKE_SOURCE_DIR}/ptlsim/sim
    --extra-ldflags=-L${CMAKE_BINARY_DIR}/ptlsim
    --disable-debug-tcg
    --disable-sparse
    --disable-strip
    --audio-drv-list=
    --disable-xen
    --disable-brlapi
    --disable-vnc-tls
    --disable-vnc-sasl
    --disable-vnc-jpeg
    --disable-vnc-png
    --disable-vnc-thread
    --enable-curses
    --disable-curl
    --enable-fdt
    --disable-check-utests
    --disable-bluez
    --disable-kvm
    --enable-nptl
    --enable-system
    --disable-user
    --disable-linux-user
    --disable-darwin-user
    --disable-bsd-user
    --disable-guest-base
    --disable-user-pie
    --disable-uuid
    --disable-vde
    --disable-linux-aio
    --disable-attr
    --disable-blobs
    --disable-docs
    --disable-vhost-net
    --enable-trace-backend=nop
    --disable-spice
    --disable-rbd
    ${QEMU_CONFIGURE_SWITCHES}
    COMMAND $(MAKE) generated-sources
    BUILD_COMMAND $(MAKE)
)

ExternalProject_Add_StepTargets(qemu configure)

#
# Force qemu to rebuild each time a 'make' command is issued to the top-level
# build directory. This will force qemu to relink against the newest version
# of the simulator libraries.
#
ExternalProject_Add_Step(qemu force-build
  COMMAND ${CMAKE_COMMAND} -E echo "Force (re)build of qemu"
  DEPENDERS build
  DEPENDS ptlsim
  ALWAYS 1
)

#
# Build the nested version of PTLsim for MARSS.
#
ExternalProject_Add(ptlsim
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/ptlsim
  CMAKE_ARGS "-DNUM_SIM_CORES=${NUM_SIM_CORES};-DQEMU_SOURCE_DIR=${CMAKE_SOURCE_DIR}/qemu;-DQEMU_GENERATED_DIR=${CMAKE_BINARY_DIR}/qemu-prefix/src/qemu-build"
  BINARY_DIR ${CMAKE_BINARY_DIR}/ptlsim
  DEPENDS qemu-configure
  INSTALL_COMMAND ""
)

ExternalProject_Add_Step(ptlsim force-build
	COMMAND ${CMAKE_COMMAND} -E echo "Force (re)build of PTLsim"
  DEPENDERS build
  DEPENDS ptlsim
  ALWAYS 1
)
