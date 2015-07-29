/*!
 * \file hyscan-db-file.c
 *
 * \brief Исходный файл класса файловой базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 *
*/

#include "hyscan-db-file.h"
#include "hyscan-db-channel-file.h"
#include "hyscan-db-param-file.h"

#include <glib/gstdio.h>
#include <string.h>


#define HYSCAN_DB_FILE_API   20150700            // Версия API файловой базы данных.

#define MAX_CHANNEL_PARTS    999999              // Максимальное число частей данных канала,
                                                 // должно совпадать с MAX_PARTS в файле hyscan-db-channel-file.c.


enum { PROP_O, PROP_PATH };


typedef struct HyScanDBFileProjectInfo {         // Информация о проекте.

  gint32                     id;                 // Идентификатор сессии.
  gint                       ref_counts;         // Число ссылок на проект.

  gchar                     *project_name;       // Название проекта.
  gchar                     *path;               // Путь к каталогу с проектом.

} HyScanDBFileProjectInfo;


typedef struct HyScanDBFileTrackInfo {           // Информация о галсе.

  gint32                     id;                 // Идентификатор сессии.
  gint                       ref_counts;         // Число ссылок на галс.

  gchar                     *project_name;       // Название проекта.
  gchar                     *track_name;         // Название галса.
  gchar                     *path;               // Путь к каталогу с галсом.

} HyScanDBFileTrackInfo;


typedef struct HyScanDBFileChannelInfo {         // Информация о канале данных.

  gint32                     id;                 // Идентификатор сессии.
  gint                       ref_counts;         // Число ссылок на канал данных.

  gchar                     *project_name;       // Название проекта.
  gchar                     *track_name;         // Название галса.
  gchar                     *channel_name;       // Название канала данных.
  gchar                     *path;               // Путь к каталогу с каналом данных.

  HyScanDBChannelFile       *channel;            // Объект канала данных.
  gboolean                   readonly;           // Канал открыт только для чтения.

} HyScanDBFileChannelInfo;


typedef struct HyScanDBFileParamInfo {           // Информация о параметрах.

  gint32                     id;                 // Идентификатор сессии.
  gint                       ref_counts;         // Число ссылок на параметры.

  gchar                     *project_name;       // Название проекта.
  gchar                     *track_name;         // Название галса.
  gchar                     *group_name;         // Название группы параметров.
  gchar                     *path;               // Путь к каталогу с параметрами.

  HyScanDBParamFile         *param;              // Объект параметров.
  gboolean                   readonly;           // Параметры открыты только для чтения.

} HyScanDBFileParamInfo;


typedef struct HyScanDBFileObjectName {          // Информация об объекте базы данных.

  gchar                     *project_name;       // Название проекта.
  gchar                     *track_name;         // Название галса.
  gchar                     *channel_name;       // Название канала данных.

} HyScanDBFileObjectName;


typedef struct HyScanDBFilePriv {                // Внутренние данные объекта.

  gchar                     *path;               // Путь к каталогу с проектами.

  gint32                     next_id;            // Следующий используемый идентификатор объектов.

  GHashTable                *projects;           // Список открытых проектов.
  GRWLock                    projects_lock;      // Блокировка операций над списком проектов.

  GHashTable                *tracks;             // Список открытых галсов.
  GRWLock                    tracks_lock;        // Блокировка операций над списком галсов.

  GHashTable                *channels;           // Список открытых каналов данных.
  GRWLock                    channels_lock;      // Блокировка операций над списком каналов данных.

  GHashTable                *params;             // Список открытых групп параметров.
  GRWLock                    params_lock;        // Блокировка операций над списком групп параметров.

} HyScanDBFilePriv;


#define HYSCAN_DB_FILE_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), G_TYPE_HYSCAN_DB_FILE, HyScanDBFilePriv ) )


static void hyscan_db_file_interface_init( HyScanDBInterface *iface );
static void hyscan_db_file_set_property( HyScanDBFile *db, guint prop_id, const GValue *value, GParamSpec *pspec );
static GObject* hyscan_db_file_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );
static void hyscan_db_file_object_finalize( HyScanDBFile *db );

static void hyscan_db_remove_project_info( gpointer value );
static void hyscan_db_remove_track_info( gpointer value );
static void hyscan_db_remove_channel_info( gpointer value );
static void hyscan_db_remove_param_info( gpointer value );


G_DEFINE_TYPE_WITH_CODE( HyScanDBFile, hyscan_db_file, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE( G_TYPE_HYSCAN_DB, hyscan_db_file_interface_init ) );


static void hyscan_db_file_init( HyScanDBFile *db ){ ; }


static void hyscan_db_file_class_init( HyScanDBFileClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_db_file_set_property;

  this_class->constructor = hyscan_db_file_object_constructor;
  this_class->finalize = hyscan_db_file_object_finalize;

  g_object_class_install_property( this_class, PROP_PATH,
    g_param_spec_string( "path", "Path", "Path to projects", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_type_class_add_private( klass, sizeof( HyScanDBFilePriv ) );

}


static void hyscan_db_file_set_property( HyScanDBFile *db, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  switch ( prop_id )
    {

    case PROP_PATH:
      priv->path = g_value_dup_string( value );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( db, prop_id, pspec );
      break;

    }

}


// Конструктор объекта.
static GObject* hyscan_db_file_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *db = G_OBJECT_CLASS( hyscan_db_file_parent_class )->constructor( g_type, n_construct_properties, construct_properties );

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  // Начальные значения.
  priv->next_id = 1;
  priv->projects = g_hash_table_new_full( g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_project_info );
  priv->tracks = g_hash_table_new_full( g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_track_info );
  priv->channels = g_hash_table_new_full( g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_channel_info );
  priv->params = g_hash_table_new_full( g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_param_info );

  g_rw_lock_init( &priv->projects_lock );
  g_rw_lock_init( &priv->tracks_lock );
  g_rw_lock_init( &priv->channels_lock );
  g_rw_lock_init( &priv->params_lock );

  return db;

}


// Деструктор объекта.
static void hyscan_db_file_object_finalize( HyScanDBFile *db )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  // Удаляем списки открытых объектов базы данных.
  g_hash_table_destroy( priv->params );
  g_hash_table_destroy( priv->channels );
  g_hash_table_destroy( priv->tracks );
  g_hash_table_destroy( priv->projects );

  // Удаляем мьютексы доступа к спискам данных.
  g_rw_lock_clear( &priv->projects_lock );
  g_rw_lock_clear( &priv->tracks_lock );
  g_rw_lock_clear( &priv->channels_lock );
  g_rw_lock_clear( &priv->params_lock );

  g_free( priv->path );

}


// Вспомогательная функция для удаления структуры HyScanDBFileProjectInfo.
static void hyscan_db_remove_project_info( gpointer value )
{

  HyScanDBFileProjectInfo *project_info = value;

  g_free( project_info->project_name );
  g_free( project_info->path );

  g_free( project_info );

}


