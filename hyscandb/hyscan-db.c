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

  if( HYSCAN_DB_GET_CLASS( db )->get_project_type_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_project_type_list( db );

  return NULL;

}


gchar **hyscan_db_get_project_list( HyScanDB *db )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_project_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_project_list( db );

  return NULL;

}


gint32 hyscan_db_open_project( HyScanDB *db, const gchar *project_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_project !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_project( db, project_name );

  return -1;

}


gint32 hyscan_db_create_project( HyScanDB *db, const gchar *project_name, const gchar *project_type )
{

  if( HYSCAN_DB_GET_CLASS( db )->create_project !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->create_project( db, project_name, project_type );

  return -1;

}


gboolean hyscan_db_remove_project( HyScanDB *db, const gchar *project_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_project !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_project( db, project_name );

  return FALSE;

}


void hyscan_db_close_project( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->close_project !=  NULL )
    HYSCAN_DB_GET_CLASS( db )->close_project( db, project_id );

}


GDateTime *hyscan_db_get_project_ctime( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_project_ctime !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_project_ctime( db, project_id );

  return NULL;

}


gchar **hyscan_db_get_track_list( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_track_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_track_list( db, project_id );

  return NULL;

}


gint32 hyscan_db_open_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_track !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_track( db, project_id, track_name );

  return -1;

}


gint32 hyscan_db_create_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->create_track !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->create_track( db, project_id, track_name );

  return -1;

}


gboolean hyscan_db_remove_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_track !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_track( db, project_id, track_name );

  return FALSE;

}


void hyscan_db_close_track( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->close_track !=  NULL )
    HYSCAN_DB_GET_CLASS( db )->close_track( db, track_id );

}


GDateTime *hyscan_db_get_track_ctime( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_track_ctime !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_track_ctime( db, track_id );

  return NULL;

}


gchar **hyscan_db_get_channel_list( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_channel_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_channel_list( db, track_id );

  return NULL;

}


gint32 hyscan_db_open_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_channel !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_channel( db, track_id, channel_name );

  return -1;

}


gint32 hyscan_db_create_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->create_channel !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->create_channel( db, track_id, channel_name );

  return -1;

}


gboolean hyscan_db_remove_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_channel !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_channel( db, track_id, channel_name );

  return FALSE;

}


void hyscan_db_close_channel( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->close_channel !=  NULL )
    HYSCAN_DB_GET_CLASS( db )->close_channel( db, channel_id );

}


gint32 hyscan_db_open_channel_param( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_channel_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_channel_param( db, channel_id );

  return -1;

}


gboolean hyscan_db_set_channel_chunk_size( HyScanDB *db, gint32 channel_id, gint32 chunk_size )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_channel_chunk_size !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_channel_chunk_size( db, channel_id, chunk_size );

  return FALSE;

}


gboolean hyscan_db_set_channel_save_time( HyScanDB *db, gint32 channel_id, gint64 save_time )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_channel_save_time !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_channel_save_time( db, channel_id, save_time );

  return FALSE;

}


gboolean hyscan_db_set_channel_save_size( HyScanDB *db, gint32 channel_id, gint64 save_size )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_channel_save_size !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_channel_save_size( db, channel_id, save_size );

  return FALSE;

}


void hyscan_db_finalize_channel( HyScanDB *db, gint32 channel_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->finalize_channel !=  NULL )
    HYSCAN_DB_GET_CLASS( db )->finalize_channel( db, channel_id );

}


gboolean hyscan_db_get_channel_data_range( HyScanDB *db, gint32 channel_id, gint32 *first_index, gint32 *last_index )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_channel_data_range !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_channel_data_range( db, channel_id, first_index, last_index );

  return FALSE;

}


gboolean hyscan_db_add_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gpointer data, gint32 size, gint32 *index )
{

  if( HYSCAN_DB_GET_CLASS( db )->add_channel_data !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->add_channel_data( db, channel_id, time, data, size, index );

  return FALSE;

}


gboolean hyscan_db_get_channel_data( HyScanDB *db, gint32 channel_id, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_channel_data !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_channel_data( db, channel_id, index, buffer, buffer_size, time );

  return FALSE;

}


gboolean hyscan_db_find_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime )
{

  if( HYSCAN_DB_GET_CLASS( db )->find_channel_data !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->find_channel_data( db, channel_id, time, lindex, rindex, ltime, rtime );

  return FALSE;

}


gchar **hyscan_db_get_project_param_list( HyScanDB *db, gint32 project_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_project_param_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_project_param_list( db, project_id );

  return NULL;

}


gint32 hyscan_db_open_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_project_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_project_param( db, project_id, group_name );

  return -1;

}


gboolean hyscan_db_remove_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_project_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_project_param( db, project_id, group_name );

  return FALSE;

}


gchar **hyscan_db_get_track_param_list( HyScanDB *db, gint32 track_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_track_param_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_track_param_list( db, track_id );

  return NULL;

}


gint32 hyscan_db_open_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->open_track_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->open_track_param( db, track_id, group_name );

  return -1;

}


gboolean hyscan_db_remove_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_track_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_track_param( db, track_id, group_name );

  return FALSE;

}


gchar **hyscan_db_get_param_list( HyScanDB *db, gint32 param_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_param_list !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_param_list( db, param_id );

  return NULL;

}


gboolean hyscan_db_copy_param( HyScanDB *db, gint32 src_param_id, gint32 dst_param_id, const gchar *mask )
{

  if( HYSCAN_DB_GET_CLASS( db )->copy_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->copy_param( db, src_param_id, dst_param_id, mask );

  return FALSE;

}


gboolean hyscan_db_remove_param( HyScanDB *db, gint32 param_id, const gchar *mask )
{

  if( HYSCAN_DB_GET_CLASS( db )->remove_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->remove_param( db, param_id, mask );

  return FALSE;

}


void hyscan_db_close_param( HyScanDB *db, gint32 param_id )
{

  if( HYSCAN_DB_GET_CLASS( db )->close_param !=  NULL )
    HYSCAN_DB_GET_CLASS( db )->close_param( db, param_id );

}


gint64 hyscan_db_inc_integer_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_CLASS( db )->inc_integer_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->inc_integer_param( db, param_id, name );

  return 0;

}


gboolean hyscan_db_set_integer_param( HyScanDB *db, gint32 param_id, const gchar *name, gint64 value )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_integer_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_integer_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_double_param( HyScanDB *db, gint32 param_id, const gchar *name, gdouble value )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_double_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_double_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name, gboolean value )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_boolean_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_boolean_param( db, param_id, name, value );

  return FALSE;

}


gboolean hyscan_db_set_string_param( HyScanDB *db, gint32 param_id, const gchar *name, const gchar *value )
{

  if( HYSCAN_DB_GET_CLASS( db )->set_string_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->set_string_param( db, param_id, name, value );

  return FALSE;

}


gint64 hyscan_db_get_integer_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_integer_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_integer_param( db, param_id, name );

  return 0;

}


gdouble hyscan_db_get_double_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_double_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_double_param( db, param_id, name );

  return 0.0;

}


gboolean hyscan_db_get_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_boolean_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_boolean_param( db, param_id, name );

  return FALSE;

}


gchar *hyscan_db_get_string_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  if( HYSCAN_DB_GET_CLASS( db )->get_string_param !=  NULL )
    return HYSCAN_DB_GET_CLASS( db )->get_string_param( db, param_id, name );

  return NULL;

}
