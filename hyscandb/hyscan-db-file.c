/*
 * \file hyscan-db-file.c
 *
 * \brief Исходный файл класса файловой базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-file.h"
#include "hyscan-db-channel-file.h"
#include "hyscan-db-param-file.h"

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#define PROJECT_ID_FILE        "project.id"            /* Название файла идентификатора проекта. */
#define PROJECT_SCHEMA_FILE    "project.sch"           /* Название файла со схемой данных проекта. */
#define TRACK_ID_FILE          "track.id"              /* Название файла идентификатора галса. */
#define TRACK_SCHEMA_FILE      "track.sch"             /* Название файла со схемой данных. */
#define TRACK_PARAMETERS_FILE  "track.prm"             /* Название файла с параметрами галса. */
#define TRACK_GROUP_ID         "parameters"            /* Название группы параметров галса. */
#define TRACK_PARAMETERS_ID    "track"                 /* Название объекта с параметрами галса. */
#define PARAMETERS_FILE_EXT    "prm"                   /* Расширения для файлов со значениями параметров. */

#define HYSCAN_DB_FILE_API     20160400                /* Версия API файловой базы данных. */

#define MAX_CHANNEL_PARTS      999999                  /* Максимальное число частей канала - число файлов,
                                                          должно совпадать с MAX_PARTS в файле hyscan-db-channel-file.c. */

/* Свойства HyScanDBFile. */
enum
{
  PROP_O,
  PROP_PATH
};

/* Информация о проекте. */
typedef struct
{
  gint                 ref_count;              /* Число ссылок на объект. */
  guint                mod_count;              /* Номер изменения объекта. */

  gchar               *project_name;           /* Название проекта. */
  gchar               *path;                   /* Путь к каталогу с проектом. */

  gint64               ctime;                  /* Время создания проекта. */
} HyScanDBFileProjectInfo;

/* Информация о галсе. */
typedef struct
{
  gint                 ref_count;              /* Число ссылок на объект. */
  guint                mod_count;              /* Номер изменения объекта. */

  gchar               *project_name;           /* Название проекта. */
  gchar               *track_name;             /* Название галса. */
  gchar               *path;                   /* Путь к каталогу с галсом. */

  gint32               wid;                    /* Дескриптор с правами на запись. */
  gint64               ctime;                  /* Время создания галса. */
} HyScanDBFileTrackInfo;

/* Информация о канале данных. */
typedef struct
{
  gint                 ref_count;              /* Число ссылок на объект. */
  guint                mod_count;              /* Номер изменения объекта. */

  gchar               *project_name;           /* Название проекта. */
  gchar               *track_name;             /* Название галса. */
  gchar               *channel_name;           /* Название канала данных. */
  gchar               *path;                   /* Путь к каталогу с каналом данных. */

  gint32               wid;                    /* Дескриптор с правами на запись. */
  HyScanDBChannelFile *channel;                /* Объект канала данных. */
} HyScanDBFileChannelInfo;

/* Информация о параметрах. */
typedef struct
{
  gint                 ref_count;              /* Число ссылок на объект. */
  guint                mod_count;              /* Номер изменения объекта. */

  gchar               *project_name;           /* Название проекта. */
  gchar               *track_name;             /* Название галса. */
  gchar               *group_name;             /* Название группы параметров. */
  gchar               *object_name;            /* Название объекта. */

  gint32               track_object_wid;       /* Дескриптор с правами на запись параметров галса. */
  gint32               channel_object_wid;     /* Дескриптор с правами на запись параметров канала данных. */

  HyScanDBParamFile   *param;                  /* Объект параметров. */
} HyScanDBFileParamInfo;

/* Информация об объекте базы данных. */
typedef struct
{
  const gchar         *project_name;           /* Название проекта. */
  const gchar         *track_name;             /* Название галса. */
  const gchar         *group_name;             /* Название группы параметров. */
  const gchar         *object_name;            /* Название объекта. */
} HyScanDBFileObjectInfo;

/* Внутренние данные объекта. */
struct _HyScanDBFilePrivate
{
  gchar               *path;                   /* Путь к каталогу с проектами. */
  guint                mod_count;              /* Номер изменения объекта. */

  GHashTable          *projects;               /* Список открытых проектов. */
  GHashTable          *tracks;                 /* Список открытых галсов. */
  GHashTable          *channels;               /* Список открытых каналов данных. */
  GHashTable          *params;                 /* Список открытых групп параметров. */

  GRWLock              lock;                   /* Блокировка многопоточного доступа. */
};

static void            hyscan_db_file_interface_init           (HyScanDBInterface     *iface);
static void            hyscan_db_file_set_property             (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_db_file_object_finalize          (GObject               *object);

static gboolean        hyscan_db_file_check_name               (const gchar           *name,
                                                                gboolean               allow_slash);

static void            hyscan_db_remove_project_info           (gpointer               value);
static void            hyscan_db_remove_track_info             (gpointer               value);
static void            hyscan_db_remove_channel_info           (gpointer               value);
static void            hyscan_db_remove_param_info             (gpointer               value);

static gint32          hyscan_db_file_create_id                (HyScanDBFilePrivate   *priv);

static gboolean        hyscan_db_check_project_by_object_name  (gpointer               key,
                                                                gpointer               value,
                                                                gpointer               data);
static gboolean        hyscan_db_check_track_by_object_name    (gpointer               key,
                                                                gpointer               value,
                                                                gpointer               data);
static gboolean        hyscan_db_check_channel_by_object_name  (gpointer               key,
                                                                gpointer               value,
                                                                gpointer               data);
static gboolean        hyscan_db_check_param_by_object_name    (gpointer               key,
                                                                gpointer               value,
                                                                gpointer               data);

static gboolean        hyscan_db_project_test                  (const gchar           *path,
                                                                gint64                *ctime);
static gboolean        hyscan_db_track_test                    (const gchar           *path,
                                                                gint64                *ctime);
static gboolean        hyscan_db_channel_test                  (const gchar           *path,
                                                                const gchar           *name);

static gchar         **hyscan_db_file_get_directory_param_list (const gchar           *path);

static gboolean        hyscan_db_file_remove_directory         (const gchar           *path);
static gboolean        hyscan_db_file_remove_channel_files     (const gchar           *path,
                                                                const gchar           *name);

G_DEFINE_TYPE_WITH_CODE (HyScanDBFile, hyscan_db_file, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDBFile)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DB, hyscan_db_file_interface_init));

static void
hyscan_db_file_class_init (HyScanDBFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_file_set_property;
  object_class->finalize = hyscan_db_file_object_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
                                   g_param_spec_string ("path", "Path", "Path to projects", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_file_init (HyScanDBFile *dbf)
{
  HyScanDBFilePrivate *priv;

  dbf->priv = hyscan_db_file_get_instance_private (dbf);
  priv = dbf->priv;

  priv->projects = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_project_info);
  priv->tracks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_track_info);
  priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_channel_info);
  priv->params = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_param_info);

  g_rw_lock_init (&priv->lock);
}

static void
hyscan_db_file_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (object);
  HyScanDBFilePrivate *priv = dbf->priv;

  switch (prop_id)
    {
    case PROP_PATH:
      priv->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_file_object_finalize (GObject *object)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (object);
  HyScanDBFilePrivate *priv = dbf->priv;

  /* Удаляем списки открытых объектов базы данных. */
  g_hash_table_destroy (priv->params);
  g_hash_table_destroy (priv->channels);
  g_hash_table_destroy (priv->tracks);
  g_hash_table_destroy (priv->projects);

  g_rw_lock_clear (&priv->lock);

  g_free (priv->path);

  G_OBJECT_CLASS (hyscan_db_file_parent_class)->finalize (object);
}

/* Функция проверяет имя на валидность. */
static gboolean
hyscan_db_file_check_name (const gchar *name,
                           gboolean     allow_slash)
{
  gint i;

  for (i = 0; name[i] != '\0'; i++)
    {
      gboolean match = FALSE;

      if (name[i] >= 'a' && name[i] <= 'z')
        match = TRUE;
      if (name[i] >= 'A' && name[i] <= 'Z')
        match = TRUE;
      if (name[i] >= '0' && name[i] <= '9')
        match = TRUE;
      if (name[i] == '-' || name[i] == '.')
        match = TRUE;
      if (name[i] == '/' && allow_slash)
        match = TRUE;

      if (!match)
        return FALSE;
    }

  return TRUE;
}

