/**
 * \file hyscan-db-server.c
 *
 * \brief Исходный файл RPC сервера базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-server.h"
#include "hyscan-db-rpc.h"

#include <urpc-server.h>

#define hyscan_db_server_acl_error()       do { \
                                             g_warning ("%s: access denied", __FUNCTION__); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_server_get_error(p)      do { \
                                             g_warning ("%s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_server_set_error(p)      do { \
                                             g_warning ("%s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI,
  PROP_N_THREADS,
  PROP_N_CLIENTS,
  PROP_DB,
  PROP_ACL_FN
};

struct _HyScanDBServer
{
  GObject              parent_instance;

  volatile gint        running;

  hyscan_db_server_acl acl;

  uRpcServer          *rpc;
  gchar               *uri;
  HyScanDB            *db;

  guint                n_threads;
  guint                n_clients;
};

static void    hyscan_db_server_set_property                   (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_db_server_object_finalize                (GObject               *object);

G_DEFINE_TYPE (HyScanDBServer, hyscan_db_server, G_TYPE_OBJECT);

static void hyscan_db_server_class_init( HyScanDBServerClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_db_server_set_property;
  object_class->finalize = hyscan_db_server_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
                                   g_param_spec_object ("db", "DB", "HyScan db", HYSCAN_TYPE_DB,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "HyScan db uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_THREADS,
                                   g_param_spec_uint ("n-threads", "Number of threads", "Number of threads",
                                                      1, URPC_MAX_THREADS_NUM, 1,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_CLIENTS,
                                   g_param_spec_uint ("n-clients", "Number of clients", "Maximum number of clients",
                                                      1, 1000, 1000,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_ACL_FN,
                                   g_param_spec_pointer ("acl-fn", "Acl function", "Access control function",
                                                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_server_init (HyScanDBServer *db_server)
{
}

static void
hyscan_db_server_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanDBServer *dbs = HYSCAN_DB_SERVER (object);

  switch (prop_id)
    {
    case PROP_DB:
      dbs->db = g_value_get_object (value);
      if (HYSCAN_IS_DB (dbs->db))
        g_object_ref (dbs->db);
      else
        dbs->db = NULL;
      break;

    case PROP_URI:
      dbs->uri = g_value_dup_string (value);
      break;

    case PROP_N_THREADS:
      dbs->n_threads = g_value_get_uint (value);
      break;

    case PROP_N_CLIENTS:
      dbs->n_clients = g_value_get_uint (value);
      break;

    case PROP_ACL_FN:
      dbs->acl = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_server_object_finalize (GObject *object)
{
  HyScanDBServer *dbs = HYSCAN_DB_SERVER (object);

  if (dbs->rpc != NULL)
    urpc_server_destroy (dbs->rpc);

  if (dbs->db != NULL)
    g_object_unref (dbs->db);

  g_free (dbs->uri);

  G_OBJECT_CLASS (hyscan_db_server_parent_class)->finalize (object);
}

/* RPC функция HYSCAN_DB_RPC_PROC_VERSION. */
static gint
hyscan_db_server_rpc_proc_version (guint32   session,
                                   uRpcData *urpc_data,
                                   void     *proc_data,
                                   void     *key_data)
{
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_VERSION, HYSCAN_DB_RPC_VERSION);

  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_PROJECT_TYPE_LIST. */
static gint
hyscan_db_server_rpc_proc_get_project_type_list (guint32   session,
                                                 uRpcData *urpc_data,
                                                 void     *proc_data,
                                                 void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **type_list = NULL;

  if (dbs->acl != NULL && !dbs->acl ("get_project_type_list", key_data))
    hyscan_db_server_acl_error ();

  type_list = hyscan_db_get_project_type_list (dbs->db);
  if (type_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_TYPE, type_list) != 0)
        hyscan_db_server_set_error ("project_type");
    }

   rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (type_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_URI. */
static gint
hyscan_db_server_rpc_proc_get_uri (guint32   session,
                                   uRpcData *urpc_data,
                                   void     *proc_data,
                                   void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar *uri = NULL;

  if (dbs->acl != NULL && !dbs->acl ("get_uri", key_data))
    hyscan_db_server_acl_error ();

  uri = hyscan_db_get_uri (dbs->db);
  if (uri == NULL)
    goto exit;

  if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_URI, uri) != 0)
    hyscan_db_server_set_error ("uri");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_free (uri);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_PROJECT_LIST. */
