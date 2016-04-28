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

guint64
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
                          const gchar *project_schema)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_project != NULL)
    return iface->create_project (db, project_name, project_schema);

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
                        const gchar *track_name,
                        const gchar *track_schema,
                        const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_track != NULL)
    return iface->create_track (db, project_id, track_name, track_schema, schema_id);

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
                          const gchar *channel_name,
                          const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->create_channel != NULL)
    return iface->create_channel (db, track_id, channel_name, schema_id);

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

void
hyscan_db_param_close (HyScanDB *db,
                       gint32    param_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_close != NULL)
    iface->param_close (db, param_id);
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
hyscan_db_param_set (HyScanDB             *db,
                     gint32                param_id,
                     const gchar          *object_name,
                     const gchar          *param_name,
                     HyScanDataSchemaType  param_type,
                     gconstpointer         param_value,
                     gint32                param_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_set != NULL)
    return iface->param_set (db, param_id, object_name, param_name, param_type, param_value, param_size);

  return FALSE;
}

gboolean
hyscan_db_param_get (HyScanDB             *db,
                     gint32                param_id,
                     const gchar          *object_name,
                     const gchar          *param_name,
                     HyScanDataSchemaType  param_type,
                     gpointer              buffer,
                     gint32               *buffer_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_get != NULL)
    return iface->param_get (db, param_id, object_name, param_name, param_type, buffer, buffer_size);

  return FALSE;
}

gboolean
hyscan_db_param_set_boolean (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *object_name,
                             const gchar *param_name,
                             gboolean     value)
{
  return hyscan_db_param_set (db, param_id, object_name, param_name,
                              HYSCAN_DATA_SCHEMA_TYPE_BOOLEAN, &value, sizeof (value));
}

gboolean
hyscan_db_param_set_integer (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *object_name,
                             const gchar *param_name,
                             gint64       value)
{
  return hyscan_db_param_set (db, param_id, object_name, param_name,
                              HYSCAN_DATA_SCHEMA_TYPE_INTEGER, &value, sizeof (value));
}

gboolean
hyscan_db_param_set_double (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *object_name,
                            const gchar *param_name,
                            gdouble      value)
{
  return hyscan_db_param_set (db, param_id, object_name, param_name,
                              HYSCAN_DATA_SCHEMA_TYPE_DOUBLE, &value, sizeof (value));
}

gboolean
hyscan_db_param_set_string (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *object_name,
                            const gchar *param_name,
                            const gchar *value)
{
  return hyscan_db_param_set (db, param_id, object_name, param_name,
                              HYSCAN_DATA_SCHEMA_TYPE_STRING, value, strlen (value));
}

gboolean
hyscan_db_param_get_boolean (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *object_name,
                             const gchar *param_name)
{
  gboolean status;
  gboolean value;
  gint32 size;

  size = sizeof (value);
  status = hyscan_db_param_get (db, param_id, object_name, param_name,
                                HYSCAN_DATA_SCHEMA_TYPE_BOOLEAN, &value, &size);
  if (!status)
    return FALSE;

  return value;
}

gint64
hyscan_db_param_get_integer (HyScanDB    *db,
                             gint32       param_id,
                             const gchar *object_name,
                             const gchar *param_name)
{
  gboolean status;
  gint64 value;
  gint32 size;

  size = sizeof (value);
  status = hyscan_db_param_get (db, param_id, object_name, param_name,
                                HYSCAN_DATA_SCHEMA_TYPE_INTEGER, &value, &size);
  if (!status)
    return 0;

  return value;
}

gdouble
hyscan_db_param_get_double (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *object_name,
                            const gchar *param_name)
{
  gboolean status;
  gdouble value;
  gint32 size;

  size = sizeof (value);
  status = hyscan_db_param_get (db, param_id, object_name, param_name,
                                HYSCAN_DATA_SCHEMA_TYPE_DOUBLE, &value, &size);
  if (!status)
    return 0.0;

  return value;
}

gchar *
hyscan_db_param_get_string (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *object_name,
                            const gchar *param_name)
{
  gboolean status;
  gchar *value;
  gint32 size;

  status = hyscan_db_param_get (db, param_id, object_name, param_name,
                                HYSCAN_DATA_SCHEMA_TYPE_STRING, NULL, &size);
  if (!status)
    return NULL;

  value = g_malloc (size);
  status = hyscan_db_param_get (db, param_id, object_name, param_name,
                                HYSCAN_DATA_SCHEMA_TYPE_STRING, value, &size);
  if (!status)
    {
      g_free (value);
      return NULL;
    }

  return value;
}