/* Вспомогательная функция для удаления структуры HyScanDBFileProjectInfo. */
static void
hyscan_db_remove_project_info (gpointer value)
{
  HyScanDBFileProjectInfo *project_info = value;

  project_info->ref_count -= 1;
  if (project_info->ref_count > 0)
    return;

  g_free (project_info->project_name);
  g_free (project_info->path);

  g_free (project_info);
}

/* Вспомогательная функция для удаления структуры HyScanDBFileTrackInfo. */
static void
hyscan_db_remove_track_info (gpointer value)
{
  HyScanDBFileTrackInfo *track_info = value;

  track_info->ref_count -= 1;
  if (track_info->ref_count > 0)
    return;

  g_free (track_info->project_name);
  g_free (track_info->track_name);
  g_free (track_info->path);

  g_free (track_info);
}

/* Вспомогательная функция для удаления структуры HyScanDBFileChannelInfo. */
static void
hyscan_db_remove_channel_info (gpointer value)
{
  HyScanDBFileChannelInfo *channel_info = value;

  channel_info->ref_count -= 1;
  if (channel_info->ref_count > 0)
    return;

  g_object_unref (channel_info->channel);

  g_free (channel_info->project_name);
  g_free (channel_info->track_name);
  g_free (channel_info->channel_name);
  g_free (channel_info->path);

  g_free (channel_info);
}

/* Вспомогательная функция для удаления структуры HyScanDBFileParamInfo. */
static void
hyscan_db_remove_param_info (gpointer value)
{
  HyScanDBFileParamInfo *param_info = value;

  param_info->ref_count -= 1;
  if (param_info->ref_count > 0)
    return;

  g_object_unref (param_info->param);

  g_free (param_info->project_name);
  g_free (param_info->track_name);
  g_free (param_info->group_name);
  g_free (param_info->object_name);

  g_free (param_info);
}

/* Функция расчета и проверки уникальности идентификатора открываемого объекта
   HyScanDB (идентификатор выбирается случайным образом).*/
static gint32
hyscan_db_file_create_id (HyScanDBFilePrivate *priv)
{
  gboolean info;
  guint64 total_count = 0;
  gint id;

  total_count += g_hash_table_size (priv->projects);
  total_count += g_hash_table_size (priv->tracks);
  total_count += g_hash_table_size (priv->channels);
  total_count += g_hash_table_size (priv->params);

  if (total_count >= G_MAXINT32)
    return -1;

  do
    {
      id = g_random_int_range (1, G_MAXINT32);
      info = g_hash_table_contains (priv->projects, GINT_TO_POINTER (id));
      info |= g_hash_table_contains (priv->tracks, GINT_TO_POINTER (id));
      info |= g_hash_table_contains (priv->channels, GINT_TO_POINTER (id));
      info |= g_hash_table_contains (priv->params, GINT_TO_POINTER (id));
    }
  while (info);

  return id;
}

/* Вспомогательная функция поиска открытых проектов по имени проекта. */
static gboolean
hyscan_db_check_project_by_object_name (gpointer key,
                                        gpointer value,
                                        gpointer data)
{
  HyScanDBFileProjectInfo *project_info = value;
  HyScanDBFileObjectInfo *object_info = data;

  if (!g_pattern_match_simple (object_info->project_name, project_info->project_name))
    return FALSE;

  return TRUE;
}

/* Вспомогательная функция поиска открытых галсов по имени галса. */
static gboolean
hyscan_db_check_track_by_object_name (gpointer key,
                                      gpointer value,
                                      gpointer data)
{
  HyScanDBFileTrackInfo *track_info = value;
  HyScanDBFileObjectInfo *object_info = data;

  if (!g_pattern_match_simple (object_info->project_name, track_info->project_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->track_name, track_info->track_name))
    return FALSE;

  return TRUE;
}

/* Вспомогательная функция поиска открытых каналов данных по имени канала. */
static gboolean
hyscan_db_check_channel_by_object_name (gpointer key,
                                        gpointer value,
                                        gpointer data)
{
  HyScanDBFileChannelInfo *channel_info = value;
  HyScanDBFileObjectInfo *object_info = data;

  if (!g_pattern_match_simple (object_info->project_name, channel_info->project_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->track_name, channel_info->track_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->group_name, channel_info->channel_name))
    return FALSE;

  return TRUE;
}

/* Вспомогательная функция поиска открытых групп параметров по имени группы. */
static gboolean
hyscan_db_check_param_by_object_name (gpointer key,
                                      gpointer value,
                                      gpointer data)
{
  HyScanDBFileParamInfo *param_info = value;
  HyScanDBFileObjectInfo *object_info = data;

  if (!g_pattern_match_simple (object_info->project_name, param_info->project_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->track_name, param_info->track_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->group_name, param_info->group_name))
    return FALSE;
  if (!g_pattern_match_simple (object_info->object_name, param_info->object_name))
    return FALSE;

  return TRUE;
}

/* Функция проверяет, что каталог path содержит проект. */
static gboolean
hyscan_db_project_test (const gchar *path,
                        gint64      *ctime)
{
  gboolean status = FALSE;

  GKeyFile *project_param = g_key_file_new ();
  gchar *project_param_file = g_build_filename (path, PROJECT_ID_FILE, NULL);

  /* Загружаем файл с информацией о проекте. */
  if (!g_key_file_load_from_file (project_param, project_param_file, G_KEY_FILE_NONE, NULL))
    goto exit;

  /* Проверяем версию API файловой базы данных. */
  if (g_key_file_get_integer (project_param, "project", "version", NULL) != HYSCAN_DB_FILE_API)
    goto exit;

  /* Время создания проекта. */
  if (ctime != NULL)
    *ctime = g_key_file_get_int64 (project_param, "project", "ctime", NULL);

  /* Этот каталог содержит проект. */
  status = TRUE;

exit:
  g_key_file_free (project_param);
  g_free (project_param_file);

  return status;
}

/* Функция проверяет, что каталог path содержит галс. */
static gboolean
hyscan_db_track_test (const gchar *path,
                      gint64      *ctime)
{
  gboolean status = FALSE;

  GKeyFile *track_param = g_key_file_new ();
  gchar *track_param_file = g_build_filename (path, TRACK_ID_FILE, NULL);

  /* Загружаем файл с информацией о галсе. */
  if (!g_key_file_load_from_file (track_param, track_param_file, G_KEY_FILE_NONE, NULL))
    goto exit;

  /* Проверяем версию API галса. */
  if (g_key_file_get_integer (track_param, "track", "version", NULL) != HYSCAN_DB_FILE_API)
    goto exit;

  /* Время создания галса. */
  if (ctime != NULL)
    *ctime = g_key_file_get_int64 (track_param, "track", "ctime", NULL);

  /* Этот каталог содержит галс. */
  status = TRUE;

exit:
  g_key_file_free (track_param);
  g_free (track_param_file);

  return status;
}

/* Функция проверяет, что галс в каталоге path содержит канал name. */
static gboolean
hyscan_db_channel_test (const gchar *path,
                        const gchar *name)
{
  gboolean status = TRUE;
  gchar *channel_file = NULL;

  /* Проверяем наличие файла name.000000.d */
  channel_file = g_strdup_printf ("%s%s%s.000000.d", path, G_DIR_SEPARATOR_S, name);
  if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
    status = FALSE;
  g_free (channel_file);

  /* Проверяем наличие файла name.000000.i */
  channel_file = g_strdup_printf ("%s%s%s.000000.i", path, G_DIR_SEPARATOR_S, name);
  if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
    status = FALSE;
  g_free (channel_file);

  return status;
}

/* Функция возвращает список доступных групп параметров в указанном каталоге. */
static gchar **
hyscan_db_file_get_directory_param_list (const gchar *path)
{
  GDir *dir;
  const gchar *name;
  gchar **params = NULL;

  GError *error = NULL;
  gint i = 0;

  /* Открываем каталог с группами параметров. */
  error = NULL;
  dir = g_dir_open (path, 0, &error);
  if (dir == NULL)
    {
      g_warning ("HyScanDBFile: %s", error->message);
      return NULL;
    }

  /* Выбираем все файлы с расширением PARAMETERS_FILE_EXT. */
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *sub_path;
      gchar **group_name;

      /* Пропускаем все файлы с расширением не PARAMETERS_FILE_EXT. */
      if (!g_str_has_suffix (name, PARAMETERS_FILE_EXT))
        continue;

      /* Пропускаем все каталоги. */
      sub_path = g_build_filename (path, name, NULL);
      if (g_file_test (sub_path, G_FILE_TEST_IS_DIR))
        {
          g_free (sub_path);
          continue;
        }
      g_free (sub_path);

      /* Название группы параметров. */
      group_name = g_strsplit (name, ".", 2);
      if (g_strv_length (group_name) != 2 || strlen (group_name[0]) + strlen (group_name[1]) != strlen (name) - 1)
        {
          g_strfreev (group_name);
          continue;
        }

      /* Список групп параметров. */
      params = g_realloc (params, 16 * (((i + 1) / 16) + 1) * sizeof (gchar *));
      params[i] = g_strdup (group_name[0]);
      i++;
      params[i] = NULL;

      g_strfreev (group_name);
    }

  g_dir_close (dir);

  return params;
}

