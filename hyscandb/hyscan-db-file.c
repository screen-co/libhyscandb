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

#ifdef G_OS_UNIX
#include <sys/file.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define DB_LOCK_FILE           "hyscan.db"             /* Название файла блокировки доступа к системе хранения. */
#define PROJECT_ID_FILE        "project.id"            /* Название файла идентификатора проекта. */
#define PROJECT_SCHEMA_FILE    "project.sch"           /* Название файла со схемой данных проекта. */
#define PROJECT_PARAMETERS_DIR "project.prm"           /* Название каталога для хранения параметров проекта. */
#define TRACK_ID_FILE          "track.id"              /* Название файла идентификатора галса. */
#define TRACK_SCHEMA_FILE      "track.sch"             /* Название файла со схемой данных. */
#define TRACK_PARAMETERS_FILE  "track.prm"             /* Название файла с параметрами галса. */
#define TRACK_GROUP_ID         "parameters"            /* Название группы параметров галса. */
#define TRACK_PARAMETERS_ID    "track"                 /* Название объекта с параметрами галса. */
#define PARAMETERS_FILE_EXT    "prm"                   /* Расширения для файлов со значениями параметров. */

#define PROJECT_FILE_MAGIC     0x52505348              /* HSPR в виде строки. */
#define TRACK_FILE_MAGIC       0x52545348              /* HSTR в виде строки. */
#define FILE_VERSION           0x31303731              /* 1701 в виде строки. */

/* Свойства HyScanDBFile. */
enum
{
  PROP_O,
  PROP_PATH
};

/* Стуктура файла - метки проекта и галса. */
typedef struct
{
  guint32              magic;                  /* Идентификатор файла. */
  guint32              version;                /* Версия API системы хранения. */
  gint64               ctime;                  /* Дата создания. */
} HyScanDBFileID;

/* Информация о проекте. */
typedef struct
{
  gint                 ref_count;              /* Число ссылок на объект. */
  guint                mod_count;              /* Номер изменения объекта. */

  gchar               *project_name;           /* Название проекта. */
  gchar               *path;                   /* Путь к каталогу с проектом. */
  gchar               *param_path;             /* Путь к каталогу с параметрами. */

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
  gint64               ctime;                  /* Время создания канала данных. */
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

  gchar               *flock_name;             /* Имя файла блокировки. */
#ifdef G_OS_UNIX
  FILE                *flock;                  /* Дескриптор файла блокировки. */
#endif
#ifdef G_OS_WIN32
  HANDLE               flock;                  /* Дескриптор файла блокировки */
#endif
  gboolean             flocked;                /* Признак блокировки доступа. */

  GHashTable          *projects;               /* Список открытых проектов. */
  GHashTable          *tracks;                 /* Список открытых галсов. */
  GHashTable          *channels;               /* Список открытых каналов данных. */
  GHashTable          *params;                 /* Список открытых групп параметров. */

  GMutex               lock;                   /* Блокировка многопоточного доступа. */
};

static void            hyscan_db_file_interface_init           (HyScanDBInterface     *iface);
static void            hyscan_db_file_set_property             (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_db_file_object_constructed       (GObject               *object);
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

static gboolean        hyscan_db_file_id_test                  (const gchar           *path,
                                                                guint32                magic,
                                                                gint64                *ctime);
static gboolean        hyscan_db_channel_test                  (const gchar           *path,
                                                                const gchar           *name);

static gchar         **hyscan_db_file_get_directory_param_list (const gchar           *path);

static gboolean        hyscan_db_file_remove_directory         (const gchar           *path);

G_DEFINE_TYPE_WITH_CODE (HyScanDBFile, hyscan_db_file, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDBFile)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DB, hyscan_db_file_interface_init));

static void
hyscan_db_file_class_init (HyScanDBFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_file_set_property;

  object_class->constructed = hyscan_db_file_object_constructed;
  object_class->finalize = hyscan_db_file_object_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
                                   g_param_spec_string ("path", "Path", "Path to projects", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_file_init (HyScanDBFile *dbf)
{
  dbf->priv = hyscan_db_file_get_instance_private (dbf);
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
hyscan_db_file_object_constructed (GObject *object)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (object);
  HyScanDBFilePrivate *priv = dbf->priv;

#ifdef G_OS_WIN32
  wchar_t *wflock_name;
  DWORD written;
  OVERLAPPED overlapped = {0};
#endif

  priv->projects = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_project_info);
  priv->tracks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_track_info);
  priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_channel_info);
  priv->params = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, hyscan_db_remove_param_info);

  g_mutex_init (&priv->lock);

  priv->flock_name = g_build_filename (priv->path, DB_LOCK_FILE, NULL);