// Вспомогательная функция для удаления структуры HyScanDBFileTrackInfo.
static void hyscan_db_remove_track_info( gpointer value )
{

  HyScanDBFileTrackInfo *track_info = value;

  g_free( track_info->project_name );
  g_free( track_info->track_name );
  g_free( track_info->path );

  g_free( track_info );

}


// Вспомогательная функция для удаления структуры HyScanDBFileChannelInfo.
static void hyscan_db_remove_channel_info( gpointer value )
{

  HyScanDBFileChannelInfo *channel_info = value;

  g_object_unref( channel_info->channel );

  g_free( channel_info->project_name );
  g_free( channel_info->track_name );
  g_free( channel_info->channel_name );
  g_free( channel_info->path );

  g_free( channel_info );

}


// Вспомогательная функция для удаления структуры HyScanDBFileParamInfo.
static void hyscan_db_remove_param_info( gpointer value )
{

  HyScanDBFileParamInfo *param_info = value;

  g_object_unref( param_info->param );

  g_free( param_info->project_name );
  g_free( param_info->track_name );
  g_free( param_info->group_name );
  g_free( param_info->path );

  g_free( param_info );

}


// Вспомогательная функция поиска открытых проектов по имени проекта.
static gboolean hyscan_db_check_project_by_object_name( gpointer key, gpointer value, gpointer data )
{

  HyScanDBFileProjectInfo *project_info = value;
  HyScanDBFileObjectName *object_name = data;

  if( object_name->project_name != NULL && g_strcmp0( project_info->project_name, object_name->project_name ) ) return FALSE;

  return TRUE;

}


// Вспомогательная функция поиска открытых галсов по имени галса.
static gboolean hyscan_db_check_track_by_object_name( gpointer key, gpointer value, gpointer data )
{

  HyScanDBFileTrackInfo *track_info = value;
  HyScanDBFileObjectName *object_name = data;

  if( object_name->project_name != NULL && g_strcmp0( track_info->project_name, object_name->project_name ) ) return FALSE;
  if( object_name->track_name != NULL && g_strcmp0( track_info->track_name, object_name->track_name ) ) return FALSE;

  return TRUE;

}


// Вспомогательная функция поиска открытых каналов данных по имени канала.
static gboolean hyscan_db_check_channel_by_object_name( gpointer key, gpointer value, gpointer data )
{

  HyScanDBFileChannelInfo *channel_info = value;
  HyScanDBFileObjectName *object_name = data;

  if( object_name->project_name != NULL && g_strcmp0( channel_info->project_name, object_name->project_name ) ) return FALSE;
  if( object_name->track_name != NULL && g_strcmp0( channel_info->track_name, object_name->track_name ) ) return FALSE;
  if( object_name->channel_name != NULL && g_strcmp0( channel_info->channel_name, object_name->channel_name ) ) return FALSE;

  return TRUE;

}


// Вспомогательная функция поиска открытых групп параметров по имени группы.
static gboolean hyscan_db_check_param_by_object_name( gpointer key, gpointer value, gpointer data )
{

  HyScanDBFileParamInfo *channel_info = value;
  HyScanDBFileObjectName *object_name = data;

  if( object_name->project_name != NULL && g_strcmp0( channel_info->project_name, object_name->project_name ) ) return FALSE;
  if( object_name->track_name != NULL && g_strcmp0( channel_info->track_name, object_name->track_name ) ) return FALSE;
  if( object_name->channel_name != NULL && g_strcmp0( channel_info->group_name, object_name->channel_name ) ) return FALSE;

  return TRUE;

}


// Функция проверяет, что каталог path содержит проект.
static gboolean hyscan_db_project_test( const gchar *path )
{

  gboolean status = FALSE;

  GKeyFile *project_param = g_key_file_new();
  gchar *project_param_file = g_strdup_printf( "%s%sproject.ini", path, G_DIR_SEPARATOR_S );

  // Загружаем файл с информацией о проекте.
  if( !g_key_file_load_from_file( project_param, project_param_file, G_KEY_FILE_NONE, NULL ) ) goto exit;

  // Проверяем версию API файловой базы данных.
  if( g_key_file_get_integer( project_param, "project", "version", NULL ) != HYSCAN_DB_FILE_API ) goto exit;

  // Этот каталог содержит проект.
  status = TRUE;

  exit:

  g_key_file_free( project_param );
  g_free( project_param_file );

  return status;

}


// Функция проверяет, что каталог path содержит галс.
static gboolean hyscan_db_track_test( const gchar *path )
{

  gboolean status = FALSE;

  GKeyFile *track_param = g_key_file_new();
  gchar *track_param_file = g_strdup_printf( "%s%strack.ini", path, G_DIR_SEPARATOR_S );

  // Загружаем файл с информацией о галсе.
  if( !g_key_file_load_from_file( track_param, track_param_file, G_KEY_FILE_NONE, NULL ) ) goto exit;

  // Проверяем версию API галса.
  if( g_key_file_get_integer( track_param, "track", "version", NULL ) != HYSCAN_DB_FILE_API ) goto exit;

  // Этот каталог содержит галс.
  status = TRUE;

  exit:

  g_key_file_free( track_param );
  g_free( track_param_file );

  return status;

}


// Функция проверяет, что галс в каталоге path содержит канал name.
static gboolean hyscan_db_channel_test( const gchar *path, const gchar *name )
{

  gboolean status = TRUE;
  gchar *channel_file = NULL;

  // Проверяем наличие файла name.000000.d
  channel_file = g_strdup_printf( "%s%s%s.000000.d", path, G_DIR_SEPARATOR_S, name );
  if( !g_file_test( channel_file, G_FILE_TEST_IS_REGULAR ) ) status = FALSE;
  g_free( channel_file );

  // Проверяем наличие файла name.000000.i
  channel_file = g_strdup_printf( "%s%s%s.000000.i", path, G_DIR_SEPARATOR_S, name );
  if( !g_file_test( channel_file, G_FILE_TEST_IS_REGULAR ) ) status = FALSE;
  g_free( channel_file );

  return status;

}