/* Функция рекурсивно удаляет каталог со всеми его файлами и подкаталогами. */
static gboolean
hyscan_db_file_remove_directory (const gchar *path)
{
  gboolean status = TRUE;
  const gchar *name;

  /* Открываем каталог для удаления. */
  GDir *dir = g_dir_open (path, 0, NULL);
  if (dir == NULL)
    return status;

  /* Проходимся по всем элементам в нём. */
  while ((name = g_dir_read_name (dir)) != NULL)
    {

      gchar *sub_path = g_build_filename (path, name, NULL);

      /* Если это каталог - рекурсивно удаляем его. */
      if (g_file_test (sub_path, G_FILE_TEST_IS_DIR))
        status = hyscan_db_file_remove_directory (sub_path);

      /* Файл удаляем непосредственно. */
      else if (g_unlink (sub_path) != 0)
        {
          g_warning ("HyScanDBFile: can't remove file %s", sub_path);
          status = FALSE;
        }

      g_free (sub_path);

      if (!status)
        break;

    }

  g_dir_close (dir);

  if (g_rmdir (path) != 0)
    {
      g_warning ("HyScanDBFile: can't remove directory %s", path);
      status = FALSE;
    }

  return status;
}

/* Функция удаляет все файлы в каталоге path относящиеся к каналу name. */
static gboolean
hyscan_db_file_remove_channel_files (const gchar *path,
                                     const gchar *name)
{
  gboolean status = TRUE;
  gchar *channel_file = NULL;
  gint i;

  /* Удаляем файлы индексов. */
  for (i = 0; i < MAX_CHANNEL_PARTS; i++)
    {
      channel_file = g_strdup_printf ("%s%s%s.%06d.i", path, G_DIR_SEPARATOR_S, name, i);

      /* Если файлы закончились - выходим. */
      if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
        {
          g_free (channel_file);
          break;
        }

      if (g_unlink (channel_file) != 0)
        {
          g_warning ("HyScanDBFile: can't remove file %s", channel_file);
          status = FALSE;
        }
      g_free (channel_file);

      if (!status)
        return status;
    }

  /* Удаляем файлы данных. */
  for (i = 0; i < MAX_CHANNEL_PARTS; i++)
    {
      channel_file = g_strdup_printf ("%s%s%s.%06d.d", path, G_DIR_SEPARATOR_S, name, i);

      /* Если файлы закончились - выходим. */
      if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
        {
          g_free (channel_file);
          break;
        }

      if (g_unlink (channel_file) != 0)
        {
          g_warning ("HyScanDBFile: can't remove file %s", channel_file);
          status = FALSE;
        }
      g_free (channel_file);

      if (!status)
        return status;
    }

  return status;
}

/* Функция возвращает путь к базе данных. */
static gchar *
hyscan_db_file_get_uri (HyScanDB *db)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  GFile *path = g_file_new_for_path (priv->path);
  gchar *uri = g_file_get_uri (path);

  g_object_unref (path);

  return uri;
}

/* Функция возвращает номер изменения в объекте. */
static guint64
hyscan_db_file_get_mod_count (HyScanDB *db,
                              gint32    id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;

  guint64 mod_count = 0;

  g_rw_lock_reader_lock (&priv->lock);

  if (id == 0)
    {
      mod_count = g_atomic_int_get (&priv->mod_count);
      goto exit;
    }

  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (id));
  if (project_info != NULL)
    {
      mod_count = g_atomic_int_get (&project_info->mod_count);
      goto exit;
    }

  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (id));
  if (track_info != NULL)
    {
      mod_count = g_atomic_int_get (&track_info->mod_count);
      goto exit;
    }

  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (id));
  if (channel_info != NULL)
    {
      mod_count = g_atomic_int_get (&channel_info->mod_count);
      goto exit;
    }

  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (id));
  if (param_info != NULL)
    {
      mod_count = g_atomic_int_get (&param_info->mod_count);
      goto exit;
    }

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return mod_count;
}

/* Функция проверяет существование проекта, галса или канала данных в системе хранения. */
static gboolean
hyscan_db_file_is_exist (HyScanDB    *db,
                         const gchar *project_name,
                         const gchar *track_name,
                         const gchar *channel_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileObjectInfo object_info;

  gchar *project_path = NULL;
  gchar *track_path = NULL;

  gboolean exist = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Проверяем существование проекта. */
  project_path = g_build_filename (priv->path, project_name, NULL);
  if (!hyscan_db_project_test (project_path, NULL))
    goto exit;

  if (track_name == NULL && channel_name == NULL)
    {
      exist = TRUE;
      goto exit;
    }

  /* Проверяем существование галса. */
  track_path = g_build_filename (project_path, track_name, NULL);
  if (!hyscan_db_track_test (track_path, NULL))
    goto exit;

  if (channel_name == NULL)
    {
      exist = TRUE;
      goto exit;
    }

  /* Проверяем существование канала данных. */
  if (hyscan_db_channel_test (track_path, channel_name))
    {
      exist = TRUE;
      goto exit;
    }

  /* Возможно в канал данных еще ничего не записали, но он создан. */
  object_info.project_name = project_name;
  object_info.track_name = track_name;
  object_info.group_name = channel_name;
  if (g_hash_table_find (priv->channels, hyscan_db_check_channel_by_object_name, &object_info))
    exist = TRUE;

exit:
  g_rw_lock_reader_unlock (&priv->lock);
  g_free (project_path);
  g_free (track_path);

  return exist;
}

/* Функция возвращает список доступных проектов. */
static gchar **
hyscan_db_file_project_list (HyScanDB *db)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  GDir *db_dir;
  const gchar *project_name;
  gchar **projects = NULL;

  GError *error = NULL;
  gint i = 0;

  /* Открываем каталог с проектами. */
  db_dir = g_dir_open (priv->path, 0, &error);
  if (db_dir == NULL)
    {
      g_warning ("HyScanDBFile: %s", error->message);
      return NULL;
    }

  g_rw_lock_reader_lock (&priv->lock);

  /* Проверяем все найденые каталоги - содержат они проект или нет. */
  while ((project_name = g_dir_read_name (db_dir)) != NULL)
    {

      gboolean status = FALSE;
      gchar *project_path;

      /* Проверяем содержит каталог проект или нет. */
      project_path = g_build_filename (priv->path, project_name, NULL);
      status = hyscan_db_project_test (project_path, NULL);
      g_free (project_path);

      if (!status)
        continue;

      /* Список проектов. */
      projects = g_realloc (projects, 16 * (((i + 1) / 16) + 1) * sizeof (gchar *));
      projects[i] = g_strdup (project_name);
      i++;
      projects[i] = NULL;

    }

  g_rw_lock_reader_unlock (&priv->lock);

  g_clear_pointer (&db_dir, g_dir_close);

  return projects;
}

/* Функция открывает проект для работы. */
static gint32
hyscan_db_file_project_open (HyScanDB    *db,
                             const gchar *project_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  gint32 nid;
  gint32 id = -1;

  gchar *project_path = NULL;
  HyScanDBFileProjectInfo *project_info = NULL;
  HyScanDBFileObjectInfo object_info;

  gint64 ctime;

  g_rw_lock_writer_lock (&priv->lock);

  /* Генерация нового идентификатора открываемого объекта. */
  nid = hyscan_db_file_create_id (priv);
  if (nid < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      goto exit;
    }

  /* Ищем проект в списке открытых. */
  object_info.project_name = project_name;
  object_info.track_name = "";
  object_info.group_name = "";
  project_info = g_hash_table_find (priv->projects, hyscan_db_check_project_by_object_name, &object_info);

  /* Проект уже открыт. */
  if (project_info != NULL)
    {
      id = nid;
      project_info->ref_count += 1;
    }

  /* Открываем проект для работы. */
  else
    {
      /* Проверяем, что каталог содержит проект. */
      project_path = g_build_filename (priv->path, project_name, NULL);
      if (!hyscan_db_project_test (project_path, &ctime))
        {
          g_warning ("HyScanDBFile: '%s' no such project", project_name);
          goto exit;
        }

      project_info = g_new (HyScanDBFileProjectInfo, 1);
      project_info->ref_count = 1;
      project_info->mod_count = 1;
      project_info->project_name = g_strdup (project_name);
      project_info->path = project_path;
      project_info->ctime = ctime;
      project_path = NULL;
      id = nid;
    }

  g_hash_table_insert (priv->projects, GINT_TO_POINTER (id), project_info);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  g_free (project_path);

  return id;
}