#ifdef G_OS_UNIX
  priv->flock = fopen (priv->flock_name, "a");
  if (priv->flock == NULL)
    {
      g_warning ("HyScanDBFile: can't create lock on db directory '%s'", priv->path);
      return;
    }

  if (flock (fileno (priv->flock), LOCK_EX | LOCK_NB) != 0)
    {
      g_clear_pointer (&priv->flock, fclose);
      g_warning ("HyScanDBFile: can't lock db directory '%s'", priv->path);
      return;
    }

  priv->flocked = TRUE;
#endif

#ifdef G_OS_WIN32
  wflock_name = g_utf8_to_utf16 (priv->flock_name, -1, NULL, NULL, NULL);
  priv->flock = CreateFileW (wflock_name,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  g_free (wflock_name);

  if (priv->flock == INVALID_HANDLE_VALUE)
    {
      g_warning ("HyScanDBFile: can't create lock on db directory '%s'", priv->path);
      return;
    }

  if (!WriteFile (priv->flock, "HYSCAN", 6, &written, NULL) || (written != 6))
    {
      CloseHandle (priv->flock);
      g_warning ("HyScanDBFile: can't write to lock file %s'", priv->path);
      return;
    }

  overlapped.Offset = 0;
  overlapped.OffsetHigh = 0;
  if (!LockFileEx (priv->flock, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 6, 0, &overlapped))
    {
      CloseHandle (priv->flock);
      g_warning ("HyScanDBFile: can't lock db directory '%s'", priv->path);
      return;
    }

  priv->flocked = TRUE;
#endif
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

  g_mutex_clear (&priv->lock);

#ifdef G_OS_UNIX
  if (priv->flocked)
    fclose (priv->flock);
#endif

#ifdef G_OS_WIN32
  if (priv->flocked)
    {
      OVERLAPPED overlapped = {0};

      overlapped.Offset = 0;
      overlapped.OffsetHigh = 0;
      UnlockFileEx (priv->flock, 0, 6, 0, &overlapped);

      CloseHandle (priv->flock);
    }
#endif

  g_free (priv->path);
  g_free (priv->flock_name);

  G_OBJECT_CLASS (hyscan_db_file_parent_class)->finalize (object);
}

/* Функция проверяет имя на валидность. */
static gboolean
hyscan_db_file_check_name (const gchar *name,
                           gboolean     allow_slash)
{
  gint i;

  /* Проверка на спец имена. */
  if (g_strcmp0 (name, PROJECT_ID_FILE) == 0)
    return FALSE;
  if (g_strcmp0 (name, PROJECT_PARAMETERS_DIR) == 0)
    return FALSE;

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
  g_free (project_info->param_path);

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

/* Функция проверяет, что каталог path содержит проект или галс. */
static gboolean
hyscan_db_file_id_test (const gchar *path,
                        guint32      magic,
                        gint64      *ctime)
{
  gboolean status = FALSE;
  HyScanDBFileID *id = NULL;
  gchar *file = NULL;
  gsize size;

  /* Загружаем файл с информацияей о проекте или галсе. */
  if (magic == PROJECT_FILE_MAGIC)
    file = g_build_filename (path, PROJECT_ID_FILE, NULL);
  else if (magic == TRACK_FILE_MAGIC)
    file = g_build_filename (path, TRACK_ID_FILE, NULL);
  else
    goto exit;

  if (!g_file_get_contents (file, (gpointer)&id, &size, NULL))
    goto exit;

  if (size != sizeof (HyScanDBFileID))
    goto exit;

  /* Проверяем версию API файловой базы данных. */
  if ((GUINT32_FROM_LE (id->magic) != magic) ||
      (GUINT32_FROM_LE (id->version) != FILE_VERSION))
    goto exit;

  /* Время создания проекта. */
  if (ctime != NULL)
    *ctime = GUINT64_FROM_LE (id->ctime);

  /* Этот каталог содержит проект. */
  status = TRUE;

exit:
  g_free (file);
  g_free (id);

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
  GArray *params;
  const gchar *name;

  /* Открываем каталог с группами параметров. */
  if ((dir = g_dir_open (path, 0, NULL)) == NULL)
    {
      g_warning ("HyScanDBFile: can't open project parameters directory '%s'", path);
      return NULL;
    }

  params = g_array_new (TRUE, TRUE, sizeof (gchar *));

  /* Выбираем все файлы с расширением PARAMETERS_FILE_EXT. */
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *sub_path;
      gchar *group_name;
      gchar **splited_group_name;

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
      splited_group_name = g_strsplit (name, ".", 2);
      if (splited_group_name == NULL)
        continue;

      /* Список групп параметров. */
      if (g_strcmp0 (splited_group_name[1], PARAMETERS_FILE_EXT) == 0)
        {
          group_name = g_strdup (splited_group_name[0]);
          g_array_append_val (params, group_name);
        }

      g_strfreev (splited_group_name);
    }

  g_dir_close (dir);

  if (params->len == 0)
    {
      g_array_unref (params);
      return NULL;
    }

  return (gchar**)g_array_free (params, FALSE);
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

/* Функция возвращает путь к базе данных. */
static gchar *
hyscan_db_file_get_uri (HyScanDB *db)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  GFile *path;
  gchar *uri;

  if (!priv->flocked)
    return NULL;

  path = g_file_new_for_path (priv->path);
  uri = g_file_get_uri (path);
  g_object_unref (path);

  return uri;
}

