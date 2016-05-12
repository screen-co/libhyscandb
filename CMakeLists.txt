cmake_minimum_required (VERSION 2.6)

project (libhyscandb)

include (FindPkgConfig)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

#
# Compilers options.
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -Wall $ENV{CFLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4244 /wd4305 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /wd4244 /wd4305 $ENV{CFLAGS}")
endif ()

#
# Output directories.
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

#
# System dependencies.
pkg_check_modules (GLIB2 REQUIRED "glib-2.0 gobject-2.0 gthread-2.0 gio-2.0")
add_definitions (${GLIB2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS})

#
# uRPC library default path.
if (NOT URPC_SOURCE_DIR)
  set (URPC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../urpc)
endif ()

#
# HyScan libraries default paths.
if (NOT HYSCAN_TYPES_SOURCE_DIR)
  set (HYSCAN_TYPES_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes)
endif ()

#
# uRPC dependencies.
pkg_check_modules (URPC QUIET urpc)
if (${URPC_FOUND})
  add_definitions (${URPC_CFLAGS})
  link_directories (${URPC_LIBRARY_DIRS})
else ()
  include_directories ("${URPC_SOURCE_DIR}/urpc")
  link_directories ("${URPC_SOURCE_DIR}/bin")
  set (URPC_LIBRARIES urpc)
endif ()

#
# HyScan dependencies.
pkg_check_modules (HYSCAN QUIET hyscantypes)
if (${HYSCAN_FOUND})
  add_definitions (${HYSCAN_CFLAGS})
  link_directories (${HYSCAN_LIBRARY_DIRS})
else ()
  include_directories ("${HYSCAN_TYPES_SOURCE_DIR}/hyscantypes")
  link_directories ("${HYSCAN_TYPES_SOURCE_DIR}/bin")
  set (HYSCAN_LIBRARIES hyscantypes)
endif ()

#
# Include directories.
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscandb")

#
# Target.
add_definitions (-DG_LOG_DOMAIN="HyScanDB")
add_subdirectory ("hyscandb")
add_subdirectory ("tests")
