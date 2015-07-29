/*!
 * \file hyscan-db.h
 *
 * \brief Заголовочный файл интерфейса базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 *
*/

#ifndef _hyscan_db_h
#define _hyscan_db_h

#include <glib-object.h>

G_BEGIN_DECLS


#define G_TYPE_HYSCAN_DB                   hyscan_db_get_type()
#define HYSCAN_DB( obj )                   ( G_TYPE_CHECK_INSTANCE_CAST ( ( obj ), G_TYPE_HYSCAN_DB, HyScanDB ) )
#define HYSCAN_DB_CLASS( vtable )          ( G_TYPE_CHECK_CLASS_CAST ( ( vtable ), G_TYPE_HYSCAN_DB, HyScanDBInterface ) )
#define HYSCAN_DB_GET_CLASS( inst )        ( G_TYPE_INSTANCE_GET_INTERFACE ( ( inst ), G_TYPE_HYSCAN_DB, HyScanDBInterface ) )

GType hyscan_db_get_type( void );


typedef struct HyScanDB HyScanDB;

typedef struct HyScanDBInterface {

  GTypeInterface parent;

  gchar**        (*get_project_list)( HyScanDB *db );
  gint32             (*open_project)( HyScanDB *db, const gchar *project_name );
  gint32           (*create_project)( HyScanDB *db, const gchar *project_name );
  gboolean         (*remove_project)( HyScanDB *db, const gchar *project_name );
  void              (*close_project)( HyScanDB *db, gint32 project_id );

  gchar**          (*get_track_list)( HyScanDB *db, gint32 project_id );
  gint32               (*open_track)( HyScanDB *db, gint32 project_id, const gchar *track_name );
  gint32             (*create_track)( HyScanDB *db, gint32 project_id, const gchar *track_name );
  gboolean           (*remove_track)( HyScanDB *db, gint32 project_id, const gchar *track_name );
  void                (*close_track)( HyScanDB *db, gint32 track_id );

  gchar**        (*get_channel_list)( HyScanDB *db, gint32 track_id );
  gboolean         (*create_channel)( HyScanDB *db, gint32 track_id, const gchar *channel_name );
  gboolean         (*remove_channel)( HyScanDB *db, gint32 track_id, const gchar *channel_name );
  gint32             (*open_channel)( HyScanDB *db, gint32 track_id, const gchar *channel_name );
  void              (*close_channel)( HyScanDB *db, gint32 channel_id );
  gint32       (*open_channel_param)( HyScanDB *db, gint32 channel_id );

  gboolean (*set_channel_chunk_size)( HyScanDB *db, gint32 channel_id, gint32 chunk_size );
  gboolean  (*set_channel_save_time)( HyScanDB *db, gint32 channel_id, gint64 save_time );
  gboolean  (*set_channel_save_size)( HyScanDB *db, gint32 channel_id, gint64 save_size );
  void           (*finalize_channel)( HyScanDB *db, gint32 channel_id );

  gboolean (*get_channel_data_range)( HyScanDB *db, gint32 channel_id, gint32 *first_index, gint32 *last_index );
  gboolean       (*add_channel_data)( HyScanDB *db, gint32 channel_id, gint64 time, gpointer data, gint32 size, gint32 *index );
  gboolean       (*get_channel_data)( HyScanDB *db, gint32 channel_id, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time );
  gboolean      (*find_channel_data)( HyScanDB *db, gint32 channel_id, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime );

  gchar**  (*get_project_param_list)( HyScanDB *db, gint32 project_id );
  gint32       (*open_project_param)( HyScanDB *db, gint32 project_id, const gchar *group_name );
  gboolean   (*remove_project_param)( HyScanDB *db, gint32 project_id, const gchar *group_name );

  gchar**    (*get_track_param_list)( HyScanDB *db, gint32 track_id );
  gint32         (*open_track_param)( HyScanDB *db, gint32 track_id, const gchar *group_name );
  gboolean     (*remove_track_param)( HyScanDB *db, gint32 track_id, const gchar *group_name );

  gboolean      (*set_integer_param)( HyScanDB *db, gint32 param_id, const gchar *name, gint64 value );
  gboolean       (*set_double_param)( HyScanDB *db, gint32 param_id, const gchar *name, gdouble value );
  gboolean      (*set_boolean_param)( HyScanDB *db, gint32 param_id, const gchar *name, gboolean value );
  gboolean       (*set_string_param)( HyScanDB *db, gint32 param_id, const gchar *name, const gchar *value );
  gint64        (*get_integer_param)( HyScanDB *db, gint32 param_id, const gchar *name );
  gdouble        (*get_double_param)( HyScanDB *db, gint32 param_id, const gchar *name );
  gboolean      (*get_boolean_param)( HyScanDB *db, gint32 param_id, const gchar *name );
  gchar*         (*get_string_param)( HyScanDB *db, gint32 param_id, const gchar *name );
  void                (*close_param)( HyScanDB *db, gint32 param_id );

} HyScanDBInterface;


