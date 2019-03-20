/*
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

#define hyscan_db_server_get_error(p)      do { \
                                             g_warning ("HyScanDBServer: %s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_db_server_set_error(p)      do { \
                                             g_warning ("HyScanDBServer: %s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI,
  PROP_N_THREADS,
  PROP_N_CLIENTS,
  PROP_DB
};

typedef struct
{
  HyScanBuffer        *buffer;                 /* Буфер данных. */
  HyScanParamList     *list;                   /* Буфер параметров. */
} HyScanDBServerThreadPrivate;

struct _HyScanDBServerPrivate
{
  volatile gint        running;                /* Признак запуска сервера. */

  uRpcServer          *rpc;                    /* RPC сервер. */
  gchar               *uri;                    /* Путь к RPC серверу. */
  HyScanDB            *db;                     /* Интерфейс HyScanDB. */

  guint                n_threads;              /* Число рабочих потоков. */
  guint                n_clients;              /* Максимальное число клиентов. */
};

static void    hyscan_db_server_set_property                   (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_db_server_object_finalize                (GObject               *object);

static void *  hyscan_db_server_rpc_thread_start               (gpointer               user_data);

static void    hyscan_db_server_rpc_thread_stop                (gpointer               thread_data,
                                                                gpointer               user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBServer, hyscan_db_server, G_TYPE_OBJECT);

static void hyscan_db_server_class_init( HyScanDBServerClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_db_server_set_property;
  object_class->finalize = hyscan_db_server_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "HyScan db uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DB,
                                   g_param_spec_object ("db", "DB", "HyScan db", HYSCAN_TYPE_DB,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_THREADS,
                                   g_param_spec_uint ("n-threads", "Number of threads", "Number of threads",
                                                      1, URPC_MAX_THREADS_NUM, 1,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_CLIENTS,
                                   g_param_spec_uint ("n-clients", "Number of clients", "Maximum number of clients",
                                                      1, 1000, 1000,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_server_init (HyScanDBServer *server)
{
  server->priv = hyscan_db_server_get_instance_private (server);
}

static void
hyscan_db_server_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanDBServer *dbs = HYSCAN_DB_SERVER (object);
  HyScanDBServerPrivate *priv = dbs->priv;

  switch (prop_id)
    {
    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;

    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_N_THREADS:
      priv->n_threads = g_value_get_uint (value);
      break;

    case PROP_N_CLIENTS:
      priv->n_clients = g_value_get_uint (value);
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
  HyScanDBServerPrivate *priv = dbs->priv;

  if (priv->rpc != NULL)
    urpc_server_destroy (priv->rpc);

  if (priv->db != NULL)
    g_object_unref (priv->db);

  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_db_server_parent_class)->finalize (object);
}

/* Функция инициализирует структуру данных потока исполнения RPC. */
static void *
hyscan_db_server_rpc_thread_start (gpointer user_data)
{
  HyScanDBServerThreadPrivate *thread_priv;

  thread_priv = g_new (HyScanDBServerThreadPrivate, 1);
  thread_priv->buffer = hyscan_buffer_new ();
  thread_priv->list = hyscan_param_list_new ();

  return thread_priv;
}

/* Функция удаляет структуру данных потока исполнения RPC. */
static void
hyscan_db_server_rpc_thread_stop (gpointer thread_data,
                                  gpointer user_data)
{
  HyScanDBServerThreadPrivate *thread_priv = thread_data;

  g_object_unref (thread_priv->buffer);
  g_object_unref (thread_priv->list);

  g_free (thread_data);
}

/* RPC функция HYSCAN_DB_RPC_PROC_VERSION. */
static gint
hyscan_db_server_rpc_proc_version (uRpcData *urpc_data,
                                   void     *thread_data,
                                   void     *session_data,
                                   void     *proc_data)
{
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_VERSION, HYSCAN_DB_RPC_VERSION);

  return 0;
}

static gint
hyscan_db_server_rpc_proc_get_uri (uRpcData *urpc_data,
                                   void     *thread_data,
                                   void     *session_data,
                                   void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar *uri = NULL;

  uri = hyscan_db_get_uri (priv->db);
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

static gint
hyscan_db_server_rpc_proc_get_mod_count (uRpcData *urpc_data,
                                         void     *thread_data,
                                         void     *session_data,
                                         void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 id;
  guint32 mod_count;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_ID, &id) != 0)
    hyscan_db_server_get_error ("id");

  mod_count = hyscan_db_get_mod_count (priv->db, id);
  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_MOD_COUNT, mod_count) != 0)
    hyscan_db_server_set_error ("mod_count");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_is_exist (uRpcData *urpc_data,
                                    void     *thread_data,
                                    void     *session_data,
                                    void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;
  const gchar *track_name;
  const gchar *channel_name;

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);

  if (hyscan_db_is_exist (priv->db, project_name, track_name, channel_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_list (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **project_list = NULL;

  project_list = hyscan_db_project_list (priv->db);
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

static gint
hyscan_db_server_rpc_proc_project_open (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;
  gint32 project_id;

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  project_id = hyscan_db_project_open (priv->db, project_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_server_set_error ("project_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_create (uRpcData *urpc_data,
                                          void     *thread_data,
                                          void     *session_data,
                                          void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;
  const gchar *project_schema;
  gint32 project_id;

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  project_schema = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_SCHEMA, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  project_id = hyscan_db_project_create (priv->db, project_name, project_schema);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, project_id) != 0)
    hyscan_db_server_set_error ("project_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_remove (uRpcData *urpc_data,
                                          void     *thread_data,
                                          void     *session_data,
                                          void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *project_name;

  project_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_NAME, 0);
  if (project_name == NULL)
    hyscan_db_server_get_error ("project_name");

  if (hyscan_db_project_remove (priv->db, project_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_get_ctime (uRpcData *urpc_data,
                                             void     *thread_data,
                                             void     *session_data,
                                             void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 project_id;
  GDateTime *ctime;
  gint64 itime;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  ctime = hyscan_db_project_get_ctime (priv->db, project_id);
  if (ctime == NULL)
    goto exit;

  itime = g_date_time_to_unix (ctime);
  g_date_time_unref (ctime);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_CTIME, itime) != 0)
    hyscan_db_server_set_error ("ctime");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}


static gint
hyscan_db_server_rpc_proc_project_param_list (uRpcData *urpc_data,
                                              void     *thread_data,
                                              void     *session_data,
                                              void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **param_list = NULL;
  gint32 project_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  param_list = hyscan_db_project_param_list (priv->db, project_id);
  if (param_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_LIST, param_list) != 0)
        hyscan_db_server_set_error ("param_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (param_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_param_open (uRpcData *urpc_data,
                                              void     *thread_data,
                                              void     *session_data,
                                              void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *group_name;
  gint32 project_id;
  gint32 param_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  param_id = hyscan_db_project_param_open (priv->db, project_id, group_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_project_param_remove (uRpcData *urpc_data,
                                                void     *thread_data,
                                                void     *session_data,
                                                void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *group_name;
  gint32 project_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  group_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_GROUP_NAME, 0);
  if (group_name == NULL)
    hyscan_db_server_get_error ("group_name");

  if (hyscan_db_project_param_remove (priv->db, project_id, group_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_track_list (uRpcData *urpc_data,
                                      void     *thread_data,
                                      void     *session_data,
                                      void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **track_list = NULL;
  gint32 project_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_list = hyscan_db_track_list (priv->db, project_id);
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

static gint
hyscan_db_server_rpc_proc_track_open (uRpcData *urpc_data,
                                      void     *thread_data,
                                      void     *session_data,
                                      void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  gint32 project_id;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  track_id = hyscan_db_track_open (priv->db, project_id, track_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_server_set_error ("track_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_track_create (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  const gchar *track_schema;
  const gchar *schema_id;
  gint32 project_id;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  track_schema = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_SCHEMA, 0);
  schema_id = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_SCHEMA_ID, 0);

  track_id = hyscan_db_track_create (priv->db, project_id, track_name, track_schema, schema_id);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, track_id) != 0)
    hyscan_db_server_set_error ("track_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_track_remove (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *track_name;
  gint32 project_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PROJECT_ID, &project_id) != 0)
    hyscan_db_server_get_error ("project_id");

  track_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_NAME, 0);
  if (track_name == NULL)
    hyscan_db_server_get_error ("track_name");

  if (hyscan_db_track_remove (priv->db, project_id, track_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_track_get_ctime (uRpcData *urpc_data,
                                           void     *thread_data,
                                           void     *session_data,
                                           void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  GDateTime *ctime;
  gint32 track_id;
  gint64 itime;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  ctime = hyscan_db_track_get_ctime (priv->db, track_id);
  if (ctime == NULL)
    goto exit;

  itime = g_date_time_to_unix (ctime);
  g_date_time_unref (ctime);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_CTIME, itime) != 0)
    hyscan_db_server_set_error ("ctime");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_track_param_open (uRpcData *urpc_data,
                                            void     *thread_data,
                                            void     *session_data,
                                            void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 track_id;
  gint32 param_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  param_id = hyscan_db_track_param_open (priv->db, track_id);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_list (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **channel_list = NULL;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_list = hyscan_db_channel_list (priv->db, track_id);
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

static gint
hyscan_db_server_rpc_proc_channel_open (uRpcData *urpc_data,
                                        void     *thread_data,
                                        void     *session_data,
                                        void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  gint32 channel_id;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  channel_id = hyscan_db_channel_open (priv->db, track_id, channel_name);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_server_set_error ("channel_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_create (uRpcData *urpc_data,
                                          void     *thread_data,
                                          void     *session_data,
                                          void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  const gchar *schema_id;
  gint32 channel_id;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  schema_id = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_SCHEMA_ID, 0);

  channel_id = hyscan_db_channel_create (priv->db, track_id, channel_name, schema_id);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, channel_id) != 0)
    hyscan_db_server_set_error ("channel_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_remove (uRpcData *urpc_data,
                                          void     *thread_data,
                                          void     *session_data,
                                          void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  const gchar *channel_name;
  gint32 track_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_TRACK_ID, &track_id) != 0)
    hyscan_db_server_get_error ("track_id");

  channel_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_NAME, 0);
  if (channel_name == NULL)
    hyscan_db_server_get_error ("channel_name");

  if (hyscan_db_channel_remove (priv->db, track_id, channel_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_get_ctime (uRpcData *urpc_data,
                                             void     *thread_data,
                                             void     *session_data,
                                             void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  GDateTime *ctime;
  gint32 channel_id;
  gint64 itime;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  ctime = hyscan_db_channel_get_ctime (priv->db, channel_id);
  if (ctime == NULL)
    goto exit;

  itime = g_date_time_to_unix (ctime);
  g_date_time_unref (ctime);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_CTIME, itime) != 0)
    hyscan_db_server_set_error ("ctime");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_finalize (uRpcData *urpc_data,
                                            void     *thread_data,
                                            void     *session_data,
                                            void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  hyscan_db_channel_finalize (priv->db, channel_id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_is_writable (uRpcData *urpc_data,
                                               void     *thread_data,
                                               void     *session_data,
                                               void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (hyscan_db_channel_is_writable (priv->db, channel_id))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_param_open (uRpcData *urpc_data,
                                              void     *thread_data,
                                              void     *session_data,
                                              void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint32 param_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  param_id = hyscan_db_channel_param_open (priv->db, channel_id);
  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, param_id) != 0)
    hyscan_db_server_set_error ("param_id");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_set_chunk_size (uRpcData *urpc_data,
                                                  void     *thread_data,
                                                  void     *session_data,
                                                  void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  guint64 chunk_size;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if( urpc_data_get_uint64 (urpc_data, HYSCAN_DB_RPC_PARAM_CHUNK_SIZE, &chunk_size) != 0)
    hyscan_db_server_get_error ("chunk_size");

  if (hyscan_db_channel_set_chunk_size (priv->db, channel_id, chunk_size))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_set_save_time (uRpcData *urpc_data,
                                                 void     *thread_data,
                                                 void     *session_data,
                                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  gint64 save_time;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_TIME, &save_time) != 0)
    hyscan_db_server_get_error ("save_time");

  if (hyscan_db_channel_set_save_time (priv->db, channel_id, save_time))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_set_save_size (uRpcData *urpc_data,
                                                 void     *thread_data,
                                                 void     *session_data,
                                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  guint64 save_size;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_DB_RPC_PARAM_SAVE_SIZE, &save_size) != 0)
    hyscan_db_server_get_error ("save_size");

  if (hyscan_db_channel_set_save_size (priv->db, channel_id, save_size))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_get_data_range (uRpcData *urpc_data,
                                                  void     *thread_data,
                                                  void     *session_data,
                                                  void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  guint32 first_index;
  guint32 last_index;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (hyscan_db_channel_get_data_range (priv->db, channel_id, &first_index, &last_index))
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LINDEX, first_index) != 0)
        hyscan_db_server_set_error ("first_index");

      if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RINDEX, last_index) != 0)
        hyscan_db_server_set_error ("last_index");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_add_data (uRpcData *urpc_data,
                                            void     *thread_data,
                                            void     *session_data,
                                            void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  HyScanBuffer *buffer;
  gpointer data;
  guint32 size;

  gint32 channel_id;
  guint32 index;
  gint64 time;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, &time) != 0)
    hyscan_db_server_get_error ("time");

  data = urpc_data_get (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_DATA, &size);
  if (data == NULL)
    hyscan_db_server_get_error ("data");

  buffer = ((HyScanDBServerThreadPrivate *)thread_data)->buffer;
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, size);

  if (hyscan_db_channel_add_data (priv->db, channel_id, time, buffer, &index))
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, index) != 0)
        hyscan_db_server_set_error ("index");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_get_data (uRpcData *urpc_data,
                                            void     *thread_data,
                                            void     *session_data,
                                            void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  HyScanBuffer *buffer;
  gpointer data;
  guint32 size;

  gint32 channel_id;
  guint32 index;
  gint64 time;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, &index) != 0)
    hyscan_db_server_get_error ("index");

  size = URPC_MAX_DATA_SIZE - 1024;
  data = urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_DATA, NULL, size);
  if (data == NULL)
    hyscan_db_server_set_error ("data");

  buffer = ((HyScanDBServerThreadPrivate *)thread_data)->buffer;
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, size);

  if (hyscan_db_channel_get_data (priv->db, channel_id, index, buffer, &time))
    {
      if (hyscan_buffer_get_data (buffer, &size) != NULL)
        if (urpc_data_set (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_DATA, NULL, size) == NULL )
          hyscan_db_server_set_error ("data-size");

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, time) != 0)
        hyscan_db_server_set_error ("time");

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_get_data_size (uRpcData *urpc_data,
                                                 void     *thread_data,
                                                 void     *session_data,
                                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  guint32 index;
  guint32 size;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, &index) != 0)
    hyscan_db_server_get_error ("index");

  size = hyscan_db_channel_get_data_size (priv->db, channel_id, index);
  if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_SIZE, size) != 0)
    hyscan_db_server_set_error ("size");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_get_data_time (uRpcData *urpc_data,
                                                 void     *thread_data,
                                                 void     *session_data,
                                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 channel_id;
  guint32 index;
  gint64 time;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_INDEX, &index) != 0)
    hyscan_db_server_get_error ("index");

  time = hyscan_db_channel_get_data_time (priv->db, channel_id, index);
  if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, time) != 0)
    hyscan_db_server_set_error ("time");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_channel_find_data (uRpcData *urpc_data,
                                             void     *thread_data,
                                             void     *session_data,
                                             void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 find_status;
  gint32 channel_id;
  gint64 time;
  guint32 lindex;
  guint32 rindex;
  gint64 ltime;
  gint64 rtime;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_CHANNEL_ID, &channel_id) != 0)
    hyscan_db_server_get_error ("channel_id");

  if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_TIME, &time) != 0)
    hyscan_db_server_get_error ("time");

  find_status = hyscan_db_channel_find_data (priv->db, channel_id, time, &lindex, &rindex, &ltime, &rtime);
  if (find_status == HYSCAN_DB_FIND_OK)
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LINDEX, lindex) != 0)
        hyscan_db_server_set_error ("lindex");

      if (urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RINDEX, rindex) != 0)
        hyscan_db_server_set_error ("rindex");

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_LTIME, ltime) != 0)
        hyscan_db_server_set_error ("ltime");

      if (urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_DATA_RTIME, rtime) != 0)
        hyscan_db_server_set_error ("rtime");
    }

  if (urpc_data_set_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_FIND_STATUS, find_status) != 0)
    hyscan_db_server_set_error ("find_status");

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_get_param_object_list (uRpcData *urpc_data,
                                                 void     *thread_data,
                                                 void     *session_data,
                                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gchar **param_list = NULL;
  gint32 param_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  param_list = hyscan_db_param_object_list (priv->db, param_id);
  if (param_list != NULL)
    {
      if (urpc_data_set_strings (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_LIST, param_list) != 0)
        hyscan_db_server_set_error ("param_list");
    }

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  g_strfreev (param_list);
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_param_object_create (uRpcData *urpc_data,
                                               void     *thread_data,
                                               void     *session_data,
                                               void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 param_id;
  const gchar *object_name;
  const gchar *schema_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  object_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, 0);
  if (object_name == NULL)
    hyscan_db_server_get_error ("object_name");

  schema_id = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA_ID, 0);
  if (object_name == NULL)
    hyscan_db_server_get_error ("schema_id");

  if (hyscan_db_param_object_create (priv->db, param_id, object_name, schema_id))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_param_object_remove (uRpcData *urpc_data,
                                               void     *thread_data,
                                               void     *session_data,
                                               void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 param_id;
  const gchar *object_name;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  object_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, 0);
  if (object_name == NULL)
    hyscan_db_server_get_error ("object_name");

  if (hyscan_db_param_object_remove (priv->db, param_id, object_name))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_param_object_get_schema (uRpcData *urpc_data,
                                                   void     *thread_data,
                                                   void     *session_data,
                                                   void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 param_id;
  const gchar *object_name;
  HyScanDataSchema *schema;
  const gchar *schema_data;
  const gchar *schema_id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  object_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, 0);

  rpc_status = HYSCAN_DB_RPC_STATUS_OK;

  schema = hyscan_db_param_object_get_schema (priv->db, param_id, object_name);
  if (schema != NULL)
    {
      schema_data = hyscan_data_schema_get_data (schema);
      schema_id = hyscan_data_schema_get_id (schema);

      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA, schema_data) != 0)
        hyscan_db_server_set_error ("schema_data");

      if (urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_SCHEMA_ID, schema_id) != 0)
        hyscan_db_server_set_error ("schema_id");

      g_object_unref (schema);
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_param_set (uRpcData *urpc_data,
                                     void     *thread_data,
                                     void     *session_data,
                                     void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  HyScanParamList *param_list;
  const gchar *object_name;
  gint32 param_id;

  gint n_params;
  gint i;

  param_list = ((HyScanDBServerThreadPrivate *)thread_data)->list;
  hyscan_param_list_clear (param_list);

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  object_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, 0);

  for (i = 0; i < HYSCAN_DB_RPC_MAX_PARAMS; i++)
    {
      const gchar *param_name;
      guint32 param_type;

      param_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, 0);
      if (param_name == NULL)
        break;

      if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, &param_type) != 0)
        hyscan_db_server_get_error ("param_type");

      switch (param_type)
        {
        case HYSCAN_DB_RPC_TYPE_NULL:
          break;

        case HYSCAN_DB_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;
            if (urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_server_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_INT64:
          {
            gint64 param_value;
            if (urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_server_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;
            if (urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_db_server_get_error ("param_value");
          }
          break;

        case HYSCAN_DB_RPC_TYPE_STRING:
          {
            if (urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, 0) == NULL)
              hyscan_db_server_get_error ("param_value");
          }
          break;

        default:
          goto exit;
        }
    }

  if (i == 0 || i >= HYSCAN_DB_RPC_MAX_PARAMS)
    hyscan_db_server_get_error ("n_params");

  n_params = i;

  for (i = 0; i < n_params; i++)
    {
      const gchar *param_name;
      GVariant *param_value;
      guint32 param_type;

      param_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, 0);
      urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, &param_type);
      param_value = NULL;

      switch (param_type)
        {
        case HYSCAN_DB_RPC_TYPE_NULL:
          break;

        case HYSCAN_DB_RPC_TYPE_BOOLEAN:
          {
            guint32 param_bvalue;
            urpc_data_get_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_bvalue);
            param_value = g_variant_new_boolean (param_bvalue ? TRUE : FALSE);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_INT64:
          {
            gint64 param_ivalue;
            urpc_data_get_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_ivalue);
            param_value = g_variant_new_int64 (param_ivalue);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_DOUBLE:
          {
            gdouble param_dvalue;
            urpc_data_get_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, &param_dvalue);
            param_value = g_variant_new_double (param_dvalue);
          }
          break;

        case HYSCAN_DB_RPC_TYPE_STRING:
          {
            const gchar *param_svalue;
            param_svalue = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, 0);
            param_value = g_variant_new_string (param_svalue);
          }
          break;

        default:
          break;
        }

      hyscan_param_list_set (param_list, param_name, param_value);
    }

  if (hyscan_db_param_set (priv->db, param_id, object_name, param_list))
    rpc_status = HYSCAN_DB_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_param_get (uRpcData *urpc_data,
                                     void     *thread_data,
                                     void     *session_data,
                                     void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  HyScanParamList *param_list;
  const gchar *object_name;
  gint32 param_id;

  gint n_params;
  gint i;

  param_list = ((HyScanDBServerThreadPrivate *)thread_data)->list;
  hyscan_param_list_clear (param_list);

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_ID, &param_id) != 0)
    hyscan_db_server_get_error ("param_id");

  object_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_OBJECT_NAME, 0);

  for (i = 0; i < HYSCAN_DB_RPC_MAX_PARAMS; i++)
    {
      const gchar *param_name;

      param_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, 0);
      if (param_name != NULL)
        hyscan_param_list_add (param_list, param_name);
      else
        break;
    }

  if (i == 0 || i >= HYSCAN_DB_RPC_MAX_PARAMS)
    hyscan_db_server_get_error ("param_name");

  n_params = i;

  if (hyscan_db_param_get (priv->db, param_id, object_name, param_list))
    {
      for (i = 0; i < n_params; i++)
        {
          GVariantClass param_type;
          const gchar *param_name;
          GVariant *param_value;
          gint set_status;

          param_name = urpc_data_get_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_NAME0 + i, 0);
          param_value = hyscan_param_list_get (param_list, param_name);

          if (param_value != NULL)
            {
              param_type = g_variant_classify (param_value);
            }
          else
            {
              set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_NULL);
              if (set_status != 0)
                hyscan_db_server_set_error ("param_type");

              continue;
            }

          switch (param_type)
            {
            case G_VARIANT_CLASS_BOOLEAN:
              {
                gboolean param_bvalue = g_variant_get_boolean (param_value);

                set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_BOOLEAN);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_type");

                set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, param_bvalue ? 1 : 0);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_value");
              }
              break;

            case G_VARIANT_CLASS_INT64:
              {
                gint64 param_ivalue = g_variant_get_int64 (param_value);

                set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_INT64);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_type");

                set_status = urpc_data_set_int64 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, param_ivalue);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_value");
              }
              break;

            case G_VARIANT_CLASS_DOUBLE:
              {
                gdouble param_dvalue = g_variant_get_double (param_value);

                set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_DOUBLE);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_type");

                set_status = urpc_data_set_double (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, param_dvalue);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_value");
              }
              break;

            case G_VARIANT_CLASS_STRING:
              {
                const gchar *param_svalue = g_variant_get_string (param_value, NULL);

                set_status = urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_TYPE0 + i, HYSCAN_DB_RPC_TYPE_STRING);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_type");

                set_status = urpc_data_set_string (urpc_data, HYSCAN_DB_RPC_PARAM_PARAM_VALUE0 + i, param_svalue);
                if (set_status != 0)
                  hyscan_db_server_set_error ("param_value");
              }
              break;

            default:
              break;
            }

          g_clear_pointer (&param_value, g_variant_unref);
        }

      rpc_status = HYSCAN_DB_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

