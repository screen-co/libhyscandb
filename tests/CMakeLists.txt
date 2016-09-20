
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/resources/db-logic-resources.c
                    COMMAND ${RESOURCE_COMPILER} --generate-source
                            --target ${CMAKE_BINARY_DIR}/resources/db-logic-resources.c
                            db-logic-resources.xml
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    DEPENDS db-logic-resources.xml
                    VERBATIM)

if (UNIX)
  add_executable (channel-file-test channel-file-test.c)
endif ()
add_executable (db-logic-test db-logic-test.c ${CMAKE_BINARY_DIR}/resources/db-logic-resources.c)
add_executable (simple-db-server simple-db-server.c)

if (UNIX)
  target_link_libraries (channel-file-test ${GLIB2_LIBRARIES} hyscandb)
endif ()
target_link_libraries (db-logic-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} hyscandb)
target_link_libraries (simple-db-server ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} hyscandb)
