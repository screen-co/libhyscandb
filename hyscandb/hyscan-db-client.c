/*
 * \file hyscan-db-client.c
 *
 * \brief Исходный файл RPC клиента базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-client.h"
#include "hyscan-db-rpc.h"

#include <urpc-client.h>
#include <string.h>

#define hyscan_db_client_lock_error()      do { \
                                             g_warning ("HyScanDBClient: %s: can't lock rpc transport to '%s'", __FUNCTION__, priv->uri); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_get_error(p)      do { \
                                             g_warning ("HyScanDBClient: %s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_set_error(p)      do { \
                                             g_warning ("HyScanDBClient: %s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_exec_error()      do { \
                                             g_warning ("HyScanDBClient: %s: can't execute procedure", __FUNCTION__); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI
};

struct _HyScanDBClientPrivate
{
  gchar               *uri;                    /* Путь к RPC серверу. */
  uRpcClient          *rpc;                    /* RPC клиент. */
};

static void    hyscan_db_client_interface_init         (HyScanDBInterface      *iface);
static void    hyscan_db_client_set_property           (GObject                *object,
                                                        guint                   prop_id,
                                                        const GValue           *value,
                                                        GParamSpec             *pspec);
static void    hyscan_db_client_object_constructed     (GObject                *object);
static void    hyscan_db_client_object_finalize        (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanDBClient, hyscan_db_client, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDBClient)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DB, hyscan_db_client_interface_init));

static void hyscan_db_client_class_init( HyScanDBClientClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_db_client_set_property;
  object_class->constructed = hyscan_db_client_object_constructed;
  object_class->finalize = hyscan_db_client_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "HyScan db uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_client_init (HyScanDBClient *dbc)
{
  dbc->priv = hyscan_db_client_get_instance_private (dbc);
}

static void
hyscan_db_client_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (object);
  HyScanDBClientPrivate *priv = dbc->priv;

  switch (prop_id)
    {
    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_client_object_constructed (GObject *object)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (object);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 version;

  priv->rpc = urpc_client_create (priv->uri, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (priv->rpc == NULL)
    return;

  if (urpc_client_connect (priv->rpc) != 0)
    {
      g_warning ("HyScanDBClient: can't connect to '%s'", priv->uri);
      goto fail;
    }

  /* Проверка версии сервера базы данных. */
  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    {
      g_warning ("HyScanDBClient: can't lock rpc transport to '%s'", priv->uri);
      goto fail;
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_VERSION) != URPC_STATUS_OK)
    {
      urpc_client_unlock (priv->rpc);
      g_warning ("HyScanDBClient: can't execute procedure");
      goto fail;
    }

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_VERSION, &version) != 0 ||
      version != HYSCAN_DB_RPC_VERSION)
    {
      g_warning ("HyScanDBClient: server version mismatch: need %d, got: %d",
                 HYSCAN_DB_RPC_VERSION, version);
      urpc_client_unlock (priv->rpc);
      goto fail;
    }

  urpc_client_unlock (priv->rpc);
  return;

fail:
  urpc_client_destroy (priv->rpc);
  priv->rpc = NULL;
}

static void
hyscan_db_client_object_finalize (GObject *object)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (object);
  HyScanDBClientPrivate *priv = dbc->priv;

  if (priv->rpc != NULL)
    urpc_client_destroy (priv->rpc);

  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_db_client_parent_class)->finalize (object);
}

/* Все функции этого класса реализованы одинаково:
    - блокируется канал передачи данных RPC;
    - устанавливаются значения параметров вызываемой функции;
    - производится вызов RPC функции;
    - считывается результат вызова функции;
    - освобождается канал передачи. */

static gchar *
hyscan_db_client_get_uri (HyScanDB *db)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar *uri = NULL;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_GET_URI) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  uri = g_strdup (urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_URI, 0));

exit:
  urpc_client_unlock (priv->rpc);
  return uri;
}

guint64
hyscan_db_client_get_mod_count (HyScanDB *db,
                                gint32    id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  guint64 mod_count;

  if (priv->rpc == NULL)
    return 0;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_ID, id) != 0)
    hyscan_db_client_set_error ("id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_GET_MOD_COUNT) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_DB_RPC_PARAM_MOD_COUNT, &mod_count) != 0)
    {
      mod_count = 0;
      hyscan_db_client_get_error ("mod_count");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return mod_count;
}

static gboolean
hyscan_db_client_is_exist (HyScanDB    *db,
                           const gchar *project_name,
                           const gchar *track_name,
                           const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean exist = FALSE;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (track_name != NULL)
    if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
      hyscan_db_client_set_error ("track_name");

  if (channel_name != NULL)
    if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
      hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_IS_EXIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  exist = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return exist;
}

static gchar **
hyscan_db_client_project_list (HyScanDB *db)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **project_list = NULL;
  guint32 n_project_list;
  guint32 i;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_project_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_LIST);
  if (n_project_list == 0)
    goto exit;

  project_list = g_malloc0 ((n_project_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_project_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_LIST, i);
      project_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (priv->rpc);
  return project_list;
}

