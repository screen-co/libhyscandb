
#
# Searching for Glib resource compiler.
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

#
# Generate resource C file.
file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
execute_process (COMMAND ${RESOURCE_COMPILER}
                         --generate-source
                         --target ${CMAKE_BINARY_DIR}/resources/db-logic-resources.c
                         db-logic-resources.xml
                         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

#if (UNIX)
#  add_executable (channel-file-test channel-file-test.c)
#endif ()
add_executable (db-logic-test db-logic-test.c ${CMAKE_BINARY_DIR}/resources/db-logic-resources.c)
#add_executable (simple-db-server simple-db-server.c)

#if (UNIX)
#  target_link_libraries (channel-file-test ${GLIB2_LIBRARIES} hyscandb)
#endif ()
target_link_libraries (db-logic-test ${GLIB2_LIBRARIES} hyscandb)
#target_link_libraries (simple-db-server ${GLIB2_LIBRARIES} ${URPC_LIBRARIES} hyscandb)