/* Функция возвращает номер изменения в объекте. */
static guint32
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

  if (!priv->flocked)
    return 0;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Проверяем существование проекта. */
  project_path = g_build_filename (priv->path, project_name, NULL);
  if (!hyscan_db_file_id_test (project_path, PROJECT_FILE_MAGIC, NULL))
    goto exit;

  if (track_name == NULL && channel_name == NULL)
    {
      exist = TRUE;
      goto exit;
    }

  /* Проверяем существование галса. */
  track_path = g_build_filename (project_path, track_name, NULL);
  if (!hyscan_db_file_id_test (track_path, TRACK_FILE_MAGIC, NULL))
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
  g_mutex_unlock (&priv->lock);
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
  GArray *projects;
  const gchar *project_name;

  if (!priv->flocked)
    return NULL;

  /* Открываем каталог с проектами. */
  if ((db_dir = g_dir_open (priv->path, 0, NULL)) == NULL)
    {
      g_warning ("HyScanDBFile: can't open project directory '%s'", priv->path);
      return NULL;
    }

  projects = g_array_new (TRUE, TRUE, sizeof (gchar *));

  g_mutex_lock (&priv->lock);

  /* Проверяем все найденые каталоги - содержат они проект или нет. */
  while ((project_name = g_dir_read_name (db_dir)) != NULL)
    {
      gchar *project_path;
      gboolean status;

      /* Проверяем содержит каталог проект или нет. */
      project_path = g_build_filename (priv->path, project_name, NULL);
      status = hyscan_db_file_id_test (project_path, PROJECT_FILE_MAGIC, NULL);
      g_free (project_path);

      if (!status)
        continue;

      /* Список проектов. */
      project_name = g_strdup (project_name);
      g_array_append_val (projects, project_name);
    }

  g_mutex_unlock (&priv->lock);

  g_dir_close (db_dir);

  if (projects->len == 0)
    {
      g_array_unref (projects);
      return NULL;
    }

  return (gchar**)g_array_free (projects, FALSE);
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

  if (!priv->flocked)
    return -1;

  g_mutex_lock (&priv->lock);

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
      if (!hyscan_db_file_id_test (project_path, PROJECT_FILE_MAGIC, &ctime))
        {
          g_warning ("HyScanDBFile: '%s' no such project", project_name);
          goto exit;
        }

      project_info = g_new (HyScanDBFileProjectInfo, 1);
      project_info->ref_count = 1;
      project_info->mod_count = 1;
      project_info->project_name = g_strdup (project_name);
      project_info->path = project_path;
      project_info->param_path = g_build_filename (project_path, PROJECT_PARAMETERS_DIR, NULL);
      project_info->ctime = ctime;
      project_path = NULL;
      id = nid;
    }

  g_hash_table_insert (priv->projects, GINT_TO_POINTER (id), project_info);