static gint32
hyscan_db_client_project_open (HyScanDB    *db,
                               const gchar *project_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 project_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    {
      project_id = -1;
      hyscan_db_client_get_error ("project_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return project_id;
}

static gint32
hyscan_db_client_project_create (HyScanDB    *db,
                                 const gchar *project_name,
                                 const gchar *project_schema)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 project_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (project_schema != NULL)
    {
      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_SCHEMA, project_schema) != 0)
        hyscan_db_client_set_error ("project_schema");
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_CREATE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    {
      project_id = -1;
      hyscan_db_client_get_error ("project_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return project_id;
}

static gboolean
hyscan_db_client_project_remove (HyScanDB    *db,
                                 const gchar *project_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_REMOVE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static GDateTime *
hyscan_db_client_project_get_ctime (HyScanDB *db,
                                    gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  GDateTime *ctime = NULL;
  gint64 itime;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_GET_CTIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_CTIME, &itime) != 0)
    hyscan_db_client_get_error ("itime");

  ctime = g_date_time_new_from_unix_utc (itime);

exit:
  urpc_client_unlock (priv->rpc);
  return ctime;
}

static gchar **
hyscan_db_client_project_param_list (HyScanDB *db,
                                     gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **param_list = NULL;
  guint32 n_param_list;
  guint32 i;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_param_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_LIST);
  if (n_param_list == 0)
    goto exit;

  param_list = g_malloc0 ((n_param_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_param_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_LIST, i);
      param_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (priv->rpc);
  return param_list;
}

static gint32
hyscan_db_client_project_open_param (HyScanDB    *db,
                                     gint32       project_id,
                                     const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_client_get_error ("param_id");

exit:
  urpc_client_unlock (priv->rpc);
  return param_id;
}

static gboolean
hyscan_db_client_project_param_remove (HyScanDB    *db,
                                       gint32       project_id,
                                       const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_REMOVE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gchar **
hyscan_db_client_track_list (HyScanDB *db,
                                 gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **track_list = NULL;
  guint32 n_track_list;
  guint32 i;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_track_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_LIST);
  if (n_track_list == 0)
    goto exit;

  track_list = g_malloc0 ((n_track_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_track_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_LIST, i);
      track_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (priv->rpc);
  return track_list;
}

static gint32
hyscan_db_client_track_open (HyScanDB    *db,
                             gint32       project_id,
                             const gchar *track_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 track_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    {
      track_id = -1;
      hyscan_db_client_get_error ("track_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return track_id;
}

static gint32
hyscan_db_client_track_create (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name,
                               const gchar *track_schema,
                               const gchar *schema_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 track_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (track_schema != NULL)
    {
      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_SCHEMA, track_schema) != 0)
        hyscan_db_client_set_error ("track_schema");
    }

  if (schema_id != NULL)
    {
      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_SCHEMA_ID, schema_id) != 0)
        hyscan_db_client_set_error ("schema_id");
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_CREATE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    {
      track_id = -1;
      hyscan_db_client_get_error ("track_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return track_id;
}

static gboolean
hyscan_db_client_track_remove (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_REMOVE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static GDateTime *
hyscan_db_client_track_get_ctime (HyScanDB *db,
                                  gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  GDateTime *ctime = NULL;
  gint64 itime;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_GET_CTIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_CTIME, &itime) != 0)
    hyscan_db_client_get_error ("itime");

  ctime = g_date_time_new_from_unix_local (itime);

exit:
  urpc_client_unlock (priv->rpc);
  return ctime;
}

static gint32
hyscan_db_client_track_param_open (HyScanDB    *db,
                                   gint32       track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_PARAM_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_client_get_error ("param_id");

exit:
  urpc_client_unlock (priv->rpc);
  return param_id;
}

static gchar **
hyscan_db_client_channel_list (HyScanDB *db,
                                   gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **channel_list = NULL;
  guint32 n_channel_list;
  guint32 i;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_channel_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_LIST);
  if (n_channel_list == 0)
    goto exit;

  channel_list = g_malloc0 ((n_channel_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_channel_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_LIST, i);
      channel_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (priv->rpc);
  return channel_list;
}

static gint32
hyscan_db_client_channel_open (HyScanDB    *db,
                               gint32       track_id,
                               const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 channel_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    {
      channel_id = -1;
      hyscan_db_client_get_error ("channel_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return channel_id;
}

static gint32
hyscan_db_client_channel_create (HyScanDB    *db,
                                 gint32       track_id,
                                 const gchar *channel_name,
                                 const gchar *schema_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 channel_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (schema_id != NULL)
    {
      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_SCHEMA_ID, schema_id) != 0)
        hyscan_db_client_set_error ("schema_id");
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_CREATE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    {
      channel_id = -1;
      hyscan_db_client_get_error ("channel_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return channel_id;
}

static gboolean
hyscan_db_client_channel_remove (HyScanDB    *db,
                                 gint32       track_id,
                                 const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_REMOVE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gint32
hyscan_db_client_channel_param_open (HyScanDB *db,
                                     gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (priv->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_PARAM_OPEN) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    {
      param_id = -1;
      hyscan_db_client_get_error ("param_id");
    }

exit:
  urpc_client_unlock (priv->rpc);
  return param_id;
}

static gboolean
hyscan_db_client_channel_set_chunk_size (HyScanDB *db,
                                         gint32    channel_id,
                                         gint32    chunk_size)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHUNK_SIZE, chunk_size) != 0)
    hyscan_db_client_set_error ("chunk_size");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_CHUNK_SIZE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_set_save_time (HyScanDB *db,
                                        gint32    channel_id,
                                        gint64    save_time)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_TIME, save_time) != 0)
    hyscan_db_client_set_error ("save_time");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_SAVE_TIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_set_save_size (HyScanDB *db,
                                        gint32    channel_id,
                                        gint64    save_size)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_SIZE, save_size) != 0)
    hyscan_db_client_set_error ("save_size");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_SAVE_SIZE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static void
