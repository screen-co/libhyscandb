
add_library (hyscandb SHARED
             hyscan-db.c
             hyscan-db-common.c
             hyscan-db-file.c
             hyscan-db-channel-file.c
             hyscan-db-param-file.c)

target_link_libraries (hyscandb ${GLIB2_LIBRARIES})
