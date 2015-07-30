cmake_minimum_required( VERSION 2.6 )

project( libhyscandb )

include( FindPkgConfig )

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

#
# Compilers options.
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU  )
  set( CMAKE_C_FLAGS_RELEASE "-O2 -Wall" )
  set( CMAKE_C_FLAGS_DEBUG "-g -Wall" )
  add_definitions( -D_GCC_COMPILER )
elseif( ${CMAKE_C_COMPILER_ID} STREQUAL MSVC )
  add_definitions( -D_MSVC_COMPILER )
else()
  message( FATAL_ERROR "Unsupported compiler" )
endif()

# Additional tuning for GCC on UNIX platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU AND UNIX )
  set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -pthread" )
  set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pthread" )
  set( PLATFORM "posix" )
endif()

# Additional tuning for GCC on Windows platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU AND WIN32 )
  set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mthreads" )
  set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -mthreads" )
  add_definitions( -D_WIN32_WINNT=0x0600 )
  set( PLATFORM "win32" )
endif()

# Additional tuning for MSVC on Windows platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL MSVC AND WIN32 )
  add_definitions( -D_WIN32_WINNT=0x0600 )
  add_definitions( -D_CRT_SECURE_NO_WARNINGS )
  set( PLATFORM "win32" )
endif()

#
# Output directories.
if( NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin )
endif()

if( NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin )
endif()

#
# Dependencies
pkg_check_modules( GLIB2 REQUIRED glib-2.0 gobject-2.0 gio-2.0 )
add_definitions( ${GLIB2_CFLAGS} )
link_directories( ${GLIB2_LIBRARY_DIRS} )

#
# Include directories.
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/hyscandb )

#
# Target.
add_definitions( -DG_LOG_DOMAIN="HyScanDB" )
add_subdirectory( hyscandb )
add_subdirectory( tests )

#
# Pkgconfig support.
#configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/urpc/urpc.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/bin/urpc.pc @ONLY )
#install( FILES bin/urpc.pc DESTINATION lib/pkgconfig PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ )
