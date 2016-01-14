/**
 * \file hyscan-db-client.h
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
                                             g_warning ("%s: can't lock rpc transport to '%s'", __FUNCTION__, dbc->uri); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_get_error(p)      do { \
                                             g_warning ("%s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_set_error(p)      do { \
                                             g_warning ("%s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_client_exec_error()      do { \
                                             g_warning ("%s: can't execute procedure", __FUNCTION__); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI
};

struct _HyScanDBClient
{
  GObject              parent_instance;

  gchar               *uri;
  uRpcClient          *rpc;
};

static void    hyscan_db_client_interface_init         (HyScanDBInterface      *iface);
static void    hyscan_db_client_set_property           (GObject                *object,
                                                        guint                   prop_id,
                                                        const GValue           *value,
                                                        GParamSpec             *pspec);
static void    hyscan_db_client_object_constructed     (GObject                *object);
static void    hyscan_db_client_object_finalize        (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanDBClient, hyscan_db_client, G_TYPE_OBJECT,
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
hyscan_db_client_init (HyScanDBClient *db_client)
{
}

static void
hyscan_db_client_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (object);

  switch (prop_id)
    {
    case PROP_URI:
      dbc->uri = g_value_dup_string (value);
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
  uRpcData *urpc_data;
  guint32 version;

  dbc->rpc = urpc_client_create (dbc->uri, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (dbc->rpc == NULL)
    return;

  if (urpc_client_connect (dbc->rpc) != 0)
    {
      g_warning ("hyscan_db_client: can't connect to '%s'", dbc->uri);
      goto fail;
    }

  /* Проверка версии сервера базы данных. */
  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    {
      g_warning ("hyscan_db_client: can't lock rpc transport to '%s'", dbc->uri);
      goto fail;
    }

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_VERSION) != URPC_STATUS_OK)
    {
      urpc_client_unlock (dbc->rpc);
      g_warning ("hyscan_db_client: can't execute procedure");
      goto fail;
    }

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_VERSION, &version) != 0 ||
      version != HYSCAN_DB_RPC_VERSION)
    {
      g_warning ("hyscan_db_client: server version mismatch: need %d, got: %d",
                 HYSCAN_DB_RPC_VERSION, version);
      urpc_client_unlock (dbc->rpc);
      goto fail;
    }

  urpc_client_unlock (dbc->rpc);
  return;

fail:
  urpc_client_destroy (dbc->rpc);
  dbc->rpc = NULL;
}

static void
hyscan_db_client_object_finalize (GObject *object)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (object);

  if (dbc->rpc != NULL)
    urpc_client_destroy (dbc->rpc);

  g_free (dbc->uri);

  G_OBJECT_CLASS (hyscan_db_client_parent_class)->finalize (object);
}

/* Все функции этого класса реализованы одинаково:
    - блокируется канал передачи данных RPC;
    - устанавливаются значения параметров вызываемой функции;
    - производится вызов RPC функции;
    - считывается результат вызова функции;
    - освобождается канал передачи. */

static gchar **
hyscan_db_client_get_project_type_list (HyScanDB *db)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **type_list = NULL;
  guint32 n_type_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_TYPE_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_type_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_TYPE);
  if (n_type_list == 0)
    goto exit;

  type_list = g_malloc0 ((n_type_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_type_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_TYPE, i);
      type_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return type_list;
}

static gchar *
hyscan_db_client_get_uri (HyScanDB *db)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar *uri = NULL;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_URI) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  uri = g_strdup (urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_URI, 0));

exit:
  urpc_client_unlock (dbc->rpc);
  return uri;
}

guint32
hyscan_db_client_get_mod_count (HyScanDB *db,
                                gint32    id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  guint32 mod_count;

  if (dbc->rpc == NULL)
    return 0;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_ID, id) != 0)
    hyscan_db_client_set_error ("id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_MOD_COUNT) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_MOD_COUNT, &mod_count) != 0)
    {
      mod_count = 0;
      hyscan_db_client_get_error ("mod_count");
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return mod_count;
}

static gchar **
hyscan_db_client_get_project_list (HyScanDB *db)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **project_list = NULL;
  guint32 n_project_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_LIST) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return project_list;
}

