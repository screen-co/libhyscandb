
#include "hyscan-db-file.h"

#include <glib/gstdio.h>
#include <string.h>


// Параметры программы.

gchar *db_path = NULL;          // Путь к каталогу с базой данных.
gint projects_num = 8;          // Число проектов в базе данных.
gint tracks_num = 8;            // Число галсов в каждом проекте.
gint channels_num = 8;          // Число каналов данных в каждом галсе.
gint params_num = 8;            // Число групп параметров в каждом проекте и галсе.


int main( int argc, char **argv )
{

  HyScanDB *db;

  gint *project_id;
  gint **track_id;
  gint ***channel_id;
  gint **project_param_id;
  gint ***track_param_id;
  gint ***channel_param_id;

  gint sample_size = 0;
  gchar *sample1 = "THIS IS SAMPLE STRING";
  gchar *sample2 = "SAMPLE STRING THIS IS";
  gchar *buffer = NULL;

  gint i, j, k;

  { // Разбор командной строки.

  gchar **args;
  GError          *error = NULL;
  GOptionContext  *context;
  GOptionEntry     entries[] =
  {
    { "projects", 'p', 0, G_OPTION_ARG_INT, &projects_num, "Number of projects", NULL },
    { "tracks", 't', 0, G_OPTION_ARG_INT, &tracks_num, "Number of tracks per project", NULL },
    { "channels", 'c', 0, G_OPTION_ARG_INT, &channels_num, "Number of channels per track", NULL },
    { "params", 'g', 0, G_OPTION_ARG_INT, &params_num, "Number of grouped parameters per project or track", NULL },
    { NULL }
  };

  #ifdef G_OS_WIN32
    args = g_win32_get_command_line();
  #else
    args = g_strdupv( argv );
  #endif

  context = g_option_context_new( "<db-path>" );
  g_option_context_set_help_enabled( context, TRUE );
  g_option_context_add_main_entries( context, entries, NULL );
  g_option_context_set_ignore_unknown_options( context, FALSE );
  if( !g_option_context_parse_strv( context, &args, &error ) )
    { g_message( error->message ); return -1; }

  if( g_strv_length( args ) != 2 )
    { g_print( "%s", g_option_context_get_help( context, FALSE, NULL ) ); return 0; }

  g_option_context_free( context );

  db_path = args[1];
  g_free( args[0] );
  g_free( args );

  }

  if( projects_num < 2 ) projects_num = 2;
  if( tracks_num < 2 ) tracks_num = 2;
  if( channels_num < 2 ) channels_num = 2;
  if( params_num < 2 ) params_num = 2;

  // Идентификаторы открытых объектов.
  project_id = g_malloc( projects_num * sizeof( gint ) );

  track_id = g_malloc( projects_num * sizeof( gint* ) );
  project_param_id = g_malloc( projects_num * sizeof( gint* ) );
  for( i = 0; i < projects_num; i++ )
    {
    track_id[i] = g_malloc( tracks_num * sizeof( gint ) );
    project_param_id[i] = g_malloc( params_num * sizeof( gint ) );
    }

  channel_id = g_malloc( projects_num * sizeof( gint* ) );
  channel_param_id = g_malloc( projects_num * sizeof( gint* ) );
  track_param_id = g_malloc( projects_num * sizeof( gint* ) );

  for( i = 0; i < projects_num; i++ )
    {
    channel_id[i] = g_malloc( tracks_num * sizeof( gint* ) );
    channel_param_id[i] = g_malloc( tracks_num * sizeof( gint* ) );
    track_param_id[i] = g_malloc( tracks_num * sizeof( gint* ) );
    }

  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      channel_id[i][j] = g_malloc( channels_num * sizeof( gint* ) );
      channel_param_id[i][j] = g_malloc( channels_num * sizeof( gint* ) );
      track_param_id[i][j] = g_malloc( params_num * sizeof( gint* ) );
      }

  sample_size = strlen( sample1 ) + 1;
  buffer = g_malloc( sample_size );


  { // Проверяем, что указанный каталог базы данных пустой.

  g_message( "creating db directory" );

  GDir *dir;

  // Создаём каталог, если его еще нет.
  if( g_mkdir_with_parents( db_path, 0777 ) != 0 ) g_error( "can't create directory '%s'", db_path );

  dir = g_dir_open( db_path, 0, NULL );
  if( dir == NULL ) g_error( "can't open directory '%s'", db_path );
  if( g_dir_read_name( dir ) != NULL ) g_error( "db directory '%s' must be empty", db_path );
  g_dir_close( dir );

  }


  // Создаём объект базы данных.
  db = g_object_new( G_TYPE_HYSCAN_DB_FILE, "path", db_path, NULL );


  // Проверяем, что список проектов изначально пустой.
  g_message( "checking db projects list is empty" );
  if( hyscan_db_get_project_list( db ) != NULL ) g_error( "projects list must be empty" );


  // Создаём проекты.
  g_message( "creating projects" );
  for( i = 0; i < projects_num; i++ )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    project_id[i] = hyscan_db_create_project( db, project_name );
    if( project_id[i] < 0 ) g_error( "can't create '%s'", project_name );
    g_free( project_name );
    }


  // Проверяем названия созданных проектов.
  g_message( "checking projects names" );
  {

  gint sum;
  gchar **projects = hyscan_db_get_project_list( db );
  if( projects == NULL ) g_error( "can't get projects list" );
  if( g_strv_length( projects ) != projects_num ) g_error( "wrong number of projects" );

  for( i = 0, sum = 0; i < projects_num; i++ ) sum += ( i + 1 );
  for( i = 0; projects[i] != NULL; i++ )
    sum -= g_ascii_strtoll( projects[i] + strlen( "Project " ), NULL, 10 );
  if( sum != 0 ) g_error( "wrong projects list" );

  g_strfreev( projects );

  }


  // Проверяем, что нет возможности создать проект с именем уже существующего проекта.
  g_message( "checking projects duplication" );
  for( i = 0; i < projects_num; i++ )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    if( hyscan_db_create_project( db, project_name ) > 0 ) g_error( "'%s' duplicated", project_name );
    g_free( project_name );
    }


  // Проверяем, что при открытии уже открытых проектов возвращаются правильные идентификаторы.
  g_message( "checking opened projects identifiers" );
  for( i = 0; i < projects_num; i++ )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    if( project_id[i] != hyscan_db_open_project( db, project_name ) ) g_error( "wrong opened '%s' id", project_name );
    g_free( project_name );
    }


  // Проверяем, что список галсов в каждом проекте изначально пустой.
  g_message( "checking project tracks list is empty" );
  for( i = 0; i < projects_num; i++ )
    if( hyscan_db_get_track_list( db, project_id[i] ) != NULL ) g_error( "tracks list of 'Project %d' must be empty", i + 1 );


  // Создаём галсы в каждом проекте.
  g_message( "creating tracks" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      gchar *track_name = g_strdup_printf( "Track %d", j + 1 );
      track_id[i][j] = hyscan_db_create_track( db, project_id[i], track_name );
      if( track_id[i][j] < 0 ) g_error( "can't create 'Project %d.%s'", i + 1, track_name );
      g_free( track_name );
      }


  // Проверяем названия созданных галсов.
  g_message( "checking tracks names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **tracks = hyscan_db_get_track_list( db, project_id[i] );
    if( tracks == NULL ) g_error( "can't get tracks list" );
    if( g_strv_length( tracks ) != tracks_num ) g_error( "wrong number of tracks in 'Project %d'", i + 1 );

    for( j = 0, sum = 0; j < tracks_num; j++ ) sum += ( j + 1 );
    for( j = 0; tracks[j] != NULL; j++ )
      sum -= g_ascii_strtoll( tracks[j] + strlen( "Track " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong tracks list in 'Project %d'", i + 1 );

    g_strfreev( tracks );

    }


  // Проверяем, что нет возможности создать галс с именем уже существующего галса.
  g_message( "checking tracks duplication" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      gchar *track_name = g_strdup_printf( "Track %d", j + 1 );
      if( hyscan_db_create_track( db, project_id[i], track_name ) > 0 ) g_error( "'Project %d.%s' duplicated", i + 1, track_name );
      g_free( track_name );
      }


  // Проверяем, что при открытии уже открытых галсов возвращаются правильные идентификаторы.
  g_message( "checking opened tracks identifiers" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      gchar *track_name = g_strdup_printf( "Track %d", j + 1 );
      if( track_id[i][j] != hyscan_db_open_track( db, project_id[i], track_name ) ) g_error( "wrong opened 'Project %d.%s' id", i + 1, track_name );
      g_free( track_name );
      }


  // Проверяем, что список каналов данных в каждом галсе изначально пустой.
  g_message( "checking track channels list is empty" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      if( hyscan_db_get_channel_list( db, track_id[i][j] ) != NULL ) g_error( "channels list of 'Project %d.Track %d' must be empty", i + 1, j + 1 );


  // Создаём каналы данных в каждом галсе.
  g_message( "creating channels" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
      {
      gchar *channel_name = g_strdup_printf( "Channel %d", k + 1 );
      channel_id[i][j][k] = hyscan_db_create_channel( db, track_id[i][j], channel_name );
      if( track_id[i][j] < 0 ) g_error( "can't create 'Project %d.Track %d.%s'", i + 1, j + 1, channel_name );
      if( !hyscan_db_add_channel_data( db, channel_id[i][j][k], 0, sample1, sample_size, NULL ) )
        g_error( "can't add data to 'Project %d.Track %d.%s'", i + 1, j + 1, channel_name );
      g_free( channel_name );
      }


  // Проверяем названия созданных каналов данных.
  g_message( "checking channels names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **channels = hyscan_db_get_channel_list( db, track_id[i][j] );
      if( channels == NULL ) g_error( "can't get channels list" );
      if( g_strv_length( channels ) != channels_num ) g_error( "wrong number of channels in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 0, sum = 0; k < channels_num; k++ ) sum += ( k + 1 );
      for( k = 0; channels[k] != NULL; k++ )
        sum -= g_ascii_strtoll( channels[k] + strlen( "Channel " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong channels list in 'Project %d.Track %d'", i + 1, j + 1 );

      g_strfreev( channels );

      }


  // Проверяем, что нет возможности создать канал данных с именем уже существующего канала.
  g_message( "checking channels duplication" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {
        gchar *channel_name = g_strdup_printf( "Channel %d", k + 1 );
        if( hyscan_db_create_channel( db, track_id[i][j], channel_name ) > 0 ) g_error( "'Project %d.Track %d.%s' duplicated", i + 1, j + 1, channel_name );
        g_free( channel_name );
        }


  // Проверяем, что при открытии уже открытых каналов данных возвращаются правильные идентификаторы.
  g_message( "checking opened channels identifiers" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {
        gchar *channel_name = g_strdup_printf( "Channel %d", k + 1 );
        if( channel_id[i][j][k] != hyscan_db_open_channel( db, track_id[i][j], channel_name ) ) g_error( "wrong opened 'Project %d.Track %d.%s' id", i + 1, j + 1, channel_name );
        g_free( channel_name );
        }


  // Проверяем, что данные записанные в канал совпадают с образцом.
  g_message( "checking channels content" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {
        gint readed_size = sample_size;
        if( !hyscan_db_get_channel_data( db, channel_id[i][j][k], 0, buffer, &sample_size, NULL ) )
          g_error( "can't read data from 'Project %d.Track %d.Channel %d'", i + 1, j + 1, k + 1 );
        if( readed_size != sample_size || g_strcmp0( sample1, buffer ) != 0 )
          g_error( "wrong 'Project %d.Track %d.Channel %d' data", i + 1, j + 1, k + 1 );
        }


  // Устанавливаем параметры канала данных и проверяем их правильность.
  g_message( "setting channel parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {

        gint integer_param = k;
        gdouble double_param = k + 0.1;
        gboolean boolean_param = k % 2 ? TRUE : FALSE;
        gchar *string_param = sample1;

        channel_param_id[i][j][k] = hyscan_db_open_channel_param( db, channel_id[i][j][k] );

        if( !hyscan_db_set_integer_param( db, channel_param_id[i][j][k], "common.integer", integer_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_double_param( db, channel_param_id[i][j][k], "common.double", double_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_boolean_param( db, channel_param_id[i][j][k], "common.boolean", boolean_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' boolean parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_string_param( db, channel_param_id[i][j][k], "common.string", string_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        integer_param = hyscan_db_get_integer_param( db, channel_param_id[i][j][k], "common.integer" );
        double_param = hyscan_db_get_double_param( db, channel_param_id[i][j][k], "common.double" );
        boolean_param = hyscan_db_get_boolean_param( db, channel_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, channel_param_id[i][j][k], "common.string" );

        if( integer_param != k ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( double_param - k > 0.11 ) g_error( "error in 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( boolean_param != ( k % 2 ? TRUE : FALSE ) ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( g_strcmp0( string_param, sample1 ) != 0 ) g_error( "error in 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        g_free( string_param );

        }

  // Проверяем, что параметры канала данных можно изменить и проверяем их правильность.
  g_message( "changing channel parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {

        gint integer_param = 2 * k;
        gdouble double_param = 2 * k + 0.1;
        gboolean boolean_param = k % 2 ? FALSE : TRUE;
        gchar *string_param = sample2;

        channel_param_id[i][j][k] = hyscan_db_open_channel_param( db, channel_id[i][j][k] );

        if( !hyscan_db_set_integer_param( db, channel_param_id[i][j][k], "common.integer", integer_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_double_param( db, channel_param_id[i][j][k], "common.double", double_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_boolean_param( db, channel_param_id[i][j][k], "common.boolean", boolean_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' boolean parameter", i + 1, j + 1, k + 1 );
        if( !hyscan_db_set_string_param( db, channel_param_id[i][j][k], "common.string", string_param ) )
          g_error( "can't set 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        integer_param = hyscan_db_get_integer_param( db, channel_param_id[i][j][k], "common.integer" );
        double_param = hyscan_db_get_double_param( db, channel_param_id[i][j][k], "common.double" );
        boolean_param = hyscan_db_get_boolean_param( db, channel_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, channel_param_id[i][j][k], "common.string" );

        if( integer_param != 2 * k ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( double_param - 2 * k > 0.11 ) g_error( "error in 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( boolean_param != ( k % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( g_strcmp0( string_param, sample2 ) != 0 ) g_error( "error in 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        g_free( string_param );

        }


  // Проверяем, что список групп параметров в каждом проекте изначально пустой.
  g_message( "checking project parameters list is empty" );
  for( i = 0; i < projects_num; i++ )
    if( hyscan_db_get_project_param_list( db, project_id[i] ) != NULL ) g_error( "project parameters list must be empty" );


  // Создаём группы параметров в каждом проекте, устанавливаем параметры и проверяем их правильность
  g_message( "creating project parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < params_num; j++ )
      {

      gchar *project_parameters_name = g_strdup_printf( "Parameters %d", j + 1 );

      gint integer_param = 2 * j;
      gdouble double_param = 2 * j + 0.1;
      gboolean boolean_param = j % 2 ? FALSE : TRUE;
      gchar *string_param = sample1;

      project_param_id[i][j] = hyscan_db_open_project_param( db, project_id[i], project_parameters_name );
      if( project_param_id[i][j] < 0 ) g_error( "can't create 'Project %d.%s'", i + 1, project_parameters_name );

      if( !hyscan_db_set_integer_param( db, project_param_id[i][j], "integer", integer_param ) )
        g_error( "can't set 'Project %d.%s' integer parameter", i + 1, project_parameters_name );
      if( !hyscan_db_set_double_param( db, project_param_id[i][j], "double", double_param ) )
        g_error( "can't set 'Project %d.%s' double parameter", i + 1, project_parameters_name );
      if( !hyscan_db_set_boolean_param( db, project_param_id[i][j], "common.boolean", boolean_param ) )
        g_error( "can't set 'Project %d.%s' boolean parameter", i + 1, project_parameters_name );
      if( !hyscan_db_set_string_param( db, project_param_id[i][j], "common.string", string_param ) )
        g_error( "can't set 'Project %d.%s' string parameter", i + 1, project_parameters_name );

      integer_param = hyscan_db_get_integer_param( db, project_param_id[i][j], "integer" );
      double_param = hyscan_db_get_double_param( db, project_param_id[i][j], "double" );
      boolean_param = hyscan_db_get_boolean_param( db, project_param_id[i][j], "common.boolean" );
      string_param = hyscan_db_get_string_param( db, project_param_id[i][j], "common.string" );

      if( integer_param != 2 * j ) g_error( "error in 'Project %d.%s' integer parameter", i + 1, project_parameters_name );
      if( double_param - 2 * j > 0.11 ) g_error( "error in 'Project %d.%s' double parameter", i + 1, project_parameters_name );
      if( boolean_param != ( j % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.%s' integer parameter", i + 1, project_parameters_name );
      if( g_strcmp0( string_param, sample1 ) != 0 ) g_error( "error in 'Project %d.%s' string parameter", i + 1, project_parameters_name );

      g_free( string_param );
      g_free( project_parameters_name );

      }


  // Проверяем, что при открытии уже открытых групп параметров возвращаются правильные идентификаторы.
  g_message( "checking opened project parameters identifiers" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < params_num; j++ )
      {
      gchar *project_parameters_name = g_strdup_printf( "Parameters %d", j + 1 );
      if( project_param_id[i][j] != hyscan_db_open_project_param( db, project_id[i], project_parameters_name ) )
        g_error( "wrong opened '%s' in 'Project %d' id", project_parameters_name, i + 1 );
      g_free( project_parameters_name );
      }


  // Проверяем названия созданных групп параметров.
  g_message( "checking project parameters names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **projects_parameters = hyscan_db_get_project_param_list( db, project_id[i] );
    if( projects_parameters == NULL ) g_error( "can't get project parameters list" );
    if( g_strv_length( projects_parameters ) != params_num ) g_error( "wrong number of project parameters in 'Project %d'", i + 1 );

    for( j = 0, sum = 0; j < params_num; j++ ) sum += ( j + 1 );
    for( j = 0; projects_parameters[j] != NULL; j++ )
      sum -= g_ascii_strtoll( projects_parameters[j] + strlen( "Parameters " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong project parameters list in 'Project %d'", i + 1 );

    g_strfreev( projects_parameters );

    }


  // Проверяем, что список групп параметров в каждом галсе изначально пустой.
  g_message( "checking track parameters list is empty" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      if( hyscan_db_get_track_param_list( db, track_id[i][j] ) != NULL ) g_error( "track parameters list must be empty" );


  // Создаём группы параметров в каждом галсе.
  g_message( "creating track parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < params_num; k++ )
        {

        gchar *track_parameters_name = g_strdup_printf( "Parameters %d", k + 1 );

        gint integer_param = 2 * k;
        gdouble double_param = 2 * k + 0.1;
        gboolean boolean_param = k % 2 ? FALSE : TRUE;
        gchar *string_param = sample1;

        track_param_id[i][j][k] = hyscan_db_open_track_param( db, track_id[i][j], track_parameters_name );
        if( track_param_id[i][j][k] < 0 ) g_error( "can't create 'Project %d.Track %d.%s'", i + 1, j + 1, track_parameters_name );

        if( !hyscan_db_set_integer_param( db, track_param_id[i][j][k], "integer", integer_param ) )
          g_error( "can't set 'Project %d.Track %d.%s' integer parameter", i + 1, j + 1, track_parameters_name );
        if( !hyscan_db_set_double_param( db, track_param_id[i][j][k], "double", double_param ) )
          g_error( "can't set 'Project %d.Track %d.%s' double parameter", i + 1, j + 1, track_parameters_name );
        if( !hyscan_db_set_boolean_param( db, track_param_id[i][j][k], "common.boolean", boolean_param ) )
          g_error( "can't set 'Project %d.Track %d.%s' boolean parameter", i + 1, j + 1, track_parameters_name );
        if( !hyscan_db_set_string_param( db, track_param_id[i][j][k], "common.string", string_param ) )
          g_error( "can't set 'Project %d.Track %d.%s' string parameter", i + 1, j + 1, track_parameters_name );

        integer_param = hyscan_db_get_integer_param( db, track_param_id[i][j][k], "integer" );
        double_param = hyscan_db_get_double_param( db, track_param_id[i][j][k], "double" );
        boolean_param = hyscan_db_get_boolean_param( db, track_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, track_param_id[i][j][k], "common.string" );

        if( integer_param != 2 * k ) g_error( "error in 'Project %d.Track %d.%s' integer parameter", i + 1, j + 1, track_parameters_name );
        if( double_param - 2 * k > 0.11 ) g_error( "error in 'Project %d.Track %d.%s' double parameter", i + 1, j + 1, track_parameters_name );
        if( boolean_param != ( k % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.Track %d.%s' integer parameter", i + 1, j + 1, track_parameters_name );
        if( g_strcmp0( string_param, sample1 ) != 0 ) g_error( "error in 'Project %d.Track %d.%s' string parameter", i + 1, j + 1, track_parameters_name );

        g_free( string_param );
        g_free( track_parameters_name );

        }


  // Проверяем, что при открытии уже открытых групп параметров возвращаются правильные идентификаторы.
  g_message( "checking opened track parameters identifiers" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < params_num; k++ )
      {
      gchar *track_parameters_name = g_strdup_printf( "Parameters %d", k + 1 );
      if( track_param_id[i][j][k] != hyscan_db_open_track_param( db, track_id[i][j], track_parameters_name ) )
        g_error( "wrong opened '%s' in 'Project %d.Track %d' id", track_parameters_name, i + 1, j + 1 );
      g_free( track_parameters_name );
      }


  // Проверяем названия созданных групп параметров.
  g_message( "checking track parameters names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **track_parameters = hyscan_db_get_track_param_list( db, track_id[i][j] );
      if( track_parameters == NULL ) g_error( "can't get track parameters list" );
      if( g_strv_length( track_parameters ) != params_num ) g_error( "wrong number of track parameters in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 0, sum = 0; k < params_num; k++ ) sum += ( k + 1 );
      for( k = 0; track_parameters[k] != NULL; k++ )
        sum -= g_ascii_strtoll( track_parameters[k] + strlen( "Parameters " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong track parameters list in 'Project %d'", i + 1 );

      g_strfreev( track_parameters );

      }


  // Удаляем объект базы данных.
  g_object_unref( db );


  g_message( "re-opening db" );


  // Пересоздаём объект базы данных с уже существующими проектами.
  db = g_object_new( G_TYPE_HYSCAN_DB_FILE, "path", db_path, NULL );


  // Проверяем список проектов.
  g_message( "checking projects names" );
  {

  gint sum;
  gchar **projects = hyscan_db_get_project_list( db );
  if( projects == NULL ) g_error( "can't get projects list" );
  if( g_strv_length( projects ) != projects_num ) g_error( "wrong number of projects" );

  for( i = 0, sum = 0; i < projects_num; i++ ) sum += ( i + 1 );
  for( i = 0; projects[i] != NULL; i++ )
    sum -= g_ascii_strtoll( projects[i] + strlen( "Project " ), NULL, 10 );
  if( sum != 0 ) g_error( "wrong projects list" );

  g_strfreev( projects );

  }


  // Открываем все проекты.
  g_message( "opening projects" );
  for( i = 0; i < projects_num; i++ )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    project_id[i] = hyscan_db_open_project( db, project_name );
    if( project_id[i] < 0 ) g_error( "can't open '%s' id", project_name );
    g_free( project_name );
    }


  // Проверяем список галсов.
  g_message( "checking tracks names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **tracks = hyscan_db_get_track_list( db, project_id[i] );
    if( tracks == NULL ) g_error( "can't get tracks list" );
    if( g_strv_length( tracks ) != tracks_num ) g_error( "wrong number of tracks in 'Project %d'", i + 1 );

    for( j = 0, sum = 0; j < tracks_num; j++ ) sum += ( j + 1 );
    for( j = 0; tracks[j] != NULL; j++ )
      sum -= g_ascii_strtoll( tracks[j] + strlen( "Track " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong tracks list in 'Project %d'", i + 1 );

    g_strfreev( tracks );

    }


  // Открываем все галсы.
  g_message( "opening tracks" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      gchar *track_name = g_strdup_printf( "Track %d", j + 1 );
      track_id[i][j] = hyscan_db_open_track( db, project_id[i], track_name );
      if( track_id[i][j] < 0 ) g_error( "can't open 'Project %d.%s' id", i + 1, track_name );
      g_free( track_name );
      }


  // Проверяем список каналов данных.
  g_message( "checking channels names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **channels = hyscan_db_get_channel_list( db, track_id[i][j] );
      if( channels == NULL ) g_error( "can't get channels list" );
      if( g_strv_length( channels ) != channels_num ) g_error( "wrong number of channels in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 0, sum = 0; k < channels_num; k++ ) sum += ( k + 1 );
      for( k = 0; channels[k] != NULL; k++ )
        sum -= g_ascii_strtoll( channels[k] + strlen( "Channel " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong channels list in 'Project %d.Track %d'", i + 1, j + 1 );

      g_strfreev( channels );

      }


  // Открываем все каналы данных.
  g_message( "opening channels" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {
        gchar *channel_name = g_strdup_printf( "Channel %d", k + 1 );
        channel_id[i][j][k] = hyscan_db_open_channel( db, track_id[i][j], channel_name );
        if( channel_id[i][j][k] < 0 ) g_error( "can't open 'Project %d.Track %d.%s' id", i + 1, j + 1, channel_name );
        g_free( channel_name );
        }


  // Проверяем правильность записанных данных каналов.
  g_message( "checking channels content" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {
        gint readed_size = sample_size;
        if( !hyscan_db_get_channel_data( db, channel_id[i][j][k], 0, buffer, &sample_size, NULL ) )
          g_error( "can't read data from 'Project %d.Track %d.Channel %d'", i + 1, j + 1, k + 1 );
        if( readed_size != sample_size || g_strcmp0( sample1, buffer ) != 0 )
          g_error( "wrong 'Project %d.Track %d.Channel %d' data", i + 1, j + 1, k + 1 );
        }


  // Проверяем правильность записанных параметров каналов.
  g_message( "checking channels parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {

        gint integer_param;
        gdouble double_param;
        gboolean boolean_param;
        gchar *string_param;

        channel_param_id[i][j][k] = hyscan_db_open_channel_param( db, channel_id[i][j][k] );

        integer_param = hyscan_db_get_integer_param( db, channel_param_id[i][j][k], "common.integer" );
        double_param = hyscan_db_get_double_param( db, channel_param_id[i][j][k], "common.double" );
        boolean_param = hyscan_db_get_boolean_param( db, channel_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, channel_param_id[i][j][k], "common.string" );

        if( integer_param != 2 * k ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( double_param - 2 * k > 0.11 ) g_error( "error in 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( boolean_param != ( k % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( g_strcmp0( string_param, sample2 ) != 0 ) g_error( "error in 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        g_free( string_param );

        }


  // Проверяем, что параметры каналов данных нельзя изменить.
  g_message( "trying to change channel parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        {

        gint integer_param = k;
        gdouble double_param = k + 0.1;
        gboolean boolean_param = k % 2 ? TRUE : FALSE;
        gchar *string_param = sample1;

        channel_param_id[i][j][k] = hyscan_db_open_channel_param( db, channel_id[i][j][k] );

        if( hyscan_db_set_integer_param( db, channel_param_id[i][j][k], "common.integer", integer_param ) )
          g_error( "can set 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( hyscan_db_set_double_param( db, channel_param_id[i][j][k], "common.double", double_param ) )
          g_error( "can set 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( hyscan_db_set_boolean_param( db, channel_param_id[i][j][k], "common.boolean", boolean_param ) )
          g_error( "can set 'Project %d.Track %d. Channel %d' boolean parameter", i + 1, j + 1, k + 1 );
        if( hyscan_db_set_string_param( db, channel_param_id[i][j][k], "common.string", string_param ) )
          g_error( "can set 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        integer_param = hyscan_db_get_integer_param( db, channel_param_id[i][j][k], "common.integer" );
        double_param = hyscan_db_get_double_param( db, channel_param_id[i][j][k], "common.double" );
        boolean_param = hyscan_db_get_boolean_param( db, channel_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, channel_param_id[i][j][k], "common.string" );

        if( integer_param != 2 * k ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( double_param - 2 * k > 0.11 ) g_error( "error in 'Project %d.Track %d. Channel %d' double parameter", i + 1, j + 1, k + 1 );
        if( boolean_param != ( k % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.Track %d. Channel %d' integer parameter", i + 1, j + 1, k + 1 );
        if( g_strcmp0( string_param, sample2 ) != 0 ) g_error( "error in 'Project %d.Track %d. Channel %d' string parameter", i + 1, j + 1, k + 1 );

        g_free( string_param );

        }


  // Проверяем список групп параметров проектов.
  g_message( "checking project parameters names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **projects_parameters = hyscan_db_get_project_param_list( db, project_id[i] );
    if( projects_parameters == NULL ) g_error( "can't get project parameters list" );
    if( g_strv_length( projects_parameters ) != params_num ) g_error( "wrong number of project parameters in 'Project %d'", i + 1 );

    for( j = 0, sum = 0; j < params_num; j++ ) sum += ( j + 1 );
    for( j = 0; projects_parameters[j] != NULL; j++ )
      sum -= g_ascii_strtoll( projects_parameters[j] + strlen( "Parameters " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong project parameters list in 'Project %d'", i + 1 );

    g_strfreev( projects_parameters );

    }


  // Проверяем правильность параметров проектов.
  g_message( "checking project parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < params_num; j++ )
      {

      gchar *project_parameters_name = g_strdup_printf( "Parameters %d", j + 1 );

      gint integer_param;
      gdouble double_param;
      gboolean boolean_param;
      gchar *string_param;

      project_param_id[i][j] = hyscan_db_open_project_param( db, project_id[i], project_parameters_name );
      if( project_param_id[i][j] < 0 ) g_error( "can't open 'Project %d.%s'", i + 1, project_parameters_name );

      integer_param = hyscan_db_get_integer_param( db, project_param_id[i][j], "integer" );
      double_param = hyscan_db_get_double_param( db, project_param_id[i][j], "double" );
      boolean_param = hyscan_db_get_boolean_param( db, project_param_id[i][j], "common.boolean" );
      string_param = hyscan_db_get_string_param( db, project_param_id[i][j], "common.string" );

      if( integer_param != 2 * j ) g_error( "error in 'Project %d.%s' integer parameter", i + 1, project_parameters_name );
      if( double_param - 2 * j > 0.11 ) g_error( "error in 'Project %d.%s' double parameter", i + 1, project_parameters_name );
      if( boolean_param != ( j % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.%s' integer parameter", i + 1, project_parameters_name );
      if( g_strcmp0( string_param, sample1 ) != 0 ) g_error( "error in 'Project %d.%s' string parameter", i + 1, project_parameters_name );

      g_free( string_param );
      g_free( project_parameters_name );

      }


  // Проверяем список групп параметров галсов.
  g_message( "checking track parameters names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **track_parameters = hyscan_db_get_track_param_list( db, track_id[i][j] );
      if( track_parameters == NULL ) g_error( "can't get track parameters list" );
      if( g_strv_length( track_parameters ) != params_num ) g_error( "wrong number of track parameters in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 0, sum = 0; k < params_num; k++ ) sum += ( k + 1 );
      for( k = 0; track_parameters[k] != NULL; k++ )
        sum -= g_ascii_strtoll( track_parameters[k] + strlen( "Parameters " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong track parameters list in 'Project %d'", i + 1 );

      g_strfreev( track_parameters );

      }


  // Проверяем правильность параметров галсов.
  g_message( "checking track parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < params_num; k++ )
        {

        gchar *track_parameters_name = g_strdup_printf( "Parameters %d", k + 1 );

        gint integer_param;
        gdouble double_param;
        gboolean boolean_param;
        gchar *string_param;

        track_param_id[i][j][k] = hyscan_db_open_track_param( db, track_id[i][j], track_parameters_name );
        if( track_param_id[i][j][k] < 0 ) g_error( "can't open 'Project %d.Track %d.%s'", i + 1, j + 1, track_parameters_name );

        integer_param = hyscan_db_get_integer_param( db, track_param_id[i][j][k], "integer" );
        double_param = hyscan_db_get_double_param( db, track_param_id[i][j][k], "double" );
        boolean_param = hyscan_db_get_boolean_param( db, track_param_id[i][j][k], "common.boolean" );
        string_param = hyscan_db_get_string_param( db, track_param_id[i][j][k], "common.string" );

        if( integer_param != 2 * k ) g_error( "error in 'Project %d.Track %d.%s' integer parameter", i + 1, j + 1, track_parameters_name );
        if( double_param - 2 * k > 0.11 ) g_error( "error in 'Project %d.Track %d.%s' double parameter", i + 1, j + 1, track_parameters_name );
        if( boolean_param != ( k % 2 ? FALSE : TRUE ) ) g_error( "error in 'Project %d.Track %d.%s' integer parameter", i + 1, j + 1, track_parameters_name );
        if( g_strcmp0( string_param, sample1 ) != 0 ) g_error( "error in 'Project %d.Track %d.%s' string parameter", i + 1, j + 1, track_parameters_name );

        g_free( string_param );
        g_free( track_parameters_name );

        }


  // Удаляем половину каналов данных.
  g_message( "removing half channels" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k += 2 )
        {
        gchar *channel_name = g_strdup_printf( "Channel %d", k + 1 );
        if( !hyscan_db_remove_channel( db, track_id[i][j], channel_name ) ) g_error( "can't remove 'Project %d.Track %d.%s' id", i + 1, j + 1, channel_name );
        g_free( channel_name );
        }


  // Проверяем изменившейся список.
  g_message( "checking channels names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **channels = hyscan_db_get_channel_list( db, track_id[i][j] );
      if( channels == NULL ) g_error( "can't get channels list" );
      if( g_strv_length( channels ) != channels_num / 2 ) g_error( "wrong number of channels in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 1, sum = 0; k < channels_num; k += 2 ) sum += ( k + 1 );
      for( k = 0; channels[k] != NULL; k++ )
        sum -= g_ascii_strtoll( channels[k] + strlen( "Channel " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong channels list in 'Project %d.Track %d'", i + 1, j + 1 );

      g_strfreev( channels );

      }

  // Удаляем половину групп параметров галсов.
  g_message( "removing half track parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < params_num; k += 2 )
        {
        gchar *track_parameters_name = g_strdup_printf( "Parameters %d", k + 1 );
        if( !hyscan_db_remove_track_param( db, track_id[i][j], track_parameters_name ) ) g_error( "can't remove 'Project %d.Track %d.%s' id", i + 1, j + 1, track_parameters_name );
        g_free( track_parameters_name );
        }


  // Проверяем изменившейся список.
  g_message( "checking track parameters names" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {

      gint sum;
      gchar **track_parameters = hyscan_db_get_track_param_list( db, track_id[i][j] );
      if( track_parameters == NULL ) g_error( "can't get track parameters list" );
      if( g_strv_length( track_parameters ) != params_num / 2 ) g_error( "wrong number of track parameters in 'Project %d.Track %d'", i + 1, j + 1 );

      for( k = 1, sum = 0; k < params_num; k += 2 ) sum += ( k + 1 );
      for( k = 0; track_parameters[k] != NULL; k++ )
        sum -= g_ascii_strtoll( track_parameters[k] + strlen( "Parameters " ), NULL, 10 );
      if( sum != 0 ) g_error( "wrong track parameters list in 'Project %d'", i + 1 );

      g_strfreev( track_parameters );

      }


  // Удаляем половину галсов.
  g_message( "removing half tracks" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j += 2 )
      {
      gchar *track_name = g_strdup_printf( "Track %d", j + 1 );
      if( !hyscan_db_remove_track( db, project_id[i], track_name ) ) g_error( "can't remove 'Project %d.%s'", i + 1, track_name );
      g_free( track_name );
      }


  // Проверяем изменившейся список.
  g_message( "checking tracks names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **tracks = hyscan_db_get_track_list( db, project_id[i] );
    if( tracks == NULL ) g_error( "can't get tracks list" );
    if( g_strv_length( tracks ) != tracks_num / 2 ) g_error( "wrong number of tracks in 'Project %d'", i + 1 );

    for( j = 1, sum = 0; j < tracks_num; j += 2 ) sum += ( j + 1 );
    for( j = 0; tracks[j] != NULL; j++ )
      sum -= g_ascii_strtoll( tracks[j] + strlen( "Track " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong tracks list in 'Project %d'", i + 1 );

    g_strfreev( tracks );

    }


  // Удаляем половину групп параметров проектов.
  g_message( "removing half project parameters" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < params_num; j += 2 )
      {
      gchar *project_parameters_name = g_strdup_printf( "Parameters %d", j + 1 );
      if( !hyscan_db_remove_project_param( db, project_id[i], project_parameters_name ) ) g_error( "can't remove 'Project %d.%s' id", i + 1, project_parameters_name );
      g_free( project_parameters_name );
      }

  // Проверяем изменившейся список.
  g_message( "checking project parameters names" );
  for( i = 0; i < projects_num; i++ )
    {

    gint sum;
    gchar **projects_parameters = hyscan_db_get_project_param_list( db, project_id[i] );
    if( projects_parameters == NULL ) g_error( "can't get project parameters list" );
    if( g_strv_length( projects_parameters ) != params_num / 2 ) g_error( "wrong number of project parameters in 'Project %d'", i + 1 );

    for( j = 1, sum = 0; j < params_num; j += 2 ) sum += ( j + 1 );
    for( j = 0; projects_parameters[j] != NULL; j++ )
      sum -= g_ascii_strtoll( projects_parameters[j] + strlen( "Parameters " ), NULL, 10 );
    if( sum != 0 ) g_error( "wrong project parameters list in 'Project %d'", i + 1 );

    g_strfreev( projects_parameters );

    }


  // Удаляем половину проектов.
  g_message( "removing half projects" );
  for( i = 0; i < projects_num; i += 2 )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    if( !hyscan_db_remove_project( db, project_name ) ) g_error( "can't remove '%s'", project_name );
    g_free( project_name );
    }


  // Проверяем изменившейся список.
  g_message( "checking projects names" );
  {

  gint sum;
  gchar **projects = hyscan_db_get_project_list( db );
  if( projects == NULL ) g_error( "can't get projects list" );
  if( g_strv_length( projects ) != projects_num / 2 ) g_error( "wrong number of projects" );

  for( i = 1, sum = 0; i < projects_num; i += 2 ) sum += ( i + 1 );
  for( i = 0; projects[i] != NULL; i++ )
    sum -= g_ascii_strtoll( projects[i] + strlen( "Project " ), NULL, 10 );
  if( sum != 0 ) g_error( "wrong projects list" );

  g_strfreev( projects );

  }


  // Удаляем оставшиеся проекты и проверяем, что список проектов пустой.
  g_message( "removing all projects" );
  for( i = 1; i < projects_num; i += 2 )
    {
    gchar *project_name = g_strdup_printf( "Project %d", i + 1 );
    if( !hyscan_db_remove_project( db, project_name ) ) g_error( "can't delete '%s'", project_name );
    g_free( project_name );
    }
  g_message( "checking db projects list is empty" );
  if( hyscan_db_get_project_list( db ) != NULL ) g_error( "projects list must be empty" );


  // Проверяем, что все идентификаторы открытых объектов более не работоспособны.
  g_message( "checking projects id validity" );
  for( i = 0; i < projects_num; i++ )
    if( hyscan_db_create_track( db, project_id[i], "Track" ) > 0 )
      g_error( "'Project %d' still alive", i + 1 );

  g_message( "checking tracks id validity" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      if( hyscan_db_create_channel( db, track_id[i][j], "Channel" ) > 0 )
        g_error( "'Project %d.Track %d' is still alive", i + 1, j + 1 );

  g_message( "checking channels id validity" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        if( hyscan_db_get_channel_data_range( db, channel_id[i][j][k], NULL, NULL ) )
          g_error( "'Project %d.Track %d.Channel %d' is still alive", i + 1, j + 1, k + 1 );

  g_message( "checking channel parameters id validity" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        if( hyscan_db_get_string_param( db, channel_param_id[i][j][k], "common.string" ) != NULL )
          g_error( "'Project %d.Track %d.Channel %d' is still alive", i + 1, j + 1, k + 1 );

  g_message( "checking track parameters id validity" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      for( k = 0; k < channels_num; k++ )
        if( hyscan_db_get_string_param( db, track_param_id[i][j][k], "common.string" ) != NULL )
          g_error( "'Project %d.Track %d.Parameters %d' is still alive", i + 1, j + 1, k + 1 );

  g_message( "checking project parameters id validity" );
  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      if( hyscan_db_get_string_param( db, project_param_id[i][j], "common.string" ) != NULL )
        g_error( "'Project %d.Parameters %d' is still alive", i + 1, j + 1 );


  // Удаляем объект базы данных.
  g_object_unref( db );


  // Удаляем каталог проектов.
  if( g_rmdir( db_path ) != 0 ) g_error( "can't delete directory '%s'", db_path );


  // Освобождаем память (для нормальной работы valgind).
  g_free( project_id );

  for( i = 0; i < projects_num; i++ )
    {
    g_free( track_id[i] );
    g_free( project_param_id[i] );
    }

  g_free( track_id );
  g_free( project_param_id );

  for( i = 0; i < projects_num; i++ )
    for( j = 0; j < tracks_num; j++ )
      {
      g_free( channel_id[i][j] );
      g_free( channel_param_id[i][j] );
      g_free( track_param_id[i][j] );
      }

  for( i = 0; i < projects_num; i++ )
    {
    g_free( channel_id[i] );
    g_free( channel_param_id[i] );
    g_free( track_param_id[i] );
    }

  g_free( channel_id );
  g_free( channel_param_id );
  g_free( track_param_id );

  g_free( buffer );

  return 0;

}