static gint
hyscan_db_server_rpc_proc_get_project_list (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **project_list = NULL;

  if (dbs->acl != NULL && !dbs->acl ("get_project_list", key_data))
    hyscan_db_server_acl_error ();

  project_list = hyscan_db_get_project_list (dbs->db);
  if (project_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_LIST, project_list) != 0)
        hyscan_db_server_set_error ("project_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (project_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_PROJECT. */
static gint
hyscan_db_server_rpc_proc_open_project (guint32   session,
                                        uRpcData *urpc_data,
                                        void     *proc_data,
                                        void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("open_project", key_data))
    hyscan_db_server_acl_error ();

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  project_id = hyscan_db_open_project (dbs->db, project_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_server_set_error ("project_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CREATE_PROJECT. */
static gint
hyscan_db_server_rpc_proc_create_project (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;
  const gchar *project_type;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("create_project", key_data))
    hyscan_db_server_acl_error ();

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  project_type = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_TYPE, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  project_id = hyscan_db_create_project (dbs->db, project_name, project_type);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_server_set_error ("project_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_PROJECT. */
static gint
hyscan_db_server_rpc_proc_remove_project (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;

  if (dbs->acl != NULL && !dbs->acl ("remove_project", key_data))
    hyscan_db_server_acl_error ();

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  if (hyscan_db_remove_project (dbs->db, project_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CLOSE_PROJECT. */
static gint
hyscan_db_server_rpc_proc_close_project (guint32   session,
                                         uRpcData *urpc_data,
                                         void     *proc_data,
                                         void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("close_project", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  hyscan_db_close_project (dbs->db, project_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_PROJECT_CTIME. */
static gint
hyscan_db_server_rpc_proc_get_project_ctime (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 project_id;
  GDateTime *ctime;
  gint64 itime;

  if (dbs->acl != NULL && !dbs->acl ("get_project_ctime", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  ctime = hyscan_db_get_project_ctime (dbs->db, project_id);
  if (ctime == NULL)
    goto exit;

  itime = g_date_time_to_unix (ctime);
  g_date_time_unref (ctime);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_CTIME, itime) != 0)
    hyscan_db_server_set_error ("ctime");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_TRACK_LIST. */
static gint
hyscan_db_server_rpc_proc_get_track_list (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **track_list = NULL;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("get_track_list", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_list = hyscan_db_get_track_list (dbs->db, project_id);
  if (track_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_LIST, track_list) != 0)
        hyscan_db_server_set_error ("track_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (track_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_TRACK. */
static gint
hyscan_db_server_rpc_proc_open_track (guint32   session,
                                      uRpcData *urpc_data,
                                      void     *proc_data,
                                      void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  gint32 project_id;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("open_track", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  track_id = hyscan_db_open_track (dbs->db, project_id, track_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_server_set_error ("track_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CREATE_TRACK. */
static gint
hyscan_db_server_rpc_proc_create_track (guint32   session,
                                        uRpcData *urpc_data,
                                        void     *proc_data,
                                        void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  gint32 project_id;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("create_track", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  track_id = hyscan_db_create_track (dbs->db, project_id, track_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_server_set_error ("track_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_TRACK. */
static gint
hyscan_db_server_rpc_proc_remove_track (guint32   session,
                                        uRpcData *urpc_data,
                                        void     *proc_data,
                                        void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("remove_track", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  if (hyscan_db_remove_track (dbs->db, project_id, track_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CLOSE_TRACK. */
static gint
hyscan_db_server_rpc_proc_close_track (guint32   session,
                                       uRpcData *urpc_data,
                                       void     *proc_data,
                                       void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("close_track", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  hyscan_db_close_track (dbs->db, track_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_TRACK_CTIME. */
static gint
hyscan_db_server_rpc_proc_get_track_ctime (guint32   session,
                                           uRpcData *urpc_data,
                                           void     *proc_data,
                                           void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  GDateTime *ctime;
  gint32 track_id;
  gint64 itime;

  if (dbs->acl != NULL && !dbs->acl ("get_track_ctime", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  ctime = hyscan_db_get_track_ctime (dbs->db, track_id);
  if (ctime == NULL)
    goto exit;

  itime = g_date_time_to_unix (ctime);
  g_date_time_unref (ctime);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_CTIME, itime) != 0)
    hyscan_db_server_set_error ("ctime");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_CHANNEL_LIST. */
static gint
hyscan_db_server_rpc_proc_get_channel_list (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **channel_list = NULL;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("get_channel_list", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_list = hyscan_db_get_channel_list (dbs->db, track_id);
  if (channel_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_LIST, channel_list) != 0)
        hyscan_db_server_set_error ("channel_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (channel_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_CHANNEL. */
static gint
hyscan_db_server_rpc_proc_open_channel (guint32   session,
                                        uRpcData *urpc_data,
                                        void     *proc_data,
                                        void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  gint32 channel_id;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("open_channel", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  channel_id = hyscan_db_open_channel (dbs->db, track_id, channel_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_server_set_error ("channel_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CREATE_CHANNEL. */
static gint
hyscan_db_server_rpc_proc_create_channel (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  gint32 channel_id;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("create_channel", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  channel_id = hyscan_db_create_channel (dbs->db, track_id, channel_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_server_set_error ("channel_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_CHANNEL. */
static gint
hyscan_db_server_rpc_proc_remove_channel (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("remove_channel", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  if (hyscan_db_remove_channel (dbs->db, track_id, channel_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CLOSE_CHANNEL. */
static gint
hyscan_db_server_rpc_proc_close_channel (guint32   session,
                                         uRpcData *urpc_data,
                                         void     *proc_data,
                                         void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;

  if (dbs->acl != NULL && !dbs->acl ("close_channel", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  hyscan_db_close_channel (dbs->db, channel_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_CHANNEL_PARAM. */
static gint
hyscan_db_server_rpc_proc_open_channel_param (guint32   session,
                                              uRpcData *urpc_data,
                                              void     *proc_data,
                                              void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint32 param_id;

  if (dbs->acl != NULL && !dbs->acl ("open_channel_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  param_id = hyscan_db_open_channel_param (dbs->db, channel_id);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_CHANNEL_CHUNK_SIZE. */
static gint
hyscan_db_server_rpc_proc_set_channel_chunk_size (guint32   session,
                                                  uRpcData *urpc_data,
                                                  void     *proc_data,
                                                  void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint32 chunk_size;

  if (dbs->acl != NULL && !dbs->acl ("set_channel_chunk_size", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if( urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHUNK_SIZE, &chunk_size) != 0)
    hyscan_db_server_get_error ("chunk_size");

  if (hyscan_db_set_channel_chunk_size (dbs->db, channel_id, chunk_size))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_TIME. */
static gint
hyscan_db_server_rpc_proc_set_channel_save_time (guint32   session,
                                                 uRpcData *urpc_data,
                                                 void     *proc_data,
                                                 void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint64 save_time;

  if (dbs->acl != NULL && !dbs->acl ("set_channel_save_time", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_TIME, &save_time) != 0)
    hyscan_db_server_get_error ("save_time");

  if (hyscan_db_set_channel_save_time (dbs->db, channel_id, save_time))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_SIZE. */
static gint
hyscan_db_server_rpc_proc_set_channel_save_size (guint32   session,
                                                 uRpcData *urpc_data,
                                                 void     *proc_data,
                                                 void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint64 save_size;

  if (dbs->acl != NULL && !dbs->acl ("set_channel_save_size", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_SIZE, &save_size) != 0)
    hyscan_db_server_get_error ("save_size");

  if (hyscan_db_set_channel_save_size (dbs->db, channel_id, save_size))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_FINALIZE_CHANNEL. */
static gint
hyscan_db_server_rpc_proc_finalize_channel (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;

  if (dbs->acl != NULL && !dbs->acl ("finalize_channel", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  hyscan_db_finalize_channel (dbs->db, channel_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA_RANGE. */
static gint
hyscan_db_server_rpc_proc_get_channel_data_range (guint32   session,
                                                  uRpcData *urpc_data,
                                                  void     *proc_data,
                                                  void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint32 first_index;
  gint32 last_index;

  if (dbs->acl != NULL && !dbs->acl ("get_channel_data_range", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (hyscan_db_get_channel_data_range (dbs->db, channel_id, &first_index, &last_index))
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_LINDEX, first_index) != 0)
        hyscan_db_server_set_error ("first_index");

      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_RINDEX, last_index) != 0)
        hyscan_db_server_set_error ("last_index");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_ADD_CHANNEL_DATA. */
static gint
hyscan_db_server_rpc_proc_add_channel_data (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint64 time;
  gpointer data;
  guint32 size;
  gint32 index;

  if (dbs->acl != NULL && !dbs->acl ("add_channel_data", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, &time) != 0)
    hyscan_db_server_get_error ("time");

  data = urpc_data_get (urpc_data, HYSCAN_DB_RPC_PARAM_DATA, &size);
  if (data == NULL)
    hyscan_db_server_get_error ("data");

  if (hyscan_db_add_channel_data (dbs->db, channel_id, time, data, size, &index))
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_INDEX, index) != 0)
        hyscan_db_server_set_error ("index");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA. */
static gint
hyscan_db_server_rpc_proc_get_channel_data (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint32 index;
  gpointer buffer = NULL;
  gint32 buffer_size;
  gint64 time;

  if (dbs->acl != NULL && !dbs->acl ("get_channel_data", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_INDEX, &index) != 0)
    hyscan_db_server_get_error ("index");

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_SIZE, &buffer_size) == 0)
    {
      buffer = urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA, NULL, buffer_size);
      if (buffer == NULL)
        hyscan_db_server_set_error ("data");
    }

  if (hyscan_db_get_channel_data (dbs->db, channel_id, index, buffer, &buffer_size, &time))
    {
      if (buffer != NULL)
        {
          if (urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA, NULL, buffer_size) == NULL )
            hyscan_db_server_set_error ("data-size");
        }
      else
        {
          if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_SIZE, buffer_size) != 0)
            hyscan_db_server_set_error ("buffer_size");
        }

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, time) != 0)
        hyscan_db_server_set_error ("time");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_FIND_CHANNEL_DATA. */
static gint
hyscan_db_server_rpc_proc_find_channel_data (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint64 time;
  gint32 lindex;
  gint32 rindex;
  gint64 ltime;
  gint64 rtime;

  if (dbs->acl != NULL && !dbs->acl ("find_channel_data", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_TIME, &time) != 0)
    hyscan_db_server_get_error ("time");

  if (hyscan_db_find_channel_data (dbs->db, channel_id, time, &lindex, &rindex, &ltime, &rtime))
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_LINDEX, lindex) != 0)
        hyscan_db_server_set_error ("lindex");

      if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_RINDEX, rindex) != 0)
        hyscan_db_server_set_error ("rindex");

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_LTIME, ltime) != 0)
        hyscan_db_server_set_error ("ltime");

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_RTIME, rtime) != 0)
        hyscan_db_server_set_error ("rtime");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_PROJECT_PARAM_LIST. */
static gint
hyscan_db_server_rpc_proc_get_project_param_list (guint32   session,
                                                  uRpcData *urpc_data,
                                                  void     *proc_data,
                                                  void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **param_list = NULL;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("get_project_param_list", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  param_list = hyscan_db_get_project_param_list (dbs->db, project_id);
  if (param_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, param_list) != 0)
        hyscan_db_server_set_error ("param_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (param_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_PROJECT_PARAM. */
static gint
hyscan_db_server_rpc_proc_open_project_param (guint32   session,
                                              uRpcData *urpc_data,
                                              void     *proc_data,
                                              void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *group_name;
  gint32 project_id;
  gint32 param_id;

  if (dbs->acl != NULL && !dbs->acl ("open_project_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  param_id = hyscan_db_open_project_param (dbs->db, project_id, group_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_PROJECT_PARAM. */
static gint
hyscan_db_server_rpc_proc_remove_project_param (guint32   session,
                                                uRpcData *urpc_data,
                                                void     *proc_data,
                                                void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *group_name;
  gint32 project_id;

  if (dbs->acl != NULL && !dbs->acl ("remove_project_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  if (hyscan_db_remove_project_param (dbs->db, project_id, group_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_TRACK_PARAM_LIST. */
static gint
hyscan_db_server_rpc_proc_get_track_param_list (guint32   session,
                                                uRpcData *urpc_data,
                                                void     *proc_data,
                                                void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **param_list = NULL;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("get_track_param_list", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  param_list = hyscan_db_get_track_param_list (dbs->db, track_id);
  if (param_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, param_list) != 0)
        hyscan_db_server_set_error ("param_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (param_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_OPEN_TRACK_PARAM. */
static gint
hyscan_db_server_rpc_proc_open_track_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *group_name;
  gint32 track_id;
  gint32 param_id;

  if (dbs->acl != NULL && !dbs->acl ("open_track_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  param_id = hyscan_db_open_track_param (dbs->db, track_id, group_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_TRACK_PARAM. */
static gint
hyscan_db_server_rpc_proc_remove_track_param (guint32   session,
                                              uRpcData *urpc_data,
                                              void     *proc_data,
                                              void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *group_name;
  gint32 track_id;

  if (dbs->acl != NULL && !dbs->acl ("remove_track_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  if (hyscan_db_remove_track_param (dbs->db, track_id, group_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_PARAM_LIST. */
static gint
hyscan_db_server_rpc_proc_get_param_list (guint32   session,
                                          uRpcData *urpc_data,
                                          void     *proc_data,
                                          void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **param_list = NULL;
  gint32 param_id;

  if (dbs->acl != NULL && !dbs->acl ("get_param_list", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  param_list = hyscan_db_get_param_list (dbs->db, param_id);
  if (param_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_LIST, param_list) != 0)
        hyscan_db_server_set_error ("param_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (param_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_COPY_PARAM. */
static gint
hyscan_db_server_rpc_proc_copy_param (guint32   session,
                                      uRpcData *urpc_data,
                                      void     *proc_data,
                                      void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 src_param_id;
  gint32 dst_param_id;
  const gchar *mask;

  if (dbs->acl != NULL && !dbs->acl ("copy_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &src_param_id) != 0)
    hyscan_db_server_get_error ("src_param_id");

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_DST_ID, &dst_param_id) != 0)
    hyscan_db_server_get_error ("dst_param_id");

  mask = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_MASK, 0);
  if (mask == NULL)
    hyscan_db_server_get_error ("mask");

  if (hyscan_db_copy_param (dbs->db, src_param_id, dst_param_id, mask))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_REMOVE_PARAM. */
static gint
hyscan_db_server_rpc_proc_remove_param (guint32   session,
                                        uRpcData *urpc_data,
                                        void     *proc_data,
                                        void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 param_id;
  const gchar *mask;

  if (dbs->acl != NULL && !dbs->acl ("remove_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  mask = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_MASK, 0);
  if (mask == NULL)
    hyscan_db_server_get_error ("mask");

  if (hyscan_db_remove_param (dbs->db, param_id, mask))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_CLOSE_PARAM. */
static gint
hyscan_db_server_rpc_proc_close_param (guint32   session,
                                       uRpcData *urpc_data,
                                       void     *proc_data,
                                       void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  gint32 param_id;

  if (dbs->acl != NULL && !dbs->acl ("close_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  hyscan_db_close_param (dbs->db, param_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, HYSCAN_DB_RPC_STATUS_OK);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_HAS_PARAM. */
static gint
hyscan_db_server_rpc_proc_has_param (guint32   session,
                                     uRpcData *urpc_data,
                                     void     *proc_data,
                                     void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  gint32 param_id;
  const gchar *name;

  if (dbs->acl != NULL && !dbs->acl ("has_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  if (hyscan_db_has_param (dbs->db, param_id, name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_INC_INTEGER_PARAM. */
static gint
hyscan_db_server_rpc_proc_inc_integer_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *name;
  gint32 param_id;
  gint64 value;

  if (dbs->acl != NULL && !dbs->acl ("inc_integer_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = hyscan_db_inc_integer_param (dbs->db, param_id, name);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_server_set_error ("value");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_INTEGER_PARAM. */
static gint
hyscan_db_server_rpc_proc_set_integer_param (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  gint32 param_id;
  const gchar *name;
  gint64 value;

  if (dbs->acl != NULL && !dbs->acl ("set_integer_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    hyscan_db_server_get_error ("value");

  if (hyscan_db_set_integer_param (dbs->db, param_id, name, value))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_DOUBLE_PARAM. */
static gint
hyscan_db_server_rpc_proc_set_double_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  gint32 param_id;
  const gchar *name;
  gdouble value;

  if (dbs->acl != NULL && !dbs->acl ("set_double_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  if (urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    hyscan_db_server_get_error ("value");

  if (hyscan_db_set_double_param (dbs->db, param_id, name, value))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_BOOLEAN_PARAM. */
static gint
hyscan_db_server_rpc_proc_set_boolean_param (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  gint32 param_id;
  const gchar *name;
  gboolean value;

  if (dbs->acl != NULL && !dbs->acl ("set_boolean_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, &value) != 0)
    hyscan_db_server_get_error ("value");

  if (hyscan_db_set_boolean_param (dbs->db, param_id, name, value))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_SET_STRING_PARAM. */
static gint
hyscan_db_server_rpc_proc_set_string_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  gint32 param_id;
  const gchar *name;
  const gchar *value;

  if (dbs->acl != NULL && !dbs->acl ("set_string_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, 0);
  if( value == NULL)
    hyscan_db_server_get_error ("value");

  if (hyscan_db_set_string_param (dbs->db, param_id, name, value))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_INTEGER_PARAM. */
static gint
hyscan_db_server_rpc_proc_get_integer_param (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *name;
  gint32 param_id;
  gint64 value;

  if (dbs->acl != NULL && !dbs->acl ("get_integer_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = hyscan_db_get_integer_param (dbs->db, param_id, name);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_server_set_error ("value");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_DOUBLE_PARAM. */
static gint
hyscan_db_server_rpc_proc_get_double_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *name;
  gint32 param_id;
  gdouble value;

  if (dbs->acl != NULL && !dbs->acl ("get_double_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = hyscan_db_get_double_param (dbs->db, param_id, name);
  if (urpc_data_set_double (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_server_set_error ("value");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC Функция HYSCAN_DB_RPC_PROC_GET_BOOLEAN_PARAM. */
static gint
hyscan_db_server_rpc_proc_get_boolean_param (guint32   session,
                                             uRpcData *urpc_data,
                                             void     *proc_data,
                                             void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *name;
  gint32 param_id;
  gboolean value;

  if (dbs->acl != NULL && !dbs->acl ("get_boolean_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = hyscan_db_get_boolean_param (dbs->db, param_id, name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value ? 1 : 0) != 0)
    hyscan_db_server_set_error ("value");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}


/* RPC Функция HYSCAN_DB_RPC_PROC_GET_STRING_PARAM. */
static gint
hyscan_db_server_rpc_proc_get_string_param (guint32   session,
                                            uRpcData *urpc_data,
                                            void     *proc_data,
                                            void     *key_data)
{
  HyScanDBServer *dbs = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;
  const gchar *name;
  gint32 param_id;
  const gchar *value;

  if (dbs->acl != NULL && !dbs->acl ("get_string_param", key_data))
    hyscan_db_server_acl_error ();

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME, 0);
  if (name == NULL)
    hyscan_db_server_get_error ("name");

  value = hyscan_db_get_string_param (dbs->db, param_id, name);
  if (value != NULL && urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_VALUE, value) != 0)
    hyscan_db_server_set_error ("value");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

HyScanDBServer *
hyscan_db_server_new (const gchar          *uri,
                      HyScanDB             *db,
                      guint                 n_threads,
                      guint                 n_clients,
                      hyscan_db_server_acl  acl_fn)
{
  return g_object_new (HYSCAN_TYPE_DB_SERVER, "uri", uri, "db", db,
                                              "n-threads", n_threads, "n-clients", n_clients,
                                              "acl-fn", acl_fn, NULL);
}

gboolean
hyscan_db_server_start (HyScanDBServer *server)
{
  uRpcType rpc_type;
  gint status;

  /* Проверяем что сервер ещё не запущен. */
  if (g_atomic_int_get (&server->running))
    return FALSE;

  /* Проверяем адрес сервера. */
  if (server->uri == NULL)
    return FALSE;

  /* Проверяем тип RPC протокола. */
  rpc_type = urpc_get_type (server->uri);
  if (rpc_type != URPC_TCP && rpc_type != URPC_SHM)
    return FALSE;

  /* Проверяем базу данных HyScan. */
  if (server->db == NULL)
    return FALSE;

  /* Создаём RPC сервер. */
  server->rpc = urpc_server_create (server->uri, server->n_threads, server->n_clients,
                                    URPC_DEFAULT_SESSION_TIMEOUT, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (server->rpc == NULL)
    return FALSE;

  /* RPC функции. */
  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_VERSION,
                                 hyscan_db_server_rpc_proc_version, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_TYPE_LIST,
                                 hyscan_db_server_rpc_proc_get_project_type_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_URI,
                                 hyscan_db_server_rpc_proc_get_uri, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_LIST,
                                 hyscan_db_server_rpc_proc_get_project_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_PROJECT,
                                 hyscan_db_server_rpc_proc_open_project, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CREATE_PROJECT,
                                 hyscan_db_server_rpc_proc_create_project, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PROJECT,
                                 hyscan_db_server_rpc_proc_remove_project, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CLOSE_PROJECT,
                                 hyscan_db_server_rpc_proc_close_project, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_CTIME,
                                 hyscan_db_server_rpc_proc_get_project_ctime, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_LIST,
                                 hyscan_db_server_rpc_proc_get_track_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_TRACK,
                                 hyscan_db_server_rpc_proc_open_track, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CREATE_TRACK,
                                 hyscan_db_server_rpc_proc_create_track, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_TRACK,
                                 hyscan_db_server_rpc_proc_remove_track, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CLOSE_TRACK,
                                 hyscan_db_server_rpc_proc_close_track, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_CTIME,
                                 hyscan_db_server_rpc_proc_get_track_ctime, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_LIST,
                                 hyscan_db_server_rpc_proc_get_channel_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_CHANNEL,
                                 hyscan_db_server_rpc_proc_open_channel, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CREATE_CHANNEL,
                                 hyscan_db_server_rpc_proc_create_channel, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_CHANNEL,
                                 hyscan_db_server_rpc_proc_remove_channel, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CLOSE_CHANNEL,
                                 hyscan_db_server_rpc_proc_close_channel, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_CHANNEL_PARAM,
                                 hyscan_db_server_rpc_proc_open_channel_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_CHUNK_SIZE,
                                 hyscan_db_server_rpc_proc_set_channel_chunk_size, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_TIME,
                                 hyscan_db_server_rpc_proc_set_channel_save_time, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_CHANNEL_SAVE_SIZE,
                                 hyscan_db_server_rpc_proc_set_channel_save_size, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_FINALIZE_CHANNEL,
                                 hyscan_db_server_rpc_proc_finalize_channel, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA_RANGE,
                                 hyscan_db_server_rpc_proc_get_channel_data_range, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_ADD_CHANNEL_DATA,
                                 hyscan_db_server_rpc_proc_add_channel_data, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_CHANNEL_DATA,
                                 hyscan_db_server_rpc_proc_get_channel_data, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_FIND_CHANNEL_DATA,
                                 hyscan_db_server_rpc_proc_find_channel_data, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_PROJECT_PARAM_LIST,
                                 hyscan_db_server_rpc_proc_get_project_param_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_PROJECT_PARAM,
                                 hyscan_db_server_rpc_proc_open_project_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PROJECT_PARAM,
                                 hyscan_db_server_rpc_proc_remove_project_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_TRACK_PARAM_LIST,
                                 hyscan_db_server_rpc_proc_get_track_param_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_OPEN_TRACK_PARAM,
                                 hyscan_db_server_rpc_proc_open_track_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_TRACK_PARAM,
                                 hyscan_db_server_rpc_proc_remove_track_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_PARAM_LIST,
                                 hyscan_db_server_rpc_proc_get_param_list, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_COPY_PARAM,
                                 hyscan_db_server_rpc_proc_copy_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_REMOVE_PARAM,
                                 hyscan_db_server_rpc_proc_remove_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_CLOSE_PARAM,
                                 hyscan_db_server_rpc_proc_close_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_HAS_PARAM,
                                 hyscan_db_server_rpc_proc_has_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_INC_INTEGER_PARAM,
                                 hyscan_db_server_rpc_proc_inc_integer_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_INTEGER_PARAM,
                                 hyscan_db_server_rpc_proc_set_integer_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_DOUBLE_PARAM,
                                 hyscan_db_server_rpc_proc_set_double_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_BOOLEAN_PARAM,
                                 hyscan_db_server_rpc_proc_set_boolean_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_SET_STRING_PARAM,
                                 hyscan_db_server_rpc_proc_set_string_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_INTEGER_PARAM,
                                 hyscan_db_server_rpc_proc_get_integer_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_DOUBLE_PARAM,
                                 hyscan_db_server_rpc_proc_get_double_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_BOOLEAN_PARAM,
                                 hyscan_db_server_rpc_proc_get_boolean_param, server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (server->rpc, HYSCAN_DB_RPC_PROC_GET_STRING_PARAM,
                                 hyscan_db_server_rpc_proc_get_string_param, server);
  if (status != 0)
    goto fail;

  /* Запуск RPC сервера. */
  status = urpc_server_bind (server->rpc);
  if (status != 0)
    goto fail;

  g_atomic_int_set (&server->running, 1);
  return TRUE;

fail:
  urpc_server_destroy (server->rpc);
  server->rpc = NULL;
  return FALSE;
}
