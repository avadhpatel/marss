#
# PTLsim: Cycle Accurate x86-64 Simulator
#
# This file is subject to the terms and conditions defined in
# 'LICENSE', which is part of this source code package.
#
#
# I apologize in advance for this CMakeLists - the original source was
# based in SCons and highly dynamic. CMake 3.19 will make this a little
# more elegant since it can finagle with JSON blobs.
#

cmake_minimum_required(VERSION 3.18)
project(ptlsim)

set(CMAKE_C_FLAGS "-g -O0 -fno-strict-aliasing -fno-trapping-math -fstack-protector -fno-rtti -fno-strict-aliasing -fdiagnostics-color=always -DDISABLE_ASSERT -DDISABLE_LOGGING")
set(CMAKE_CXX_FLAGS "-g -O0 -fno-strict-aliasing -fno-trapping-math -fstack-protector -fno-rtti -fno-strict-aliasing -fdiagnostics-color=always -std=gnu++11 -fpermissive -DDISABLE_ASSERT -DDISABLE_LOGGING")

message(STATUS "core count: ${NUM_SIM_CORES}")
message(STATUS "qemu source dir: ${QEMU_SOURCE_DIR}")
message(STATUS "qemu generated dir: ${QEMU_GENERATED_DIR}")

# TODO: Clean up this mess... MONGO_HAVE_STDINT?
add_compile_definitions(MARSS_QEMU)
add_compile_definitions(MONGO_HAVE_STDINT)
add_compile_definitions(NEED_CPU_H)
add_compile_definitions(NUM_SIM_CORES=${NUM_SIM_CORES})
add_compile_definitions(__x86_64__)

include_directories(${QEMU_SOURCE_DIR})
include_directories(${QEMU_SOURCE_DIR}/fpu)
include_directories(${QEMU_SOURCE_DIR}/target-i386)
include_directories(${QEMU_GENERATED_DIR})
include_directories(${QEMU_GENERATED_DIR}/x86_64-softmmu)

include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/sim)
include_directories(${CMAKE_SOURCE_DIR}/stats)
include_directories(${CMAKE_SOURCE_DIR}/x86)

SET(LIBPTLSIM_OBJECTS)

