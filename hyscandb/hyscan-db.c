/*!
 * \file hyscan-db.c
 *
 * \brief Исходный файл интерфейса базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-db.h"


G_DEFINE_INTERFACE( HyScanDB, hyscan_db, G_TYPE_OBJECT );

static void hyscan_db_default_init( HyScanDBInterface *iface ){ ; }


gchar **hyscan_db_get_project_type_list( HyScanDB *db )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_project_type_list )
    return HYSCAN_DB_GET_IFACE( db )->get_project_type_list( db );

  return NULL;

}


gchar *hyscan_db_get_uri( HyScanDB *db )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_uri )
    return HYSCAN_DB_GET_IFACE( db )->get_uri( db );

  return NULL;

}


gchar **hyscan_db_get_project_list( HyScanDB *db )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_project_list )
    return HYSCAN_DB_GET_IFACE( db )->get_project_list( db );

  return NULL;

}


gint32 hyscan_db_open_project( HyScanDB *db, const gchar *project_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_project )
    return HYSCAN_DB_GET_IFACE( db )->open_project( db, project_name );

  return -1;

}


gint32 hyscan_db_create_project( HyScanDB *db, const gchar *project_name, const gchar *project_type )
{

  if( HYSCAN_DB_GET_IFACE( db )->create_project )
    return HYSCAN_DB_GET_IFACE( db )->create_project( db, project_name, project_type );

  return -1;

}


gboolean hyscan_db_remove_project( HyScanDB *db, const gchar *project_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_project )
    return HYSCAN_DB_GET_IFACE( db )->remove_project( db, project_name );

  return FALSE;

}


void hyscan_db_close_project( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->close_project )
    HYSCAN_DB_GET_IFACE( db )->close_project( db, project_id );

}


GDateTime *hyscan_db_get_project_ctime( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_project_ctime )
    return HYSCAN_DB_GET_IFACE( db )->get_project_ctime( db, project_id );

  return NULL;

}


gchar **hyscan_db_get_track_list( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_track_list )
    return HYSCAN_DB_GET_IFACE( db )->get_track_list( db, project_id );

  return NULL;

}


gint32 hyscan_db_open_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_track )
    return HYSCAN_DB_GET_IFACE( db )->open_track( db, project_id, track_name );

  return -1;

}


gint32 hyscan_db_create_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->create_track )
    return HYSCAN_DB_GET_IFACE( db )->create_track( db, project_id, track_name );

  return -1;

}


gboolean hyscan_db_remove_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_track )
    return HYSCAN_DB_GET_IFACE( db )->remove_track( db, project_id, track_name );

  return FALSE;

}


void hyscan_db_close_track( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->close_track )
    HYSCAN_DB_GET_IFACE( db )->close_track( db, track_id );

}


GDateTime *hyscan_db_get_track_ctime( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_track_ctime )
    return HYSCAN_DB_GET_IFACE( db )->get_track_ctime( db, track_id );

  return NULL;

}


gchar **hyscan_db_get_channel_list( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_channel_list )
    return HYSCAN_DB_GET_IFACE( db )->get_channel_list( db, track_id );

  return NULL;

}


gint32 hyscan_db_open_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_channel )
    return HYSCAN_DB_GET_IFACE( db )->open_channel( db, track_id, channel_name );

  return -1;

}


gint32 hyscan_db_create_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->create_channel )
    return HYSCAN_DB_GET_IFACE( db )->create_channel( db, track_id, channel_name );

  return -1;

}


gboolean hyscan_db_remove_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_channel )
    return HYSCAN_DB_GET_IFACE( db )->remove_channel( db, track_id, channel_name );

  return FALSE;

}


void hyscan_db_close_channel( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->close_channel )
    HYSCAN_DB_GET_IFACE( db )->close_channel( db, channel_id );

}


gint32 hyscan_db_open_channel_param( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_channel_param )
    return HYSCAN_DB_GET_IFACE( db )->open_channel_param( db, channel_id );

  return -1;

}


gboolean hyscan_db_set_channel_chunk_size( HyScanDB *db, gint32 channel_id, gint32 chunk_size )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_channel_chunk_size )
    return HYSCAN_DB_GET_IFACE( db )->set_channel_chunk_size( db, channel_id, chunk_size );

  return FALSE;

}


gboolean hyscan_db_set_channel_save_time( HyScanDB *db, gint32 channel_id, gint64 save_time )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_channel_save_time )
    return HYSCAN_DB_GET_IFACE( db )->set_channel_save_time( db, channel_id, save_time );

  return FALSE;

}


gboolean hyscan_db_set_channel_save_size( HyScanDB *db, gint32 channel_id, gint64 save_size )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_channel_save_size )
    return HYSCAN_DB_GET_IFACE( db )->set_channel_save_size( db, channel_id, save_size );

  return FALSE;

}


void hyscan_db_finalize_channel( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->finalize_channel )
    HYSCAN_DB_GET_IFACE( db )->finalize_channel( db, channel_id );

}


gboolean hyscan_db_get_channel_data_range( HyScanDB *db, gint32 channel_id, gint32 *first_index, gint32 *last_index )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_channel_data_range )
    return HYSCAN_DB_GET_IFACE( db )->get_channel_data_range( db, channel_id, first_index, last_index );

  return FALSE;

}


gboolean hyscan_db_add_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gpointer data, gint32 size, gint32 *index )
{

  if( HYSCAN_DB_GET_IFACE( db )->add_channel_data )
    return HYSCAN_DB_GET_IFACE( db )->add_channel_data( db, channel_id, time, data, size, index );

  return FALSE;

}


gboolean hyscan_db_get_channel_data( HyScanDB *db, gint32 channel_id, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_channel_data )
    return HYSCAN_DB_GET_IFACE( db )->get_channel_data( db, channel_id, index, buffer, buffer_size, time );

  return FALSE;

}


gboolean hyscan_db_find_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime )
{

  if( HYSCAN_DB_GET_IFACE( db )->find_channel_data )
    return HYSCAN_DB_GET_IFACE( db )->find_channel_data( db, channel_id, time, lindex, rindex, ltime, rtime );

  return FALSE;

}


gchar **hyscan_db_get_project_param_list( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_project_param_list )
    return HYSCAN_DB_GET_IFACE( db )->get_project_param_list( db, project_id );

  return NULL;

}


gint32 hyscan_db_open_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_project_param )
    return HYSCAN_DB_GET_IFACE( db )->open_project_param( db, project_id, group_name );

  return -1;

}


gboolean hyscan_db_remove_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_project_param )
    return HYSCAN_DB_GET_IFACE( db )->remove_project_param( db, project_id, group_name );

  return FALSE;

}


gchar **hyscan_db_get_track_param_list( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_track_param_list )
    return HYSCAN_DB_GET_IFACE( db )->get_track_param_list( db, track_id );

  return NULL;

}


gint32 hyscan_db_open_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->open_track_param )
    return HYSCAN_DB_GET_IFACE( db )->open_track_param( db, track_id, group_name );

  return -1;

}


gboolean hyscan_db_remove_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_track_param )
    return HYSCAN_DB_GET_IFACE( db )->remove_track_param( db, track_id, group_name );

  return FALSE;

}


gchar **hyscan_db_get_param_list( HyScanDB *db, gint32 param_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_param_list )
    return HYSCAN_DB_GET_IFACE( db )->get_param_list( db, param_id );

  return NULL;

}


gboolean hyscan_db_copy_param( HyScanDB *db, gint32 src_param_id, gint32 dst_param_id, const gchar *mask )
{

  if( HYSCAN_DB_GET_IFACE( db )->copy_param )
    return HYSCAN_DB_GET_IFACE( db )->copy_param( db, src_param_id, dst_param_id, mask );

  return FALSE;

}


gboolean hyscan_db_remove_param( HyScanDB *db, gint32 param_id, const gchar *mask )
{

  if( HYSCAN_DB_GET_IFACE( db )->remove_param )
    return HYSCAN_DB_GET_IFACE( db )->remove_param( db, param_id, mask );

  return FALSE;

}


void hyscan_db_close_param( HyScanDB *db, gint32 param_id )
{

  if( HYSCAN_DB_GET_IFACE( db )->close_param )
    HYSCAN_DB_GET_IFACE( db )->close_param( db, param_id );

}


gboolean hyscan_db_has_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->has_param )
    return HYSCAN_DB_GET_IFACE( db )->has_param( db, param_id, name );

  return 0;

}


gint64 hyscan_db_inc_integer_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->inc_integer_param )
    return HYSCAN_DB_GET_IFACE( db )->inc_integer_param( db, param_id, name );

  return 0;

}


gboolean hyscan_db_set_integer_param( HyScanDB *db, gint32 param_id, const gchar *name, gint64 value )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_integer_param )
    return HYSCAN_DB_GET_IFACE( db )->set_integer_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_double_param( HyScanDB *db, gint32 param_id, const gchar *name, gdouble value )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_double_param )
    return HYSCAN_DB_GET_IFACE( db )->set_double_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name, gboolean value )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_boolean_param )
    return HYSCAN_DB_GET_IFACE( db )->set_boolean_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_string_param( HyScanDB *db, gint32 param_id, const gchar *name, const gchar *value )
{

  if( HYSCAN_DB_GET_IFACE( db )->set_string_param )
    return HYSCAN_DB_GET_IFACE( db )->set_string_param( db, param_id, name, value );

  return FALSE;

}


gint64 hyscan_db_get_integer_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_integer_param )
    return HYSCAN_DB_GET_IFACE( db )->get_integer_param( db, param_id, name );

  return 0;

}


gdouble hyscan_db_get_double_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_double_param )
    return HYSCAN_DB_GET_IFACE( db )->get_double_param( db, param_id, name );

  return 0.0;

}


gboolean hyscan_db_get_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_boolean_param )
    return HYSCAN_DB_GET_IFACE( db )->get_boolean_param( db, param_id, name );

  return FALSE;

}


gchar *hyscan_db_get_string_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_IFACE( db )->get_string_param )
    return HYSCAN_DB_GET_IFACE( db )->get_string_param( db, param_id, name );

  return NULL;

}