// Функция возвращает список доступных групп параметров в указанном каталоге.
static gchar **hyscan_db_file_get_directory_param_list( const gchar *path, const gchar *reserved_name )
{

  GDir *dir;
  const gchar *name;
  gchar **params = NULL;

  GError *error = NULL;
  gint i = 0;

  // Открываем каталог с группами параметров.
  error = NULL;
  dir = g_dir_open( path, 0, &error );
  if( dir == NULL )
    {
    g_critical( "hyscan_db_file_get_directory_param_list: %s", error->message );
    return NULL;
    }

  // Проверяем все найденые ini файлы. Если имя файла без расширения
  // совпадает с restrict_name или имеется канал данных с таким именем,
  // то исключаем это имя из списка групп параметров.
  while( ( name = g_dir_read_name( dir ) ) != NULL )
    {

    gchar *sub_path;
    gchar **group_name;

    // Пропускаем все каталоги.
    sub_path = g_strdup_printf( "%s%s%s", path, G_DIR_SEPARATOR_S, name );
    if( g_file_test( sub_path, G_FILE_TEST_IS_DIR ) )
      { g_free( sub_path ); continue; }
    g_free( sub_path );

    // Пропускаем все файлы с расширением не '.ini'.
    if( !g_str_has_suffix( name, ".ini" ) ) continue;

    // Название группы параметров.
    group_name = g_strsplit( name, ".", 2 );
    if( g_strv_length( group_name ) != 2 || strlen( group_name[0] ) + strlen( group_name[1] ) != strlen( name ) - 1 )
      { g_strfreev( group_name ); continue; }

    // Проверяем, что имя не совпадает с зарезервированным.
    if( g_strcmp0( group_name[0], reserved_name ) == 0 )
      { g_strfreev( group_name ); continue; }

    // Проверяем, что нет канала данных с таким именем.
    if( hyscan_db_channel_test( path, group_name[0] ) )
      { g_strfreev( group_name ); continue; }

    // Список групп параметров.
    params = g_realloc( params, 16 * ( ( ( i + 1 ) / 16 ) + 1 ) * sizeof( gpointer ) );
    params[i] = g_strdup( group_name[0] );
    i++; params[i] = NULL;

    g_strfreev( group_name );

    }

  g_dir_close( dir );

  return params;

}


// Функция рекурсивно удаляет каталог со всеми его файлами и подкаталогами.
static gboolean hyscan_db_file_remove_directory( const gchar *path )
{

  gboolean status = TRUE;
  const gchar *name;

  // Открываем каталог для удаления.
  GDir *dir = g_dir_open( path, 0, NULL );
  if( dir == NULL ) return status;

  // Проходимся по всем элементам в нём.
  while( ( name = g_dir_read_name( dir ) ) != NULL )
    {

    gchar *sub_path = g_strdup_printf( "%s%s%s", path, G_DIR_SEPARATOR_S, name );

    // Если это каталог - рекурсивно удаляем его.
    if( g_file_test( sub_path, G_FILE_TEST_IS_DIR ) )
      status = hyscan_db_file_remove_directory( sub_path );

    // Файл удаляем непосредственно.
    else
      if( g_unlink( sub_path ) != 0 )
        {
        g_critical( "hyscan_db_file_remove_directory: can't remove file %s", sub_path );
        status = FALSE;
        }

    g_free( sub_path );

    if( !status ) break;

    }

  g_dir_close( dir );

  if( g_rmdir( path ) != 0 )
    {
    g_critical( "hyscan_db_file_remove_directory: can't remove directory %s", path );
    status = FALSE;
    }

  return status;

}


// Функция удаляет все файлы в каталоге path относящиеся к каналу name.
static gboolean hyscan_db_file_remove_channel_files( const gchar *path, const gchar *name )
{

  gboolean status = TRUE;
  gchar *channel_file = NULL;
  gint i;

  // Удаляем файл с параметрами, если он есть.
  channel_file = g_strdup_printf( "%s%s%s.ini", path, G_DIR_SEPARATOR_S, name );
  if( g_file_test( channel_file, G_FILE_TEST_IS_REGULAR ) )
    if( g_unlink( channel_file ) != 0 )
      {
      g_critical( "hyscan_db_file_remove_channel_files: can't remove file %s", channel_file );
      status = FALSE;
      }
  g_free( channel_file );

  if( !status ) return status;

  // Удаляем файлы индексов.
  for( i = 0; i < MAX_CHANNEL_PARTS; i++ )
    {

    channel_file = g_strdup_printf( "%s%s%s.%06d.i", path, G_DIR_SEPARATOR_S, name, i );

    // Если файлы закончились - выходим.
    if( !g_file_test( channel_file, G_FILE_TEST_IS_REGULAR ) )
      { g_free( channel_file ); break; }

    if( g_unlink( channel_file ) != 0 )
      {
      g_critical( "hyscan_db_file_remove_channel_files: can't remove file %s", channel_file );
      status = FALSE;
      }
    g_free( channel_file );

    if( !status ) return status;

    }

  // Удаляем файлы данных.
  for( i = 0; i < MAX_CHANNEL_PARTS; i++ )
    {

    channel_file = g_strdup_printf( "%s%s%s.%06d.d", path, G_DIR_SEPARATOR_S, name, i );

    // Если файлы закончились - выходим.
    if( !g_file_test( channel_file, G_FILE_TEST_IS_REGULAR ) )
      { g_free( channel_file ); break; }

    if( g_unlink( channel_file ) != 0 )
      {
      g_critical( "hyscan_db_file_remove_channel_files: can't remove file %s", channel_file );
      status = FALSE;
      }
    g_free( channel_file );

    if( !status ) return status;

    }

  return status;

}


// Функция возвращает список доступных проектов.
gchar **hyscan_db_file_get_project_list( HyScanDB *db )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  GDir *db_dir;
  const gchar *project_name;
  gchar **projects = NULL;

  GError *error = NULL;
  gint i = 0;

  g_rw_lock_reader_lock( &priv->projects_lock );

  // Открываем каталог с проектами.
  db_dir = g_dir_open( priv->path, 0, &error );
  if( db_dir == NULL )
    {
    g_critical( "hyscan_db_file_get_project_list: %s", error->message );
    goto exit;
    }

  // Проверяем все найденые каталоги - содержат они проект или нет.
  while( ( project_name = g_dir_read_name( db_dir ) ) != NULL )
    {

    gboolean status = FALSE;
    gchar *project_path = g_strdup_printf( "%s%s%s", priv->path, G_DIR_SEPARATOR_S, project_name );

    // Проверяем содержит каталог проект или нет.
    status = hyscan_db_project_test( project_path );
    g_free( project_path );

    if( !status ) continue;

    // Список проектов.
    projects = g_realloc( projects, 16 * ( ( ( i + 1 ) / 16 ) + 1 ) * sizeof( gpointer ) );
    projects[i] = g_strdup( project_name );
    i++; projects[i] = NULL;

    }

  exit:

  g_rw_lock_reader_unlock( &priv->projects_lock );

  if( db_dir != NULL ) g_dir_close( db_dir );

  return projects;

}


// Функция открывает проект для работы.
gint32 hyscan_db_file_open_project( HyScanDB *db, const gchar *project_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  gint32 id = -1;
  gchar *project_path = NULL;
  HyScanDBFileProjectInfo *project_info = NULL;
  HyScanDBFileObjectName object_name;

  g_rw_lock_writer_lock( &priv->projects_lock );

  // Полное имя проекта.
  object_name.project_name = (gchar*)project_name;
  object_name.track_name = NULL;
  object_name.channel_name = NULL;

  // Ищем проект в списке открытых.  Если нашли - используем.
  project_info = g_hash_table_find( priv->projects, hyscan_db_check_project_by_object_name, &object_name );
  if( project_info != NULL )
    {
    project_info->ref_counts += 1;
    id = project_info->id;
    goto exit;
    }

  // Проверяем, что каталог содержит проект.
  project_path = g_strdup_printf( "%s%s%s", priv->path, G_DIR_SEPARATOR_S, project_name );
  if( !hyscan_db_project_test( project_path ) )
    {
    g_critical( "hyscan_db_file_open_project: '%s' not a project", project_name );
    goto exit;
    }

  // Открываем проект для работы.
  project_info = g_malloc( sizeof( HyScanDBFileProjectInfo ) );
  project_info->project_name = g_strdup( project_name );
  project_info->id = priv->next_id++;
  project_info->path = project_path;
  project_info->ref_counts = 1;
  project_path = NULL;

  id = project_info->id;
  g_hash_table_insert( priv->projects, GINT_TO_POINTER( id ), project_info );

  exit:

  g_rw_lock_writer_unlock( &priv->projects_lock );

  g_free( project_path );

  return id;

}


