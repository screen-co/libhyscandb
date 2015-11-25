
add_executable (channel-file-test channel-file-test.c)
add_executable (db-logic-test db-logic-test.c)

target_link_libraries (channel-file-test ${GLIB2_LIBRARIES} hyscandb)
target_link_libraries (db-logic-test ${GLIB2_LIBRARIES} hyscandb)
