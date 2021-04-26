
add_library (${HYSCAN_DB_LIBRARY} SHARED
             hyscan-db.c
             hyscan-db-common.c
             hyscan-db-file.c
             hyscan-db-client.c
             hyscan-db-server.c
             hyscan-db-channel-file.c
             hyscan-db-param-file.c)

target_link_libraries (${HYSCAN_DB_LIBRARY} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} ${URPC_LIBRARIES})

set_target_properties (${HYSCAN_DB_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_DB_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_DB_VERSION})

configure_file (hyscandb.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscandb-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_DB_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscandb-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES hyscan-db.h
               hyscan-db-file.h
               hyscan-db-client.h
               hyscan-db-server.h
         COMPONENT development
         DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/hyscan-${HYSCAN_MAJOR_VERSION}/hyscandb"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
