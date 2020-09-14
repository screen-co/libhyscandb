/* hyscan-db-client.h
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

#ifndef __HYSCAN_DB_CLIENT_H__
#define __HYSCAN_DB_CLIENT_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_CLIENT             (hyscan_db_client_get_type ())
#define HYSCAN_DB_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_CLIENT, HyScanDBClient))
#define HYSCAN_IS_DB_CLIENT(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_CLIENT))
#define HYSCAN_DB_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_CLIENT, HyScanDBClientClass))
#define HYSCAN_IS_DB_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_CLIENT))
#define HYSCAN_DB_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_CLIENT, HyScanDBClientClass))

typedef struct _HyScanDBClient HyScanDBClient;
typedef struct _HyScanDBClientPrivate HyScanDBClientPrivate;
typedef struct _HyScanDBClientClass HyScanDBClientClass;

struct _HyScanDBClient
{
  GObject parent_instance;

  HyScanDBClientPrivate *priv;
};

struct _HyScanDBClientClass
{
  GObjectClass parent_class;
};

GType                  hyscan_db_client_get_type       (void);

HyScanDBClient        *hyscan_db_client_new            (const gchar           *uri);

G_END_DECLS

#endif /* __HYSCAN_DB_CLIENT_H__ */
