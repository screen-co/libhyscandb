/*!
 * \file hyscan-db-channel-file.h
 *
 * \brief Заголовочный файл класса хранения данных канала в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 *
*/

#ifndef _hyscan_db_channel_file_h
#define _hyscan_db_channel_file_h

#include <glib-object.h>

G_BEGIN_DECLS


#define G_TYPE_HYSCAN_DB_CHANNEL_FILE            hyscan_db_channel_file_get_type()
#define HYSCAN_DB_CHANNEL_FILE( obj )            ( G_TYPE_CHECK_INSTANCE_CAST ( ( obj ), G_TYPE_HYSCAN_DB_CHANNEL_FILE, HyScanDBChannelFile ) )
#define HYSCAN_DB_CHANNEL_FILE_CLASS( vtable )   ( G_TYPE_CHECK_CLASS_CAST ( ( vtable ), G_TYPE_HYSCAN_DB_CHANNEL_FILE, HyScanDBChannelFileClass ) )
#define HYSCAN_DB_CHANNEL_FILE_GET_CLASS( inst ) ( G_TYPE_INSTANCE_GET_INTERFACE ( ( inst ), G_TYPE_HYSCAN_DB_CHANNEL_FILE, HyScanDBChannelFileClass ) )

GType hyscan_db_channel_file_get_type( void );


typedef GObject HyScanDBChannelFile;
typedef GObjectClass HyScanDBChannelFileClass;


gboolean hyscan_db_channel_file_get_channel_data_range( HyScanDBChannelFile *channel, gint32 *first_index, gint32 *last_index );


gboolean hyscan_db_channel_file_add_channel_data( HyScanDBChannelFile *channel, gint64 time, gpointer data, gint32 size, gint32 *index );


gboolean hyscan_db_channel_file_get_channel_data( HyScanDBChannelFile *channel, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time );


gboolean hyscan_db_channel_file_find_channel_data( HyScanDBChannelFile *channel, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime );


gboolean hyscan_db_channel_file_set_channel_chunk_size( HyScanDBChannelFile *channel, gint32 chunk_size );


gboolean hyscan_db_channel_file_set_channel_save_time( HyScanDBChannelFile *channel, gint64 save_time );


gboolean hyscan_db_channel_file_set_channel_save_size( HyScanDBChannelFile *channel, gint64 save_size );


void hyscan_db_channel_file_finalize_channel( HyScanDBChannelFile *channel );


G_END_DECLS

#endif // _hyscan_db_channel_file_h