// Функция создаёт новый проект и открывает его для работы.
gint32 hyscan_db_file_create_project( HyScanDB *db, const gchar *project_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  gboolean status = FALSE;

  GKeyFile *project_param = NULL;
  gchar *project_path = NULL;
  gchar *project_param_file = NULL;
  gchar *param_data = NULL;

  g_rw_lock_writer_lock( &priv->projects_lock );

  project_param = g_key_file_new();
  project_path = g_strdup_printf( "%s%s%s", priv->path, G_DIR_SEPARATOR_S, project_name );
  project_param_file = g_strdup_printf( "%s%s%s%sproject.ini", priv->path, G_DIR_SEPARATOR_S, project_name, G_DIR_SEPARATOR_S );

  // Проверяем, что каталога с названием проекта нет.
  if( g_file_test( project_path, G_FILE_TEST_IS_DIR ) )
    {
//    g_critical( "hyscan_db_file_create_project: project '%s' already exists", project_name );
    goto exit;
    }

  // Создаём каталог для проекта.
  if( g_mkdir_with_parents( project_path, 0777 ) != 0  )
    {
    g_critical( "hyscan_db_file_create_project: can't create project '%s' directory", project_name );
    goto exit;
    }

  // Файл параметров проекта.
  g_key_file_set_integer( project_param, "project", "version", HYSCAN_DB_FILE_API );
  g_key_file_set_int64( project_param, "project", "ctime", g_get_real_time() / G_USEC_PER_SEC );
  param_data = g_key_file_to_data( project_param, NULL, NULL );
  if( !g_file_set_contents( project_param_file, param_data, -1, NULL ) )
    {
    g_critical( "hyscan_db_file_create_project: can't save project '%s' parameters", project_name );
    goto exit;
    }

  status = TRUE;

  exit:

  g_rw_lock_writer_unlock( &priv->projects_lock );

  g_key_file_free( project_param );
  g_free( project_param_file );
  g_free( project_path );
  g_free( param_data );

  if( status ) return hyscan_db_file_open_project( db, project_name );
  return -1;

}


// Функция закрывает проект.
void hyscan_db_file_close_project( HyScanDB *db, gint32 project_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;

  g_rw_lock_writer_lock( &priv->projects_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Уменьшили счётчик ссылок, если счётчик станет равным нулю - закрываем проект.
  project_info->ref_counts -= 1;
  if( project_info->ref_counts > 0 ) goto exit;

  g_hash_table_remove( priv->projects, GINT_TO_POINTER( project_info->id ) );

  exit:

  g_rw_lock_writer_unlock( &priv->projects_lock );

}


// Функция удаляет проект.
gboolean hyscan_db_file_remove_project( HyScanDB *db, const gchar *project_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileObjectName object_name;

  gboolean status = FALSE;
  gchar *project_path = NULL;

  g_rw_lock_writer_lock( &priv->projects_lock );

  // Полное имя проекта.
  object_name.project_name = (gchar*)project_name;
  object_name.track_name = NULL;
  object_name.channel_name = NULL;

  // Ищем все открытые каналы данных этого проекта и закрываем их.
  while( TRUE )
    {
    HyScanDBFileChannelInfo *channel_info = g_hash_table_find( priv->channels, hyscan_db_check_channel_by_object_name, &object_name );
    if( channel_info != NULL ) g_hash_table_remove( priv->channels, GINT_TO_POINTER( channel_info->id ) );
    else break;
    }

  // Ищем все открытые галсы этого проекта и закрываем их.
  while( TRUE )
    {
    HyScanDBFileTrackInfo *track_info = g_hash_table_find( priv->tracks, hyscan_db_check_track_by_object_name, &object_name );
    if( track_info != NULL ) g_hash_table_remove( priv->tracks, GINT_TO_POINTER( track_info->id ) );
    else break;
    }

  // Ищем все открытые группы параметров этого проекта и закрываем их.
  while( TRUE )
    {
    HyScanDBFileParamInfo *param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
    if( param_info != NULL ) g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );
    else break;
    }

  // Если проект открыт закрываем его.
  {
  HyScanDBFileProjectInfo *project_info = g_hash_table_find( priv->projects, hyscan_db_check_project_by_object_name, &object_name );
  if( project_info != NULL ) g_hash_table_remove( priv->projects, GINT_TO_POINTER( project_info->id ) );
  }

  project_path = g_strdup_printf( "%s%s%s", priv->path, G_DIR_SEPARATOR_S, project_name );

  // Проверяем, что каталог содержит проект.
  if( !( status = hyscan_db_project_test( project_path ) ) )
    {
    g_critical( "hyscan_db_file_remove_project: '%s' not a project", project_name );
    goto exit;
    }

  // Удаляем каталог с проектом.
  status = hyscan_db_file_remove_directory( project_path );

  exit:

  g_rw_lock_writer_unlock( &priv->projects_lock );

  g_free( project_path );

  return status;

}


