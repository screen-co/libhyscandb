
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/resources")
add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/resources/db-logic-resources.c"
                    COMMAND ${RESOURCE_COMPILER} --generate-source
                            --target "${CMAKE_BINARY_DIR}/resources/db-logic-resources.c"
                            db-logic-resources.xml
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    DEPENDS db-logic-resources.xml
                    VERBATIM)

set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_DB_LIBRARY})

if (UNIX)
  add_executable (channel-file-test channel-file-test.c)
endif ()
add_executable (db-logic-test db-logic-test.c "${CMAKE_BINARY_DIR}/resources/db-logic-resources.c")
add_executable (simple-db-server simple-db-server.c)

if (UNIX)
  target_link_libraries (channel-file-test ${TEST_LIBRARIES})
endif ()
target_link_libraries (db-logic-test ${TEST_LIBRARIES})
target_link_libraries (simple-db-server ${TEST_LIBRARIES})

install (TARGETS db-logic-test
                 simple-db-server
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
