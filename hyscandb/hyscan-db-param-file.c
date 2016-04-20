/*
 * \file hyscan-db-param-file.c
 *
 * \brief Исходный файл класса хранения параметров в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-param-file.h"

#include <gio/gio.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_PATH,
  PROP_NAME,
};

/* Внутренние данные объекта. */
struct _HyScanDBParamFilePrivate
{
  gchar               *path;           /* Путь к каталогу с файлом параметров. */
  gchar               *name;           /* Название файла параметров. */
  gchar               *file;           /* Полное имя файла параметров. */

  gboolean             fail;           /* Признак ошибки. */

  GMutex               lock;           /* Блокировка многопоточного доступа. */

  GKeyFile            *params;         /* Параметры. */
  GFile               *fd;             /* Объект работы с файлом параметров. */
  GOutputStream       *ofd;            /* Поток записи файла параметров. */
};

static void            hyscan_db_param_file_set_property       (GObject          *object,
                                                                guint             prop_id,
                                                                const GValue     *value,
                                                                GParamSpec       *pspec);
static void            hyscan_db_param_file_object_constructed (GObject          *object);
static void            hyscan_db_param_file_object_finalize    (GObject          *object);

static gboolean        hyscan_db_param_file_parse_name         (const gchar      *name,
                                                                gchar           **group,
                                                                gchar           **key);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBParamFile, hyscan_db_param_file, G_TYPE_OBJECT)