// Функция возвращает список доступных галсов проекта.
gchar **hyscan_db_file_get_track_list( HyScanDB *db, gint32 project_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;

  GDir *db_dir = NULL;
  const gchar *track_name;
  gchar **tracks = NULL;

  GError *error;
  gint i = 0;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_reader_lock( &priv->tracks_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Открываем каталог проекта с галсами.
  error = NULL;
  db_dir = g_dir_open( project_info->path, 0, &error );
  if( db_dir == NULL )
    {
    g_critical( "hyscan_db_file_get_track_list: %s", error->message );
    goto exit;
    }

  // Проверяем все найденые каталоги - содержат они галс или нет.
  while( ( track_name = g_dir_read_name( db_dir ) ) != NULL )
    {

    gboolean status = FALSE;
    gchar *track_path = g_strdup_printf( "%s%s%s", project_info->path, G_DIR_SEPARATOR_S, track_name );

    // Проверяем содержит каталог галс или нет.
    status = hyscan_db_track_test( track_path );
    g_free( track_path );

    if( !status ) continue;

    // Список галсов.
    tracks = g_realloc( tracks, 16 * ( ( ( i + 1 ) / 16 ) + 1 ) * sizeof( gpointer ) );
    tracks[i] = g_strdup( track_name );
    i++; tracks[i] = NULL;

    }

  exit:

  g_rw_lock_reader_unlock( &priv->tracks_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  if( db_dir != NULL ) g_dir_close( db_dir );

  return tracks;

}


// Функция открывает галс для работы.
gint32 hyscan_db_file_open_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  gint32 id = -1;
  gchar *track_path = NULL;
  HyScanDBFileProjectInfo *project_info = NULL;
  HyScanDBFileTrackInfo *track_info = NULL;
  HyScanDBFileObjectName object_name;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_writer_lock( &priv->tracks_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Полное имя галса.
  object_name.project_name = project_info->project_name;
  object_name.track_name = (gchar*)track_name;
  object_name.channel_name = NULL;

  // Ищем галс в списке открытых. Если нашли - используем.
  track_info = g_hash_table_find( priv->tracks, hyscan_db_check_track_by_object_name, &object_name );
  if( track_info != NULL )
    {
    track_info->ref_counts += 1;
    id = track_info->id;
    goto exit;
    }

  // Проверяем, что каталог содержит галс.
  track_path = g_strdup_printf( "%s%s%s", project_info->path, G_DIR_SEPARATOR_S, track_name );
  if( !hyscan_db_track_test( track_path ) )
    {
    g_critical( "hyscan_db_file_open_track: '%s.%s' not a track", project_info->project_name, track_name );
    goto exit;
    }

  // Открываем проект для работы.
  track_info = g_malloc( sizeof( HyScanDBFileTrackInfo ) );
  track_info->project_name = g_strdup( project_info->project_name );
  track_info->track_name = g_strdup( track_name );
  track_info->id = priv->next_id++;
  track_info->ref_counts = 1;
  track_info->path = track_path;
  track_path = NULL;

  id = track_info->id;
  g_hash_table_insert( priv->tracks, GINT_TO_POINTER( id ), track_info );

  exit:

  g_rw_lock_writer_unlock( &priv->tracks_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  g_free( track_path );

  return id;

}


// Функция создаёт новый галс и открывает его для работы.
gint32 hyscan_db_file_create_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;

  gboolean status = FALSE;

  GKeyFile *track_param = NULL;
  gchar *track_path = NULL;
  gchar *track_param_file = NULL;
  gchar *param_data = NULL;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_writer_lock( &priv->tracks_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  track_param = g_key_file_new();
  track_path = g_strdup_printf( "%s%s%s", project_info->path, G_DIR_SEPARATOR_S, track_name );
  track_param_file = g_strdup_printf( "%s%s%s%strack.ini", project_info->path, G_DIR_SEPARATOR_S, track_name, G_DIR_SEPARATOR_S );

  // Проверяем, что каталога с названием проекта нет.
  if( g_file_test( track_path, G_FILE_TEST_IS_DIR ) )
    {
//    g_critical( "hyscan_db_file_create_track: track '%s.%s' already exists", project_info->project_name, track_name );
    goto exit;
    }

  // Создаём каталог для проекта.
  if( g_mkdir_with_parents( track_path, 0777 ) != 0  )
    {
    g_critical( "hyscan_db_file_create_track: can't create track '%s.s%s' directory", project_info->project_name, track_name );
    goto exit;
    }

  // Файл параметров проекта.
  g_key_file_set_integer( track_param, "track", "version", HYSCAN_DB_FILE_API );
  g_key_file_set_int64( track_param, "track", "ctime", g_get_real_time() / G_USEC_PER_SEC );
  param_data = g_key_file_to_data( track_param, NULL, NULL );
  if( !g_file_set_contents( track_param_file, param_data, -1, NULL ) )
    {
    g_critical( "hyscan_db_file_create_track: can't save track '%s.%s' parameters", project_info->project_name, track_name );
    goto exit;
    }

  status = TRUE;

  exit:

  g_rw_lock_writer_unlock( &priv->tracks_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  if( track_param != NULL ) g_key_file_free( track_param );
  g_free( track_param_file );
  g_free( track_path );
  g_free( param_data );

  if( status ) return hyscan_db_file_open_track( db, project_id, track_name );
  return -1;

}


// Функция закрывает галс.
void hyscan_db_file_close_track( HyScanDB *db, gint32 track_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;

  g_rw_lock_writer_lock( &priv->tracks_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Уменьшили счётчик ссылок, если счётчик станет равным нулю - закрываем галс.
  track_info->ref_counts -= 1;
  if( track_info->ref_counts > 0 ) goto exit;

  g_hash_table_remove( priv->tracks, GINT_TO_POINTER( track_info->id ) );

  exit:

  g_rw_lock_writer_unlock( &priv->tracks_lock );

}


// Функция удаляет галс.
gboolean hyscan_db_file_remove_track( HyScanDB *db, gint32 project_id, const gchar *track_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileObjectName object_name;

  gboolean status = FALSE;
  gchar *track_path = NULL;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_writer_lock( &priv->tracks_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Полное имя галса.
  object_name.project_name = project_info->project_name;
  object_name.track_name = (gchar*)track_name;
  object_name.channel_name = NULL;

  // Ищем все открытые каналы данных этого галса и закрываем их.
  while( TRUE )
    {
    HyScanDBFileChannelInfo *channel_info = g_hash_table_find( priv->channels, hyscan_db_check_channel_by_object_name, &object_name );
    if( channel_info != NULL ) g_hash_table_remove( priv->channels, GINT_TO_POINTER( channel_info->id ) );
    else break;
    }

  // Ищем все открытые группы параметров этого галса и закрываем их.
  while( TRUE )
    {
    HyScanDBFileParamInfo *param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
    if( param_info != NULL ) g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );
    else break;
    }

  // Если галс открыт закрываем его.
  {
  HyScanDBFileTrackInfo *track_info = g_hash_table_find( priv->tracks, hyscan_db_check_track_by_object_name, &object_name );
  if( track_info != NULL ) g_hash_table_remove( priv->tracks, GINT_TO_POINTER( track_info->id ) );
  }

  track_path = g_strdup_printf( "%s%s%s", project_info->path, G_DIR_SEPARATOR_S, track_name );

  // Проверяем, что каталог содержит галс.
  if( !( status = hyscan_db_track_test( track_path ) ) )
    {
    g_critical( "hyscan_db_file_remove_track: '%s.%s' not a track", project_info->project_name, track_name );
    goto exit;
    }

  // Удаляем каталог с галсом.
  status = hyscan_db_file_remove_directory( track_path );

  exit:

  g_rw_lock_writer_unlock( &priv->tracks_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  g_free( track_path );

  return status;

}


// Функция возвращает список доступных каналов данных галса.
gchar **hyscan_db_file_get_channel_list( HyScanDB *db, gint32 track_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;

  GDir *db_dir = NULL;
  const gchar *file_name;
  gchar **channels = NULL;

  GError *error;
  gint i = 0;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Открываем каталог галса.
  error = NULL;
  db_dir = g_dir_open( track_info->path, 0, &error );
  if( db_dir == NULL )
    {
    g_critical( "hyscan_db_file_get_channel_list: %s", error->message );
    goto exit;
    }

  // Проверяем все найденые файлы на совпадение с именем name.000000.d
  while( ( file_name = g_dir_read_name( db_dir ) ) != NULL )
    {

    gboolean status = FALSE;
    gchar **splited_channel_name = g_strsplit( file_name, ".", 2 );
    gchar *channel_name = NULL;

    if( splited_channel_name == NULL ) continue;

    // Если совпадение найдено проверяем, что существует канал с именем name.
    if( g_strcmp0( splited_channel_name[1], "000000.d" ) == 0 )
      status = hyscan_db_channel_test( track_info->path, splited_channel_name[0] );

    channel_name = splited_channel_name[0];
    if( !status ) g_free( splited_channel_name[0] );
    if( splited_channel_name[1] != NULL ) g_free( splited_channel_name[1] );
    g_free( splited_channel_name );

    if( !status ) continue;

    // Список каналов.
    channels = g_realloc( channels, 16 * ( ( ( i + 1 ) / 16 ) + 1 ) * sizeof( gpointer ) );
    channels[i] = channel_name;
    i++; channels[i] = NULL;

    }

  exit:

  g_rw_lock_reader_unlock( &priv->channels_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  if( db_dir != NULL ) g_dir_close( db_dir );

  return channels;

}


// Функция создаёт или открывает существующий канал данных.
gint32 hyscan_db_file_open_channel_int( HyScanDB *db, gint32 track_id, const gchar *channel_name, gboolean readonly )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  gint32 id = -1;

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileObjectName object_name;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_writer_lock( &priv->channels_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Полное имя канала данных.
  object_name.project_name = track_info->project_name;
  object_name.track_name = track_info->track_name;
  object_name.channel_name = (gchar*)channel_name;

  // Ищем канал в списке открытых.
  // Если канал уже открыт и находится в режиме записи - возвращаем ошибку, если
  // запрошено создание, или используем.
  channel_info = g_hash_table_find( priv->channels, hyscan_db_check_channel_by_object_name, &object_name );
  if( channel_info != NULL )
    {
    if( readonly )
      {
      channel_info->ref_counts += 1;
      id = channel_info->id;
      }
//    else
//      g_critical( "hyscan_db_file_create_channel: channel '%s.%s.%s' already exists", track_info->project_name, track_info->track_name, channel_name );
    goto exit;
    }

  // Если канала нет и запрашивается его открытие на чтение или
  // если канал есть и запрашивается открытие на запись - ошибка.
  if( hyscan_db_channel_test( track_info->path, channel_name ) != readonly )
    {
    if( readonly ) g_critical( "hyscan_db_file_open_channel: '%s.%s.%s' - no such channel", track_info->project_name, track_info->track_name, channel_name );
    else g_critical( "hyscan_db_file_create_channel: project '%s.%s.%s' already created", track_info->project_name, track_info->track_name, channel_name );
    goto exit;
    }

  // Открываем канал для работы.
  channel_info = g_malloc( sizeof( HyScanDBFileChannelInfo ) );
  channel_info->project_name = g_strdup( track_info->project_name );
  channel_info->track_name = g_strdup( track_info->track_name );
  channel_info->channel_name = g_strdup( channel_name );
  channel_info->path = g_strdup( track_info->path );
  channel_info->id = priv->next_id++;
  channel_info->ref_counts = 1;
  channel_info->readonly = readonly;
  channel_info->channel = g_object_new( G_TYPE_HYSCAN_DB_CHANNEL_FILE, "path", track_info->path, "name", channel_name, "readonly", readonly, NULL );

  id = channel_info->id;
  g_hash_table_insert( priv->channels, GINT_TO_POINTER( id ), channel_info );

  exit:

  g_rw_lock_writer_unlock( &priv->channels_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  return id;

}


// Функция открывает существующий канал данных.
gint32 hyscan_db_file_open_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  return hyscan_db_file_open_channel_int( db, track_id, channel_name, TRUE );

}


// Функция создаёт канал данных.
gint32 hyscan_db_file_create_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{

  return hyscan_db_file_open_channel_int( db, track_id, channel_name, FALSE );

}


// Функция закрывает канал данных.
void hyscan_db_file_close_channel( HyScanDB *db, gint32 channel_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;

  g_rw_lock_writer_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info == NULL ) goto exit;

  // Уменьшили счётчик ссылок, если счётчик станет равным нулю - закрываем канал данных.
  channel_info->ref_counts -= 1;
  if( channel_info->ref_counts > 0 ) goto exit;

  g_hash_table_remove( priv->channels, GINT_TO_POINTER( channel_info->id ) );

  exit:

  g_rw_lock_writer_unlock( &priv->channels_lock );

}


// Функция удаляет канал данных.
gboolean hyscan_db_file_remove_channel( HyScanDB *db, gint32 track_id, const gchar *channel_name )
{


  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_writer_lock( &priv->channels_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Полное имя канала данных.
  object_name.project_name = track_info->project_name;
  object_name.track_name = track_info->track_name;
  object_name.channel_name = (gchar*)channel_name;

  // Ищем канал в списке открытых, при необходимости закрываем.
  channel_info = g_hash_table_find( priv->channels, hyscan_db_check_channel_by_object_name, &object_name );
  if( channel_info != NULL ) g_hash_table_remove( priv->channels, GINT_TO_POINTER( channel_info->id ) );

  // Ищем параметры канала в списке открытых, при необходимости закрываем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL ) g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );

  // Удаляем файлы канала данных.
  status = hyscan_db_file_remove_channel_files( track_info->path, channel_name );

  exit:

  g_rw_lock_writer_unlock( &priv->channels_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  return status;

}


// Функция открывает группу параметров канала данных.
gint32 hyscan_db_file_open_channel_param( HyScanDB *db, gint32 channel_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  gint32 id = -1;

  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  g_rw_lock_reader_lock( &priv->channels_lock );
  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info == NULL ) goto exit;

  // Полное имя группы параметров.
  object_name.project_name = channel_info->project_name;
  object_name.track_name = channel_info->track_name;
  object_name.channel_name = channel_info->channel_name;

  // Ищем группу параметров в списке открытых.  Если нашли - используем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL )
    {
    param_info->ref_counts += 1;
    id = param_info->id;
    goto exit;
    }

  // Открываем группу параметров для работы.
  param_info = g_malloc( sizeof( HyScanDBFileParamInfo ) );
  param_info->project_name = g_strdup( channel_info->project_name );
  param_info->track_name = g_strdup( channel_info->track_name );
  param_info->group_name = g_strdup( channel_info->channel_name );
  param_info->path = g_strdup( channel_info->path );
  param_info->id = priv->next_id++;
  param_info->ref_counts = 1;
  param_info->param = g_object_new( G_TYPE_HYSCAN_DB_PARAM_FILE, "path", channel_info->path, "name", channel_info->channel_name, "readonly", channel_info->readonly, NULL );

  id = param_info->id;
  g_hash_table_insert( priv->params, GINT_TO_POINTER( id ), param_info );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->channels_lock );

  return id;

}


// Функция устанавливает максимальный размер файла данных канала.
gboolean hyscan_db_file_set_channel_chunk_size( HyScanDB *db, gint32 channel_id, gint32 chunk_size )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_set_channel_chunk_size( channel_info->channel, chunk_size );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция устанавливает интервал времени в микросекундах для которого хранятся записываемые данные.
gboolean hyscan_db_file_set_channel_save_time( HyScanDB *db, gint32 channel_id, gint64 save_time )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_set_channel_save_time( channel_info->channel, save_time );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция устанавливает максимальный объём сохраняемых данных.
gboolean hyscan_db_file_set_channel_save_size( HyScanDB *db, gint32 channel_id, gint64 save_size )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_set_channel_save_size( channel_info->channel, save_size );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция завершает запись в канал данных.
void hyscan_db_file_finalize_channel( HyScanDB *db, gint32 channel_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    hyscan_db_channel_file_finalize_channel( channel_info->channel );

  g_rw_lock_reader_unlock( &priv->channels_lock );

}


// Функция возвращает диапазон текущих значений индексов данных.
gboolean hyscan_db_file_get_channel_data_range( HyScanDB *db, gint32 channel_id, gint32 *first_index, gint32 *last_index )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_get_channel_data_range( channel_info->channel, first_index, last_index );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция записывает новые данные канала.
gboolean hyscan_db_file_add_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gpointer data, gint32 size, gint32 *index )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_add_channel_data( channel_info->channel, time, data, size, index );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция считывает данные.
gboolean hyscan_db_file_get_channel_data( HyScanDB *db, gint32 channel_id, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_get_channel_data( channel_info->channel, index, buffer, buffer_size, time );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция ищет данные по метке времени.
gboolean hyscan_db_file_find_channel_data( HyScanDB *db, gint32 channel_id, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->channels_lock );

  // Ищем канал данных в списке открытых.
  channel_info = g_hash_table_lookup( priv->channels, GINT_TO_POINTER( channel_id ) );
  if( channel_info != NULL )
    status = hyscan_db_channel_file_find_channel_data( channel_info->channel, time, lindex, rindex, ltime, rtime );

  g_rw_lock_reader_unlock( &priv->channels_lock );

  return status;

}


// Функция возвращает список доступных групп параметров проекта.
gchar **hyscan_db_file_get_project_param_list( HyScanDB *db, gint32 project_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;

  gchar **params = NULL;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Список групп параметров в каталоге проекта.
  params = hyscan_db_file_get_directory_param_list( project_info->path, "project" );

  exit:

  g_rw_lock_reader_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  return params;

}


// Функция открывает группу параметров проекта.
gint32 hyscan_db_file_open_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  gint32 id = -1;

  // project - зарезервированное имя для проекта.
  if( g_strcmp0( group_name, "project" ) == 0 ) return id;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Полное имя группы параметров.
  object_name.project_name = project_info->project_name;
  object_name.track_name = NULL;
  object_name.channel_name = (gchar*)group_name;

  // Ищем группу параметров в списке открытых.  Если нашли - используем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL )
    {
    param_info->ref_counts += 1;
    id = param_info->id;
    goto exit;
    }

  // Открываем группу параметров для работы.
  param_info = g_malloc( sizeof( HyScanDBFileParamInfo ) );
  param_info->project_name = g_strdup( project_info->project_name );
  param_info->track_name = NULL;
  param_info->group_name = g_strdup( group_name );
  param_info->path = g_strdup( project_info->path );
  param_info->id = priv->next_id++;
  param_info->ref_counts = 1;
  param_info->param = g_object_new( G_TYPE_HYSCAN_DB_PARAM_FILE, "path", project_info->path, "name", group_name, "readonly", FALSE, NULL );

  id = param_info->id;
  g_hash_table_insert( priv->params, GINT_TO_POINTER( id ), param_info );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  return id;

}


// Функция удаляет группу параметров проекта.
gboolean hyscan_db_file_remove_project_param( HyScanDB *db, gint32 project_id, const gchar *group_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  gchar *param_path;

  gboolean status = FALSE;

  // project - зарезервированное имя для проекта.
  if( g_strcmp0( group_name, "project" ) == 0 ) return status;

  g_rw_lock_reader_lock( &priv->projects_lock );
  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем проект в списке открытых.
  project_info = g_hash_table_lookup( priv->projects, GINT_TO_POINTER( project_id ) );
  if( project_info == NULL ) goto exit;

  // Полное имя группы параметров.
  object_name.project_name = project_info->project_name;
  object_name.track_name = NULL;
  object_name.channel_name = (gchar*)group_name;

  // Ищем группу параметров в списке открытых.  Если нашли закрываем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL ) g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );

  // Путь к файлу с параметрами.
  param_path = g_strdup_printf( "%s%s%s.ini", project_info->path, G_DIR_SEPARATOR_S, group_name );

  status = TRUE;
  // Удаляем файл с параметрами, если он существует.
  if( g_file_test( param_path, G_FILE_TEST_IS_REGULAR ) )
    if( g_unlink( param_path ) != 0 )
      {
      g_critical( "hyscan_db_file_remove_project_param: can't remove file: %s", param_path );
      status = FALSE;
      }

  g_free( param_path );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->projects_lock );

  return status;

}


// Функция возвращает список доступных групп параметров галса.
gchar **hyscan_db_file_get_track_param_list( HyScanDB *db, gint32 track_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;

  gchar **params = NULL;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Список групп параметров в каталоге галса.
  params = hyscan_db_file_get_directory_param_list( track_info->path, "track" );

  exit:

  g_rw_lock_reader_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  return params;

}


// Функция открывает группу параметров галса.
gint32 hyscan_db_file_open_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  gint32 id = -1;

  // track - зарезервированное имя для галса.
  if( g_strcmp0( group_name, "track" ) == 0 ) return id;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Полное имя группы параметров.
  object_name.project_name = track_info->project_name;
  object_name.track_name = track_info->track_name;
  object_name.channel_name = (gchar*)group_name;

  // Ищем группу параметров в списке открытых.  Если нашли - используем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL )
    {
    param_info->ref_counts += 1;
    id = param_info->id;
    goto exit;
    }

  // Открываем группу параметров для работы.
  param_info = g_malloc( sizeof( HyScanDBFileParamInfo ) );
  param_info->project_name = g_strdup( track_info->project_name );
  param_info->track_name = g_strdup( track_info->track_name );
  param_info->group_name = g_strdup( group_name );
  param_info->path = g_strdup( track_info->path );
  param_info->id = priv->next_id++;
  param_info->ref_counts = 1;
  param_info->param = g_object_new( G_TYPE_HYSCAN_DB_PARAM_FILE, "path", track_info->path, "name", group_name, "readonly", FALSE, NULL );

  id = param_info->id;
  g_hash_table_insert( priv->params, GINT_TO_POINTER( id ), param_info );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  return id;

}


// Функция удаляет группу параметров галса.
gboolean hyscan_db_file_remove_track_param( HyScanDB *db, gint32 track_id, const gchar *group_name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectName object_name;

  gchar *param_path;

  gboolean status = FALSE;

  // track - зарезервированное имя для галса.
  if( g_strcmp0( group_name, "track" ) == 0 ) return status;

  g_rw_lock_reader_lock( &priv->tracks_lock );
  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем галс в списке открытых.
  track_info = g_hash_table_lookup( priv->tracks, GINT_TO_POINTER( track_id ) );
  if( track_info == NULL ) goto exit;

  // Полное имя группы параметров.
  object_name.project_name = track_info->project_name;
  object_name.track_name = track_info->track_name;
  object_name.channel_name = (gchar*)group_name;

  // Ищем группу параметров в списке открытых.  Если нашли закрываем.
  param_info = g_hash_table_find( priv->params, hyscan_db_check_param_by_object_name, &object_name );
  if( param_info != NULL ) g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );

  // Путь к файлу с параметрами.
  param_path = g_strdup_printf( "%s%s%s.ini", track_info->path, G_DIR_SEPARATOR_S, group_name );

  status = TRUE;
  // Удаляем файл с параметрами, если он существует.
  if( g_file_test( param_path, G_FILE_TEST_IS_REGULAR ) )
    if( g_unlink( param_path ) != 0 )
      {
      g_critical( "hyscan_db_file_remove_track_param: can't remove file: %s", param_path );
      status = FALSE;
      }

  g_free( param_path );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );
  g_rw_lock_reader_unlock( &priv->tracks_lock );

  return status;

}


