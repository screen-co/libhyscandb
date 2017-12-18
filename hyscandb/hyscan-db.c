/*
 * \file hyscan-db.c
 *
 * \brief Исходный файл интерфейса базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db.h"
#include <string.h>

G_DEFINE_INTERFACE (HyScanDB, hyscan_db, G_TYPE_OBJECT);

static void
hyscan_db_default_init (HyScanDBInterface *iface)
{
}

gchar *
hyscan_db_get_uri (HyScanDB *db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
 if (iface->get_uri != NULL)
    return iface->get_uri (db);

  return NULL;
}

guint32
hyscan_db_get_mod_count (HyScanDB *db,
                         gint32    id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), 0);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_mod_count != NULL)
    return iface->get_mod_count (db, id);

  return 0;
}

gboolean
hyscan_db_is_exist (HyScanDB    *db,
                    const gchar *project_name,
                    const gchar *track_name,
                    const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->is_exist != NULL)
    return iface->is_exist (db, project_name, track_name, channel_name);

  return FALSE;
}

gchar **
hyscan_db_project_list (HyScanDB * db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_list != NULL)
    return iface->project_list (db);

  return NULL;
}

gint32
hyscan_db_project_open (HyScanDB    *db,
                        const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_open != NULL)
    return iface->project_open (db, project_name);

  return -1;
}

gint32
hyscan_db_project_create (HyScanDB    *db,
                          const gchar *project_name,
                          const gchar *project_schema)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_create != NULL)
    return iface->project_create (db, project_name, project_schema);

  return -1;
}

gboolean
hyscan_db_project_remove (HyScanDB    *db,
                          const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_remove != NULL)
    return iface->project_remove (db, project_name);

  return FALSE;
}

GDateTime *
hyscan_db_project_get_ctime (HyScanDB *db,
                             gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_get_ctime != NULL)
    return iface->project_get_ctime (db, project_id);

  return NULL;
}

gchar **
hyscan_db_project_param_list (HyScanDB *db,
                                    gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_list != NULL)
    return iface->project_param_list (db, project_id);

  return NULL;
}

gint32
hyscan_db_project_param_open (HyScanDB    *db,
                              gint32       project_id,
                              const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_open != NULL)
    return iface->project_param_open (db, project_id, group_name);

  return -1;
}

gboolean
hyscan_db_project_param_remove (HyScanDB    *db,
                                gint32       project_id,
                                const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_remove != NULL)
    return iface->project_param_remove (db, project_id, group_name);

  return FALSE;
}

gchar **
hyscan_db_track_list (HyScanDB *db,
                      gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_list != NULL)
    return iface->track_list (db, project_id);

  return NULL;
}

gint32
hyscan_db_track_open (HyScanDB    *db,
                      gint32       project_id,
                      const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_open != NULL)
    return iface->track_open (db, project_id, track_name);

  return -1;
}

gint32
hyscan_db_track_create (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name,
                        const gchar *track_schema,
                        const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_create != NULL)
    return iface->track_create (db, project_id, track_name, track_schema, schema_id);

  return -1;
}

gboolean
hyscan_db_track_remove (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_remove != NULL)
    return iface->track_remove (db, project_id, track_name);

  return FALSE;
}

GDateTime *
hyscan_db_track_get_ctime (HyScanDB *db,
                           gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_get_ctime != NULL)
    return iface->track_get_ctime (db, track_id);

  return NULL;
}

gint32
hyscan_db_track_param_open (HyScanDB *db,
                            gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_param_open != NULL)
    return iface->track_param_open (db, track_id);

  return -1;
}

gchar **
hyscan_db_channel_list (HyScanDB *db,
                        gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_list != NULL)
    return iface->channel_list (db, track_id);

  return NULL;
}

gint32
hyscan_db_channel_open (HyScanDB    *db,
                        gint32       track_id,
                        const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_open != NULL)
    return iface->channel_open (db, track_id, channel_name);

  return -1;
}

gint32
hyscan_db_channel_create (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name,
                          const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_create != NULL)
    return iface->channel_create (db, track_id, channel_name, schema_id);

  return -1;
}

gboolean
hyscan_db_channel_remove (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_remove != NULL)
    return iface->channel_remove (db, track_id, channel_name);

  return FALSE;
}

GDateTime *
hyscan_db_channel_get_ctime (HyScanDB *db,
                             gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_ctime != NULL)
    return iface->channel_get_ctime (db, channel_id);

  return NULL;
}

void
hyscan_db_channel_finalize (HyScanDB *db,
                            gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_finalize != NULL)
    iface->channel_finalize (db, channel_id);
}

gboolean
hyscan_db_channel_is_writable (HyScanDB *db,
                               gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_is_writable != NULL)
    return iface->channel_is_writable (db, channel_id);

  return FALSE;
}

gint32
hyscan_db_channel_param_open (HyScanDB *db,
                              gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_param_open != NULL)
    return iface->channel_param_open (db, channel_id);

  return -1;
}

gboolean
hyscan_db_channel_set_chunk_size (HyScanDB *db,
                                  gint32    channel_id,
                                  guint64   chunk_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_chunk_size != NULL)
    return iface->channel_set_chunk_size (db, channel_id, chunk_size);

  return FALSE;
}

gboolean
hyscan_db_channel_set_save_time (HyScanDB *db,
                                 gint32    channel_id,
                                 gint64    save_time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_save_time != NULL)
    return iface->channel_set_save_time (db, channel_id, save_time);

  return FALSE;
}

gboolean
hyscan_db_channel_set_save_size (HyScanDB *db,
                                 gint32    channel_id,
                                 guint64   save_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_save_size != NULL)
    return iface->channel_set_save_size (db, channel_id, save_size);

  return FALSE;
}

gboolean
hyscan_db_channel_get_data_range (HyScanDB *db,
                                  gint32    channel_id,
                                  guint32  *first_index,
                                  guint32  *last_index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data_range != NULL)
    return iface->channel_get_data_range (db, channel_id, first_index, last_index);

  return FALSE;
}

gboolean
hyscan_db_channel_add_data (HyScanDB      *db,
                            gint32         channel_id,
                            gint64         time,
                            gconstpointer  data,
                            guint32        size,
                            guint32       *index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_add_data != NULL)
    return iface->channel_add_data (db, channel_id, time, data, size, index);

  return FALSE;
}

gboolean
hyscan_db_channel_get_data (HyScanDB *db,
                            gint32    channel_id,
                            guint32   index,
                            gpointer  buffer,
                            guint32  *buffer_size,
                            gint64   *time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data != NULL)
    return iface->channel_get_data (db, channel_id, index, buffer, buffer_size, time);

  return FALSE;
}

HyScanDBFindStatus
hyscan_db_channel_find_data (HyScanDB *db,
                             gint32    channel_id,
                             gint64    time,
                             guint32  *lindex,
                             guint32  *rindex,
                             gint64   *ltime,
                             gint64   *rtime)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), HYSCAN_DB_FIND_FAIL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_find_data != NULL)
    return iface->channel_find_data (db, channel_id, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

gchar **
hyscan_db_param_object_list (HyScanDB *db,
                             gint32    param_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_list != NULL)
    return iface->param_object_list (db, param_id);

  return NULL;
}

gboolean
hyscan_db_param_object_create (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *object_name,
                               const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_create != NULL)
    return iface->param_object_create (db, param_id, object_name, schema_id);

  return FALSE;
}

gboolean
hyscan_db_param_object_remove (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *object_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_remove != NULL)
    return iface->param_object_remove (db, param_id, object_name);

  return FALSE;
}

HyScanDataSchema *
hyscan_db_param_object_get_schema (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *object_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_get_schema != NULL)
    return iface->param_object_get_schema (db, param_id, object_name);

  return NULL;
}

gboolean
hyscan_db_param_set (HyScanDB        *db,
                     gint32           param_id,
                     const gchar     *object_name,
                     HyScanParamList *param_list)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_set != NULL)
    return iface->param_set (db, param_id, object_name, param_list);

  return FALSE;
}

gboolean
hyscan_db_param_get (HyScanDB        *db,
                     gint32           param_id,
                     const gchar     *object_name,
                     HyScanParamList *param_list)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_get != NULL)
    return iface->param_get (db, param_id, object_name, param_list);

  return FALSE;
}

void
hyscan_db_close (HyScanDB *db,
                 gint32    id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close != NULL)
    iface->close (db, id);
}