static gint32
hyscan_db_client_open_project (HyScanDB    *db,
                               const gchar *project_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 project_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_PROJECT) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return project_id;
}

static gint32
hyscan_db_client_create_project (HyScanDB    *db,
                                 const gchar *project_name,
                                 const gchar *project_type)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 project_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (project_type != NULL)
    {
      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_TYPE, project_type) != 0)
        hyscan_db_client_set_error ("project_type");
    }

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CREATE_PROJECT) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return project_id;
}

static gboolean
hyscan_db_client_remove_project (HyScanDB    *db,
                                 const gchar *project_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, project_name) != 0)
    hyscan_db_client_set_error ("project_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PROJECT) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static void
hyscan_db_client_close_project (HyScanDB *db,
                                gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  if (dbc->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CLOSE_PROJECT) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (dbc->rpc);
}

static GDateTime *
hyscan_db_client_get_project_ctime (HyScanDB *db,
                                    gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  GDateTime *ctime = NULL;
  gint64 itime;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_CTIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_CTIME, &itime) != 0)
    hyscan_db_client_get_error ("itime");

  ctime = g_date_time_new_from_unix_utc (itime);

exit:
  urpc_client_unlock (dbc->rpc);
  return ctime;
}

static gchar **
hyscan_db_client_get_track_list (HyScanDB *db,
                                 gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **track_list = NULL;
  guint32 n_track_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_LIST) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return track_list;
}

static gint32
hyscan_db_client_open_track (HyScanDB    *db,
                             gint32       project_id,
                             const gchar *track_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 track_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_TRACK) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return track_id;
}

static gint32
hyscan_db_client_create_track (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 track_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CREATE_TRACK) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return track_id;
}

static gboolean
hyscan_db_client_remove_track (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, track_name) != 0)
    hyscan_db_client_set_error ("track_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_TRACK) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static void
hyscan_db_client_close_track (HyScanDB *db,
                              gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  if (dbc->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CLOSE_TRACK) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (dbc->rpc);
}

static GDateTime *
hyscan_db_client_get_track_ctime (HyScanDB *db,
                                  gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  GDateTime *ctime = NULL;
  gint64 itime;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_CTIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_CTIME, &itime) != 0)
    hyscan_db_client_get_error ("itime");

  ctime = g_date_time_new_from_unix_local (itime);

exit:
  urpc_client_unlock (dbc->rpc);
  return ctime;
}

static gchar **
hyscan_db_client_get_channel_list (HyScanDB *db,
                                   gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **channel_list = NULL;
  guint32 n_channel_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_LIST) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return channel_list;
}

static gint32
hyscan_db_client_open_channel (HyScanDB    *db,
                               gint32       track_id,
                               const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 channel_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_CHANNEL) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return channel_id;
}

static gint32
hyscan_db_client_create_channel (HyScanDB    *db,
                                 gint32       track_id,
                                 const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 channel_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CREATE_CHANNEL) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return channel_id;
}

static gboolean
hyscan_db_client_remove_channel (HyScanDB    *db,
                                 gint32       track_id,
                                 const gchar *channel_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, channel_name) != 0)
    hyscan_db_client_set_error ("channel_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_CHANNEL) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static void
hyscan_db_client_close_channel (HyScanDB *db,
                                gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  if (dbc->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CLOSE_CHANNEL) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (dbc->rpc);
}

static gint32
hyscan_db_client_open_channel_param (HyScanDB *db,
                                     gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_CHANNEL_PARAM) != URPC_STATUS_OK)
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
  urpc_client_unlock (dbc->rpc);
  return param_id;
}

static gboolean
hyscan_db_client_set_channel_chunk_size (HyScanDB *db,
                                         gint32    channel_id,
                                         gint32    chunk_size)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHUNK_SIZE, chunk_size) != 0)
    hyscan_db_client_set_error ("chunk_size");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_CHUNK_SIZE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_set_channel_save_time (HyScanDB *db,
                                        gint32    channel_id,
                                        gint64    save_time)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_TIME, save_time) != 0)
    hyscan_db_client_set_error ("save_time");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_TIME) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_set_channel_save_size (HyScanDB *db,
                                        gint32    channel_id,
                                        gint64    save_size)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_SIZE, save_size) != 0)
    hyscan_db_client_set_error ("save_size");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_SIZE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static void