/* Функция создаёт новый проект и открывает его для работы. */
static gint32
hyscan_db_file_project_create (HyScanDB    *db,
                               const gchar *project_name,
                               const gchar *project_schema)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  gboolean status = FALSE;

  GKeyFile *project_param = NULL;
  gchar *project_path = NULL;
  gchar *project_param_file = NULL;
  gchar *project_schema_file = NULL;
  gchar *param_data = NULL;

  /* Проверяем название проекта. */
  if (!hyscan_db_file_check_name (project_name, FALSE))
    return -1;

  g_rw_lock_writer_lock (&priv->lock);

  project_param = g_key_file_new ();
  project_path = g_build_filename (priv->path, project_name, NULL);
  project_param_file = g_build_filename (priv->path, project_name, PROJECT_ID_FILE, NULL);
  project_schema_file = g_build_filename (priv->path,project_name, PROJECT_SCHEMA_FILE, NULL);

  /* Проверяем, что каталога с названием проекта нет. */
  if (g_file_test (project_path, G_FILE_TEST_IS_DIR))
    {
      g_warning ("HyScanDBFile: project '%s' already exists", project_name);
      goto exit;
    }

  /* Создаём каталог для проекта. */
  if (g_mkdir_with_parents (project_path, 0777) != 0)
    {
      g_warning ("HyScanDBFile: can't create project '%s' directory", project_name);
      goto exit;
    }

  /* Файл идентификации проекта. */
  g_key_file_set_integer (project_param, "project", "version", HYSCAN_DB_FILE_API);
  g_key_file_set_int64 (project_param, "project", "ctime", g_get_real_time () / G_USEC_PER_SEC);
  param_data = g_key_file_to_data (project_param, NULL, NULL);
  if (!g_file_set_contents (project_param_file, param_data, -1, NULL))
    {
      g_warning ("HyScanDBFile: can't save project '%s' identification file", project_name);
      goto exit;
    }

  /* Схема параметров проекта. */
  if (project_schema != NULL)
    {
      if (!g_file_set_contents (project_schema_file, project_schema, -1, NULL))
        {
          g_warning ("HyScanDBFile: can't save project '%s' schema", project_name);
          goto exit;
        }
    }

  g_atomic_int_inc (&priv->mod_count);
  status = TRUE;

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  g_key_file_free (project_param);
  g_free (project_schema_file);
  g_free (project_param_file);
  g_free (project_path);
  g_free (param_data);

  if (status)
    return hyscan_db_file_project_open (db, project_name);

  return -1;
}

/* Функция удаляет проект. */
static gboolean
hyscan_db_file_project_remove (HyScanDB    *db,
                               const gchar *project_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileObjectInfo object_info;

  gboolean status = FALSE;
  gchar *project_path = NULL;

  GHashTableIter iter;
  gpointer key, value;

  g_rw_lock_writer_lock (&priv->lock);

  object_info.project_name = project_name;
  object_info.track_name = "*";
  object_info.group_name = "*";
  object_info.object_name = "*";

  /* Ищем все открытые группы параметров этого проекта и закрываем их. */
  g_hash_table_iter_init (&iter, priv->params);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_param_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Ищем все открытые каналы данных этого проекта и закрываем их. */
  g_hash_table_iter_init (&iter, priv->channels);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_channel_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Ищем все открытые галсы этого проекта и закрываем их. */
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_track_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Если проект открыт закрываем его. */
  g_hash_table_iter_init (&iter, priv->projects);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_project_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Проверяем, что каталог содержит проект. */
  project_path = g_build_filename (priv->path, project_name, NULL);
  if (!(status = hyscan_db_project_test (project_path, NULL)))
    {
      g_warning ("HyScanDBFile: '%s' not a project", project_name);
      goto exit;
    }

  /* Удаляем каталог с проектом. */
  status = hyscan_db_file_remove_directory (project_path);
  g_atomic_int_inc (&priv->mod_count);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  g_free (project_path);

  return status;
}

/* Функция возвращает информацию о дате и времени создания проекта. */
static GDateTime *
hyscan_db_file_project_get_ctime (HyScanDB *db,
                                  gint32    project_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  GDateTime *ctime = NULL;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info != NULL)
    ctime = g_date_time_new_from_unix_local (project_info->ctime);

  g_rw_lock_reader_unlock (&priv->lock);

  return ctime;
}

/* Функция возвращает список доступных галсов проекта. */
static gchar **
hyscan_db_file_track_list (HyScanDB *db,
                               gint32    project_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;

  GDir *db_dir = NULL;
  const gchar *track_name;
  gchar **tracks = NULL;

  GError *error;
  gint i = 0;

  g_rw_lock_reader_lock (&priv->lock);

  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  /* Открываем каталог проекта с галсами. */
  error = NULL;
  db_dir = g_dir_open (project_info->path, 0, &error);
  if (db_dir == NULL)
    {
      g_warning ("HyScanDBFile: %s", error->message);
      goto exit;
    }

  /* Проверяем все найденые каталоги - содержат они галс или нет. */
  while ((track_name = g_dir_read_name (db_dir)) != NULL)
    {
      gboolean status = FALSE;
      gchar *track_path;

      /* Проверяем содержит каталог галс или нет. */
      track_path = g_build_filename (project_info->path, track_name, NULL);
      status = hyscan_db_track_test (track_path, NULL);
      g_free (track_path);

      if (!status)
        continue;

      /* Список галсов. */
      tracks = g_realloc (tracks, 16 * (((i + 1) / 16) + 1) *sizeof (gchar *));
      tracks[i] = g_strdup (track_name);
      i++;
      tracks[i] = NULL;
    }

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  g_clear_pointer (&db_dir, g_dir_close);

  return tracks;
}

/* Внутренняя функция открытия галса. */
static gint32
hyscan_db_file_open_track_int (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name,
                               gboolean     readonly)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  gint32 id;
  gchar *track_path = NULL;

  HyScanDBFileProjectInfo *project_info = NULL;
  HyScanDBFileTrackInfo *track_info = NULL;
  HyScanDBFileObjectInfo object_info;

  gint64 ctime;

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    return -1;

  /* Генерация нового идентификатора открываемого объекта. */
  id = hyscan_db_file_create_id (priv);
  if (id < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      return -1;
    }

  object_info.project_name = project_info->project_name;
  object_info.track_name = track_name;
  object_info.group_name = "";
  track_info = g_hash_table_find (priv->tracks, hyscan_db_check_track_by_object_name, &object_info);

  /* Галс уже открыт. */
  if (track_info != NULL)
    {
      track_info->ref_count += 1;
    }

  /* Открываем галс для работы. */
  else
    {
      /* Проверяем, что каталог содержит галс. */
      track_path = g_build_filename (project_info->path, track_name, NULL);
      if (!hyscan_db_track_test (track_path, &ctime))
        {
          g_warning ("HyScanDBFile: '%s.%s' - no such track", project_info->project_name, track_name);
          g_free (track_path);
          return -1;
        }

      track_info = g_new (HyScanDBFileTrackInfo, 1);
      track_info->ref_count = 1;
      track_info->project_name = g_strdup (project_info->project_name);
      track_info->track_name = g_strdup (track_name);
      track_info->path = track_path;
      track_info->ctime = ctime;
      if (readonly)
        track_info->wid = -1;
      else
        track_info->wid = id;
      track_path = NULL;

      g_free (track_path);
    }

  g_hash_table_insert (priv->tracks, GINT_TO_POINTER (id), track_info);

  return id;
}

/* Функция открывает галс для работы. */
static gint32
hyscan_db_file_track_open (HyScanDB    *db,
                           gint32       project_id,
                           const gchar *track_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  gint32 id;

  g_rw_lock_writer_lock (&priv->lock);
  id = hyscan_db_file_open_track_int (db, project_id, track_name, TRUE);
  g_rw_lock_writer_unlock (&priv->lock);

  return id;
}