exit:
  g_mutex_unlock (&priv->lock);

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

  gboolean exist = FALSE;
  gboolean status = FALSE;

  gint64 ctime;
  HyScanDBFileID id;
  gchar *project_path = NULL;
  gchar *param_path = NULL;
  gchar *project_param_file = NULL;
  gchar *project_schema_file = NULL;

  if (!priv->flocked)
    return -1;

  /* Проверяем название проекта. */
  if (!hyscan_db_file_check_name (project_name, FALSE))
    return -1;

  g_mutex_lock (&priv->lock);

  ctime = g_get_real_time () / G_USEC_PER_SEC;
  id.magic = GUINT32_TO_LE (PROJECT_FILE_MAGIC);
  id.version = GUINT32_TO_LE (FILE_VERSION);
  id.ctime = GUINT64_TO_LE (ctime);
  project_path = g_build_filename (priv->path, project_name, NULL);
  param_path = g_build_filename (project_path, PROJECT_PARAMETERS_DIR, NULL);
  project_param_file = g_build_filename (priv->path, project_name, PROJECT_ID_FILE, NULL);
  project_schema_file = g_build_filename (param_path, PROJECT_SCHEMA_FILE, NULL);

  /* Проверяем, что каталога с названием проекта нет. */
  if (g_file_test (project_path, G_FILE_TEST_IS_DIR))
    {
      exist = TRUE;
      g_info ("HyScanDBFile: project '%s' already exists", project_name);
      goto exit;
    }

  /* Создаём каталог для проекта. */
  if (g_mkdir_with_parents (param_path, 0777) != 0)
    {
      g_warning ("HyScanDBFile: can't create project '%s' directory", project_name);
      goto exit;
    }

  /* Файл идентификации проекта. */
  if (!g_file_set_contents (project_param_file, (gpointer)&id, sizeof (HyScanDBFileID), NULL))
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
  g_mutex_unlock (&priv->lock);

  g_free (project_schema_file);
  g_free (project_param_file);
  g_free (project_path);
  g_free (param_path);

  if (status)
    return hyscan_db_file_project_open (db, project_name);

  if (exist)
    return 0;

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  if (!(status = hyscan_db_file_id_test (project_path, PROJECT_FILE_MAGIC, NULL)))
    {
      g_warning ("HyScanDBFile: '%s' not a project", project_name);
      goto exit;
    }

  /* Удаляем каталог с проектом. */
  status = hyscan_db_file_remove_directory (project_path);
  g_atomic_int_inc (&priv->mod_count);

exit:
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info != NULL)
    ctime = g_date_time_new_from_unix_local (project_info->ctime);

  g_mutex_unlock (&priv->lock);

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

  GDir *db_dir;
  GArray *tracks;
  const gchar *track_name;

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  tracks = g_array_new (TRUE, TRUE, sizeof (gchar *));

  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  /* Открываем каталог проекта с галсами. */
  if ((db_dir = g_dir_open (project_info->path, 0, NULL)) == NULL)
    {
      g_warning ("HyScanDBFile: can't open project directory '%s'", project_info->path);
      goto exit;
    }

  /* Проверяем все найденые каталоги - содержат они галс или нет. */
  while ((track_name = g_dir_read_name (db_dir)) != NULL)
    {
      gboolean status = FALSE;
      gchar *track_path;

      /* Проверяем содержит каталог галс или нет. */
      track_path = g_build_filename (project_info->path, track_name, NULL);
      status = hyscan_db_file_id_test (track_path, TRACK_FILE_MAGIC, NULL);
      g_free (track_path);

      if (!status)
        continue;

      /* Список галсов. */
      track_name = g_strdup (track_name);
      g_array_append_val (tracks, track_name);
    }

  g_dir_close (db_dir);

exit:
  g_mutex_unlock (&priv->lock);

  if (tracks->len == 0)
    {
      g_array_unref (tracks);
      return NULL;
    }

  return (gchar**)g_array_free (tracks, FALSE);
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
      if (!hyscan_db_file_id_test (track_path, TRACK_FILE_MAGIC, &ctime))
        {
          g_warning ("HyScanDBFile: '%s.%s' - no such track", project_info->project_name, track_name);
          g_free (track_path);
          return -1;
        }

      track_info = g_new (HyScanDBFileTrackInfo, 1);
      track_info->ref_count = 1;
      track_info->mod_count = 1;
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

  if (!priv->flocked)
    return -1;

  g_mutex_lock (&priv->lock);
  id = hyscan_db_file_open_track_int (db, project_id, track_name, TRUE);
  g_mutex_unlock (&priv->lock);

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
  HyScanDBParamFile *params = NULL;

  gint32 track_id = -1;

  gint64 ctime;
  HyScanDBFileID id;
  gchar *track_path = NULL;
  gchar *track_id_file = NULL;
  gchar *track_param_file = NULL;
  gchar *track_schema_file = NULL;

  if (!priv->flocked)
    return -1;

  /* Проверяем название галса. */
  if (!hyscan_db_file_check_name (track_name, FALSE))
    return -1;

  g_mutex_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info == NULL)
    goto exit;

  ctime = g_get_real_time () / G_USEC_PER_SEC;
  id.magic = GUINT32_TO_LE (TRACK_FILE_MAGIC);
  id.version = GUINT32_TO_LE (FILE_VERSION);
  id.ctime = GUINT64_TO_LE (ctime);
  track_path = g_build_filename (project_info->path, track_name, NULL);
  track_id_file = g_build_filename (project_info->path, track_name, TRACK_ID_FILE, NULL);
  track_param_file = g_build_filename (project_info->path, track_name, TRACK_PARAMETERS_FILE, NULL);
  track_schema_file = g_build_filename (project_info->path, track_name, TRACK_SCHEMA_FILE, NULL);

  /* Проверяем, что каталога с названием галса нет. */
  if (g_file_test (track_path, G_FILE_TEST_IS_DIR))
    {
      track_id = 0;
      g_info ("HyScanDBFile: track '%s.%s' already exists", project_info->project_name, track_name);
      goto exit;
    }

  /* Создаём каталог для галса. */
  if (g_mkdir_with_parents (track_path, 0777) != 0)
    {
      g_warning ("HyScanDBFile: can't create track '%s.%s' directory",
                 project_info->project_name, track_name);
      goto exit;
    }

  /* Файл идентификатор галса. */
  if (!g_file_set_contents (track_id_file, (gpointer)&id, sizeof (HyScanDBFileID), NULL))
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
      if (!hyscan_db_param_file_object_create (params, TRACK_PARAMETERS_ID, schema_id))
        goto exit;
    }

  /* Открываем галс. */
  track_id = hyscan_db_file_open_track_int (db, project_id, track_name, FALSE);

  g_atomic_int_inc (&project_info->mod_count);