// Функция устанавливает значение параметра типа integer.
gboolean hyscan_db_file_set_integer_param( HyScanDB *db, gint32 param_id, const gchar *name, gint64 value )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    status = hyscan_db_param_file_set_integer( param_info->param, name, value );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return status;

}


// Функция устанавливает значение параметра типа double.
gboolean hyscan_db_file_set_double_param( HyScanDB *db, gint32 param_id, const gchar *name, gdouble value )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    status = hyscan_db_param_file_set_double( param_info->param, name, value );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return status;

}


// Функция устанавливает значение параметра типа double.
gboolean hyscan_db_file_set_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name, gboolean value )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    status = hyscan_db_param_file_set_boolean( param_info->param, name, value );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return status;

}


// Функция устанавливает значение параметра типа string.
gboolean hyscan_db_file_set_string_param( HyScanDB *db, gint32 param_id, const gchar *name, const gchar *value )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    status = hyscan_db_param_file_set_string( param_info->param, name, value );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return status;

}


// Функция возвращает значение параметра типа integer.
gint64 hyscan_db_file_get_integer_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gint64 value = 0;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    value = hyscan_db_param_file_get_integer( param_info->param, name );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return value;

}


// Функция возвращает значение параметра типа double.
gdouble hyscan_db_file_get_double_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gdouble value = 0.0;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    value = hyscan_db_param_file_get_double( param_info->param, name );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return value;

}


