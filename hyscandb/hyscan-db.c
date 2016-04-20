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

G_DEFINE_INTERFACE (HyScanDB, hyscan_db, G_TYPE_OBJECT);

static void
hyscan_db_default_init (HyScanDBInterface *iface)
{
}

gchar **
hyscan_db_get_project_type_list (HyScanDB *db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_project_type_list != NULL)
    return iface->get_project_type_list (db);

  return NULL;
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

gchar **
hyscan_db_get_project_list (HyScanDB * db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_project_list != NULL)
    return iface->get_project_list (db);

  return NULL;
}

gint32
hyscan_db_open_project (HyScanDB    *db,
                        const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_project != NULL)
    return iface->open_project (db, project_name);

  return -1;
}

gint32
hyscan_db_create_project (HyScanDB    *db,
                          const gchar *project_name,
                          const gchar *project_type)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_project != NULL)
    return iface->create_project (db, project_name, project_type);

  return -1;
}

gboolean
hyscan_db_remove_project (HyScanDB    *db,
                          const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_project != NULL)
    return iface->remove_project (db, project_name);

  return FALSE;
}

void
hyscan_db_close_project (HyScanDB *db,
                         gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close_project != NULL)
    iface->close_project (db, project_id);
}

GDateTime *
hyscan_db_get_project_ctime (HyScanDB *db,
                             gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_project_ctime != NULL)
    return iface->get_project_ctime (db, project_id);

  return NULL;
}

gchar **
hyscan_db_get_track_list (HyScanDB *db,
                          gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_track_list != NULL)
    return iface->get_track_list (db, project_id);

  return NULL;
}

gint32
hyscan_db_open_track (HyScanDB    *db,
                      gint32       project_id,
                      const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_track != NULL)
    return iface->open_track (db, project_id, track_name);

  return -1;
}

gint32
hyscan_db_create_track (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_track != NULL)
    return iface->create_track (db, project_id, track_name);

  return -1;
}

gboolean
hyscan_db_remove_track (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_track != NULL)
    return iface->remove_track (db, project_id, track_name);

  return FALSE;
}

void
hyscan_db_close_track (HyScanDB *db,
                       gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close_track != NULL)
    iface->close_track (db, track_id);
}

GDateTime *
hyscan_db_get_track_ctime (HyScanDB *db,
                           gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_track_ctime != NULL)
    return iface->get_track_ctime (db, track_id);

  return NULL;
}

gchar **
hyscan_db_get_channel_list (HyScanDB *db,
                            gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_channel_list != NULL)
    return iface->get_channel_list (db, track_id);

  return NULL;
}

gint32
hyscan_db_open_channel (HyScanDB    *db,
                        gint32       track_id,
                        const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_channel != NULL)
    return iface->open_channel (db, track_id, channel_name);

  return -1;
}

gint32
hyscan_db_create_channel (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_channel != NULL)
    return iface->create_channel (db, track_id, channel_name);

  return -1;
}

gboolean
hyscan_db_remove_channel (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_channel != NULL)
    return iface->remove_channel (db, track_id, channel_name);

  return FALSE;
}

void
hyscan_db_close_channel (HyScanDB *db,
                         gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close_channel != NULL)
    iface->close_channel (db, channel_id);
}

gint32
hyscan_db_open_channel_param (HyScanDB *db,
                              gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_channel_param != NULL)
    return iface->open_channel_param (db, channel_id);

  return -1;
}

gboolean
hyscan_db_set_channel_chunk_size (HyScanDB *db,
                                  gint32    channel_id,
                                  gint32    chunk_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_channel_chunk_size != NULL)
    return iface->set_channel_chunk_size (db, channel_id, chunk_size);

  return FALSE;
}

gboolean
hyscan_db_set_channel_save_time (HyScanDB *db,
                                 gint32    channel_id,
                                 gint64    save_time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_channel_save_time != NULL)
    return iface->set_channel_save_time (db, channel_id, save_time);

  return FALSE;
}

gboolean
hyscan_db_set_channel_save_size (HyScanDB *db,
                                 gint32    channel_id,
                                 gint64    save_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_channel_save_size != NULL)
    return iface->set_channel_save_size (db, channel_id, save_size);

  return FALSE;
}

void
hyscan_db_finalize_channel (HyScanDB *db,
                            gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->finalize_channel != NULL)
    iface->finalize_channel (db, channel_id);
}

gboolean
hyscan_db_get_channel_data_range (HyScanDB *db,
                                  gint32    channel_id,
                                  gint32   *first_index,
                                  gint32   *last_index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_channel_data_range != NULL)
    return iface->get_channel_data_range (db, channel_id, first_index, last_index);

  return FALSE;
}