exit:
  g_mutex_unlock (&priv->lock);

  g_clear_object (&params);

  g_free (track_schema_file);
  g_free (track_param_file);
  g_free (track_id_file);
  g_free (track_path);

  return track_id;
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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  if (!(status = hyscan_db_file_id_test (track_path, TRACK_FILE_MAGIC, NULL)))
    {
      g_warning ("HyScanDBFile: '%s.%s' not a track", project_info->project_name, track_name);
      goto exit;
    }

  /* Удаляем каталог с галсом. */
  status = hyscan_db_file_remove_directory (track_path);
  g_atomic_int_inc (&project_info->mod_count);

exit:
  g_mutex_unlock (&priv->lock);

  g_free (track_path);

  return status;
}

/* Функция возвращает информацию о дате и времени создания галса. */
static GDateTime *
hyscan_db_file_track_get_ctime (HyScanDB *db,
                                gint32    track_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileTrackInfo *track_info;
  GDateTime *ctime = NULL;

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info != NULL)
    ctime = g_date_time_new_from_unix_local (track_info->ctime);

  g_mutex_unlock (&priv->lock);

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

  GHashTableIter iter;
  gpointer key, value;

  GDir *db_dir;
  GArray *channels;
  const gchar *file_name;

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  channels = g_array_new (TRUE, TRUE, sizeof (gchar *));

  /* Ищем галс в списке открытых. */
  track_info = g_hash_table_lookup (priv->tracks, GINT_TO_POINTER (track_id));
  if (track_info == NULL)
    goto exit;

  /* Открываем каталог галса. */
  if ((db_dir = g_dir_open (track_info->path, 0, NULL)) == NULL)
    {
      g_warning ("HyScanDBFile: can't open track directory '%s'", track_info->path);
      goto exit;
    }

  /* Проверяем все найденые файлы на совпадение с именем name.000000.d */
  while ((file_name = g_dir_read_name (db_dir)) != NULL)
    {
      gchar **splited_channel_name;
      gchar *channel_name;
      gboolean status;

      splited_channel_name = g_strsplit (file_name, ".", 2);
      if (splited_channel_name == NULL)
        continue;

      /* Если совпадение найдено проверяем, что существует канал с именем name. */
      if (g_strcmp0 (splited_channel_name[1], "000000.d") == 0)
        status = hyscan_db_channel_test (track_info->path, splited_channel_name[0]);
      else
        status = FALSE;

      /* Список каналов. */
      if (status)
        {
          channel_name = g_strdup (splited_channel_name[0]);
          g_array_append_val (channels, channel_name);
        }

      g_strfreev (splited_channel_name);
    }

  /* Добавляем в список каналов созданные, но ещё пустые. */
  g_hash_table_iter_init (&iter, priv->channels);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanDBFileChannelInfo *channel_info;
      gboolean skip_channel;
      gchar *channel_name;
      guint i;

      /* Каналы в текущем галсе. */
      channel_info = value;
      if (g_strcmp0 (track_info->track_name, channel_info->track_name) != 0)
        continue;

      /* Пропускаем канал, если он уже есть в списке. */
      for (i = 0, skip_channel = FALSE; i < channels->len; i++)
        {
          channel_name = g_array_index (channels, gchar*, i);
          if (g_strcmp0 (channel_name, channel_info->channel_name) == 0)
            {
              skip_channel = TRUE;
              break;
            }
        }

      if (skip_channel)
        continue;

      channel_name = g_strdup (channel_info->channel_name);
      g_array_append_val (channels, channel_name);
    }

  g_dir_close (db_dir);