static void
hyscan_db_param_file_class_init (HyScanDBParamFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_param_file_set_property;

  object_class->constructed = hyscan_db_param_file_object_constructed;
  object_class->finalize = hyscan_db_param_file_object_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
                                   g_param_spec_string ("path", "Path", "Path to parameters group", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NAME,
                                   g_param_spec_string ("name", "Name", "Parameters group name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_param_file_init (HyScanDBParamFile *param)
{
  param->priv = hyscan_db_param_file_get_instance_private (param);
}

static void
hyscan_db_param_file_set_property (GObject          *object,
                                   guint             prop_id,
                                   const GValue     *value,
                                   GParamSpec       *pspec)
{
  HyScanDBParamFile *param = HYSCAN_DB_PARAM_FILE (object);
  HyScanDBParamFilePrivate *priv = param->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_PATH:
      priv->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_param_file_object_constructed (GObject *object)
{
  HyScanDBParamFile *param = HYSCAN_DB_PARAM_FILE (object);
  HyScanDBParamFilePrivate *priv = param->priv;

  GError *error;

  /* Начальные значения. */
  priv->fail = FALSE;
  priv->fd = NULL;
  priv->ofd = NULL;

  g_mutex_init (&priv->lock);

  /* Параметры. */
  priv->params = g_key_file_new ();

  /* Имя файла с параметрами. */
  priv->file = g_strdup_printf ("%s%s%s.ini", priv->path, G_DIR_SEPARATOR_S, priv->name);

  /* Если файл с параметрами существует, загружаем его. */
  error = NULL;
  if (!g_key_file_load_from_file (priv->params, priv->file, G_KEY_FILE_NONE, &error))
    {
      /* Если произошла ошибка при загрузке параметров не связанная с существованием файла,
         сигнализируем о ней. */
      if (error->code != G_FILE_ERROR_NOENT)
        {
          g_warning ("HyScanDBParamFile: %s: %s", priv->file, error->message);
          priv->fail = TRUE;
        }
      g_error_free (error);
    }

  /* Объект для записи содержимого файла параметров. */
  priv->fd = g_file_new_for_path (priv->file);
  priv->ofd = G_OUTPUT_STREAM (g_file_append_to (priv->fd, G_FILE_CREATE_NONE, NULL, NULL));
}

static void
hyscan_db_param_file_object_finalize (GObject *object)
{
  HyScanDBParamFile *param = HYSCAN_DB_PARAM_FILE (object);
  HyScanDBParamFilePrivate *priv = param->priv;

  g_free (priv->file);
  g_free (priv->name);
  g_free (priv->path);

  g_key_file_free (priv->params);

  if (priv->ofd != NULL)
    g_object_unref (priv->ofd);
  if (priv->fd != NULL)
    g_object_unref (priv->fd);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_db_param_file_parent_class)->finalize (G_OBJECT (object));
}

/* Функция разбирает имя параметра на две составляющие: группа и ключ. */
static gboolean
hyscan_db_param_file_parse_name (const gchar *name,
                                 gchar      **group,
                                 gchar      **key)
{
  gchar **path = g_strsplit (name, ".", 0);

  /* Имя без группы - добавляем имя группы 'default'. */
  if (g_strv_length (path) == 1)
    {
      path[1] = path[0];
      path[0] = g_strdup ("default");
    }
  /* Проверяем, что имя представленно в виде 'группа.ключ'. */
  else if (g_strv_length (path) != 2 || strlen (path[0]) + strlen (path[1]) != strlen (name) - 1)
    {
      g_warning ("HyScanDBParamFile: syntax error in key name %s", name);
      g_strfreev (path);
      return FALSE;
    }

  *group = path[0];
  *key = path[1];

  g_free (path);

  return TRUE;
}

/* Функция записывает текущие параметры в файл. */
static gboolean
hyscan_db_param_file_flush_params (GKeyFile      *params,
                                   GOutputStream *ofd)
{
  gchar *data;
  gsize dsize;

  GError *error;

  data = g_key_file_to_data (params, &dsize, NULL);

  /* Обнуляем файл. */
  error = NULL;
  if (!g_seekable_truncate (G_SEEKABLE (ofd), 0, NULL, &error))
    {
      g_warning ("HyScanDBParamFile: %s", error->message);
      goto exit;
    }

  /* Записываем в него новые данные. */
  error = NULL;
  if (g_output_stream_write (ofd, data, dsize, NULL, &error) != dsize)
    {
      if (error != NULL)
        g_warning ("HyScanDBParamFile: %s", error->message);
      else
        g_warning ("HyScanDBParamFile: can't flush parameters");
      goto exit;
    }

  /* Очищаем IO буфер. */
  error = NULL;
  if (!g_output_stream_flush (ofd, NULL, &error))
    {
      g_warning ("HyScanDBParamFile: %s", error->message);
      goto exit;
    }

exit:

  g_free (data);
  if (error == NULL)
    return TRUE;

  g_error_free (error);
  return FALSE;
}

/* Функция создаёт новый объект HyScanDBParamFile. */
HyScanDBParamFile *
hyscan_db_param_file_new (const gchar *path,
                          const gchar *name)
{
  return g_object_new (HYSCAN_TYPE_DB_PARAM_FILE, "path", path, "name", name, NULL);
}

/* Функция возвращает список параметров. */
gchar **
hyscan_db_param_file_get_param_list (HyScanDBParamFile *param)
{
  HyScanDBParamFilePrivate *priv;

  gchar **names = NULL;
  gchar **groups;
  gchar **keys;

  gint i, j, k;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), NULL);

  priv = param->priv;

  if (priv->fail)
    return NULL;

  g_mutex_lock (&priv->lock);

  /* Группы параметров. */
  groups = g_key_file_get_groups (priv->params, NULL);
  for (i = 0, k = 0; groups != NULL && groups[i] != NULL; i++)
    {
      /* Параметры в группе. */
      keys = g_key_file_get_keys (priv->params, groups[i], NULL, NULL);
      for (j = 0; keys != NULL && keys[j] != NULL; j++)
        {
          /* Список параметров. */
          names = g_realloc (names, 16 * (((k + 1) / 16) + 1) * sizeof (gchar *));
          names[k] = g_strdup_printf ("%s.%s", groups[i], keys[j]);
          k++;
          names[k] = NULL;
        }
      g_strfreev (keys);
    }
  g_strfreev (groups);

  g_mutex_unlock (&priv->lock);

  return names;
}

/* Функция удаляет параметры по маске. */
gboolean
hyscan_db_param_file_remove_param (HyScanDBParamFile *param,
                                   const gchar       *mask)
{
  HyScanDBParamFilePrivate *priv;

  GPatternSpec *pattern;
  gchar **groups;
  gchar **keys;

  gboolean status;
  gint i, j;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return FALSE;

  pattern = g_pattern_spec_new (mask);
  if (pattern == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Группы параметров. */
  groups = g_key_file_get_groups (priv->params, NULL);
  for (i = 0; groups != NULL && groups[i] != NULL; i++)
    {
      /* Параметры в группе. */
      keys = g_key_file_get_keys (priv->params, groups[i], NULL, NULL);
      for (j = 0; keys != NULL && keys[j] != NULL; j++)
        {
          /* Проверяем имя параметра. */
          gchar *name = g_strdup_printf ("%s.%s", groups[i], keys[j]);

          /* Удаляем если оно совпадает с шаблоном. */
          if (g_pattern_match (pattern, (guint)strlen (name), name, NULL))
            g_key_file_remove_key (priv->params, groups[i], keys[j], NULL);

          g_free (name);
        }
      g_strfreev (keys);

      /* Если группа стала пустой - удаляем её. */
      keys = g_key_file_get_keys (priv->params, groups[i], NULL, NULL);
      if (keys != NULL && g_strv_length (keys) == 0)
        g_key_file_remove_group (priv->params, groups[i], NULL);
      g_strfreev (keys);
    }
  g_strfreev (groups);

  status = hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_pattern_spec_free (pattern);

  return status;
}

/* Функция проверяет существование указанного параметра. */
gboolean
hyscan_db_param_file_has_param (HyScanDBParamFile *param,
                                const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return status;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return status;

  g_mutex_lock (&priv->lock);

  status = g_key_file_has_key (priv->params, group, key, NULL);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return status;
}

/* Функция увеличивает значение параметра типа integer на единицу. */
gint64
hyscan_db_param_file_inc_integer (HyScanDBParamFile *param,
                                  const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gint64 value = 0;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), 0);

  priv = param->priv;

  if (priv->fail)
    return value;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return value;

  g_mutex_lock (&priv->lock);

  value = g_key_file_get_int64 (priv->params, group, key, NULL) + 1;
  g_key_file_set_int64 (priv->params, group, key, value);
  hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return value;
}