###############################################################################
# config
###############################################################################
file(GLOB CONFIG_YAML ${PROJECT_SOURCE_DIR}/config/*.conf)

# The target below is used for dependency tracking; this one executes
# immediately during CMakeLists evaulation and is used to construct lists
# dynamically for the build process.
execute_process(
  COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/config_helper.py config ${CMAKE_BINARY_DIR}/config.json.cmake
  OUTPUT_FILE ${CMAKE_BINARY_DIR}/config.json.cmake
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_command(
  COMMENT "Compiling PTLsim configuration..."
  COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/config_helper.py config ${CMAKE_BINARY_DIR}/config.json
  OUTPUT ${CMAKE_BINARY_DIR}/config.json
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS ${CONFIG_YAML}
  VERBATIM
)

add_custom_target(PTLSimConfig DEPENDS ${CMAKE_BINARY_DIR}/config.json)

###############################################################################
# cache
###############################################################################
add_custom_command(
  COMMENT "Generating cacheTypes.h..."
  COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/tools/config_gen.py -c ${CMAKE_BINARY_DIR}/config.json -t cache -n header -o ${CMAKE_BINARY_DIR}/cacheTypes.h
  OUTPUT ${CMAKE_BINARY_DIR}/cacheTypes.h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/cache
  DEPENDS PTLSimConfig
  VERBATIM
)

add_custom_target(CacheTypesHeader DEPENDS ${CMAKE_BINARY_DIR}/cacheTypes.h)

add_custom_command(
  COMMENT "Generating cacheTypes.cpp..."
  COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/tools/config_gen.py -c ${CMAKE_BINARY_DIR}/config.json -t cache -n logic -o ${CMAKE_BINARY_DIR}/cacheTypes.cpp
  OUTPUT ${CMAKE_BINARY_DIR}/cacheTypes.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/cache
  DEPENDS CacheTypesHeader
  VERBATIM
)

add_custom_target(CacheTypesImpl DEPENDS ${CMAKE_BINARY_DIR}/cacheTypes.cpp)

set(PTLSIM_CACHE_SOURCES
  ${CMAKE_BINARY_DIR}/cacheTypes.cpp
  cache/bus.cpp
  cache/cacheController.cpp
  cache/coherentCache.cpp
  cache/cpuController.cpp
  cache/globalDirectory.cpp
  cache/memoryController.cpp
  cache/memoryHierarchy.cpp
  cache/memoryRequest.cpp
  cache/mesiLogic.cpp
  cache/moesiLogic.cpp
  cache/p2p.cpp
  cache/splitPhaseBus.cpp
  cache/switch.cpp
)

add_library(cache OBJECT ${PTLSIM_CACHE_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:cache>)

target_compile_options(cache PRIVATE -include ${CMAKE_BINARY_DIR}/cacheTypes.h)
target_include_directories(cache PRIVATE ${CMAKE_SOURCE_DIR}/cache ${CMAKE_SOURCE_DIR}/x86)
set_source_files_properties(${PTLSIM_CACHE_SOURCES} PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/cacheTypes.h)

###############################################################################
# core
###############################################################################
set(PTLSIM_CORE_SOURCES
  core/basecore.cpp
  core/branchpred.cpp
)

add_library(core OBJECT ${PTLSIM_CORE_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:core>)

target_include_directories(core PRIVATE ${CMAKE_SOURCE_DIR}/cache ${CMAKE_SOURCE_DIR}/core)

set(atom_SOURCES
  core/atom-core/atomcore.cpp
)

set(ooo_SOURCES
  core/ooo-core/ooo.cpp
  core/ooo-core/ooo-exec.cpp
  core/ooo-core/ooo-pipe.cpp
)

foreach(CORE_TYPE atom ooo)
  execute_process(
    COMMAND jq -r ".core | with_entries(select(.value.base == \"${CORE_TYPE}\")) | keys[]"
    INPUT_FILE ${CMAKE_BINARY_DIR}/config.json.cmake
    OUTPUT_VARIABLE CORES
  )

  string(REPLACE "\n" ";" CORE_LIST ${CORES})
  set(CORE_HEADER_LIST "")

  foreach(CORE_NAME ${CORE_LIST})
    add_custom_command(
      COMMENT "Generating ${CORE_TYPE}_${CORE_NAME}.h..."
      COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/tools/config_gen.py -c ${CMAKE_BINARY_DIR}/config.json -t core -n ${CORE_NAME} -o ${CMAKE_BINARY_DIR}/${CORE_TYPE}_${CORE_NAME}.h
      OUTPUT ${CMAKE_BINARY_DIR}/${CORE_TYPE}_${CORE_NAME}.h
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/core/${CORE_TYPE}-core
      DEPENDS ${CMAKE_BINARY_DIR}/config.json
      VERBATIM
    )

    add_library(${CORE_TYPE}_${CORE_NAME} OBJECT ${${CORE_TYPE}_SOURCES} ${CMAKE_BINARY_DIR}/${CORE_TYPE}_${CORE_NAME}.h)
    target_compile_options(${CORE_TYPE}_${CORE_NAME} PRIVATE -include ${CMAKE_BINARY_DIR}/${CORE_TYPE}_${CORE_NAME}.h)
    target_include_directories(${CORE_TYPE}_${CORE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/cache ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/core/${CORE_TYPE}-core)
    list(APPEND CORE_HEADER_LIST OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/${CORE_TYPE}_${CORE_NAME}.h)
    list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:${CORE_TYPE}_${CORE_NAME}>)
  endforeach()

  set_source_files_properties(${${CORE_TYPE}_SOURCES} PROPERTIES ${CORE_HEADER_LIST})
endforeach()

###############################################################################
# lib
###############################################################################
set(PTLSIM_LIB_SOURCES
  lib/statelist.cpp
  lib/superstl.cpp
)

set(PTLSIM_LIB_BSON_SOURCES
  lib/bson/bson.c
  lib/bson/md5.c
  lib/bson/mongo.c
  lib/bson/numbers.c
)

set(PTLSIM_LIB_YAML_SOURCES
  lib/yaml/aliascontent.cpp
  lib/yaml/conversion.cpp
  lib/yaml/emitter.cpp
  lib/yaml/emitterstate.cpp
  lib/yaml/emitterutils.cpp
  lib/yaml/exp.cpp
  lib/yaml/iterator.cpp
  lib/yaml/map.cpp
  lib/yaml/node.cpp
  lib/yaml/null.cpp
  lib/yaml/ostream.cpp
  lib/yaml/parser.cpp
  lib/yaml/parserstate.cpp
  lib/yaml/regex.cpp
  lib/yaml/scalar.cpp
  lib/yaml/scanner.cpp
  lib/yaml/scanscalar.cpp
  lib/yaml/scantag.cpp
  lib/yaml/scantoken.cpp
  lib/yaml/sequence.cpp
  lib/yaml/simplekey.cpp
  lib/yaml/stream.cpp
  lib/yaml/tag.cpp
)

add_library(ptlsimbson OBJECT ${PTLSIM_LIB_BSON_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:ptlsimbson>)

target_compile_definitions(ptlsimbson PRIVATE MONGO_HAVE_STDINT)
target_compile_options(ptlsimbson PRIVATE -xc)

add_library(ptlsimyaml OBJECT ${PTLSIM_LIB_YAML_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:ptlsimyaml>)

target_compile_options(ptlsimyaml PRIVATE -fexceptions)

add_library(ptlsimlib OBJECT ${PTLSIM_LIB_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:ptlsimlib>)

###############################################################################
# sim
###############################################################################
execute_process(
  COMMAND jq -r ".machine | keys[]"
  INPUT_FILE ${CMAKE_BINARY_DIR}/config.json.cmake
  OUTPUT_VARIABLE MACHINES_RAW
)

string(REPLACE "\n" ";" MACHINES ${MACHINES_RAW})
set(MACHINE_SOURCES "")

foreach(MACHINE ${MACHINES})
  execute_process(
    COMMAND jq -r ".machine.${MACHINE} | .min_contexts,.max_contexts"
    INPUT_FILE ${CMAKE_BINARY_DIR}/config.json.cmake
    OUTPUT_VARIABLE MACHINE_CONTEXTS_RAW
  )

  string(REPLACE "\n" ";" MACHINE_CONTEXTS ${MACHINE_CONTEXTS_RAW})
  list(GET MACHINE_CONTEXTS 0 MACHINE_MIN_CONTEXTS)
  list(GET MACHINE_CONTEXTS 1 MACHINE_MAX_CONTEXTS)

  # Workout what we're able to support given NUM_SIM_CORES contexts...
  set(MIN_C 0)
  set(MAX_C ${NUM_SIM_CORES})
  set(CAN_BUILD_MACHINE TRUE)

  if(NOT ${MACHINE_MIN_CONTEXTS} MATCHES "null")
    set(MIN_C ${MACHINE_MIN_CONTEXTS})
  endif()

  if(NOT ${MACHINE_MAX_CONTEXTS} MATCHES "null")
    set(MAX_C ${MACHINE_MAX_CONTEXTS})
  endif()

  if(${MIN_C} GREATER ${NUM_SIM_CORES})
    message(WARNING "Machine ${MACHINE} requires minimum of ${MIN_C} contexts")
    set(CAN_BUILD_MACHINE FALSE)
  endif()

  if(${MAX_C} LESS ${NUM_SIM_CORES})
    message(WARNING "Machine ${MACHINE} requires maximum of ${MAX_C} contexts")
    set(CAN_BUILD_MACHINE FALSE)
  endif()

  if(${CAN_BUILD_MACHINE})
    add_custom_command(
      COMMENT "Generating ${MACHINE}_gen.cpp..."
      COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/tools/config_gen.py -c ${CMAKE_BINARY_DIR}/config.json -t machine -n ${MACHINE} -o ${CMAKE_BINARY_DIR}/${MACHINE}_gen.cpp
      OUTPUT ${CMAKE_BINARY_DIR}/${MACHINE}_gen.cpp
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/sim
      DEPENDS PTLSimConfig
      VERBATIM
    )

    add_library(m_${MACHINE} OBJECT ${CMAKE_BINARY_DIR}/${MACHINE}_gen.cpp)
    list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:m_${MACHINE}>)

    target_compile_options(m_${MACHINE} PRIVATE -include ${CMAKE_BINARY_DIR}/cacheTypes.h)
    target_include_directories(m_${MACHINE} PRIVATE ${CMAKE_SOURCE_DIR}/cache ${CMAKE_SOURCE_DIR}/core)
    set_source_files_properties(${CMAKE_BINARY_DIR}/${MACHINE}_gen.cpp PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/cacheTypes.h)
  endif()
endforeach()

set(PTLSIM_SIM_SOURCES
  sim/config-parser.cpp
  sim/machine.cpp
  sim/ptl-qemu.cpp
  sim/ptlsim.cpp
  sim/syscalls.cpp
  sim/test.cpp
)

add_library(sim OBJECT ${PTLSIM_SIM_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:sim>)

target_include_directories(sim PRIVATE ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/cache ${CMAKE_SOURCE_DIR}/tools)

###############################################################################
# stats
###############################################################################
set(PTLSIM_STATS_SOURCES
  stats/statsBuilder.cpp
)

add_library(stats OBJECT ${PTLSIM_STATS_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:stats>)

###############################################################################
# x86
###############################################################################
set(PTLSIM_X86_SOURCES
  x86/decode-complex.cpp
  x86/decode-core.cpp
  x86/decode-fast.cpp
  x86/decode-sse.cpp
  x86/decode-x87.cpp
  x86/ptlhwdef.cpp
  x86/uopimpl.cpp
)

add_library(x86 OBJECT ${PTLSIM_X86_SOURCES})
list(APPEND LIBPTLSIM_OBJECTS $<TARGET_OBJECTS:x86>)

###############################################################################
# libptlsim
###############################################################################
add_library(ptlsim STATIC ${LIBPTLSIM_OBJECTS})
