/*!
 * \file hyscan-db-file.h
 *
 * \brief Заголовочный файл класса файловой базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBFile HyScanDBFile класс хранения данных в формате HyScan 5
 *
 * Класс реализует интерфейс \link HyScanDB \endlink и хранит данные с использованием
 * \link HyScanDBChannelFile \endlink и \link HyScanDBParamFile \endlink.
 *
*/

#ifndef _hyscan_db_file_h
#define _hyscan_db_file_h

#include <hyscan-db.h>

G_BEGIN_DECLS


#define G_TYPE_HYSCAN_DB_FILE              hyscan_db_file_get_type()
#define HYSCAN_DB_FILE( obj )              ( G_TYPE_CHECK_INSTANCE_CAST ( ( obj ), G_TYPE_HYSCAN_DB_FILE, HyScanDBFile ) )
#define HYSCAN_DB_FILE_CLASS( vtable )     ( G_TYPE_CHECK_CLASS_CAST ( ( vtable ), G_TYPE_HYSCAN_DB_FILE, HyScanDBFileClass ) )
#define HYSCAN_DB_FILE_GET_CLASS( inst )   ( G_TYPE_INSTANCE_GET_INTERFACE ( ( inst ), G_TYPE_HYSCAN_DB_FILE, HyScanDBFileClass ) )

GType hyscan_db_file_get_type( void );


typedef GObject HyScanDBFile;
typedef GObjectClass HyScanDBFileClass;


G_END_DECLS

#endif // _hyscan_db_file_h
