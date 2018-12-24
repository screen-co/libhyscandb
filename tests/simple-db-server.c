
#include <hyscan-db-server.h>
#include <libxml/parser.h>
#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  gchar *db_uri = NULL;
  gchar *db_path = NULL;
  gchar *server_uri = NULL;
  gint   n_threads = 1;
  gint   n_clients = 100;

  HyScanDB *db;
  HyScanDBServer *server;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"path", 'p', 0, G_OPTION_ARG_STRING, &db_path, "Path to db directory", NULL},
      {"threads", 't', 0, G_OPTION_ARG_INT, &n_threads, "Number of server threads", NULL},
      {"clients", 'c', 0, G_OPTION_ARG_INT, &n_clients, "Maximum number of clients", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2 || db_path == NULL)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    server_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db_uri = g_strdup_printf ("file://%s", db_path);
  db = hyscan_db_new (db_uri);
  g_free (db_uri);

  server = hyscan_db_server_new (server_uri, db, n_threads, n_clients);

  if (!hyscan_db_server_start (server))
    g_error ("can't start db server");

  g_message ("Press [Enter] to terminate server...");
  getchar ();

  g_object_unref (server);
  g_object_unref (db);
  g_free (server_uri);

  return 0;
}