gboolean
hyscan_db_add_channel_data (HyScanDB *db,
                            gint32    channel_id,
                            gint64    time,
                            gpointer  data,
                            gint32    size,
                            gint32   *index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->add_channel_data != NULL)
    return iface->add_channel_data (db, channel_id, time, data, size, index);

  return FALSE;
}

gboolean
hyscan_db_get_channel_data (HyScanDB *db,
                            gint32    channel_id,
                            gint32    index,
                            gpointer  buffer,
                            gint32   *buffer_size,
                            gint64   *time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_channel_data != NULL)
    return iface->get_channel_data (db, channel_id, index, buffer, buffer_size, time);

  return FALSE;
}

gboolean
hyscan_db_find_channel_data (HyScanDB *db,
                             gint32    channel_id,
                             gint64    time,
                             gint32   *lindex,
                             gint32   *rindex,
                             gint64   *ltime,
                             gint64   *rtime)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->find_channel_data != NULL)
    return iface->find_channel_data (db, channel_id, time, lindex, rindex, ltime, rtime);

  return FALSE;
}

gchar **
hyscan_db_get_project_param_list (HyScanDB *db,
                                  gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_project_param_list != NULL)
    return iface->get_project_param_list (db, project_id);

  return NULL;
}

gint32
hyscan_db_open_project_param (HyScanDB    *db,
                              gint32       project_id,
                              const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_project_param != NULL)
    return iface->open_project_param (db, project_id, group_name);

  return -1;
}

gboolean
hyscan_db_remove_project_param (HyScanDB    *db,
                                gint32       project_id,
                                const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_project_param != NULL)
    return iface->remove_project_param (db, project_id, group_name);

  return FALSE;
}

gchar **
hyscan_db_get_track_param_list (HyScanDB *db,
                                gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_track_param_list != NULL)
    return iface->get_track_param_list (db, track_id);

  return NULL;
}

gint32
hyscan_db_open_track_param (HyScanDB    *db,
                            gint32       track_id,
                            const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->open_track_param != NULL)
    return iface->open_track_param (db, track_id, group_name);

  return -1;
}

gboolean
hyscan_db_remove_track_param (HyScanDB    *db,
                              gint32       track_id,
                              const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_track_param != NULL)
    return iface->remove_track_param (db, track_id, group_name);

  return FALSE;
}

gchar **
hyscan_db_get_param_list (HyScanDB *db,
                          gint32    param_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_param_list != NULL)
    return iface->get_param_list (db, param_id);

  return NULL;
}

gboolean
hyscan_db_copy_param (HyScanDB    *db,
                      gint32       src_param_id,
                      gint32       dst_param_id,
                      const gchar *mask)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->copy_param != NULL)
    return iface->copy_param (db, src_param_id, dst_param_id, mask);

  return FALSE;
}

gboolean
hyscan_db_remove_param (HyScanDB    *db,
                        gint32       param_id,
                        const gchar *mask)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->remove_param != NULL)
    return iface->remove_param (db, param_id, mask);

  return FALSE;
}

void
hyscan_db_close_param (HyScanDB *db,
                       gint32    param_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close_param != NULL)
    iface->close_param (db, param_id);
}

gboolean
hyscan_db_has_param (HyScanDB    *db,
                     gint32       param_id,
                     const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->has_param != NULL)
    return iface->has_param (db, param_id, name);

  return 0;
}

gint64
hyscan_db_inc_integer_param (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), 0);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->inc_integer_param != NULL)
    return iface->inc_integer_param (db, param_id, name);

  return 0;
}

gboolean
hyscan_db_set_integer_param (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *name,
                             gint64       value)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_integer_param != NULL)
    return iface->set_integer_param (db, param_id, name, value);

  return FALSE;
}

gboolean
hyscan_db_set_double_param (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *name,
                            gdouble      value)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_double_param != NULL)
    return iface->set_double_param (db, param_id, name, value);

  return FALSE;
}

gboolean
hyscan_db_set_boolean_param (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *name,
                             gboolean     value)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_boolean_param != NULL)
    return iface->set_boolean_param (db, param_id, name, value);

  return FALSE;
}

gboolean
hyscan_db_set_string_param (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *name,
                            const gchar *value)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->set_string_param != NULL)
    return iface->set_string_param (db, param_id, name, value);

  return FALSE;
}

gint64
hyscan_db_get_integer_param (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), 0);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_integer_param != NULL)
    return iface->get_integer_param (db, param_id, name);

  return 0;
}

gdouble
hyscan_db_get_double_param (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), 0.0);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_double_param != NULL)
    return iface->get_double_param (db, param_id, name);

  return 0.0;
}

gboolean
hyscan_db_get_boolean_param (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_boolean_param != NULL)
    return iface->get_boolean_param (db, param_id, name);

  return FALSE;
}

gchar *
hyscan_db_get_string_param (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_string_param != NULL)
    return iface->get_string_param (db, param_id, name);

  return NULL;
}
