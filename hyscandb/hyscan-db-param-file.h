/*!
 * \file hyscan-db-param-file.h
 *
 * \brief Заголовочный файл класса хранения параметров в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 *
*/

#ifndef _hyscan_db_param_file_h
#define _hyscan_db_param_file_h

#include <glib-object.h>

G_BEGIN_DECLS


#define G_TYPE_HYSCAN_DB_PARAM_FILE              hyscan_db_param_file_get_type()
#define HYSCAN_DB_PARAM_FILE( obj )              ( G_TYPE_CHECK_INSTANCE_CAST ( ( obj ), G_TYPE_HYSCAN_DB_PARAM_FILE, HyScanDBParamFile ) )
#define HYSCAN_DB_PARAM_FILE_CLASS( vtable )     ( G_TYPE_CHECK_CLASS_CAST ( ( vtable ), G_TYPE_HYSCAN_DB_PARAM_FILE, HyScanDBParamFileClass ) )
#define HYSCAN_DB_PARAM_FILE_GET_CLASS( inst )   ( G_TYPE_INSTANCE_GET_INTERFACE ( ( inst ), G_TYPE_HYSCAN_DB_PARAM_FILE, HyScanDBParamFileClass ) )

GType hyscan_db_param_file_get_type( void );


typedef GObject HyScanDBParamFile;
typedef GObjectClass HyScanDBParamFileClass;


gboolean hyscan_db_param_file_set_integer( HyScanDBParamFile *param, const gchar *name, gint64 value );


gboolean hyscan_db_param_file_set_double( HyScanDBParamFile *param, const gchar *name, gdouble value );


gboolean hyscan_db_param_file_set_boolean( HyScanDBParamFile *param, const gchar *name, gboolean value );


gboolean hyscan_db_param_file_set_string( HyScanDBParamFile *param, const gchar *name, const gchar *value );


gint64 hyscan_db_param_file_get_integer( HyScanDBParamFile *param, const gchar *name );


gdouble hyscan_db_param_file_get_double( HyScanDBParamFile *param, const gchar *name );


gboolean hyscan_db_param_file_get_boolean( HyScanDBParamFile *param, const gchar *name );


gchar *hyscan_db_param_file_get_string( HyScanDBParamFile *param, const gchar *name );


G_END_DECLS

#endif // _hyscan_db_param_file_h
