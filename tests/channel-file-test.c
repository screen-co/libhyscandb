
#include "hyscan-db-channel-file.h"
#include <glib/gprintf.h>

#define DATA_PATTERNS 16

int main( int argc, char **argv )
{

  gint64 time64;
  gint64 *times64;

  gint32 index;
  gchar *datap[DATA_PATTERNS];
  gchar *data;
  guint i, j, k;

  gint32 first_index, last_index;

  gchar *channel_name = NULL;

  gint32 max_file_size = 1024 * 1024 * 1024;
  gint32 data_size = 64 * 1024;
  gint32 total_records = 1000;

  GTimer *cur_timer;
  GTimer *all_timer;

  gint cur_cnts;
  gint all_cnts;

  { // Разбор командной строки.

  GError          *error = NULL;
  GOptionContext  *context;
  GOptionEntry     entries[] =
  {
    { "file-size", 'f', 0, G_OPTION_ARG_INT, &max_file_size, "Maximum data file size", NULL },
    { "data-size", 'd', 0, G_OPTION_ARG_INT, &data_size, "Data record size", NULL },
    { "records", 'r', 0, G_OPTION_ARG_INT, &total_records, "Total records number", NULL },
    { NULL }
  };

  context = g_option_context_new( "" );
  g_option_context_set_help_enabled( context, TRUE );
  g_option_context_add_main_entries( context, entries, NULL );
  g_option_context_set_ignore_unknown_options( context, FALSE );
  if( !g_option_context_parse( context, &argc, &argv, &error ) )
    { g_message( error->message ); return -1; }

  if( argc < 2 )
    { g_print( "%s", g_option_context_get_help( context, FALSE, NULL ) ); return 0; }

  g_option_context_free( context );

  }

  // Название канала с данными.
  channel_name = argv[1];

  // Буфер для данных.
  for( i = 0; i < DATA_PATTERNS; i++ ) datap[i] = g_malloc( data_size );
  data = g_malloc( data_size );
  times64 = g_malloc( total_records * sizeof( guint64 ) );

  cur_timer = g_timer_new();
  all_timer = g_timer_new();

  // Хранение данных.
  HyScanDBChannelFile *channel = g_object_new( HYSCAN_TYPE_DB_CHANNEL_FILE, "path", ".", "name", channel_name, NULL );

  // Максимальный размер файла с данными.
  hyscan_db_channel_file_set_channel_chunk_size( channel, max_file_size );

  time64 = g_random_int();

  // Генерируем шаблоны для записи.
  for( i = 0; i < DATA_PATTERNS; i++ )
    {

    guint32 hash = 0;

    // Генерируем случайные данные и считаем их контрольную сумму.
    for( j = 4; j < data_size; j++ )
      {
      datap[i][j] = g_random_int();
      hash = 33 * hash + (guchar)datap[i][j];
      }

    // Сохраняем контрольную сумму в самом начале данных.
    *(guint32*)datap[i] = hash;

    }

  g_printf( "Preloading channel '%s' with %d records with %d size\n", channel_name, total_records, data_size );

  g_timer_start( cur_timer );
  g_timer_start( all_timer );
  cur_cnts = 0;
  all_cnts = 0;

  // Записываем данные.
  for( i = 0; i < total_records; i++ )
    {

    if( g_timer_elapsed( cur_timer, NULL ) >= 1.0 )
      {
      g_message( "Current speed: %.3lf mb/s, average speed: %.3lf mb/s", ( data_size * ( cur_cnts / g_timer_elapsed( cur_timer, NULL ) ) ) / ( 1024 * 1024 ), ( data_size * ( all_cnts / g_timer_elapsed( all_timer, NULL ) ) ) / ( 1024 * 1024 ) );
      g_timer_start( cur_timer );
      cur_cnts = 0;
      }

    times64[i] = time64;

    if( !hyscan_db_channel_file_add_channel_data( channel, time64, datap[i%DATA_PATTERNS], data_size, &index ) )
      g_warning( "hyscan_db_channel_add failed" );

    if( index != i )
      g_warning( "index mismatch %d != %d", index, i );

    // Добавляем случайное смещение по времени от 50 до 100 мкс.
    time64 += g_random_int_range( 50, 101 );

    cur_cnts += 1;
    all_cnts += 1;

    }

  g_object_unref( channel );
  channel = g_object_new( HYSCAN_TYPE_DB_CHANNEL_FILE, "path", ".", "name", channel_name, NULL );

  if( !hyscan_db_channel_file_get_channel_data_range( channel, &first_index, &last_index ) )
    g_error( "First index = unknown, last index = unknown" );

  g_printf( "Reading records from %d to %d\n", first_index, last_index );

  g_timer_start( cur_timer );
  g_timer_start( all_timer );
  cur_cnts = 0;
  all_cnts = 0;

  // Последовательно считываем данные и проверяем контрольную сумму.
  for( i = first_index; i <= last_index; i++ )
    {

    guint32 hash = 0;
    gint32 readed_data_size = data_size;

    if( g_timer_elapsed( cur_timer, NULL ) >= 1.0 )
      {
      g_message( "Current speed: %.3lf mb/s, average speed: %.3lf mb/s", ( data_size * ( cur_cnts / g_timer_elapsed( cur_timer, NULL ) ) ) / ( 1024 * 1024 ), ( data_size * ( all_cnts / g_timer_elapsed( all_timer, NULL ) ) ) / ( 1024 * 1024 ) );
      g_timer_start( cur_timer );
      cur_cnts = 0;
      }

    if( !hyscan_db_channel_file_get_channel_data( channel, i, data, &readed_data_size, &time64 ) )
      g_warning( "hyscan_db_channel_get failed" );

    // Проверяем размер данных.
    if( readed_data_size != data_size )
      g_warning( "data size mismatch" );

    // Проверяем метку времени.
    if( times64[i] != time64 )
      g_warning( "time mismatch" );

    // Считаем контрольную сумму.
    for( j = 4; j < data_size; j++ )
      hash = 33 * hash + (guchar)data[j];

    // Проверяем контрольную сумму.
    if( *(guint32*)data != hash )
      g_warning( "data hash mismatch" );

    cur_cnts += 1;
    all_cnts += 1;

    }

  g_printf( "Random read, range from %d to %d\n", first_index, last_index );

  g_random_set_seed( time( NULL ) );

  g_timer_start( cur_timer );
  g_timer_start( all_timer );
  cur_cnts = 0;
  all_cnts = 0;

  // Случайным образом считываем данные и проверяем контрольную сумму.
  for( k = 0; k < 10 * total_records; k++ )
    {

    guint32 hash = 0;
    gint32 readed_data_size = data_size;

    if( g_timer_elapsed( cur_timer, NULL ) >= 1.0 )
      {
      g_message( "Current speed: %.3lf mb/s, average speed: %.3lf mb/s", ( data_size * ( cur_cnts / g_timer_elapsed( cur_timer, NULL ) ) ) / ( 1024 * 1024 ), ( data_size * ( all_cnts / g_timer_elapsed( all_timer, NULL ) ) ) / ( 1024 * 1024 ) );
      g_timer_start( cur_timer );
      cur_cnts = 0;
      }

    i = g_random_int_range( first_index, last_index + 1 );

    if( !hyscan_db_channel_file_get_channel_data( channel, i, data, &readed_data_size, &time64 ) )
      g_warning( "hyscan_db_channel_get failed" );

    // Проверяем размер данных.
    if( readed_data_size != data_size )
      g_warning( "data size mismatch" );

    // Проверяем метку времени.
    if( times64[i] != time64 )
      g_warning( "time mismatch" );

    // Считаем контрольную сумму.
    for( j = 4; j < data_size; j++ )
      hash = 33 * hash + (guchar)data[j];

    // Проверяем контрольную сумму.
    if( *(guint32*)data != hash )
      g_warning( "data hash mismatch" );

    cur_cnts += 1;
    all_cnts += 1;

    }

  g_printf( "Finding records by time, one by one\n" );

  // Последовательно ищем записи по времени.
  for( i = first_index; i <= last_index; i++ )
    {

    gint32 lindex, rindex;
    gint64 ltime, rtime;

    time64 = times64[i];

    if( !hyscan_db_channel_file_find_channel_data( channel, time64, &lindex, &rindex, &ltime, &rtime ) )
      g_warning( "hyscan_db_channel_find failed" );

    if( ltime != rtime || ltime != time64 )
      g_warning( "time %ld.%06ld mismatch ( %ld.%06ld : %ld.%06ld )", time64 / 1000000, time64 % 1000000, ltime / 1000000, ltime % 1000000, rtime / 1000000, rtime % 1000000 );

    if( lindex != i || rindex != i )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    }

  g_printf( "Finding records by random time\n" );

  // Случайно ищем записи по точно совпадающему времени.
  for( k = 0; k < 10 * total_records; k++ )
    {

    gint32 rindex, lindex;
    gint64 ltime, rtime;

    i = g_random_int_range( first_index, last_index + 1 );
    time64 = times64[i];

    if( !hyscan_db_channel_file_find_channel_data( channel, time64, &lindex, &rindex, &ltime, &rtime ) )
      g_warning( "hyscan_db_channel_find failed" );

    if( ltime != rtime || ltime != time64 )
      g_warning( "time %ld.%06ld mismatch ( %ld.%06ld : %ld.%06ld )", time64 / 1000000, time64 % 1000000, ltime / 1000000, ltime % 1000000, rtime / 1000000, rtime % 1000000 );

    if( lindex != i || rindex != i )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    }

  g_printf( "Finding records by random time - 1\n" );

  // Случайно ищем записи по времени - 1мкс.
  for( k = 0; k < 10 * total_records; k++ )
    {

    gint32 rindex, lindex;
    gint64 ltime, rtime;

    i = g_random_int_range( first_index, last_index + 1 );
    time64 = times64[i] - 1;

    if( !hyscan_db_channel_file_find_channel_data( channel, time64, &lindex, &rindex, &ltime, &rtime ) )
      g_warning( "hyscan_db_channel_find failed" );

    if( i == first_index && ( lindex != G_MININT32 || rindex != first_index ) )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    if( i != first_index && lindex != rindex - 1 )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    if( rtime - 1 != time64 || ltime >= rtime )
      g_warning( "time %ld.%06ld mismatch ( %ld.%06ld : %ld.%06ld )", time64 / 1000000, time64 % 1000000, ltime / 1000000, ltime % 1000000, rtime / 1000000, rtime % 1000000 );

    }

  g_printf( "Finding records by random time + 1\n" );

  // Случайно ищем записи по времени + 1мкс.
  for( k = 0; k < 10 * total_records; k++ )
    {

    gint32 rindex, lindex;
    gint64 ltime, rtime;

    i = g_random_int_range( first_index, last_index + 1 );
    time64 = times64[i] + 1;

    if( !hyscan_db_channel_file_find_channel_data( channel, time64, &lindex, &rindex, &ltime, &rtime ) )
      g_warning( "hyscan_db_channel_find failed" );

    if( i == last_index && ( lindex != last_index || rindex != G_MAXINT32 ) )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    if( i != last_index && lindex != rindex - 1 )
      g_warning( "index %d mismatch ( %d : %d )", i, lindex, rindex );

    if( ltime + 1 != time64 || ltime >= rtime )
      g_warning( "time %ld.%06ld mismatch ( %ld.%06ld : %ld.%06ld )", time64 / 1000000, time64 % 1000000, ltime / 1000000, ltime % 1000000, rtime / 1000000, rtime % 1000000 );

    }

  g_object_unref( channel );

  g_free( times64 );
  for( i = 0; i < DATA_PATTERNS; i++ ) g_free( datap[i] );
  g_free( data );

  g_timer_destroy( cur_timer );
  g_timer_destroy( all_timer );

  return 0;

}
