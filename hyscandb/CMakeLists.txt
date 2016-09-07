
add_library (hyscandb SHARED
             hyscan-db.c
             hyscan-db-common.c
             hyscan-db-file.c
             hyscan-db-client.c
             hyscan-db-server.c
             hyscan-db-channel-file.c
             hyscan-db-param-file.c)

target_link_libraries (hyscandb ${GLIB2_LIBRARIES} ${ADDON_LIBRARIES})

set_target_properties (hyscandb PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