// Функция возвращает значение параметра типа boolean.
gboolean hyscan_db_file_get_boolean_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gboolean value = FALSE;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    value = hyscan_db_param_file_get_boolean( param_info->param, name );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return value;

}


// Функция возвращает значение параметра типа string.
gchar *hyscan_db_file_get_string_param( HyScanDB *db, gint32 param_id, const gchar *name )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;
  gchar *value = NULL;

  g_rw_lock_reader_lock( &priv->params_lock );

  // Ищем группу параметров в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info != NULL )
    value = hyscan_db_param_file_get_string( param_info->param, name );

  g_rw_lock_reader_unlock( &priv->params_lock );

  return value;

}


// Функция закрывает группу параметров.
void hyscan_db_file_close_param( HyScanDB *db, gint32 param_id )
{

  HyScanDBFilePriv *priv = HYSCAN_DB_FILE_GET_PRIVATE( db );

  HyScanDBFileParamInfo *param_info;

  g_rw_lock_writer_lock( &priv->params_lock );

  // Ищем канал данных в списке открытых.
  param_info = g_hash_table_lookup( priv->params, GINT_TO_POINTER( param_id ) );
  if( param_info == NULL ) goto exit;

  // Уменьшили счётчик ссылок, если счётчик станет равным нулю - закрываем канал данных.
  param_info->ref_counts -= 1;
  if( param_info->ref_counts > 0 ) goto exit;

  g_hash_table_remove( priv->params, GINT_TO_POINTER( param_info->id ) );

  exit:

  g_rw_lock_writer_unlock( &priv->params_lock );

}


