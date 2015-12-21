
if (UNIX)
  add_executable (channel-file-test channel-file-test.c)
endif ()
add_executable (db-logic-test db-logic-test.c)

if (UNIX)
  target_link_libraries (channel-file-test ${GLIB2_LIBRARIES} hyscandb)
endif ()
target_link_libraries (db-logic-test ${GLIB2_LIBRARIES} hyscandb)
