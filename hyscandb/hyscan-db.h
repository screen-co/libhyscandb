/* hyscan-db.h
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

#ifndef __HYSCAN_DB_H__
#define __HYSCAN_DB_H__

#include <hyscan-buffer.h>
#include <hyscan-param-list.h>
#include <hyscan-data-schema.h>

G_BEGIN_DECLS

/**
 * HyScanDBFindStatus:
 * @HYSCAN_DB_FIND_OK: запись найдена
 * @HYSCAN_DB_FIND_FAIL: ошибка поиска
 * @HYSCAN_DB_FIND_LESS: запись не найдена, метка времени раньше записанных данных
 * @HYSCAN_DB_FIND_GREATER: запись не найдена, метка времени позже записанных данных
 *
 * Статус поиска записи.
 */
typedef enum
{
  HYSCAN_DB_FIND_OK          = 0,
  HYSCAN_DB_FIND_FAIL        = 1,
  HYSCAN_DB_FIND_LESS        = 2,
  HYSCAN_DB_FIND_GREATER     = 3
} HyScanDBFindStatus;

#define HYSCAN_TYPE_DB            (hyscan_db_get_type ())
#define HYSCAN_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB, HyScanDB))
#define HYSCAN_IS_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB))
#define HYSCAN_DB_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_DB, HyScanDBInterface))

typedef struct _HyScanDB HyScanDB;

typedef struct _HyScanDBInterface HyScanDBInterface;

struct _HyScanDBInterface
{
  GTypeInterface g_iface;

  gchar  *             (*get_uri)                              (HyScanDB              *db);

  guint32              (*get_mod_count)                        (HyScanDB              *db,
                                                                gint32                 id);

  gboolean             (*is_exist)                             (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                const gchar           *channel_name);

  gchar **             (*project_list)                         (HyScanDB              *db);

  gint32               (*project_open)                         (HyScanDB              *db,
                                                                const gchar           *project_name);

  gint32               (*project_create)                       (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *project_schema);

  gboolean             (*project_remove)                       (HyScanDB              *db,
                                                                const gchar           *project_name);

  GDateTime *          (*project_get_ctime)                    (HyScanDB              *db,
                                                                gint32                 project_id);

  gchar **             (*project_param_list)                   (HyScanDB              *db,
                                                                gint32                 project_id);

