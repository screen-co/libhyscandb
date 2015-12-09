/**
 * \file hyscan-db-common.c
 *
 * \brief Исходный файл общих функций базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-file.h"
#include <string.h>

HyScanDB *
hyscan_db_new (const gchar *uri)
{
  const char *path;
  GDir *dir;

  /* На данный момент поддерживается только протокол file:// */
  if (!g_pattern_match_simple("file://*", uri))
    {
      g_warning ("hyscan_db_new: unsupported protocol");
      return NULL;
    }

  /* Подключение с использовнием имени пользователя и пароля для
     протокола file невозможно. */
  if (g_pattern_match_simple("file://*@*", uri))
    {
      g_warning ("hyscan_db_new: authenticated connection is unsupported for 'file://' protocol");
      return NULL;
    }

  /* Путь к каталогу базы данных. */
  path = uri + strlen ("file://");

  /* Проверяем, что каталог базы данных существует. */
  dir = g_dir_open (path, 0, NULL);
  if (dir == NULL)
    {
      g_warning ("hyscan_db_new: no such directory '%s'", path);
      return NULL;
    }
  else
    {
      g_dir_close (dir);
    }

  return g_object_new (HYSCAN_TYPE_DB_FILE, "path", path, NULL);
}