/* Функция создаёт новый галс и открывает его для работы. */
static gint32
hyscan_db_file_track_create (HyScanDB    *db,
                             gint32       project_id,
                             const gchar *track_name,
                             const gchar *track_schema,
                             const gchar *schema_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  HyScanDBParamFile *params;

  gint32 id = -1;

  GKeyFile *track_id = NULL;
  gchar *track_path = NULL;
  gchar *track_id_file = NULL;
  gchar *track_param_file = NULL;
  gchar *track_schema_file = NULL;
  gchar *param_data = NULL;

  /* Проверяем название галса. */
  if (!hyscan_db_file_check_name (track_name, FALSE))
    return -1;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  track_id = g_key_file_new ();
  track_path = g_build_filename (project_info->path, track_name, NULL);
  track_id_file = g_build_filename (project_info->path, track_name, TRACK_ID_FILE, NULL);
  track_param_file = g_build_filename (project_info->path, track_name, TRACK_PARAMETERS_FILE, NULL);
  track_schema_file = g_build_filename (project_info->path, track_name, TRACK_SCHEMA_FILE, NULL);

  /* Проверяем, что каталога с названием проекта нет. */
  if (g_file_test (track_path, G_FILE_TEST_IS_DIR))
    {
      g_warning ("HyScanDBFile: track '%s.%s' already exists",
                 project_info->project_name, track_name);
      goto exit;
    }

  /* Создаём каталог для проекта. */
  if (g_mkdir_with_parents (track_path, 0777) != 0)
    {
      g_warning ("HyScanDBFile: can't create track '%s.s%s' directory",
                 project_info->project_name, track_name);
      goto exit;
    }

  /* Файл идентификатор галса. */
  g_key_file_set_integer (track_id, "track", "version", HYSCAN_DB_FILE_API);
  g_key_file_set_int64 (track_id, "track", "ctime", g_get_real_time () / G_USEC_PER_SEC);
  param_data = g_key_file_to_data (track_id, NULL, NULL);
  if (!g_file_set_contents (track_id_file, param_data, -1, NULL))
    {
      g_warning ("HyScanDBFile: can't save track '%s.%s' identification file",
                 project_info->project_name, track_name);
      goto exit;
    }

  /* Файл схемы параметров галса. */
  if (track_schema != NULL)
    {
      if (!g_file_set_contents (track_schema_file, track_schema, -1, NULL))
        {
          g_warning ("HyScanDBFile: can't save track '%s.%s' schema",
                     project_info->project_name, track_name);
          goto exit;
        }
    }

  /* Файл параметров галса. */
  if (schema_id != NULL)
    {
      params = hyscan_db_param_file_new (track_param_file, track_schema_file);
      hyscan_db_param_file_object_create (params, TRACK_PARAMETERS_ID, schema_id);
      g_object_unref (params);
    }

  /* Открываем галс. */
  id = hyscan_db_file_open_track_int (db, project_id, track_name, FALSE);

  g_atomic_int_inc (&project_info->mod_count);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  g_clear_pointer (&track_id, g_key_file_free);
  g_free (track_schema_file);
  g_free (track_param_file);
  g_free (track_id_file);
  g_free (track_path);
  g_free (param_data);

  return id;
}

/* Функция удаляет галс. */
static gboolean
hyscan_db_file_track_remove (HyScanDB    *db,
                             gint32       project_id,
                             const gchar *track_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileObjectInfo object_info;

  gboolean status = FALSE;
  gchar *track_path = NULL;

  GHashTableIter iter;
  gpointer key, value;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  object_info.project_name = project_info->project_name;
  object_info.track_name = track_name;
  object_info.group_name = "*";
  object_info.object_name = "*";

  /* Ищем все открытые группы параметров этого галса и закрываем их. */
  g_hash_table_iter_init (&iter, priv->params);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_param_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Ищем все открытые каналы данных этого галса и закрываем их. */
  g_hash_table_iter_init (&iter, priv->channels);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_channel_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Если галс открыт закрываем его. */
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_track_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Проверяем, что каталог содержит галс. */
  track_path = g_build_filename (project_info->path, track_name, NULL);
  if (!(status = hyscan_db_track_test (track_path, NULL)))
    {
      g_warning ("HyScanDBFile: '%s.%s' not a track", project_info->project_name, track_name);
      goto exit;
    }

  /* Удаляем каталог с галсом. */
  status = hyscan_db_file_remove_directory (track_path);
  g_atomic_int_inc (&project_info->mod_count);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  g_free (track_path);

  return status;
}

/* Функция возвращает информацию о дате и времени создания галса. */
static GDateTime *
hyscan_db_file_track_get_ctime (HyScanDB *db,
                                gint32    project_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileTrackInfo *track_info;
  GDateTime *ctime = NULL;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (project_id));
  if (track_info != NULL)
    ctime = g_date_time_new_from_unix_local (track_info->ctime);

  g_rw_lock_reader_unlock (&priv->lock);

  return ctime;
}

/* Функция возвращает список доступных каналов данных галса. */
static gchar **
hyscan_db_file_channel_list (HyScanDB *db,
                                 gint32    track_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileTrackInfo *track_info;

  GDir *db_dir = NULL;
  const gchar *file_name;
  gchar **channels = NULL;

  GError *error;
  gint i = 0;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    goto exit;

  /* Открываем каталог галса. */
  error = NULL;
  db_dir = g_dir_open (track_info->path, 0, &error);
  if (db_dir == NULL)
    {
      g_warning ("HyScanDBFile: %s", error->message);
      goto exit;
    }

  /* Проверяем все найденые файлы на совпадение с именем name.000000.d */
  while ((file_name = g_dir_read_name (db_dir)) != NULL)
    {

      gboolean status = FALSE;
      gchar **splited_channel_name = g_strsplit (file_name, ".", 2);
      gchar *channel_name = NULL;

      if (splited_channel_name == NULL)
        continue;

      /* Если совпадение найдено проверяем, что существует канал с именем name. */
      if (g_strcmp0 (splited_channel_name[1], "000000.d") == 0)
        status = hyscan_db_channel_test (track_info->path, splited_channel_name[0]);

      channel_name = splited_channel_name[0];
      if (!status)
        g_free (splited_channel_name[0]);
      if (splited_channel_name[1] != NULL)
        g_free (splited_channel_name[1]);
      g_free (splited_channel_name);

      if (!status)
        continue;

      /* Список каналов. */
      channels = g_realloc (channels, 16 *(((i + 1) / 16) + 1) *sizeof (gchar *));
      channels[i] = channel_name;
      i++;
      channels[i] = NULL;

    }

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  g_clear_pointer (&db_dir, g_dir_close);

  return channels;
}

/* Функция создаёт или открывает существующий канал данных. */
static gint32
hyscan_db_file_open_channel_int (HyScanDB    *db,
                                 gint32       track_id,
                                 const gchar *channel_name,
                                 const gchar *schema_id,
                                 gboolean     readonly)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  gint32 nid;
  gint32 id = -1;

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileObjectInfo object_info;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    goto exit;

  /* Генерация нового идентификатора открываемого объекта. */
  nid = hyscan_db_file_create_id (priv);
  if (nid < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      goto exit;
    }

  /* Полное имя канала данных. */
  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = channel_name;

  /* Ищем канал в списке открытых для записи. */
  channel_info = g_hash_table_find (priv->channels, hyscan_db_check_channel_by_object_name, &object_info);

  /* Канал уже открыт. */
  if (channel_info != NULL)
    {
      if (readonly)
        {
          channel_info->ref_count += 1;
          id = nid;
        }
      else
        {
          /* Если канал уже открыт и находится в режиме записи - возвращаем ошибку. */
          g_warning ("HyScanDBFile: channel '%s.%s.%s' already exists",
                     track_info->project_name, track_info->track_name, channel_name);
          goto exit;
        }
    }

  /* Открываем канал для работы. */
  else
    {
      /* Если канала нет и запрашивается его открытие на чтение или
         если канал есть и запрашивается открытие на запись - ошибка. */
      if (hyscan_db_channel_test (track_info->path, channel_name) != readonly)
        {
          if (readonly)
            g_warning ("HyScanDBFile: '%s.%s.%s' - no such channel",
                       track_info->project_name, track_info->track_name, channel_name);
          else
            g_warning ("HyScanDBFile: channel '%s.%s.%s' already exists",
                       track_info->project_name, track_info->track_name, channel_name);
          goto exit;
        }

      channel_info = g_new (HyScanDBFileChannelInfo, 1);
      channel_info->ref_count = 1;
      channel_info->mod_count = 1;
      channel_info->project_name = g_strdup (track_info->project_name);
      channel_info->track_name = g_strdup (track_info->track_name);
      channel_info->channel_name = g_strdup (channel_name);
      channel_info->path = g_strdup (track_info->path);
      channel_info->channel = hyscan_db_channel_file_new (channel_info->path,
                                                          channel_info->channel_name,
                                                          readonly);
      if (readonly)
        {
          channel_info->wid = -1;
        }
      else
        {
          channel_info->wid = nid;
          g_atomic_int_inc (&track_info->mod_count);
        }

      /* Создаём объект с параметрами канала данных. */
      if (!readonly && schema_id != NULL)
        {
          HyScanDBFileParamInfo *param_info;
          HyScanDBParamFile *param;

          gchar *track_param_file;
          gchar *track_schema_file;

          track_param_file = g_build_filename (track_info->path, TRACK_PARAMETERS_FILE, NULL);
          track_schema_file = g_build_filename (track_info->path, TRACK_SCHEMA_FILE, NULL);

          /* Полное имя параметров канала данных. */
          object_info.project_name = track_info->project_name;
          object_info.track_name = track_info->track_name;
          object_info.group_name = TRACK_GROUP_ID;

          /* Ищем группу параметров в списке открытых. */
          param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
          if (param_info != NULL)
            param = g_object_ref (param_info->param);
          else
            param = hyscan_db_param_file_new (track_param_file, track_schema_file);

          /* Создаём объект в группе параметров с именем канала данных и указанной схемой. */
          hyscan_db_param_file_object_create (param, channel_name, schema_id);

          g_free (track_schema_file);
          g_free (track_param_file);

          g_object_unref (param);
        }

      id = nid;
    }

  g_hash_table_insert (priv->channels, GINT_TO_POINTER (id), channel_info);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return id;
}

