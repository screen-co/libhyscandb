/* simple-db-server.c
 *
 * Copyright 2015-2020 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanDB.
 *
 * HyScanDB is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanDB имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanDB на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <hyscan-db-server.h>
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