/* Функция устанавливает значение параметра типа integer. */
gboolean
hyscan_db_param_file_set_integer (HyScanDBParamFile *param,
                                  const gchar       *name,
                                  gint64             value)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return FALSE;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_key_file_set_int64 (priv->params, group, key, value);
  status = hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return status;
}

/* Функция устанавливает значение параметра типа double. */
gboolean
hyscan_db_param_file_set_double (HyScanDBParamFile *param,
                                 const gchar       *name,
                                 gdouble            value)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return FALSE;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_key_file_set_double (priv->params, group, key, value);
  status = hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return status;
}

/* Функция устанавливает значение параметра типа boolean. */
gboolean
hyscan_db_param_file_set_boolean (HyScanDBParamFile *param,
                                  const gchar       *name,
                                  gboolean           value)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return FALSE;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_key_file_set_boolean (priv->params, group, key, value);
  status = hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return status;
}

/* Функция устанавливает значение параметра типа string. */
gboolean
hyscan_db_param_file_set_string (HyScanDBParamFile *param,
                                 const gchar       *name,
                                 const gchar       *value)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return FALSE;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_key_file_set_string (priv->params, group, key, value);
  status = hyscan_db_param_file_flush_params (priv->params, priv->ofd);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return status;
}

/* Функция возвращает значение параметра типа integer. */
gint64
hyscan_db_param_file_get_integer (HyScanDBParamFile *param,
                                  const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gint64 value = 0;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), 0);

  priv = param->priv;

  if (priv->fail)
    return value;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return value;

  g_mutex_lock (&priv->lock);

  value = g_key_file_get_int64 (priv->params, group, key, NULL);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return value;
}

/* Функция возвращает значение параметра типа double. */
gdouble
hyscan_db_param_file_get_double (HyScanDBParamFile *param,
                                 const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gdouble value = 0.0;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), 0.0);

  priv = param->priv;

  if (priv->fail)
    return value;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return value;

  g_mutex_lock (&priv->lock);

  value = g_key_file_get_double (priv->params, group, key, NULL);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return value;
}


/* Функция возвращает значение параметра типа boolean. */
gboolean
hyscan_db_param_file_get_boolean (HyScanDBParamFile *param,
                                  const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gboolean value = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  if (priv->fail)
    return value;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return value;

  g_mutex_lock (&priv->lock);

  value = g_key_file_get_boolean (priv->params, group, key, NULL);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return value;
}


/* Функция возвращает значение параметра типа string. */
gchar *
hyscan_db_param_file_get_string (HyScanDBParamFile *param,
                                 const gchar       *name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *group, *key;
  gchar *value = NULL;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), NULL);

  priv = param->priv;

  if (priv->fail)
    return value;

  if (!hyscan_db_param_file_parse_name (name, &group, &key))
    return value;

  g_mutex_lock (&priv->lock);

  value = g_key_file_get_string (priv->params, group, key, NULL);

  g_mutex_unlock (&priv->lock);

  g_free (group);
  g_free (key);

  return value;
}