/* Функция открывает существующий канал данных. */
static gint32
hyscan_db_file_channel_open (HyScanDB    *db,
                             gint32       track_id,
                             const gchar *channel_name)
{
  return hyscan_db_file_open_channel_int (db, track_id, channel_name, NULL, TRUE);
}

/* Функция создаёт канал данных. */
static gint32
hyscan_db_file_channel_create (HyScanDB    *db,
                               gint32       track_id,
                               const gchar *channel_name,
                               const gchar *schema_id)
{
  /* Проверяем название канала данных. */
  if (g_strcmp0 (channel_name, TRACK_PARAMETERS_ID) == 0)
    {
      g_warning ("HyScanDBFile: can't use reserved name 'track'");
      return -1;
    }
  if (!hyscan_db_file_check_name (channel_name, FALSE))
    return -1;

  return hyscan_db_file_open_channel_int (db, track_id, channel_name, schema_id, FALSE);
}

/* Функция удаляет канал данных. */
static gboolean
hyscan_db_file_channel_remove (HyScanDB    *db,
                               gint32       track_id,
                               const gchar *channel_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileObjectInfo object_info;

  HyScanDBParamFile *param = NULL;
  gchar *track_param_file;
  gchar *track_schema_file;

  gboolean status = FALSE;

  GHashTableIter iter;
  gpointer key, value;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    goto exit;

  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = channel_name;

  /* Ищем канал в списке открытых, при необходимости закрываем. */
  g_hash_table_iter_init (&iter, priv->channels);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_channel_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Полное имя параметров канала данных. */
  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = channel_name;

  /* Ищем параметры канала данных в списке открытых, при необходимости закрываем. */
  g_hash_table_iter_init (&iter, priv->params);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanDBFileParamInfo *param_info;

      if (!hyscan_db_check_param_by_object_name (key, value, &object_info))
        continue;

      /* Если параметры канала данных были открыты, используем их для удаления объекта. */
      param_info = value;
      param = g_object_ref (param_info->param);

      g_hash_table_iter_remove (&iter);
    }

  /* Удаляем объект с параметрами канала данных. */
  track_param_file = g_build_filename (track_info->path, TRACK_PARAMETERS_FILE, NULL);
  track_schema_file = g_build_filename (track_info->path, TRACK_SCHEMA_FILE, NULL);
  if (param == NULL)
    param = hyscan_db_param_file_new (track_param_file, track_schema_file);

  /* Удаляем объект в группе параметров с именем канала данных. */
  hyscan_db_param_file_object_remove (param, channel_name);
  g_object_unref (param);

  g_free (track_schema_file);
  g_free (track_param_file);

  /* Удаляем файлы канала данных. */
  status = hyscan_db_file_remove_channel_files (track_info->path, channel_name);
  g_atomic_int_inc (&track_info->mod_count);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return status;
}

/* Функция завершает запись в канал данных. */
static void
hyscan_db_file_channel_finalize (HyScanDB *db,
                                 gint32    channel_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));

  /* Закрыли дескриптор с правами на запись. */
  if (channel_info != NULL && channel_info->wid == channel_id)
    {
      object_info.project_name = channel_info->project_name;
      object_info.track_name = channel_info->track_name;
      object_info.group_name = TRACK_GROUP_ID;
      object_info.object_name = channel_info->channel_name;

      param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
      if (param_info != NULL)
        param_info->channel_object_wid = -1;

      hyscan_db_channel_file_finalize_channel (channel_info->channel);
      channel_info->wid = -1;
    }

  g_rw_lock_writer_unlock (&priv->lock);
}

/* Функция возвращает режим доступа к каналу данных. */
static gboolean
hyscan_db_file_channel_is_writable (HyScanDB *db,
                                    gint32    channel_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean writable = FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));

  /* Проверяем режим доступа. */
  if (channel_info->wid > 0)
    writable = TRUE;

  g_rw_lock_writer_unlock (&priv->lock);

  return writable;
}

/* Функция устанавливает максимальный размер файла данных канала. */
static gboolean
hyscan_db_file_channel_set_chunk_size (HyScanDB *db,
                                       gint32    channel_id,
                                       gint32    chunk_size)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_chunk_size (channel_info->channel, chunk_size);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает интервал времени в микросекундах для которого хранятся записываемые данные. */
static gboolean
hyscan_db_file_channel_set_save_time (HyScanDB *db,
                                      gint32    channel_id,
                                      gint64    save_time)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_save_time (channel_info->channel, save_time);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает максимальный объём сохраняемых данных. */
static gboolean
hyscan_db_file_channel_set_save_size (HyScanDB *db,
                                      gint32    channel_id,
                                      gint64    save_size)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_save_size (channel_info->channel, save_size);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция возвращает диапазон текущих значений индексов данных. */
static gboolean
hyscan_db_file_channel_get_data_range (HyScanDB *db,
                                       gint32    channel_id,
                                       gint32   *first_index,
                                       gint32   *last_index)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_get_channel_data_range (channel_info->channel, first_index, last_index);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция записывает новые данные канала. */
static gboolean
hyscan_db_file_channel_add_data (HyScanDB      *db,
                                 gint32         channel_id,
                                 gint64         time,
                                 gconstpointer  data,
                                 gint32         size,
                                 gint32        *index)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL && channel_info->wid == channel_id)
    {
      status = hyscan_db_channel_file_add_channel_data (channel_info->channel, time, data, size, index);
      if (status)
        g_atomic_int_inc (&channel_info->mod_count);
    }

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция считывает данные. */
static gboolean
hyscan_db_file_channel_get_data (HyScanDB *db,
                                 gint32    channel_id,
                                 gint32    index,
                                 gpointer  buffer,
                                 gint32   *buffer_size,
                                 gint64   *time)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_get_channel_data (channel_info->channel, index, buffer, buffer_size, time);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция ищет данные по метке времени. */
static gboolean
hyscan_db_file_channel_find_data (HyScanDB *db,
                                  gint32    channel_id,
                                  gint64    time,
                                  gint32   *lindex,
                                  gint32   *rindex,
                                  gint64   *ltime,
                                  gint64   *rtime)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_find_channel_data (channel_info->channel, time, lindex, rindex, ltime, rtime);

  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция возвращает список групп параметров в проекте. */
static gchar **
hyscan_db_file_project_param_list (HyScanDB *db,
                                   gint32    project_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  gchar **list = NULL;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info != NULL)
    list = hyscan_db_file_get_directory_param_list (project_info->path);

  g_rw_lock_reader_unlock (&priv->lock);

  return list;
}