static gint
hyscan_db_server_rpc_proc_close (uRpcData *urpc_data,
                                 void     *thread_data,
                                 void     *session_data,
                                 void     *proc_data)
{
  HyScanDBServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_DB_RPC_STATUS_FAIL;

  gint32 id;

  if (urpc_data_get_int32 (urpc_data, HYSCAN_DB_RPC_PARAM_ID, &id) != 0)
    hyscan_db_server_get_error ("channel_id");

  hyscan_db_close (priv->db, id);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_DB_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

HyScanDBServer *
hyscan_db_server_new (const gchar          *uri,
                      HyScanDB             *db,
                      guint                 n_threads,
                      guint                 n_clients)
{
  return g_object_new (HYSCAN_TYPE_DB_SERVER, "uri", uri, "db", db,
                                              "n-threads", n_threads,
                                              "n-clients", n_clients,
                                              NULL);
}

gboolean
hyscan_db_server_start (HyScanDBServer *server)
{
  HyScanDBServerPrivate *priv;

  uRpcType rpc_type;
  gint status;

  g_return_val_if_fail (HYSCAN_IS_DB_SERVER (server), FALSE);

  priv = server->priv;

  /* Проверяем что сервер ещё не запущен. */
  if (g_atomic_int_get (&priv->running))
    return FALSE;

  /* Проверяем адрес сервера. */
  if (priv->uri == NULL)
    return FALSE;

  /* Проверяем тип RPC протокола. */
  rpc_type = urpc_get_type (priv->uri);
  if (rpc_type != URPC_TCP && rpc_type != URPC_SHM)
    return FALSE;

  /* Проверяем базу данных HyScan. */
  if (priv->db == NULL)
    return FALSE;

  /* Создаём RPC сервер. */
  priv->rpc = urpc_server_create (priv->uri, priv->n_threads, priv->n_clients,
                                  URPC_DEFAULT_SESSION_TIMEOUT,
                                  URPC_MAX_DATA_SIZE,
                                  URPC_DEFAULT_DATA_TIMEOUT);
  if (priv->rpc == NULL)
    return FALSE;

  /* Функции инициализации потоков исполнения. */
  status = urpc_server_add_thread_start_callback (priv->rpc,
                                                  hyscan_db_server_rpc_thread_start,
                                                  NULL);
  if (status != 0)
    goto fail;

  status = urpc_server_add_thread_stop_callback (priv->rpc,
                                                 hyscan_db_server_rpc_thread_stop,
                                                 NULL);
  if (status != 0)
    goto fail;

  /* RPC функции. */
  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_VERSION,
                                     hyscan_db_server_rpc_proc_version, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_GET_URI,
                                     hyscan_db_server_rpc_proc_get_uri, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_GET_MOD_COUNT,
                                     hyscan_db_server_rpc_proc_get_mod_count, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_IS_EXIST,
                                     hyscan_db_server_rpc_proc_is_exist, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_LIST,
                                     hyscan_db_server_rpc_proc_project_list, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_OPEN,
                                     hyscan_db_server_rpc_proc_project_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_CREATE,
                                     hyscan_db_server_rpc_proc_project_create, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_REMOVE,
                                     hyscan_db_server_rpc_proc_project_remove, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_GET_CTIME,
                                     hyscan_db_server_rpc_proc_project_get_ctime, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_LIST,
                                     hyscan_db_server_rpc_proc_project_param_list, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_OPEN,
                                     hyscan_db_server_rpc_proc_project_param_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PROJECT_PARAM_REMOVE,
                                     hyscan_db_server_rpc_proc_project_param_remove, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_LIST,
                                     hyscan_db_server_rpc_proc_track_list, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_OPEN,
                                     hyscan_db_server_rpc_proc_track_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_CREATE,
                                     hyscan_db_server_rpc_proc_track_create, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_REMOVE,
                                     hyscan_db_server_rpc_proc_track_remove, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_GET_CTIME,
                                     hyscan_db_server_rpc_proc_track_get_ctime, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_TRACK_PARAM_OPEN,
                                     hyscan_db_server_rpc_proc_track_param_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_LIST,
                                     hyscan_db_server_rpc_proc_channel_list, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_OPEN,
                                     hyscan_db_server_rpc_proc_channel_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_CREATE,
                                     hyscan_db_server_rpc_proc_channel_create, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_REMOVE,
                                     hyscan_db_server_rpc_proc_channel_remove, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_CTIME,
                                     hyscan_db_server_rpc_proc_channel_get_ctime, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_FINALIZE,
                                     hyscan_db_server_rpc_proc_channel_finalize, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_IS_WRITABLE,
                                     hyscan_db_server_rpc_proc_channel_is_writable, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_PARAM_OPEN,
                                     hyscan_db_server_rpc_proc_channel_param_open, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_CHUNK_SIZE,
                                     hyscan_db_server_rpc_proc_channel_set_chunk_size, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_SAVE_TIME,
                                     hyscan_db_server_rpc_proc_channel_set_save_time, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_SET_SAVE_SIZE,
                                     hyscan_db_server_rpc_proc_channel_set_save_size, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA_RANGE,
                                     hyscan_db_server_rpc_proc_channel_get_data_range, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_ADD_DATA,
                                     hyscan_db_server_rpc_proc_channel_add_data, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA,
                                     hyscan_db_server_rpc_proc_channel_get_data, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA_SIZE,
                                     hyscan_db_server_rpc_proc_channel_get_data_size, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_GET_DATA_TIME,
                                     hyscan_db_server_rpc_proc_channel_get_data_time, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CHANNEL_FIND_DATA,
                                     hyscan_db_server_rpc_proc_channel_find_data, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_LIST,
                                     hyscan_db_server_rpc_proc_get_param_object_list, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_CREATE,
                                     hyscan_db_server_rpc_proc_param_object_create, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_REMOVE,
                                     hyscan_db_server_rpc_proc_param_object_remove, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_OBJECT_GET_SCHEMA,
                                     hyscan_db_server_rpc_proc_param_object_get_schema, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_SET,
                                     hyscan_db_server_rpc_proc_param_set, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_PARAM_GET,
                                     hyscan_db_server_rpc_proc_param_get, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_DB_RPC_PROC_CLOSE,
                                     hyscan_db_server_rpc_proc_close, priv);
  if (status != 0)
    goto fail;

  /* Запуск RPC сервера. */
  status = urpc_server_bind (priv->rpc);
  if (status != 0)
    goto fail;

  g_atomic_int_set (&priv->running, 1);
  return TRUE;

fail:
  urpc_server_destroy (priv->rpc);
  priv->rpc = NULL;
  return FALSE;
}