hyscan_db_client_channel_finalize (HyScanDB *db,
                                   gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  if (priv->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_FINALIZE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (priv->rpc);
}

static gboolean
hyscan_db_client_channel_is_writable (HyScanDB *db,
                                      gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_IS_WRITABLE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_get_data_range (HyScanDB *db,
                                         gint32    channel_id,
                                         gint32   *first_index,
                                         gint32   *last_index)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA_RANGE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (first_index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LINDEX, first_index) != 0)
        hyscan_db_client_get_error ("first_index");
    }

  if (last_index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RINDEX, last_index) != 0)
        hyscan_db_client_get_error ("last_index");
    }

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_add_data (HyScanDB      *db,
                                   gint32         channel_id,
                                   gint64         time,
                                   gconstpointer  data,
                                   gint32         size,
                                   gint32        *index)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, time) != 0)
    hyscan_db_client_set_error ("time");

  if (urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_DATA, data, size) == NULL)
    hyscan_db_client_set_error ("data");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_ADD_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, index) != 0)
        hyscan_db_client_get_error ("index");
    }

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_get_data (HyScanDB *db,
                                   gint32    channel_id,
                                   gint32    index,
                                   gpointer  buffer,
                                   gint32   *buffer_size,
                                   gint64   *time)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, index) != 0)
    hyscan_db_client_set_error ("index");

  if (buffer != NULL)
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_SIZE, *buffer_size) != 0)
        hyscan_db_client_set_error ("buffer_size");
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (buffer != NULL)
    {
      guint32 data_size;
      gpointer data;
      data = urpc_data_get (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_DATA, &data_size);
      if (data == NULL)
        hyscan_db_client_get_error ("data");
      if (data_size > *buffer_size)
        data_size = *buffer_size;
      else
        *buffer_size = data_size;
      memcpy (buffer, data, data_size);
    }
  else
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_SIZE, buffer_size) != 0)
        hyscan_db_client_get_error ("buffer_size");
    }

  if (time != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, time) != 0)
        hyscan_db_client_get_error ("time");
    }

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_channel_find_data (HyScanDB *db,
                                    gint32    channel_id,
                                    gint64    time,
                                    gint32   *lindex,
                                    gint32   *rindex,
                                    gint64   *ltime,
                                    gint64   *rtime)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, time) != 0)
    hyscan_db_client_set_error ("time");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_FIND_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (lindex != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LINDEX, lindex) != 0)
        hyscan_db_client_get_error ("lindex");
    }

  if (rindex != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RINDEX, rindex) != 0)
        hyscan_db_client_get_error ("rindex");
    }

  if (ltime != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LTIME, ltime) != 0)
        hyscan_db_client_get_error ("ltime");
    }

  if (rtime != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RTIME, rtime) != 0)
        hyscan_db_client_get_error ("rtime");
    }

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gchar **
hyscan_db_client_param_object_list (HyScanDB *db,
                                 gint32    param_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **param_list = NULL;
  guint32 n_param_list;
  guint32 i;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_param_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_LIST);
  if (n_param_list == 0)
    goto exit;

  param_list = g_malloc0 ((n_param_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_param_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_LIST, i);
      param_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (priv->rpc);
  return param_list;
}