/* Функция открывает группу параметров проекта. */
static gint32
hyscan_db_file_project_param_open (HyScanDB    *db,
                                   gint32       project_id,
                                   const gchar *group_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  gchar *param_file = NULL;
  gchar *schema_file = NULL;
  gint32 id = -1;

  /* Проверяем название группы параметров. */
  if (!hyscan_db_file_check_name (group_name, FALSE))
    return id;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  /* Генерация нового идентификатора открываемого объекта. */
  id = hyscan_db_file_create_id (priv);
  if (id < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      goto exit;
    }

  /* Полное имя группы параметров. */
  object_info.project_name = project_info->project_name;
  object_info.track_name = "";
  object_info.group_name = group_name;
  object_info.object_name = "";

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);

  /* Параметры уже открыты. */
  if (param_info != NULL)
    {
      param_info->ref_count += 1;
    }

  /* Открываем группу параметров для работы. */
  else
    {
      param_file = g_strdup_printf ("%s%s%s.%s", project_info->path, G_DIR_SEPARATOR_S,
                                                 group_name, PARAMETERS_FILE_EXT);
      schema_file = g_build_filename (project_info->path, PROJECT_SCHEMA_FILE, NULL);
      param_info = g_new (HyScanDBFileParamInfo, 1);
      param_info->ref_count = 1;
      param_info->mod_count = 1;
      param_info->project_name = g_strdup (project_info->project_name);
      param_info->track_name = g_strdup ("");
      param_info->group_name = g_strdup (group_name);
      param_info->object_name = g_strdup ("");
      param_info->track_object_wid = -1;
      param_info->channel_object_wid = -1;
      param_info->param = hyscan_db_param_file_new (param_file, schema_file);
      g_atomic_int_inc (&project_info->mod_count);
    }

  g_hash_table_insert (priv->params, GINT_TO_POINTER (id), param_info);

exit:
  g_rw_lock_writer_unlock (&priv->lock);
  g_free (param_file);
  g_free (schema_file);

  return id;
}

/* Функция удаляет группу параметров проекта. */
static gboolean
hyscan_db_file_project_param_remove (HyScanDB    *db,
                                     gint32       project_id,
                                     const gchar *group_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileProjectInfo *project_info;
  HyScanDBFileObjectInfo object_info;

  gchar *param_file = NULL;
  GHashTableIter iter;
  gpointer key, value;

  gboolean status = FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  /* Полное имя проекта. */
  object_info.project_name = project_info->project_name;
  object_info.track_name = "";
  object_info.group_name = group_name;
  object_info.object_name = "*";

  /* Ищем параметры канала в списке открытых, при необходимости закрываем. */
  g_hash_table_iter_init (&iter, priv->params);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!hyscan_db_check_param_by_object_name (key, value, &object_info))
        continue;

      g_hash_table_iter_remove (&iter);
    }

  /* Удаляем файл с параметрами. */
  param_file = g_strdup_printf ("%s%s%s.%s", project_info->path, G_DIR_SEPARATOR_S,
                                group_name, PARAMETERS_FILE_EXT);
  if (g_unlink (param_file) != 0)
    {
      g_warning ("HyScanDBFile: can't remove file %s", param_file);
      goto exit;
    }

  g_atomic_int_inc (&project_info->mod_count);

  status = TRUE;

exit:
  g_rw_lock_writer_unlock (&priv->lock);
  g_free (param_file);

  return status;
}

/* Функция открывает параметры галса. */
static gint32
hyscan_db_file_track_param_open (HyScanDB    *db,
                                 gint32       track_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileTrackInfo *track_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  HyScanDBParamFile *params = NULL;

  gchar *param_file = NULL;
  gchar *schema_file = NULL;
  gint32 id = -1;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    goto exit;

  /* Генерация нового идентификатора открываемого объекта. */
  id = hyscan_db_file_create_id (priv);
  if (id < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      goto exit;
    }

  /* Ищем галс в списке открытых параметров.  Если нашли - используем один объект доступа к параметрам. */
  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = "*";
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
  if (param_info != NULL)
    params = g_object_ref (param_info->param);

  /* Полное имя параметров галса. */
  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = TRACK_PARAMETERS_ID;

  /* Ищем группу параметров в списке открытых.  Если нашли - используем. */
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
  if (param_info != NULL)
    {
      param_info->ref_count += 1;
    }

  /* Открываем группу параметров для работы. */
  else
    {
      param_file = g_build_filename (track_info->path, TRACK_PARAMETERS_FILE, NULL);
      schema_file = g_build_filename (track_info->path, TRACK_SCHEMA_FILE, NULL);
      param_info = g_new (HyScanDBFileParamInfo, 1);
      param_info->ref_count = 1;
      param_info->mod_count = 1;
      param_info->project_name = g_strdup (track_info->project_name);
      param_info->track_name = g_strdup (track_info->track_name);
      param_info->group_name = g_strdup (TRACK_GROUP_ID);
      param_info->object_name = g_strdup (TRACK_PARAMETERS_ID);
      if (track_info->wid == track_id)
        param_info->track_object_wid = id;
      else
        param_info->track_object_wid = -1;
      param_info->channel_object_wid = -1;
      if (params == NULL)
        param_info->param = hyscan_db_param_file_new (param_file, schema_file);
      else
        param_info->param = params;
      g_free (schema_file);
      g_free (param_file);
    }

  g_hash_table_insert (priv->params, GINT_TO_POINTER (id), param_info);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return id;
}

/* Функция открывает параметры канала данных. */
static gint32
hyscan_db_file_channel_param_open (HyScanDB    *db,
                                   gint32       channel_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  HyScanDBParamFile *params = NULL;

  gchar *param_file = NULL;
  gchar *schema_file = NULL;
  gint32 id = -1;

  g_rw_lock_writer_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info == NULL)
    goto exit;

  /* Генерация нового идентификатора открываемого объекта. */
  id = hyscan_db_file_create_id (priv);
  if (id < 0)
    {
      g_warning ("HyScanDBFile: too many open objects");
      goto exit;
    }

  /* Ищем галс в списке открытых параметров.  Если нашли - используем один объект доступа к параметрам. */
  object_info.project_name = channel_info->project_name;
  object_info.track_name = channel_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = "*";
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
  if (param_info != NULL)
    params = g_object_ref (param_info->param);

  /* Полное имя параметров канала данных. */
  object_info.project_name = channel_info->project_name;
  object_info.track_name = channel_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = channel_info->channel_name;

  /* Ищем группу параметров в списке открытых.  Если нашли - используем. */
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
  if (param_info != NULL)
    {
      param_info->ref_count += 1;
    }

  /* Открываем группу параметров для работы. */
  else
    {
      param_file = g_build_filename (channel_info->path, TRACK_PARAMETERS_FILE, NULL);
      schema_file = g_build_filename (channel_info->path, TRACK_SCHEMA_FILE, NULL);
      param_info = g_new (HyScanDBFileParamInfo, 1);
      param_info->ref_count = 1;
      param_info->mod_count = 1;
      param_info->project_name = g_strdup (channel_info->project_name);
      param_info->track_name = g_strdup (channel_info->track_name);
      param_info->group_name = g_strdup (TRACK_GROUP_ID);
      param_info->object_name = g_strdup (channel_info->channel_name);
      param_info->track_object_wid = -1;
      if (channel_info->wid == channel_id)
        param_info->channel_object_wid = id;
      else
        param_info->channel_object_wid = -1;
      if (params == NULL)
        param_info->param = hyscan_db_param_file_new (param_file, schema_file);
      else
        param_info->param = params;
      g_free (schema_file);
      g_free (param_file);
    }

  g_hash_table_insert (priv->params, GINT_TO_POINTER (id), param_info);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return id;
}

/* Функция возвращает список объектов в группе параметров. */
static gchar **
hyscan_db_file_param_object_list (HyScanDB *db,
                                  gint32    param_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  gchar **list = NULL;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info != NULL)
    list = hyscan_db_param_file_object_list (param_info->param);

  g_rw_lock_reader_unlock (&priv->lock);

  return list;
}

