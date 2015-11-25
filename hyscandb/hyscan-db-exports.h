#ifndef __HYSCAN_DB_EXPORTS_H__
#define __HYSCAN_DB_EXPORTS_H__

#if defined (_WIN32)
  #if defined (hyscandb_EXPORTS)
    #define HYSCAN_DB_EXPORT __declspec (dllexport)
  #else
    #define HYSCAN_DB_EXPORT __declspec (dllimport)
  #endif
#else
  #define URPC_EXPORT
#endif

#endif /* __HYSCAN_DB_EXPORTS_H__ */