exit:
  g_mutex_unlock (&priv->lock);

  if (channels->len == 0)
    {
      g_array_unref (channels);
      return NULL;
    }

  return (gchar**)g_array_free (channels, FALSE);
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

  g_mutex_lock (&priv->lock);

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
          /* Если канал уже открыт и находится в режиме записи - сообщаем об этом. */
          id = 0;
          g_info ("HyScanDBFile: channel '%s.%s.%s' already exists",
                  track_info->project_name, track_info->track_name, channel_name);
          goto exit;
        }
    }

  /* Открываем канал для работы. */
  else
    {
      if (hyscan_db_channel_test (track_info->path, channel_name) != readonly)
        {
          if (readonly)
            {
              /* Если канала нет и запрашивается его открытие на чтение - ошибка. */
              g_info ("HyScanDBFile: '%s.%s.%s' - no such channel",
                      track_info->project_name, track_info->track_name, channel_name);
            }
          else
            {
              /* Если канал есть и запрашивается открытие на запись - сообщаем об этом. */
              id = 0;
              g_info ("HyScanDBFile: channel '%s.%s.%s' already exists",
                      track_info->project_name, track_info->track_name, channel_name);
            }
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
      channel_info->ctime = hyscan_db_channel_file_get_ctime (channel_info->channel);
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

          object_info.project_name = track_info->project_name;
          object_info.track_name = track_info->track_name;
          object_info.group_name = TRACK_GROUP_ID;
          object_info.object_name = "*";

          /* Если кто-то уже использует параметры галса, создаём через этот объект
           * или создаём временный. */
          param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
          if (param_info != NULL)
            {
              param = g_object_ref (param_info->param);
            }
          else
            {
              gchar *track_param_file = g_build_filename (track_info->path, TRACK_PARAMETERS_FILE, NULL);
              gchar *track_schema_file = g_build_filename (track_info->path, TRACK_SCHEMA_FILE, NULL);

              param = hyscan_db_param_file_new (track_param_file, track_schema_file);

              g_free (track_schema_file);
              g_free (track_param_file);
            }

          /* Создаём объект в группе параметров с именем канала данных и указанной схемой. */
          hyscan_db_param_file_object_create (param, channel_name, schema_id);
          g_object_unref (param);
        }

      id = nid;
    }

  g_hash_table_insert (priv->channels, GINT_TO_POINTER (id), channel_info);

exit:
  g_mutex_unlock (&priv->lock);

  return id;
}

/* Функция открывает существующий канал данных. */
static gint32
hyscan_db_file_channel_open (HyScanDB    *db,
                             gint32       track_id,
                             const gchar *channel_name)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);

  if (!dbf->priv->flocked)
    return -1;

  return hyscan_db_file_open_channel_int (db, track_id, channel_name, NULL, TRUE);
}

/* Функция создаёт канал данных. */
static gint32
hyscan_db_file_channel_create (HyScanDB    *db,
                               gint32       track_id,
                               const gchar *channel_name,
                               const gchar *schema_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);

  if (!dbf->priv->flocked)
    return -1;

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
  HyScanDBFileParamInfo *param_info;
  HyScanDBFileObjectInfo object_info;

  HyScanDBParamFile *param = NULL;

  gboolean status = FALSE;

  GHashTableIter iter;
  gpointer key, value;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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

  /* Удаляем объект в группе параметров с именем канала данных. */
  object_info.project_name = track_info->project_name;
  object_info.track_name = track_info->track_name;
  object_info.group_name = TRACK_GROUP_ID;
  object_info.object_name = "*";

  /* Если кто-то уже использует параметры галса, удаляем через этот объект
   * или создаём временный. */
  param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
  if (param_info != NULL)
    {
      param = g_object_ref (param_info->param);
    }
  else
    {
      gchar *param_file = g_build_filename (track_info->path, TRACK_PARAMETERS_FILE, NULL);
      gchar *schema_file = g_build_filename (track_info->path, TRACK_SCHEMA_FILE, NULL);

      param = hyscan_db_param_file_new (param_file, schema_file);

      g_free (param_file);
      g_free (schema_file);
    }

  hyscan_db_param_file_object_remove (param, channel_name);
  g_object_unref (param);

  /* Удаляем файлы канала данных. */
  status = hyscan_db_channel_remove_channel_files (track_info->path, channel_name);
  g_atomic_int_inc (&track_info->mod_count);

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция возвращает информацию о дате и времени создания канала данных. */
static GDateTime *
hyscan_db_file_channel_get_ctime (HyScanDB *db,
                                  gint32    channel_id)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  GDateTime *ctime = NULL;

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    ctime = g_date_time_new_from_unix_local (channel_info->ctime);

  g_mutex_unlock (&priv->lock);

  return ctime;
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

  if (!priv->flocked)
    return;

  g_mutex_lock (&priv->lock);

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

  g_mutex_unlock (&priv->lock);
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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));

  /* Проверяем режим доступа. */
  if ((channel_info != NULL) && (channel_info->wid > 0))
    writable = TRUE;

  g_mutex_unlock (&priv->lock);

  return writable;
}

