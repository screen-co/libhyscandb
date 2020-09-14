/* hyscan-db-param-file.h
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

#ifndef __HYSCAN_DB_PARAM_FILE_H__
#define __HYSCAN_DB_PARAM_FILE_H__

#include <hyscan-param-list.h>
#include <hyscan-data-schema.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_PARAM_FILE             (hyscan_db_param_file_get_type())
#define HYSCAN_DB_PARAM_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFile))
#define HYSCAN_IS_DB_PARAM_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))
#define HYSCAN_IS_DB_PARAM_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))

typedef struct _HyScanDBParamFile HyScanDBParamFile;
typedef struct _HyScanDBParamFilePrivate HyScanDBParamFilePrivate;
typedef struct _HyScanDBParamFileClass HyScanDBParamFileClass;

struct _HyScanDBParamFile
{
  GObject parent_instance;

  HyScanDBParamFilePrivate *priv;
};

struct _HyScanDBParamFileClass
{
  GObjectClass parent_class;
};

GType hyscan_db_param_file_get_type (void);

HyScanDBParamFile     *hyscan_db_param_file_new                (const gchar           *param_file,
                                                                const gchar           *schema_file);

gboolean               hyscan_db_param_file_is_new             (HyScanDBParamFile     *param);

gchar                **hyscan_db_param_file_object_list        (HyScanDBParamFile     *param);

HyScanDataSchema      *hyscan_db_param_file_object_get_schema  (HyScanDBParamFile     *param,
                                                                const gchar           *object_name);

gboolean               hyscan_db_param_file_object_create      (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

gboolean               hyscan_db_param_file_object_remove      (HyScanDBParamFile     *param,
                                                                const gchar           *object_name);


gboolean               hyscan_db_param_file_set                (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

gboolean               hyscan_db_param_file_get                (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

G_END_DECLS

#endif /* __HYSCAN_DB_PARAM_FILE_H__ */
