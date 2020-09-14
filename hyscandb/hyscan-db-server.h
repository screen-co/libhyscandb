/* hyscan-db-server.h
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

#ifndef __HYSCAN_DB_SERVER_H__
#define __HYSCAN_DB_SERVER_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_SERVER             (hyscan_db_server_get_type ())
#define HYSCAN_DB_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_SERVER, HyScanDBServer))
#define HYSCAN_IS_DB_SERVER(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_SERVER))
#define HYSCAN_DB_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_SERVER, HyScanDBServerClass))
#define HYSCAN_IS_DB_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_SERVER))
#define HYSCAN_DB_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_SERVER, HyScanDBServerClass))

typedef struct _HyScanDBServer HyScanDBServer;
typedef struct _HyScanDBServerPrivate HyScanDBServerPrivate;
typedef struct _HyScanDBServerClass HyScanDBServerClass;

struct _HyScanDBServer
{
  GObject parent_instance;

  HyScanDBServerPrivate *priv;
};

struct _HyScanDBServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_db_server_get_type       (void);

HYSCAN_API
HyScanDBServer        *hyscan_db_server_new            (const gchar           *uri,
                                                        HyScanDB              *db,
                                                        guint                  n_threads,
                                                        guint                  n_clients);

HYSCAN_API
gboolean               hyscan_db_server_start          (HyScanDBServer        *server);

G_END_DECLS

#endif /* __HYSCAN_DB_SERVER_H__ */