hyscan_db_client_finalize_channel (HyScanDB *db,
                                   gint32    channel_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  if (dbc->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_FINALIZE_CHANNEL) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (dbc->rpc);
}

static gboolean
hyscan_db_client_get_channel_data_range (HyScanDB *db,
                                         gint32    channel_id,
                                         gint32   *first_index,
                                         gint32   *last_index)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA_RANGE) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (first_index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_LINDEX, first_index) != 0)
        hyscan_db_client_get_error ("first_index");
    }

  if (last_index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_RINDEX, last_index) != 0)
        hyscan_db_client_get_error ("last_index");
    }

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_add_channel_data (HyScanDB *db,
                                   gint32    channel_id,
                                   gint64    time,
                                   gpointer  data,
                                   gint32    size,
                                   gint32   *index)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, time) != 0)
    hyscan_db_client_set_error ("time");

  if (urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA, data, size) == NULL)
    hyscan_db_client_set_error ("data");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_ADD_CHANNEL_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (index != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_INDEX, index) != 0)
        hyscan_db_client_get_error ("index");
    }

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_get_channel_data (HyScanDB *db,
                                   gint32    channel_id,
                                   gint32    index,
                                   gpointer  buffer,
                                   gint32   *buffer_size,
                                   gint64   *time)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_INDEX, index) != 0)
    hyscan_db_client_set_error ("index");

  if (buffer != NULL)
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_SIZE, *buffer_size) != 0)
        hyscan_db_client_set_error ("size");
    }

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (buffer != NULL)
    {
      guint32 data_size;
      gpointer data;
      data = urpc_data_get (urpc_data, HYSCAN_DB_RPC_PARAM_DATA, &data_size);
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
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_SIZE, buffer_size) != 0)
        hyscan_db_client_get_error ("buffer_size");
    }

  if (time != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, time) != 0)
        hyscan_db_client_get_error ("time");
    }

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_find_channel_data (HyScanDB *db,
                                    gint32    channel_id,
                                    gint64    time,
                                    gint32   *lindex,
                                    gint32   *rindex,
                                    gint64   *ltime,
                                    gint64   *rtime)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_client_set_error ("channel_id");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, time) != 0)
    hyscan_db_client_set_error ("time");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_FIND_CHANNEL_DATA) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (lindex != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_LINDEX, lindex) != 0)
        hyscan_db_client_get_error ("lindex");
    }

  if (rindex != NULL)
    {
      if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_RINDEX, rindex) != 0)
        hyscan_db_client_get_error ("rindex");
    }

  if (ltime != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_LTIME, ltime) != 0)
        hyscan_db_client_get_error ("ltime");
    }

  if (rtime != NULL)
    {
      if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_RTIME, rtime) != 0)
        hyscan_db_client_get_error ("rtime");
    }

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gchar **
hyscan_db_client_get_project_param_list (HyScanDB *db,
                                         gint32    project_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **param_list = NULL;
  guint32 n_param_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_PARAM_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_param_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST);
  if (n_param_list == 0)
    goto exit;

  param_list = g_malloc0 ((n_param_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_param_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, i);
      param_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return param_list;
}

static gint32
hyscan_db_client_open_project_param (HyScanDB    *db,
                                     gint32       project_id,
                                     const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_PROJECT_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_client_get_error ("param_id");

exit:
  urpc_client_unlock (dbc->rpc);
  return param_id;
}

