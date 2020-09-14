/* hyscan-db-channel-file.h
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

#ifndef __HYSCAN_DB_CHANNEL_FILE_H__
#define __HYSCAN_DB_CHANNEL_FILE_H__

#include "hyscan-db.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_CHANNEL_FILE             (hyscan_db_channel_file_get_type ())
#define HYSCAN_DB_CHANNEL_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFile))
#define HYSCAN_IS_DB_CHANNEL_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE))
#define HYSCAN_DB_CHANNEL_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFileClass))
#define HYSCAN_IS_DB_CHANNEL_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_CHANNEL_FILE))
#define HYSCAN_DB_CHANNEL_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFileClass))

typedef struct _HyScanDBChannelFile HyScanDBChannelFile;
typedef struct _HyScanDBChannelFilePrivate HyScanDBChannelFilePrivate;
typedef struct _HyScanDBChannelFileClass HyScanDBChannelFileClass;

struct _HyScanDBChannelFile
{
  GObject parent_instance;

  HyScanDBChannelFilePrivate *priv;
};

struct _HyScanDBChannelFileClass
{
  GObjectClass parent_class;
};

GType      hyscan_db_channel_file_get_type                  (void);

HyScanDBChannelFile *hyscan_db_channel_file_new             (const gchar         *path,
                                                             const gchar         *name,
                                                             gboolean             readonly);

gint64     hyscan_db_channel_file_get_ctime                 (HyScanDBChannelFile *channel);

gboolean   hyscan_db_channel_file_get_channel_data_range    (HyScanDBChannelFile *channel,
                                                             guint32             *first_index,
                                                             guint32             *last_index);

gboolean   hyscan_db_channel_file_add_channel_data          (HyScanDBChannelFile *channel,
                                                             gint64               time,
                                                             HyScanBuffer        *buffer,
                                                             guint32             *index);

gboolean   hyscan_db_channel_file_get_channel_data          (HyScanDBChannelFile *channel,
                                                             guint32              index,
                                                             HyScanBuffer        *buffer,
                                                             gint64              *time);

guint32    hyscan_db_channel_file_get_channel_data_size     (HyScanDBChannelFile *channel,
                                                             guint32              index);

gint64     hyscan_db_channel_file_get_channel_data_time     (HyScanDBChannelFile *channel,
                                                             guint32              index);

HyScanDBFindStatus hyscan_db_channel_file_find_channel_data (HyScanDBChannelFile *channel,
                                                             gint64               time,
                                                             guint32             *lindex,
                                                             guint32             *rindex,
                                                             gint64              *ltime,
                                                             gint64              *rtime);

gboolean   hyscan_db_channel_file_set_channel_chunk_size    (HyScanDBChannelFile *channel,
                                                             guint64              chunk_size);

gboolean   hyscan_db_channel_file_set_channel_save_time     (HyScanDBChannelFile *channel,
                                                             gint64               save_time);

gboolean   hyscan_db_channel_file_set_channel_save_size     (HyScanDBChannelFile *channel,
                                                             guint64              save_size);

void       hyscan_db_channel_file_finalize_channel          (HyScanDBChannelFile *channel);

gboolean   hyscan_db_channel_remove_channel_files           (const gchar         *path,
                                                             const gchar         *name);

G_END_DECLS

#endif /* __HYSCAN_DB_CHANNEL_FILE_H__ */