  gint32               (*project_param_open)                   (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

  gboolean             (*project_param_remove)                 (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

  gchar **             (*track_list)                           (HyScanDB              *db,
                                                                gint32                 project_id);

  gint32               (*track_open)                           (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

  gint32               (*track_create)                         (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name,
                                                                const gchar           *track_schema,
                                                                const gchar           *schema_id);

  gboolean             (*track_remove)                         (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

  GDateTime *          (*track_get_ctime)                      (HyScanDB              *db,
                                                                gint32                 track_id);

  gint32               (*track_param_open)                     (HyScanDB              *db,
                                                                gint32                 track_id);

  gchar **             (*channel_list)                         (HyScanDB              *db,
                                                                gint32                 track_id);

  gint32               (*channel_open)                         (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

  gint32               (*channel_create)                       (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name,
                                                                const gchar           *schema_id);

  gboolean             (*channel_remove)                       (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

  GDateTime *          (*channel_get_ctime)                    (HyScanDB              *db,
                                                                gint32                 channel_id);

  void                 (*channel_finalize)                     (HyScanDB              *db,
                                                                gint32                 channel_id);

  gboolean             (*channel_is_writable)                  (HyScanDB              *db,
                                                                gint32                 channel_id);

  gint32               (*channel_param_open)                   (HyScanDB              *db,
                                                                gint32                 channel_id);

  gboolean             (*channel_set_chunk_size)               (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                chunk_size);

  gboolean             (*channel_set_save_time)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 save_time);

  gboolean             (*channel_set_save_size)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                save_size);

  gboolean             (*channel_get_data_range)               (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

  gboolean             (*channel_add_data)                     (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                HyScanBuffer          *buffer,
                                                                guint32               *index);

  gboolean             (*channel_get_data)                     (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index,
                                                                HyScanBuffer          *buffer,
                                                                gint64                *time);

  guint32              (*channel_get_data_size)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

  gint64               (*channel_get_data_time)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

  gint32               (*channel_find_data)                    (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

  gchar **             (*param_object_list)                    (HyScanDB              *db,
                                                                gint32                 param_id);

  gboolean             (*param_object_create)                  (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

  gboolean             (*param_object_remove)                  (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

  HyScanDataSchema *   (*param_object_get_schema)              (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

  gboolean             (*param_set)                            (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

  gboolean             (*param_get)                            (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

  void                 (*close)                                (HyScanDB              *db,
                                                                gint32                 id);
};

HYSCAN_API
GType                  hyscan_db_get_type                      (void);

HYSCAN_API
HyScanDB *             hyscan_db_new                           (const gchar           *uri);

HYSCAN_API
gchar *                hyscan_db_get_uri                       (HyScanDB              *db);

HYSCAN_API
guint32                hyscan_db_get_mod_count                 (HyScanDB              *db,
                                                                gint32                 id);

HYSCAN_API
gboolean               hyscan_db_is_exist                      (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                const gchar           *channel_name);

HYSCAN_API
gchar **               hyscan_db_project_list                  (HyScanDB              *db);

HYSCAN_API
gint32                 hyscan_db_project_open                  (HyScanDB              *db,
                                                                const gchar           *project_name);

HYSCAN_API
gint32                 hyscan_db_project_create                (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *project_schema);

HYSCAN_API
gboolean               hyscan_db_project_remove                (HyScanDB              *db,
                                                                const gchar           *project_name);

HYSCAN_API
GDateTime *            hyscan_db_project_get_ctime             (HyScanDB              *db,
                                                                gint32                 project_id);

HYSCAN_API
gchar **               hyscan_db_project_param_list            (HyScanDB              *db,
                                                                gint32                 project_id);

HYSCAN_API
gint32                 hyscan_db_project_param_open            (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

HYSCAN_API
gboolean               hyscan_db_project_param_remove          (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

HYSCAN_API
gchar **               hyscan_db_track_list                    (HyScanDB              *db,
                                                                gint32                 project_id);

HYSCAN_API
gint32                 hyscan_db_track_open                    (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

HYSCAN_API
gint32                 hyscan_db_track_create                  (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name,
                                                                const gchar           *track_schema,
                                                                const gchar           *schema_id);

HYSCAN_API
gboolean               hyscan_db_track_remove                  (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

HYSCAN_API
GDateTime *            hyscan_db_track_get_ctime               (HyScanDB              *db,
                                                                gint32                 track_id);

HYSCAN_API
gint32                 hyscan_db_track_param_open              (HyScanDB              *db,
                                                                gint32                 track_id);

HYSCAN_API
gchar **               hyscan_db_channel_list                  (HyScanDB              *db,
                                                                gint32                 track_id);

HYSCAN_API
gint32                 hyscan_db_channel_open                  (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

HYSCAN_API
gint32                 hyscan_db_channel_create                (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name,
                                                                const gchar           *schema_id);

HYSCAN_API
gboolean               hyscan_db_channel_remove                (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

HYSCAN_API
GDateTime *            hyscan_db_channel_get_ctime             (HyScanDB              *db,
                                                                gint32                 channel_id);

HYSCAN_API
void                   hyscan_db_channel_finalize              (HyScanDB              *db,
                                                                gint32                 channel_id);

HYSCAN_API
gboolean               hyscan_db_channel_is_writable           (HyScanDB              *db,
                                                                gint32                 channel_id);

HYSCAN_API
gint32                 hyscan_db_channel_param_open            (HyScanDB              *db,
                                                                gint32                 channel_id);

HYSCAN_API
gboolean               hyscan_db_channel_set_chunk_size        (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                chunk_size);

HYSCAN_API
gboolean               hyscan_db_channel_set_save_time         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 save_time);

HYSCAN_API
gboolean               hyscan_db_channel_set_save_size         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                save_size);

HYSCAN_API
gboolean               hyscan_db_channel_get_data_range        (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

HYSCAN_API
gboolean               hyscan_db_channel_add_data              (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                HyScanBuffer          *buffer,
                                                                guint32               *index);

HYSCAN_API
gboolean               hyscan_db_channel_get_data              (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index,
                                                                HyScanBuffer          *buffer,
                                                                gint64                *time);

HYSCAN_API
guint32                hyscan_db_channel_get_data_size         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

HYSCAN_API
gint64                 hyscan_db_channel_get_data_time         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

HYSCAN_API
HyScanDBFindStatus     hyscan_db_channel_find_data             (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

HYSCAN_API
gchar **               hyscan_db_param_object_list             (HyScanDB              *db,
                                                                gint32                 param_id);

HYSCAN_API
gboolean               hyscan_db_param_object_create           (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

HYSCAN_API
gboolean               hyscan_db_param_object_remove           (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

HYSCAN_API
HyScanDataSchema *     hyscan_db_param_object_get_schema       (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

HYSCAN_API
gboolean               hyscan_db_param_set                     (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

HYSCAN_API
gboolean               hyscan_db_param_get                     (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

HYSCAN_API
void                   hyscan_db_close                         (HyScanDB              *db,
                                                                gint32                 id);

G_END_DECLS

#endif /* __HYSCAN_DB_H__ */
