/*!
 * \file hyscan-db-param-file.c
 *
 * \brief Исходный файл класса хранения параметров в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-db-param-file.h"

#include <gio/gio.h>
#include <string.h>


enum { PROP_O, PROP_PATH, PROP_NAME, PROP_READONLY };


typedef struct HyScanDBParamFilePriv {           // Внутренние данные объекта.

  gchar                    *path;                // Путь к каталогу с файлом параметров.
  gchar                    *name;                // Название файла параметров.
  gchar                    *file;                // Полное имя файла параметров.

  gboolean                  readonly;            // Создавать или нет файлы при открытии канала.
  gboolean                  fail;                // Признак ошибки.

  GRWLock                   mutex;               // Блокировка доступа к объекту.

  GKeyFile                 *params;              // Параметры.
  GFile                    *fd;                  // Объект работы с файлом параметров.
  GOutputStream            *ofd;                 // Поток записи файла параметров.

} HyScanDBParamFilePriv;


#define HYSCAN_DB_PARAM_FILE_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), G_TYPE_HYSCAN_DB_PARAM_FILE, HyScanDBParamFilePriv ) )


static void hyscan_db_param_file_set_property( HyScanDBParamFile *param, guint prop_id, const GValue *value, GParamSpec *pspec );
static GObject* hyscan_db_param_file_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );
static void hyscan_db_param_file_object_finalize( HyScanDBParamFile *param );

static gboolean hyscan_db_param_file_parse_name( const gchar *name, gchar **group, gchar **key );


G_DEFINE_TYPE( HyScanDBParamFile, hyscan_db_param_file, G_TYPE_OBJECT )


static void hyscan_db_param_file_init( HyScanDBParamFile *param ){ ; }


static void hyscan_db_param_file_class_init( HyScanDBParamFileClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_db_param_file_set_property;

  this_class->constructor = hyscan_db_param_file_object_constructor;
  this_class->finalize = hyscan_db_param_file_object_finalize;

  g_object_class_install_property( this_class, PROP_NAME,
    g_param_spec_string( "name", "Name", "Parameters group name", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_PATH,
    g_param_spec_string( "path", "Path", "Path to parameters group", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_READONLY,
    g_param_spec_boolean( "readonly", "ReadOnly", "Read only mode", FALSE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_type_class_add_private( klass, sizeof( HyScanDBParamFilePriv ) );

}


static void hyscan_db_param_file_set_property( HyScanDBParamFile *param, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  switch ( prop_id )
    {

    case PROP_NAME:
      priv->name = g_value_dup_string( value );
      break;

    case PROP_PATH:
      priv->path = g_value_dup_string( value );
      break;

    case PROP_READONLY:
      priv->readonly = g_value_get_boolean( value );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( param, prop_id, pspec );
      break;

    }

}


// Конструктор объекта.
static GObject* hyscan_db_param_file_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *param = G_OBJECT_CLASS( hyscan_db_param_file_parent_class )->constructor( g_type, n_construct_properties, construct_properties );

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  GError *error;

  // Начальные значения.
  priv->fail = FALSE;
  priv->fd = NULL;
  priv->ofd = NULL;

  g_rw_lock_init( &priv->mutex );

  // Параметры.
  priv->params = g_key_file_new();

  // Имя файла с параметрами.
  priv->file = g_strdup_printf( "%s%s%s.ini", priv->path, G_DIR_SEPARATOR_S, priv->name );

  // Если файл с параметрами существует, загружаем его.
  error = NULL;
  if( !g_key_file_load_from_file( priv->params, priv->file, G_KEY_FILE_NONE, &error ) )
    {
    // Если произошла ошибка при загрузке параметров не связанная с существованием файла - сигнализируем о ней.
    if( error->code != G_FILE_ERROR_NOENT )
      {
      g_critical( "hyscan_db_param_file_object_constructor: %s: %s", priv->file, error->message );
      priv->fail = TRUE;
      }
    g_error_free( error );
    }

  // Объект для записи содержимого файла параметров.
  if( !priv->readonly )
    {
    priv->fd = g_file_new_for_path( priv->file );
    priv->ofd = G_OUTPUT_STREAM( g_file_append_to( priv->fd, G_FILE_CREATE_NONE, NULL, NULL ) );
    }

  return param;

}


// Деструктор объекта.
static void hyscan_db_param_file_object_finalize( HyScanDBParamFile *param )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  g_free( priv->file );
  g_free( priv->name );
  g_free( priv->path );

  g_key_file_free( priv->params );

  if( priv->ofd != NULL ) g_object_unref( priv->ofd );
  if( priv->fd != NULL ) g_object_unref( priv->fd );

  g_rw_lock_clear( &priv->mutex );

}


// Функция разбирает имя параметра на две составляющие: группа и ключ.
static gboolean hyscan_db_param_file_parse_name( const gchar *name, gchar **group, gchar **key )
{

  gchar **path = g_strsplit( name, ".", 0 );

  // Имя без группы - добавляем имя группы 'default'.
  if( g_strv_length( path ) == 1 )
    {
    path[1] = path[0];
    path[0] = g_strdup( "default" );
    }

  // Проверяем, что имя представленно в виде 'группа.ключ'.
  else if( g_strv_length( path ) != 2 || strlen( path[0] ) + strlen( path[1] ) != strlen( name ) - 1 )
    {
    g_critical( "hyscan_db_param_file_parse_name: syntax error in key name %s", name );
    g_strfreev( path );
    return FALSE;
    }

  *group = path[0];
  *key = path[1];

  g_free( path );

  return TRUE;

}


// Функция записывает текущие параметры в файл.
static gboolean hyscan_db_param_file_flush_params( GKeyFile *params, GOutputStream *ofd )
{

  gchar *data;
  gsize dsize;

  GError *error;

  data = g_key_file_to_data( params, &dsize, NULL );

  // Обнуляем файл.
  error = NULL;
  if( !g_seekable_truncate( G_SEEKABLE( ofd ), 0, NULL, &error ) )
    {
    g_critical( "hyscan_db_param_file_flush_params: %s", error->message );
    goto exit;
    }

  // Записываем в него новые данные.
  error = NULL;
  if( g_output_stream_write( ofd, data, dsize, NULL, &error ) != dsize )
    {
    if( error != NULL )
      g_critical( "hyscan_db_param_file_flush_params: %s", error->message );
    else
      g_critical( "hyscan_db_param_file_flush_params: can't flush parameters" );
    goto exit;
    }

  // Очищаем IO буфер.
  error = NULL;
  if( !g_output_stream_flush( ofd, NULL, &error ) )
    {
    g_critical( "hyscan_db_param_file_flush_params: %s", error->message );
    goto exit;
    }

  exit:

  g_free( data );
  if( error == NULL ) return TRUE;

  g_error_free( error );
  return FALSE;

}


// Функция возвращает список параметров.
gchar **hyscan_db_param_file_get_param_list( HyScanDBParamFile *param )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar **names = NULL;
  gchar **groups;
  gchar **keys;
  gint i, j, k;

  if( priv->fail ) return NULL;

  g_rw_lock_reader_lock( &priv->mutex );

  // Группы параметров.
  groups = g_key_file_get_groups( priv->params, NULL );
  for( i = 0, k = 0; groups != NULL && groups[i] != NULL; i++ )
    {

    // Параметры в группе.
    keys = g_key_file_get_keys( priv->params, groups[i], NULL, NULL );
    for( j = 0; keys != NULL && keys[j] != NULL; j++ )
      {

      // Список параметров.
      names = g_realloc( names, 16 * ( ( ( k + 1 ) / 16 ) + 1 ) * sizeof( gchar* ) );
      names[k] = g_strdup_printf( "%s.%s", groups[i], keys[j] );
      k++; names[k] = NULL;

      }
    g_strfreev( keys );

    }
  g_strfreev( groups );

  g_rw_lock_reader_unlock( &priv->mutex );

  return names;

}


// Функция удаляет параметры по маске.
gboolean hyscan_db_param_file_remove_param( HyScanDBParamFile *param, const gchar *mask )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  GPatternSpec *pattern;
  gchar **groups;
  gchar **keys;

  gboolean status;
  gint i, j;

  if( priv->fail || priv->readonly ) return FALSE;

  pattern = g_pattern_spec_new( mask );
  if( pattern == NULL ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  // Группы параметров.
  groups = g_key_file_get_groups( priv->params, NULL );
  for( i = 0; groups != NULL && groups[i] != NULL; i++ )
    {

    // Параметры в группе.
    keys = g_key_file_get_keys( priv->params, groups[i], NULL, NULL );
    for( j = 0; keys != NULL && keys[j] != NULL; j++ )
      {

      // Проверяем имя параметра.
      gchar *name = g_strdup_printf( "%s.%s", groups[i], keys[j] );

      // Удаляем если оно совпадает с шаблоном.
      if( g_pattern_match( pattern, strlen( name ), name, NULL ) )
        g_key_file_remove_key( priv->params, groups[i], keys[j], NULL );

      g_free( name );

      }
    g_strfreev( keys );

    // Если группа стала пустой - удаляем её.
    keys = g_key_file_get_keys( priv->params, groups[i], NULL, NULL );
    if( keys != NULL && g_strv_length( keys ) == 0 ) g_key_file_remove_group( priv->params, groups[i], NULL );
    g_strfreev( keys );

    }
  g_strfreev( groups );

  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_pattern_spec_free( pattern );

  return status;

}


// Функция увеличивает значение параметра типа integer на единицу.
gint64 hyscan_db_param_file_inc_integer( HyScanDBParamFile *param, const gchar *name )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gint64 value;
  gchar *group, *key;
  gboolean status;

  if( priv->fail || priv->readonly ) return FALSE;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  value = g_key_file_get_int64( priv->params, group, key, NULL ) + 1;
  g_key_file_set_int64( priv->params, group, key, value );
  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return status;

}


// Функция устанавливает значение параметра типа integer.
gboolean hyscan_db_param_file_set_integer( HyScanDBParamFile *param, const gchar *name, gint64 value )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gboolean status;

  if( priv->fail || priv->readonly ) return FALSE;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  g_key_file_set_int64( priv->params, group, key, value );
  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return status;

}


// Функция устанавливает значение параметра типа double.
gboolean hyscan_db_param_file_set_double( HyScanDBParamFile *param, const gchar *name, gdouble value )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gboolean status;

  if( priv->fail || priv->readonly ) return FALSE;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  g_key_file_set_double( priv->params, group, key, value );
  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return status;

}


// Функция устанавливает значение параметра типа boolean.
gboolean hyscan_db_param_file_set_boolean( HyScanDBParamFile *param, const gchar *name, gboolean value )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gboolean status;

  if( priv->fail || priv->readonly ) return FALSE;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  g_key_file_set_boolean( priv->params, group, key, value );
  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return status;

}


// Функция устанавливает значение параметра типа string.
gboolean hyscan_db_param_file_set_string( HyScanDBParamFile *param, const gchar *name, const gchar *value )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gboolean status;

  if( priv->fail || priv->readonly ) return FALSE;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return FALSE;

  g_rw_lock_writer_lock( &priv->mutex );

  g_key_file_set_string( priv->params, group, key, value );
  status = hyscan_db_param_file_flush_params( priv->params, priv->ofd );

  g_rw_lock_writer_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return status;

}


// Функция возвращает значение параметра типа integer.
gint64 hyscan_db_param_file_get_integer( HyScanDBParamFile *param, const gchar *name )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gint64 value = 0;

  if( priv->fail ) return value;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return value;

  g_rw_lock_reader_lock( &priv->mutex );

  value = g_key_file_get_int64( priv->params, group, key, NULL );

  g_rw_lock_reader_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return value;

}


// Функция возвращает значение параметра типа double.
gdouble hyscan_db_param_file_get_double( HyScanDBParamFile *param, const gchar *name )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gdouble value = 0.0;

  if( priv->fail ) return value;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return value;

  g_rw_lock_reader_lock( &priv->mutex );

  value = g_key_file_get_double( priv->params, group, key, NULL );

  g_rw_lock_reader_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return value;

}


// Функция возвращает значение параметра типа boolean.
gboolean hyscan_db_param_file_get_boolean( HyScanDBParamFile *param, const gchar *name )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gboolean value = FALSE;

  if( priv->fail ) return value;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return value;

  g_rw_lock_reader_lock( &priv->mutex );

  value = g_key_file_get_boolean( priv->params, group, key, NULL );

  g_rw_lock_reader_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return value;

}


// Функция возвращает значение параметра типа string.
gchar *hyscan_db_param_file_get_string( HyScanDBParamFile *param, const gchar *name )
{

  HyScanDBParamFilePriv *priv = HYSCAN_DB_PARAM_FILE_GET_PRIVATE( param );

  gchar *group, *key;
  gchar *value = NULL;

  if( priv->fail ) return value;
  if( !hyscan_db_param_file_parse_name( name, &group, &key) ) return value;

  g_rw_lock_reader_lock( &priv->mutex );

  value = g_key_file_get_string( priv->params, group, key, NULL );

  g_rw_lock_reader_unlock( &priv->mutex );

  g_free( group );
  g_free( key );

  return value;

}