/* Функция создаёт объект в группе параметров. */
static gboolean
hyscan_db_file_param_object_create (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *object_name,
                                    const gchar *schema_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  /* Проверяем название объекта. */
  if (!hyscan_db_file_check_name (object_name, TRUE))
    return status;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    goto exit;

  /* Создавать объекты в группе параметров галса нельзя. */
  if (g_strcmp0 (param_info->track_name, "") != 0)
    goto exit;

  status = hyscan_db_param_file_object_create (param_info->param, object_name, schema_id);
  if (status)
    g_atomic_int_inc (&param_info->mod_count);

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция переименовывает объект в группе параметров. */
static gboolean
hyscan_db_file_param_object_remove (HyScanDB    *db,
                                    gint32       param_id,
                                    const gchar *object_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info != NULL)
    goto exit;

  /* Удалять объекты в группе параметров галса нельзя. */
  if (g_strcmp0 (param_info->track_name, "") != 0)
    goto exit;

  status = hyscan_db_param_file_object_remove (param_info->param, object_name);
  if (status)
    g_atomic_int_inc (&param_info->mod_count);

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция возвращает список объектов в группе параметров. */
static HyScanDataSchema *
hyscan_db_file_param_object_get_schema (HyScanDB    *db,
                                        gint32       param_id,
                                        const gchar *object_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  HyScanDataSchema *schema = NULL;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    goto exit;

  /* Проверки для параметров объектов галса - если название галса не пустое. */
  if (param_info->track_name[0] != 0)
    {
      /* Имена объектов в параметрах галсов и каналов данных выбираются автоматически. */
      if (object_name != NULL)
        goto exit;

      /* Все параметры галсов хранятся в группе параметров TRACK_GROUP_ID. */
      if (g_strcmp0 (param_info->group_name, TRACK_GROUP_ID) != 0)
        goto exit;

      /* Имя объекта с параметрами. */
      object_name = param_info->object_name;
    }

  schema = hyscan_db_param_file_object_get_schema (param_info->param, object_name);
  g_object_ref (schema);

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return schema;
}

static gboolean
hyscan_db_file_param_set (HyScanDB            *db,
                          gint32               param_id,
                          const gchar         *object_name,
                          const gchar *const  *param_names,
                          GVariant           **param_values)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    goto exit;

  /* Проверки для параметров объектов галса - если название галса не пустое. */
  if (param_info->track_name[0] != 0)
    {
      /* Имена объектов в параметрах галсов и каналов данных выбираются автоматически. */
      if (object_name != NULL)
        goto exit;

      /* Все параметры галсов хранятся в группе параметров TRACK_GROUP_ID. */
      if (g_strcmp0 (param_info->group_name, TRACK_GROUP_ID) != 0)
        goto exit;

      /* Параметры объекта галса. */
      if (g_strcmp0 (param_info->object_name, TRACK_PARAMETERS_ID) == 0)
        {
          /* Проверяем права на изменение параметров объекта галса. */
          if (param_info->track_object_wid != param_id)
            goto exit;
        }

      /* Параметры объектов каналов данных. */
      else
        {
          /* Проверяем права на изменение параметров объекта канала данных. */
          if (param_info->channel_object_wid != param_id)
            goto exit;
        }

      /* Имя объекта с параметрами. */
      object_name = param_info->object_name;
    }

  status = hyscan_db_param_file_set (param_info->param, object_name, param_names, param_values);
  if (status)
    g_atomic_int_inc (&param_info->mod_count);

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

static gboolean
hyscan_db_file_param_get (HyScanDB            *db,
                          gint32               param_id,
                          const gchar         *object_name,
                          const gchar *const  *param_names,
                          GVariant           **param_values)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileParamInfo *param_info;
  gboolean status = FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    goto exit;

  /* Проверки для параметров объектов галса - если название галса не пустое. */
  if (param_info->track_name[0] != 0)
    {
      /* Имена объектов в параметрах галсов и каналов данных выбираются автоматически. */
      if (object_name != NULL)
        goto exit;

      /* Все параметры галсов хранятся в группе параметров TRACK_GROUP_ID. */
      if (g_strcmp0 (param_info->group_name, TRACK_GROUP_ID) != 0)
        goto exit;

      /* Имя объекта с параметрами. */
      object_name = param_info->object_name;
    }

  status = hyscan_db_param_file_get (param_info->param, object_name, param_names, param_values);

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Внутренняя функция закрытия проекта. */
static gboolean
hyscan_db_file_project_close (HyScanDBFilePrivate *priv,
                              gint32               project_id)
{
  HyScanDBFileProjectInfo *project_info;

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    return FALSE;

  g_hash_table_remove (priv->projects, GINT_TO_POINTER (project_id));

  return TRUE;
}

/* Внутренняя функция закрытия галса. */
static gboolean
hyscan_db_file_track_close (HyScanDBFilePrivate *priv,
                            gint32               track_id)
{
  HyScanDBFileTrackInfo *track_info;

  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    return FALSE;

  g_hash_table_remove (priv->tracks, GINT_TO_POINTER (track_id));

  return TRUE;
}

/* Внутренняя функция закрытия канала данных. */
static gboolean
hyscan_db_file_channel_close (HyScanDBFilePrivate *priv,
                              gint32               channel_id)
{
  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info == NULL)
    return FALSE;

  /* Закрыли дескриптор с правами на запись. */
  if (channel_info->wid == channel_id)
    {
      object_info.project_name = channel_info->project_name;
      object_info.track_name = channel_info->track_name;
      object_info.group_name = TRACK_GROUP_ID;
      object_info.object_name = channel_info->channel_name;

      param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
      if (param_info != NULL)
        param_info->channel_object_wid = -1;

      channel_info->wid = -1;
    }

  /* Удаляем канал данных из списка. */
  g_hash_table_remove (priv->channels, GINT_TO_POINTER (channel_id));

  return TRUE;
}

/* Внутренняя функция закрытия группы параметров. */
static gboolean
hyscan_db_file_param_close (HyScanDBFilePrivate *priv,
                            gint32               param_id)
{
  HyScanDBFileParamInfo *param_info;

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    return FALSE;

  /* Запрещаем запись. */
  if (param_info->track_object_wid == param_id)
    param_info->track_object_wid = -1;

  /* Удаляем группу параметров из списка. */
  g_hash_table_remove (priv->params, GINT_TO_POINTER (param_id));

  return TRUE;
}

/* Функция закрывает объект базы данных. */
static void
hyscan_db_file_close (HyScanDB *db,
                      gint32    id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  g_rw_lock_writer_lock (&priv->lock);

  if (hyscan_db_file_project_close (priv, id))
    goto exit;

  if (hyscan_db_file_track_close (priv, id))
    goto exit;

  if (hyscan_db_file_channel_close (priv, id))
    goto exit;

  if (hyscan_db_file_param_close (priv, id))
    goto exit;

exit:
  g_rw_lock_writer_unlock (&priv->lock);
}

static void
hyscan_db_file_interface_init (HyScanDBInterface *iface)
{
  iface->get_uri = hyscan_db_file_get_uri;
  iface->get_mod_count = hyscan_db_file_get_mod_count;
  iface->is_exist = hyscan_db_file_is_exist;

  iface->project_list = hyscan_db_file_project_list;
  iface->project_open = hyscan_db_file_project_open;
  iface->project_create = hyscan_db_file_project_create;
  iface->project_remove = hyscan_db_file_project_remove;
  iface->project_get_ctime = hyscan_db_file_project_get_ctime;
  iface->project_param_list = hyscan_db_file_project_param_list;
  iface->project_param_open = hyscan_db_file_project_param_open;
  iface->project_param_remove = hyscan_db_file_project_param_remove;

  iface->track_list = hyscan_db_file_track_list;
  iface->track_open = hyscan_db_file_track_open;
  iface->track_create = hyscan_db_file_track_create;
  iface->track_remove = hyscan_db_file_track_remove;
  iface->track_get_ctime = hyscan_db_file_track_get_ctime;
  iface->track_param_open = hyscan_db_file_track_param_open;

  iface->channel_list = hyscan_db_file_channel_list;
  iface->channel_open = hyscan_db_file_channel_open;
  iface->channel_create = hyscan_db_file_channel_create;
  iface->channel_remove = hyscan_db_file_channel_remove;
  iface->channel_finalize = hyscan_db_file_channel_finalize;
  iface->channel_is_writable = hyscan_db_file_channel_is_writable;
  iface->channel_param_open = hyscan_db_file_channel_param_open;

  iface->channel_set_chunk_size = hyscan_db_file_channel_set_chunk_size;
  iface->channel_set_save_time = hyscan_db_file_channel_set_save_time;
  iface->channel_set_save_size = hyscan_db_file_channel_set_save_size;

  iface->channel_get_data_range = hyscan_db_file_channel_get_data_range;
  iface->channel_add_data = hyscan_db_file_channel_add_data;
  iface->channel_get_data = hyscan_db_file_channel_get_data;
  iface->channel_find_data = hyscan_db_file_channel_find_data;

  iface->param_object_list = hyscan_db_file_param_object_list;
  iface->param_object_create = hyscan_db_file_param_object_create;
  iface->param_object_remove = hyscan_db_file_param_object_remove;
  iface->param_object_get_schema = hyscan_db_file_param_object_get_schema;

  iface->param_set = hyscan_db_file_param_set;
  iface->param_get = hyscan_db_file_param_get;

  iface->close = hyscan_db_file_close;
}
