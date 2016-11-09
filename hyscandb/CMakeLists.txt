
add_library (hyscandb SHARED
             hyscan-db.c
             hyscan-db-common.c
             hyscan-db-file.c
             hyscan-db-client.c
             hyscan-db-channel-file.c
             hyscan-db-param-file.c)

add_library (hyscandbserver SHARED
             hyscan-db-server.c)

target_link_libraries (hyscandb ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})
target_link_libraries (hyscandbserver ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (hyscandb PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (hyscandbserver PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