// Настройка интерфейса.
static void hyscan_db_file_interface_init( HyScanDBInterface *iface )
{

  iface->get_project_list = hyscan_db_file_get_project_list;
  iface->open_project = hyscan_db_file_open_project;
  iface->create_project = hyscan_db_file_create_project;
  iface->close_project = hyscan_db_file_close_project;
  iface->remove_project = hyscan_db_file_remove_project;

  iface->get_track_list = hyscan_db_file_get_track_list;
  iface->open_track = hyscan_db_file_open_track;
  iface->create_track = hyscan_db_file_create_track;
  iface->close_track = hyscan_db_file_close_track;
  iface->remove_track = hyscan_db_file_remove_track;

  iface->get_channel_list = hyscan_db_file_get_channel_list;
  iface->open_channel = hyscan_db_file_open_channel;
  iface->create_channel = hyscan_db_file_create_channel;
  iface->close_channel = hyscan_db_file_close_channel;
  iface->remove_channel = hyscan_db_file_remove_channel;
  iface->open_channel_param = hyscan_db_file_open_channel_param;

  iface->set_channel_chunk_size = hyscan_db_file_set_channel_chunk_size;
  iface->set_channel_save_time = hyscan_db_file_set_channel_save_time;
  iface->set_channel_save_size = hyscan_db_file_set_channel_save_size;
  iface->finalize_channel = hyscan_db_file_finalize_channel;

  iface->get_channel_data_range = hyscan_db_file_get_channel_data_range;
  iface->add_channel_data = hyscan_db_file_add_channel_data;
  iface->get_channel_data = hyscan_db_file_get_channel_data;
  iface->find_channel_data = hyscan_db_file_find_channel_data;

  iface->get_project_param_list = hyscan_db_file_get_project_param_list;
  iface->open_project_param = hyscan_db_file_open_project_param;
  iface->remove_project_param = hyscan_db_file_remove_project_param;

  iface->get_track_param_list = hyscan_db_file_get_track_param_list;
  iface->open_track_param = hyscan_db_file_open_track_param;
  iface->remove_track_param = hyscan_db_file_remove_track_param;

  iface->set_integer_param = hyscan_db_file_set_integer_param;
  iface->set_double_param = hyscan_db_file_set_double_param;
  iface->set_boolean_param = hyscan_db_file_set_boolean_param;
  iface->set_string_param = hyscan_db_file_set_string_param;
  iface->get_integer_param = hyscan_db_file_get_integer_param;
  iface->get_double_param = hyscan_db_file_get_double_param;
  iface->get_boolean_param = hyscan_db_file_get_boolean_param;
  iface->get_string_param = hyscan_db_file_get_string_param;
  iface->close_param = hyscan_db_file_close_param;

}

#warning "Check for next_id"
#warning "replace g_file_set_contents with gio"