/* Функция устанавливает максимальный размер файла данных канала. */
static gboolean
hyscan_db_file_channel_set_chunk_size (HyScanDB *db,
                                       gint32    channel_id,
                                       guint64   chunk_size)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_chunk_size (channel_info->channel, chunk_size);

  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_save_time (channel_info->channel, save_time);

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает максимальный объём сохраняемых данных. */
static gboolean
hyscan_db_file_channel_set_save_size (HyScanDB *db,
                                      gint32    channel_id,
                                      guint64   save_size)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_set_channel_save_size (channel_info->channel, save_size);

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция возвращает диапазон текущих значений индексов данных. */
static gboolean
hyscan_db_file_channel_get_data_range (HyScanDB *db,
                                       gint32    channel_id,
                                       guint32  *first_index,
                                       guint32  *last_index)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_get_channel_data_range (channel_info->channel, first_index, last_index);

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает новые данные канала. */
static gboolean
hyscan_db_file_channel_add_data (HyScanDB      *db,
                                 gint32         channel_id,
                                 gint64         time,
                                 gconstpointer  data,
                                 guint32        size,
                                 guint32       *index)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL && channel_info->wid == channel_id)
    {
      status = hyscan_db_channel_file_add_channel_data (channel_info->channel, time, data, size, index);
      if (status)
        g_atomic_int_inc (&channel_info->mod_count);
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция считывает данные. */
static gboolean
hyscan_db_file_channel_get_data (HyScanDB *db,
                                 gint32    channel_id,
                                 guint32   index,
                                 gpointer  buffer,
                                 guint32  *buffer_size,
                                 gint64   *time)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  gboolean status = FALSE;

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_get_channel_data (channel_info->channel, index, buffer, buffer_size, time);

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция ищет данные по метке времени. */
static gboolean
hyscan_db_file_channel_find_data (HyScanDB *db,
                                  gint32    channel_id,
                                  gint64    time,
                                  guint32  *lindex,
                                  guint32  *rindex,
                                  gint64   *ltime,
                                  gint64   *rtime)
{
  HyScanDBFile *dbf = HYSCAN_DB_FILE (db);
  HyScanDBFilePrivate *priv = dbf->priv;

  HyScanDBFileChannelInfo *channel_info;
  HyScanDBFindStatus status = HYSCAN_DB_FIND_FAIL;

  if (!priv->flocked)
    return HYSCAN_DB_FIND_FAIL;

  g_mutex_lock (&priv->lock);

  /* Ищем канал данных в списке открытых. */
  channel_info = g_hash_table_lookup (priv->channels, GINT_TO_POINTER (channel_id));
  if (channel_info != NULL)
    status = hyscan_db_channel_file_find_channel_data (channel_info->channel, time, lindex, rindex, ltime, rtime);

  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Ищем проект в списке открытых. */
  project_info = g_hash_table_lookup (priv->projects, GINT_TO_POINTER (project_id));
  if (project_info != NULL)
    list = hyscan_db_file_get_directory_param_list (project_info->param_path);

  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return -1;

  /* Проверяем название группы параметров. */
  if (!hyscan_db_file_check_name (group_name, FALSE))
    return id;

  g_mutex_lock (&priv->lock);

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
      param_file = g_strdup_printf ("%s%s%s.%s", project_info->param_path, G_DIR_SEPARATOR_S,
                                                 group_name, PARAMETERS_FILE_EXT);
      schema_file = g_build_filename (project_info->param_path, PROJECT_SCHEMA_FILE, NULL);
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
      if (hyscan_db_param_file_is_new (param_info->param))
        g_atomic_int_inc (&project_info->mod_count);
    }

  g_hash_table_insert (priv->params, GINT_TO_POINTER (id), param_info);

exit:
  g_mutex_unlock (&priv->lock);
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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  param_file = g_strdup_printf ("%s%s%s.%s", project_info->param_path, G_DIR_SEPARATOR_S,
                                group_name, PARAMETERS_FILE_EXT);
  if (g_unlink (param_file) != 0)
    {
      g_warning ("HyScanDBFile: can't remove file %s", param_file);
      goto exit;
    }

  g_atomic_int_inc (&project_info->mod_count);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);
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

  if (!priv->flocked)
    return -1;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return -1;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info != NULL)
    list = hyscan_db_param_file_object_list (param_info->param);

  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  /* Проверяем название объекта. */
  if (!hyscan_db_file_check_name (object_name, TRUE))
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем группу параметров в списке открытых. */
  param_info = g_hash_table_lookup (priv->params, GINT_TO_POINTER (param_id));
  if (param_info == NULL)
    goto exit;

  /* Удалять объекты в группе параметров галса нельзя. */
  if (g_strcmp0 (param_info->track_name, "") != 0)
    goto exit;

  status = hyscan_db_param_file_object_remove (param_info->param, object_name);
  if (status)
    g_atomic_int_inc (&param_info->mod_count);