static gboolean
hyscan_db_client_param_object_create (HyScanDB    *db,
                                      gint32       param_id,
                                      const gchar *object_name,
                                      const gchar *schema_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, object_name) != 0)
    hyscan_db_client_set_error ("object_name");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA_ID, schema_id) != 0)
    hyscan_db_client_set_error ("schema_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_CREATE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_param_object_remove (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *object_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, object_name) != 0)
    hyscan_db_client_set_error ("object_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_REMOVE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static HyScanDataSchema *
hyscan_db_client_param_object_get_schema (HyScanDB    *db,
                                          gint32       param_id,
                                          const gchar *object_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  HyScanDataSchema *schema = NULL;
  const gchar *schema_data;
  const gchar *schema_id;

  if (priv->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (object_name != NULL)
    if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, object_name) != 0)
      hyscan_db_client_set_error ("object_name");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_GET_SCHEMA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  schema_data = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA, 0);
  if (schema_data == NULL)
    goto exit;

  schema_id = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA_ID, 0);
  if (schema_id == NULL)
    hyscan_db_client_get_error ("schema");

  schema = hyscan_data_schema_new_from_string (schema_data, schema_id);

exit:
  urpc_client_unlock (priv->rpc);
  return schema;
}

static gboolean
hyscan_db_client_param_set (HyScanDB            *db,
                            gint32               param_id,
                            const gchar         *object_name,
                            const gchar *const  *param_names,
                            GVariant           **param_values)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  gint i;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (object_name != NULL)
    if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, object_name) != 0)
      hyscan_db_client_set_error ("object_name");

  for (i = 0; param_names[i] != NULL; i++)
    {
      gint set_status;
      GVariantClass value_type;

      if (i == HYSCAN_DB_RPC_MAX_PARAMS - 1)
        hyscan_db_client_set_error ("n_params");

      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, param_names[i]) != 0)
        hyscan_db_client_set_error ("param_name");

      if (param_values[i] == NULL)
        {
          set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_NULL);
          if (set_status != 0)
            hyscan_db_client_set_error ("param_type");

          continue;
        }

      value_type = g_variant_classify (param_values[i]);
      switch (value_type)
        {
        case G_VARIANT_CLASS_BOOLEAN:
          {
            gboolean value = g_variant_get_boolean (param_values[i]);

            set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_BOOLEAN);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_type");

            set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, value ? 1 : 0);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_value");
          }
          break;

        case G_VARIANT_CLASS_INT64:
          {
            gint64 value = g_variant_get_int64 (param_values[i]);

            set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_INT64);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_type");

            set_status = urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, value);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_value");
          }
          break;

        case G_VARIANT_CLASS_DOUBLE:
          {
            gdouble value = g_variant_get_double (param_values[i]);

            set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_DOUBLE);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_type");

            set_status = urpc_data_set_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, value);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_value");
          }
          break;

        case G_VARIANT_CLASS_STRING:
          {
            const gchar *value = g_variant_get_string (param_values[i], NULL);

            set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_STRING);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_type");

            set_status = urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, value);
            if (set_status != 0)
              hyscan_db_client_set_error ("param_value");
          }
          break;

        default:
          break;
        }
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_SET) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  for (i = 0; param_names[i] != NULL; i++)
    g_clear_pointer (&param_values[i], g_variant_unref);

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static gboolean
hyscan_db_client_param_get (HyScanDB            *db,
                            gint32               param_id,
                            const gchar         *object_name,
                            const gchar *const  *param_names,
                            GVariant           **param_values)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  gint i;

  if (priv->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (object_name != NULL)
    if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, object_name) != 0)
      hyscan_db_client_set_error ("object_name");

  for (i = 0; param_names[i] != NULL; i++)
    {
      if (i == HYSCAN_DB_RPC_MAX_PARAMS - 1)
        hyscan_db_client_set_error ("n_params");

      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, param_names[i]) != 0)
        hyscan_db_client_set_error ("param_name");
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_GET) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  for (i = 0; param_names[i] != NULL; i++)
    {
      guint32 param_type;

      if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, &param_type) != 0)
        hyscan_db_client_get_error ("param_type");

      switch (param_type)
        {
        case HYSCAN_DB_RPC_TYPE_NULL:
          break;

        case HYSCAN_DB_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;
            if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_client_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_INT64:
          {
            gint64 param_value;
            if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_client_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;
            if (urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_client_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_STRING:
          {
            if (urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, 0) == NULL)
              hyscan_db_client_get_error ("param_value");
          }
          break;

        default:
          goto exit;
        }
    }

  for (i = 0; param_names[i] != NULL; i++)
    {
      guint32 value_type;

      urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, &value_type);
      switch (value_type)
        {
        case HYSCAN_DB_RPC_TYPE_NULL:
          {
            param_values[i] = NULL;
          }
          break;

        case HYSCAN_DB_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;

            urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value);
            param_values[i] = g_variant_new_boolean (param_value ? TRUE : FALSE);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_INT64:
          {
            gint64 param_value;

            urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value);
            param_values[i] = g_variant_new_int64 (param_value);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;

            urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value);
            param_values[i] = g_variant_new_double (param_value);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_STRING:
          {
            const gchar *param_value;

            param_value = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, 0);
            param_values[i] = g_variant_new_string (param_value);
          }
          break;

        default:
          break;
        }
    }

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);
  return status;
}

