/*
 * \file hyscan-db-common.c
 *
 * \brief Исходный файл общих функций базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-file.h"
#include "hyscan-db-client.h"

#include <urpc-types.h>
#include <string.h>

HyScanDB *
hyscan_db_new (const gchar *uri)
{
  /* Протокол file:// */
  if (g_pattern_match_simple("file://*", uri))
    {
      const gchar *path;
      GDir *dir;

      /* Путь к каталогу базы данных. */
      path = uri + strlen ("file://");

      /* Подключение с использовнием имени пользователя и пароля для
         протокола file невозможно. */
      if (g_pattern_match_simple("*@*", path))
        {
          g_warning ("HyScanDB: authenticated connection is unsupported for 'file://' protocol");
          return NULL;
        }

      /* Проверяем, что каталог базы данных существует. */
      dir = g_dir_open (path, 0, NULL);
      if (dir == NULL)
        {
          g_warning ("HyScanDB: no such directory '%s'", path);
          return NULL;
        }
      else
        {
          g_dir_close (dir);
        }

      return g_object_new (HYSCAN_TYPE_DB_FILE, "path", path, NULL);
    }

  /* Протоколы shm:// и tcp:// */
  if (g_pattern_match_simple("shm://*", uri) || g_pattern_match_simple("tcp://*", uri))
    {
      const gchar *path;
      gchar *db_uri;
      HyScanDB *db;

      /* Путь к RPC серверу. */
      path = uri + strlen ("xxx://");

      /* Подключение с использовнием имени пользователя и пароля для
         протокола file невозможно. */
      if (g_pattern_match_simple("*@*", path))
        {
          g_warning ("HyScanDB: authenticated connection to server is not yet supported");
          return NULL;
        }

      /* URI к RPC серверу. */
      if (g_pattern_match_simple("shm://*", uri))
        db_uri = g_strdup_printf ("shm://%s", path);
      else
        db_uri = g_strdup_printf ("tcp://%s", path);

      db = HYSCAN_DB (hyscan_db_client_new (db_uri));
      g_free (db_uri);
      return db;
    }

  g_warning ("HyScanDB: unsupported protocol");
  return NULL;
}