static gboolean
hyscan_db_client_remove_project_param (HyScanDB    *db,
                                       gint32       project_id,
                                       const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_client_set_error ("project_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PROJECT_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gchar **
hyscan_db_client_get_track_param_list (HyScanDB *db,
                                       gint32    track_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **param_list = NULL;
  guint32 n_param_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_PARAM_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_param_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST);
  if (n_param_list == 0)
    goto exit;

  param_list = g_malloc0 ((n_param_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_param_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, i);
      param_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return param_list;
}

static gint32
hyscan_db_client_open_track_param (HyScanDB    *db,
                                   gint32       track_id,
                                   const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint32 param_id;

  if (dbc->rpc == NULL)
    return -1;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_OPEN_TRACK_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_client_get_error ("param_id");

exit:
  urpc_client_unlock (dbc->rpc);
  return param_id;
}

static gboolean
hyscan_db_client_remove_track_param (HyScanDB    *db,
                                     gint32       track_id,
                                     const gchar *group_name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_client_set_error ("track_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, group_name) != 0)
    hyscan_db_client_set_error ("group_name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_TRACK_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gchar **
hyscan_db_client_get_param_list (HyScanDB *db,
                                 gint32    param_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar **param_list = NULL;
  guint32 n_param_list;
  guint32 i;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_PARAM_LIST) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  n_param_list = urpc_data_get_strings_length (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST);
  if (n_param_list == 0)
    goto exit;

  param_list = g_malloc0 ((n_param_list + 1) * sizeof (gchar*));
  for (i = 0; i < n_param_list; i++)
    {
      const gchar *type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, i);
      param_list[i] = g_strdup (type);
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return param_list;
}

static gboolean
hyscan_db_client_copy_param (HyScanDB    *db,
                             gint32       src_param_id,
                             gint32       dst_param_id,
                             const gchar *mask)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, src_param_id) != 0)
    hyscan_db_client_set_error ("src_param_id");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_DST_ID, dst_param_id) != 0)
    hyscan_db_client_set_error ("dst_param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_MASK, mask) != 0)
    hyscan_db_client_set_error ("mask");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_COPY_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_remove_param (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *mask)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_MASK, mask) != 0)
    hyscan_db_client_set_error ("mask");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static void
hyscan_db_client_close_param (HyScanDB *db,
                              gint32    param_id)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  if (dbc->rpc == NULL)
    return;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_CLOSE_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");

exit:
  urpc_client_unlock (dbc->rpc);
}

static gboolean
hyscan_db_client_has_param (HyScanDB    *db,
                            gint32       param_id,
                            const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_HAS_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}


static gint64
hyscan_db_client_inc_integer_param (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint64 value;

  if (dbc->rpc == NULL)
    return 0;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_INC_INTEGER_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    {
      value = 0;
      hyscan_db_client_get_error ("value");
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return value;
}

static gboolean
hyscan_db_client_set_integer_param (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *name,
                                    gint64       value)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_client_set_error ("value");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_INTEGER_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_set_double_param (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *name,
                                   gdouble      value)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_data_set_double (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_client_set_error ("value");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_DOUBLE_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_set_boolean_param (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *name,
                                    gboolean     value)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value ? 1 : 0) != 0)
    hyscan_db_client_set_error ("value");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_BOOLEAN_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gboolean
hyscan_db_client_set_string_param (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *name,
                                   const gchar *value)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_client_set_error ("value");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_SET_STRING_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (dbc->rpc);
  return status;
}

static gint64
hyscan_db_client_get_integer_param (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gint64 value;

  if (dbc->rpc == NULL)
    return 0;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_INTEGER_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    {
      value = 0;
      hyscan_db_client_get_error ("value");
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return value;
}

static gdouble
hyscan_db_client_get_double_param (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gdouble value;

  if (dbc->rpc == NULL)
    return 0.0;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_DOUBLE_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    {
      value = 0.0;
      hyscan_db_client_get_error ("value");
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return value;
}

static gboolean
hyscan_db_client_get_boolean_param (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean value;

  if (dbc->rpc == NULL)
    return FALSE;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_BOOLEAN_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    {
      value = FALSE;
      hyscan_db_client_get_error ("value");
    }

exit:
  urpc_client_unlock (dbc->rpc);
  return value;
}

static gchar *
hyscan_db_client_get_string_param (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *name)
{
  HyScanDBClient *dbc = HYSCAN_DB_CLIENT (db);
  uRpcData *urpc_data;
  guint32 exec_status;

  gchar *value = NULL;

  if (dbc->rpc == NULL)
    return NULL;

  urpc_data = urpc_client_lock (dbc->rpc);
  if (urpc_data == NULL)
    hyscan_db_client_lock_error ();

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_client_set_error ("param_id");

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, name) != 0)
    hyscan_db_client_set_error ("name");

  if (urpc_client_exec (dbc->rpc, HYSCAN_DB_RPC_PROC_GET_STRING_PARAM) != URPC_STATUS_OK)
    hyscan_db_client_exec_error ();

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_db_client_get_error ("exec_status");
  if (exec_status != HYSCAN_DB_RPC_STATUS_OK)
    goto exit;

  value = g_strdup (urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, 0));