//
// Функции работы с проектами.


gchar **hyscan_db_get_project_list( HyScanDB *db );


gint32 hyscan_db_open_project( HyScanDB *db, const gchar *project_name );


gint32 hyscan_db_create_project( HyScanDB *db, const gchar *project_name );


void hyscan_db_close_project( HyScanDB *db, gint32 project_id );


gboolean hyscan_db_remove_project( HyScanDB *db, const gchar *project_name );


//
// Функции работы с галсами.


gchar **hyscan_db_get_track_list( HyScanDB *db, gint32 project_id );


gint32 hyscan_db_open_track( HyScanDB *db, gint32 project_id, const gchar *track_name );


gint32 hyscan_db_create_track( HyScanDB *db, gint32 project_id, const gchar *track_name );


void hyscan_db_close_track( HyScanDB *db, gint32 track_id );


gboolean hyscan_db_remove_track( HyScanDB *db, gint32 project_id, const gchar *track_name );


//
// Функции работы с каналами данных.


gchar **hyscan_db_get_channel_list( HyScanDB *db, gint32 track_id );


gint32 hyscan_db_open_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name );


gint32 hyscan_db_create_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name );


void hyscan_db_close_channel( HyScanDB *db, gint32 channel_id );


gboolean hyscan_db_remove_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name );


gint32 hyscan_db_open_channel_param( HyScanDB *db, gint32 channel_id );


gboolean hyscan_db_set_channel_chunk_size( HyScanDB *db, gint32 channel_id, gint32 chunk_size );


gboolean hyscan_db_set_channel_save_time( HyScanDB *db, gint32 channel_id, gint64 save_time );


gboolean hyscan_db_set_channel_save_size( HyScanDB *db, gint32 channel_id, gint64 save_size );


void hyscan_db_finalize_channel( HyScanDB *db, gint32 channel_id );


gboolean hyscan_db_get_channel_data_range( HyScanDB *db, gint32 channel_id, gint32 *first_index, gint32 *last_index );


gboolean hyscan_db_add_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gpointer data, gint32 size, gint32 *index );


gboolean hyscan_db_get_channel_data( HyScanDB *db, gint32 channel_id, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time );


gboolean hyscan_db_find_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime );


//
// Функции работы с параметрами.


gchar **hyscan_db_get_project_param_list( HyScanDB *db, gint32 project_id );


gint32 hyscan_db_open_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name );


gboolean hyscan_db_remove_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name );


gchar **hyscan_db_get_track_param_list( HyScanDB *db, gint32 track_id );


gint32 hyscan_db_open_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name );


gboolean hyscan_db_remove_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name );


gboolean hyscan_db_set_integer_param( HyScanDB *db, gint32 param_id, const gchar *name, gint64 value );


gboolean hyscan_db_set_double_param( HyScanDB *db, gint32 param_id, const gchar *name, gdouble value );


gboolean hyscan_db_set_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name, gboolean value );


gboolean hyscan_db_set_string_param( HyScanDB *db, gint32 param_id, const gchar *name, const gchar *value );


gint64 hyscan_db_get_integer_param( HyScanDB *db, gint32 param_id, const gchar *name );


gdouble hyscan_db_get_double_param( HyScanDB *db, gint32 param_id, const gchar *name );


gboolean hyscan_db_get_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name );


gchar *hyscan_db_get_string_param( HyScanDB *db, gint32 param_id, const gchar *name );


void hyscan_db_close_param( HyScanDB *db, gint32 param_id );


G_END_DECLS

#endif // _hyscan_db_h
