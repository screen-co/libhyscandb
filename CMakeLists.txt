
cmake_minimum_required (VERSION 2.8)

project (libhyscandb)

set (HYSCAN_MAJOR_VERSION 5)
set (HYSCAN_DB_VERSION 0)
set (HYSCAN_DB_LIBRARY "hyscandb-${HYSCAN_MAJOR_VERSION}")

include (FindPkgConfig)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL GNU OR ${CMAKE_C_COMPILER_ID} STREQUAL Clang)
  set (C_FLAGS "-Wall -Wextra -Wno-unused-parameter $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O1 ${C_FLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
  set (C_FLAGS "/wd4244 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${C_FLAGS}")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

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

pkg_check_modules (GLIB2 REQUIRED glib-2.0 gobject-2.0 gthread-2.0 gio-2.0)
pkg_check_modules (LIBXML2 REQUIRED libxml-2.0)
add_definitions (${GLIB2_CFLAGS} ${LIBXML2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS} ${LIBXML2_LIBRARY_DIRS})

if (HYSCAN_USE_SYS_LIBS)
  pkg_check_modules (HYSCAN REQUIRED "hyscantypes-${HYSCAN_MAJOR_VERSION}")
  pkg_check_modules (URPC REQUIRED urpc)

  link_directories (${HYSCAN_LIBRARY_DIRS} ${URPC_LIBRARY_DIRS})
  add_definitions (${HYSCAN_CFLAGS} ${URPC_CFLAGS})
else ()
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
  set (HYSCAN_LIBRARIES "hyscantypes-${HYSCAN_MAJOR_VERSION}")

  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../urpc/urpc")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../urpc/bin")
  set (URPC_LIBRARIES urpc)
endif ()

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscandb")

add_definitions (-DG_LOG_DOMAIN="HyScanDB")
add_subdirectory (hyscandb)
add_subdirectory (tests)