exit:
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return NULL;

  g_mutex_lock (&priv->lock);

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
  if (schema != NULL)
    g_object_ref (schema);

exit:
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

  if (!priv->flocked)
    return FALSE;

  g_mutex_lock (&priv->lock);

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
  g_mutex_unlock (&priv->lock);

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

      /* Если в канал так и не записали данные, удаляем его параметры и закрываем все дескрипторы. */
      if (!hyscan_db_channel_file_get_channel_data_range (channel_info->channel, NULL, NULL))
        {
          HyScanDBParamFile *param;

          gchar *project_name;
          gchar *track_name;
          gchar *channel_name;

          GHashTableIter iter;
          gpointer key, value;

          object_info.project_name = channel_info->project_name;
          object_info.track_name = channel_info->track_name;
          object_info.group_name = TRACK_GROUP_ID;
          object_info.object_name = "*";

          /* Если кто-то уже использует параметры галса, удаляем через этот объект
           * или создаём временный. */
          param_info = g_hash_table_find (priv->params, hyscan_db_check_param_by_object_name, &object_info);
          if (param_info != NULL)
            {
              param = g_object_ref (param_info->param);
            }
          else
            {
              gchar *param_file = g_build_filename (channel_info->path, TRACK_PARAMETERS_FILE, NULL);
              gchar *schema_file = g_build_filename (channel_info->path, TRACK_SCHEMA_FILE, NULL);

              param = hyscan_db_param_file_new (param_file, schema_file);

              g_free (param_file);
              g_free (schema_file);
            }

          hyscan_db_param_file_object_remove (param, channel_info->channel_name);
          g_object_unref (param);

          project_name = g_strdup (channel_info->project_name);
          track_name = g_strdup (channel_info->track_name);
          channel_name = g_strdup (channel_info->channel_name);
          object_info.project_name = project_name;
          object_info.track_name = track_name;
          object_info.group_name = channel_name;

          /* Ищем канал в списке открытых, при необходимости закрываем. */
          g_hash_table_iter_init (&iter, priv->channels);
          while (g_hash_table_iter_next (&iter, &key, &value))
            {
              if (!hyscan_db_check_channel_by_object_name (key, value, &object_info))
                continue;

              g_hash_table_iter_remove (&iter);
            }

          g_free (project_name);
          g_free (track_name);
          g_free (channel_name);

          return TRUE;
        }

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

  if (!priv->flocked)
    return;

  g_mutex_lock (&priv->lock);

  if (hyscan_db_file_project_close (priv, id))
    goto exit;

  if (hyscan_db_file_track_close (priv, id))
    goto exit;

  if (hyscan_db_file_channel_close (priv, id))
    goto exit;

  if (hyscan_db_file_param_close (priv, id))
    goto exit;

exit:
  g_mutex_unlock (&priv->lock);
}

HyScanDBFile *
hyscan_db_file_new (const gchar *path)
{
  HyScanDBFile *db;

  db = g_object_new (HYSCAN_TYPE_DB_FILE, "path", path, NULL);
  if (!db->priv->flocked)
    g_clear_object (&db);

  return db;
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
  iface->channel_get_ctime = hyscan_db_file_channel_get_ctime;
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
