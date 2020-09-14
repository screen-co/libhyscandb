/* hyscan-db-common.c
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

#include "hyscan-db-file.h"
#include "hyscan-db-client.h"

#include <urpc-types.h>
#include <string.h>

/**
 * hyscan_db_new:
 * @uri: адрес подключения
 *
 * Функция подключается к системе хранения данных.
 *
 * Параметры подключения определяются строкой uri. Формат строки следующий:
 * "type://<user>:<password>@path", где:
 * - type - тип подключения к системе хранения (file, tcp, shm);
 * - user - имя пользователя;
 * - password - пароль пользователя;
 * - path - путь к каталогу с проектами (для типа подключения file) или адрес сервера (для tcp и shm).
 *
 * Параметры user и password являются опциональными и требуют задания только
 * при подключении к серверу на котором используется система аутентификации.
 *
 * Returns: #HyScanDB или %NULL. Для удаления #g_object_unref.
 */
HyScanDB *
hyscan_db_new (const gchar *uri)
{
  /* Протокол file:// */
  if (g_pattern_match_simple ("file://*", uri))
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

      return HYSCAN_DB (hyscan_db_file_new (path));
    }

  /* Протоколы shm:// и tcp:// */
  if (g_pattern_match_simple ("shm://*", uri) || g_pattern_match_simple ("tcp://*", uri))
    {
      const gchar *path;

      /* Путь к RPC серверу. */
      path = uri + strlen ("xxx://");

      /* Подключение с использовнием имени пользователя и пароля для
         протоколов shm и tcp пока не реализовано. */
      if (g_pattern_match_simple("*@*", path))
        {
          g_warning ("HyScanDB: authenticated connection to server is not yet supported");
          return NULL;
        }

      return HYSCAN_DB (hyscan_db_client_new (uri));
    }

  g_warning ("HyScanDB: unsupported protocol");
  return NULL;
}