exit:
  urpc_client_unlock (dbc->rpc);
  return value;
}

HyScanDBClient *
hyscan_db_client_new (const gchar *uri)
{
  return g_object_new (HYSCAN_TYPE_DB_CLIENT, "uri", uri, NULL);
}

static void
hyscan_db_client_interface_init (HyScanDBInterface *iface)
{
  iface->get_project_type_list = hyscan_db_client_get_project_type_list;
  iface->get_uri = hyscan_db_client_get_uri;
  iface->get_mod_count = hyscan_db_client_get_mod_count;
  iface->get_project_list = hyscan_db_client_get_project_list;
  iface->open_project = hyscan_db_client_open_project;
  iface->create_project = hyscan_db_client_create_project;
  iface->remove_project = hyscan_db_client_remove_project;
  iface->close_project = hyscan_db_client_close_project;
  iface->get_project_ctime = hyscan_db_client_get_project_ctime;

  iface->get_track_list = hyscan_db_client_get_track_list;
  iface->open_track = hyscan_db_client_open_track;
  iface->create_track = hyscan_db_client_create_track;
  iface->remove_track = hyscan_db_client_remove_track;
  iface->close_track = hyscan_db_client_close_track;
  iface->get_track_ctime = hyscan_db_client_get_track_ctime;

  iface->get_channel_list = hyscan_db_client_get_channel_list;
  iface->open_channel = hyscan_db_client_open_channel;
  iface->create_channel = hyscan_db_client_create_channel;
  iface->remove_channel = hyscan_db_client_remove_channel;
  iface->close_channel = hyscan_db_client_close_channel;
  iface->open_channel_param = hyscan_db_client_open_channel_param;

  iface->set_channel_chunk_size = hyscan_db_client_set_channel_chunk_size;
  iface->set_channel_save_time = hyscan_db_client_set_channel_save_time;
  iface->set_channel_save_size = hyscan_db_client_set_channel_save_size;
  iface->finalize_channel = hyscan_db_client_finalize_channel;

  iface->get_channel_data_range = hyscan_db_client_get_channel_data_range;
  iface->add_channel_data = hyscan_db_client_add_channel_data;
  iface->get_channel_data = hyscan_db_client_get_channel_data;
  iface->find_channel_data = hyscan_db_client_find_channel_data;

  iface->get_project_param_list = hyscan_db_client_get_project_param_list;
  iface->open_project_param = hyscan_db_client_open_project_param;
  iface->remove_project_param = hyscan_db_client_remove_project_param;

  iface->get_track_param_list = hyscan_db_client_get_track_param_list;
  iface->open_track_param = hyscan_db_client_open_track_param;
  iface->remove_track_param = hyscan_db_client_remove_track_param;

  iface->get_param_list = hyscan_db_client_get_param_list;
  iface->copy_param = hyscan_db_client_copy_param;
  iface->remove_param = hyscan_db_client_remove_param;
  iface->close_param = hyscan_db_client_close_param;
  iface->has_param = hyscan_db_client_has_param;

  iface->inc_integer_param = hyscan_db_client_inc_integer_param;
  iface->set_integer_param = hyscan_db_client_set_integer_param;
  iface->set_double_param = hyscan_db_client_set_double_param;
  iface->set_boolean_param = hyscan_db_client_set_boolean_param;
  iface->set_string_param = hyscan_db_client_set_string_param;
  iface->get_integer_param = hyscan_db_client_get_integer_param;
  iface->get_double_param = hyscan_db_client_get_double_param;
  iface->get_boolean_param = hyscan_db_client_get_boolean_param;
  iface->get_string_param = hyscan_db_client_get_string_param;
}