static void
hyscan_db_client_close (HyScanDB *db,
                        gint32    object_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  HyScanDBClientPrivate *priv = dbc->priv;

  uRpcData *urpc_data;
  guint32 exec_status;

  if (priv->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_ID, object_id) != 0)
    hyscan_db_client_set_error ("object_id");

  if (urpc_client_exec (priv->rpc, HYSCAN_DB_RPC_PROC_CLOSE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (priv->rpc);
}

HyScanDBClient *
hyscan_db_client_new (const gchar *uri)
{
  return g_object_new (HYSCAN_TYPE_DB_CLIENT, "uri", uri, NULL);
}

static void
hyscan_db_client_interface_init (HyScanDBInterface *iface)
{
  iface->get_uri = hyscan_db_client_get_uri;
  iface->get_mod_count = hyscan_db_client_get_mod_count;
  iface->is_exist = hyscan_db_client_is_exist;

  iface->project_list = hyscan_db_client_project_list;
  iface->project_open = hyscan_db_client_project_open;
  iface->project_create = hyscan_db_client_project_create;
  iface->project_remove = hyscan_db_client_project_remove;
  iface->project_get_ctime = hyscan_db_client_project_get_ctime;
  iface->project_param_list = hyscan_db_client_project_param_list;
  iface->project_param_open = hyscan_db_client_project_open_param;
  iface->project_param_remove = hyscan_db_client_project_param_remove;

  iface->track_list = hyscan_db_client_track_list;
  iface->track_open = hyscan_db_client_track_open;
  iface->track_create = hyscan_db_client_track_create;
  iface->track_remove = hyscan_db_client_track_remove;
  iface->track_get_ctime = hyscan_db_client_track_get_ctime;
  iface->track_param_open = hyscan_db_client_track_param_open;

  iface->channel_list = hyscan_db_client_channel_list;
  iface->channel_open = hyscan_db_client_channel_open;
  iface->channel_create = hyscan_db_client_channel_create;
  iface->channel_remove = hyscan_db_client_channel_remove;
  iface->channel_finalize = hyscan_db_client_channel_finalize;
  iface->channel_is_writable = hyscan_db_client_channel_is_writable;
  iface->channel_param_open = hyscan_db_client_channel_param_open;

  iface->channel_set_chunk_size = hyscan_db_client_channel_set_chunk_size;
  iface->channel_set_save_time = hyscan_db_client_channel_set_save_time;
  iface->channel_set_save_size = hyscan_db_client_channel_set_save_size;

  iface->channel_get_data_range = hyscan_db_client_channel_get_data_range;
  iface->channel_add_data = hyscan_db_client_channel_add_data;
  iface->channel_get_data = hyscan_db_client_channel_get_data;
  iface->channel_find_data = hyscan_db_client_channel_find_data;

  iface->param_object_list = hyscan_db_client_param_object_list;
  iface->param_object_create = hyscan_db_client_param_object_create;
  iface->param_object_remove = hyscan_db_client_param_object_remove;
  iface->param_object_get_schema = hyscan_db_client_param_object_get_schema;

  iface->param_set = hyscan_db_client_param_set;
  iface->param_get = hyscan_db_client_param_get;

  iface->close = hyscan_db_client_close;
}
