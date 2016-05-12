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
#include <stdarg.h>

enum
{
  PROP_O,
  PROP_PARAM_FILE,
  PROP_SCHEMA_FILE
};

struct _HyScanDBParamFilePrivate
{
  gchar               *param_file;     /* Имя файла параметров. */
  gchar               *schema_file;    /* Имя файла схемы параметров. */

  GHashTable          *schemas;        /* Список используемых схем. */
  GHashTable          *objects;        /* Список объектов. */
  GKeyFile            *params;         /* Параметры. */

  GFile               *fd;             /* Объект работы с файлом параметров. */
  GOutputStream       *ofd;            /* Поток записи файла параметров. */

  GMutex               lock;           /* Блокировка многопоточного доступа. */
};

static void                    hyscan_db_param_file_set_property       (GObject          *object,
                                                                        guint             prop_id,
                                                                        const GValue     *value,
                                                                        GParamSpec       *pspec);
static void                    hyscan_db_param_file_object_constructed (GObject          *object);
static void                    hyscan_db_param_file_object_finalize    (GObject          *object);

static HyScanDataSchema       *hyscan_db_param_file_schema_lookup      (GHashTable       *schemas,
                                                                        const gchar      *schema_file,
                                                                        const gchar      *schema_id);

static gboolean                hyscan_db_param_file_params_flush       (GKeyFile         *params,
                                                                        GOutputStream    *ofd);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBParamFile, hyscan_db_param_file, G_TYPE_OBJECT)

static void
hyscan_db_param_file_class_init (HyScanDBParamFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_param_file_set_property;

  object_class->constructed = hyscan_db_param_file_object_constructed;
  object_class->finalize = hyscan_db_param_file_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAM_FILE,
                                   g_param_spec_string ("param-file", "ParamFile", "Parameters file name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SCHEMA_FILE,
                                   g_param_spec_string ("schema-file", "SchemaFile", "Schema file name", NULL,
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
    case PROP_PARAM_FILE:
      priv->param_file = g_value_dup_string (value);
      break;

    case PROP_SCHEMA_FILE:
      priv->schema_file = g_value_dup_string (value);
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

  g_mutex_init (&priv->lock);

  priv->params = g_key_file_new ();
  priv->schemas = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  /* Если файл с параметрами существует, загружаем его. */
  error = NULL;
  if (!g_key_file_load_from_file (priv->params, priv->param_file, G_KEY_FILE_NONE, &error))
    {
      /* Если произошла ошибка при загрузке параметров не связанная с существованием файла,
         сигнализируем о ней. */
      if (error->code != G_FILE_ERROR_NOENT)
        {
          g_warning ("HyScanDBParamFile: %s: %s", priv->param_file, error->message);
          g_clear_pointer (&priv->params, g_key_file_unref);
        }
      g_error_free (error);
    }

  /* Объект для записи содержимого файла параметров. */
  priv->fd = g_file_new_for_path (priv->param_file);
  priv->ofd = G_OUTPUT_STREAM (g_file_append_to (priv->fd, G_FILE_CREATE_NONE, NULL, NULL));
}

static void
hyscan_db_param_file_object_finalize (GObject *object)
{
  HyScanDBParamFile *param = HYSCAN_DB_PARAM_FILE (object);
  HyScanDBParamFilePrivate *priv = param->priv;

  g_clear_object (&priv->ofd);
  g_clear_object (&priv->fd);

  g_clear_pointer (&priv->params, g_key_file_free);
  g_clear_pointer (&priv->schemas, g_hash_table_unref);

  g_free (priv->schema_file);
  g_free (priv->param_file);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_db_param_file_parent_class)->finalize (G_OBJECT (object));
}

/* Функция ищет схему данных schema_id и если не находит загружает её. */
static HyScanDataSchema *
hyscan_db_param_file_schema_lookup (GHashTable       *schemas,
                                    const gchar      *schema_file,
                                    const gchar      *schema_id)
{
  HyScanDataSchema *schema;

  schema = g_hash_table_lookup (schemas, schema_id);
  if (schema != NULL)
    return schema;

  schema = hyscan_data_schema_new_from_file (schema_file, schema_id, NULL);
  if (schema != NULL)
    g_hash_table_insert (schemas, g_strdup (schema_id), schema);

  return schema;
}

/* Функция записывает значения параметров в INI файл. */
static gboolean
hyscan_db_param_file_params_flush (GKeyFile      *params,
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
  else
    g_error_free (error);

  return FALSE;
}

/* Функция создаёт новый объект HyScanDBParamFile. */
HyScanDBParamFile *
hyscan_db_param_file_new (const gchar *param_file,
                          const gchar *schema_file)
{
  return g_object_new (HYSCAN_TYPE_DB_PARAM_FILE, "param-file", param_file,
                                                  "schema-file", schema_file,
                                                  NULL);
}

/* Функция возвращает список объектов. */
gchar **
hyscan_db_param_file_object_list (HyScanDBParamFile *param)
{
  HyScanDBParamFilePrivate *priv;

  gchar **list;
  gsize n_objects;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), NULL);

  priv = param->priv;

  g_mutex_lock (&priv->lock);
  list = g_key_file_get_groups (priv->params, &n_objects);
  g_mutex_unlock (&priv->lock);

  if (n_objects == 0)
    g_clear_pointer (&list, g_strfreev);

  return list;
}

/* Функция возвращает указатель на объект HyScanDataSchema для объекта. */
HyScanDataSchema *
hyscan_db_param_file_object_get_schema (HyScanDBParamFile *param,
                                        const gchar       *object_name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), NULL);

  priv = param->priv;

  g_mutex_lock (&priv->lock);

  if (priv->ofd == NULL || priv->params == NULL)
    goto exit;

  /* Используемая схема */
  schema_id = g_key_file_get_string (priv->params, object_name, "schema-id", NULL);
  if (schema_id == NULL)
    goto exit;

  schema = hyscan_db_param_file_schema_lookup (priv->schemas, priv->schema_file, schema_id);

exit:
  g_mutex_unlock (&priv->lock);
  g_free (schema_id);

  return schema;
}

/* Функция создаёт объект с указанным идентификатором схемы. */
gboolean
hyscan_db_param_file_object_create (HyScanDBParamFile *param,
                                    const gchar       *object_name,
                                    const gchar       *schema_id)
{
  HyScanDBParamFilePrivate *priv;

  HyScanDataSchema *schema = NULL;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  g_mutex_lock (&priv->lock);

  if (priv->ofd == NULL || priv->params == NULL)
    goto exit;

  /* Проверяем существование объекта. */
  if (g_key_file_has_group (priv->params, object_name))
    goto exit;

  /* Используемая схема */
  schema = hyscan_db_param_file_schema_lookup (priv->schemas, priv->schema_file, schema_id);
  if (schema == NULL)
    {
      g_warning ("HyScanDBParamFile: unknown schema id: '%s'", schema_id);
      goto exit;
    }

  /* Создаём объект с указанной схемой данных. */
  g_key_file_set_string (priv->params, object_name, "schema-id", schema_id);

  status = hyscan_db_param_file_params_flush (priv->params, priv->ofd);
  if (!status)
    {
      g_clear_pointer (&priv->params, g_key_file_free);
      g_clear_object (&priv->ofd);
      g_clear_object (&priv->fd);
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция удаляет объект. */
gboolean
hyscan_db_param_file_object_remove (HyScanDBParamFile *param,
                                    const gchar       *object_name)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  g_mutex_lock (&priv->lock);

  if (priv->ofd == NULL || priv->params == NULL)
    goto exit;

  /* Объект не существует. */
  if (!g_key_file_has_group (priv->params, object_name))
    goto exit;

  /* Используемая схема */
  schema_id = g_key_file_get_string (priv->params, object_name, "schema-id", NULL);
  if (schema_id == NULL)
    goto exit;
  schema = hyscan_db_param_file_schema_lookup (priv->schemas, priv->schema_file, schema_id);
  if (schema == NULL)
    goto exit;

  /* Удаляем объект. */
  g_key_file_remove_group (priv->params, object_name, NULL);

  status = hyscan_db_param_file_params_flush (priv->params, priv->ofd);
  if (!status)
    {
      g_clear_pointer (&priv->params, g_key_file_free);
      g_clear_object (&priv->ofd);
      g_clear_object (&priv->fd);
    }

exit:
  g_mutex_unlock (&priv->lock);
  g_free (schema_id);

  return status;
}

/* Функция устанавливает значение параметра. */
gboolean
hyscan_db_param_file_set (HyScanDBParamFile    *param,
                          const gchar          *object_name,
                          const gchar          *param_name,
                          HyScanDataSchemaType  type,
                          gconstpointer         value,
                          gint32                size)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  g_mutex_lock (&priv->lock);

  if (priv->ofd == NULL || priv->params == NULL)
    goto exit;

  /* Используемая схема */
  schema_id = g_key_file_get_string (priv->params, object_name, "schema-id", NULL);
  if (schema_id == NULL)
    goto exit;
  schema = hyscan_db_param_file_schema_lookup (priv->schemas, priv->schema_file, schema_id);
  if (schema == NULL)
    goto exit;

  /* Сверяем тип параметра. */
  if (type != hyscan_data_schema_key_get_type (schema, param_name))
    goto exit;

  /* Доступность на запись. */
  if (hyscan_data_schema_key_is_readonly (schema, param_name))
    goto exit;

  /* Сбрасываем значение параметра до состояния по умолчанию. */
  if (value == NULL && size == 0)
    {
      g_key_file_remove_key (priv->params, object_name, param_name, NULL);
    }

  /* Устанавливаем значение параметра. */
  else
    {
      switch (type)
        {
        case HYSCAN_DATA_SCHEMA_TYPE_BOOLEAN:
          if (size != sizeof (gboolean))
            goto exit;
          g_key_file_set_boolean (priv->params, object_name, param_name, *((gboolean*)value));
          break;

        case HYSCAN_DATA_SCHEMA_TYPE_INTEGER:
          if (size != sizeof (gint64))
            goto exit;
          if (!hyscan_data_schema_key_check_integer (schema, param_name, *((gint64*)value)))
            goto exit;
          g_key_file_set_int64 (priv->params, object_name, param_name, *((gint64*)value));
          break;

        case HYSCAN_DATA_SCHEMA_TYPE_DOUBLE:
          if (size != sizeof (gint64))
            goto exit;
          if (!hyscan_data_schema_key_check_double (schema, param_name, *((gdouble*)value)))
            goto exit;
          g_key_file_set_double (priv->params, object_name, param_name, *((gdouble*)value));
          break;

        case HYSCAN_DATA_SCHEMA_TYPE_STRING:
          if (size != strlen (value) + 1)
            goto exit;
          g_key_file_set_string (priv->params, object_name, param_name, value);
          break;

        case HYSCAN_DATA_SCHEMA_TYPE_ENUM:
          if (size != sizeof (gint64))
            goto exit;
          if (!hyscan_data_schema_key_check_enum (schema, param_name, *((gint64*)value)))
            goto exit;
          g_key_file_set_int64 (priv->params, object_name, param_name, *((gint64*)value));
          break;

        default:
          goto exit;
        }
    }

  status = hyscan_db_param_file_params_flush (priv->params, priv->ofd);
  if (!status)
    {
      g_clear_pointer (&priv->params, g_key_file_free);
      g_clear_object (&priv->ofd);
      g_clear_object (&priv->fd);
    }

exit:
  g_mutex_unlock (&priv->lock);
  g_free (schema_id);

  return status;
}

/* Функция считывает значение параметра. */
gboolean
hyscan_db_param_file_get (HyScanDBParamFile     *param,
                          const gchar           *object_name,
                          const gchar           *param_name,
                          HyScanDataSchemaType   type,
                          gpointer               buffer,
                          gint32                *buffer_size)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;

  gchar *string;
  gint32 string_length;

  gboolean use_default = FALSE;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  priv = param->priv;

  g_mutex_lock (&priv->lock);

  if (priv->ofd == NULL || priv->params == NULL)
    goto exit;

  /* Используемая схема */
  schema_id = g_key_file_get_string (priv->params, object_name, "schema-id", NULL);
  if (schema_id == NULL)
    goto exit;
  schema = hyscan_db_param_file_schema_lookup (priv->schemas, priv->schema_file, schema_id);
  if (schema == NULL)
    goto exit;

  /* Сверяем тип параметра. */
  if (type != hyscan_data_schema_key_get_type (schema, param_name))
    goto exit;

  /* Проверяем наличие установленного значения. */
  if (!g_key_file_has_key (priv->params, object_name, param_name, NULL))
    use_default = TRUE;

  /* Для параметров "только для чтения" возвращаем значение по умолчанию. */
  if (hyscan_data_schema_key_is_readonly (schema, param_name))
    use_default = TRUE;

  /* Считываем значение параметра. */
  switch (type)
    {
    case HYSCAN_DATA_SCHEMA_TYPE_BOOLEAN:
      if (buffer == NULL)
        {
          *buffer_size = sizeof (gboolean);
          break;
        }
      if (*buffer_size != sizeof (gboolean))
        goto exit;
      if (use_default)
        {
          hyscan_data_schema_key_get_default_boolean (schema, param_name, buffer);
          break;
        }
      *((gboolean*)buffer) = g_key_file_get_boolean (priv->params, object_name, param_name, NULL);
      break;

    case HYSCAN_DATA_SCHEMA_TYPE_INTEGER:
      if (buffer == NULL)
        {
          *buffer_size = sizeof (gint64);
          break;
        }
      if (*buffer_size != sizeof (gint64))
        goto exit;
      if (use_default)
        {
          hyscan_data_schema_key_get_default_integer (schema, param_name, buffer);
          break;
        }
      *((gint64*)buffer) = g_key_file_get_int64 (priv->params, object_name, param_name, NULL);
      break;

    case HYSCAN_DATA_SCHEMA_TYPE_DOUBLE:
      if (buffer == NULL)
        {
          *buffer_size = sizeof (gdouble);
          break;
        }
      if (*buffer_size != sizeof (gdouble))
        goto exit;
      if (use_default)
        {
          hyscan_data_schema_key_get_default_double (schema, param_name, buffer);
          break;
        }
      *((gdouble*)buffer) = g_key_file_get_double (priv->params, object_name, param_name, NULL);
      break;

    case HYSCAN_DATA_SCHEMA_TYPE_STRING:
      if (use_default)
        string = g_strdup (hyscan_data_schema_key_get_default_string (schema, param_name));
      else
        string = g_key_file_get_string (priv->params, object_name, param_name, NULL);
      string_length = strlen (string) + 1;
      if (buffer == NULL)
        {
          *buffer_size = string_length;
          g_free (string);
          break;
        }
      *buffer_size = (*buffer_size > string_length) ? string_length : *buffer_size;
      string [*buffer_size - 1] = 0;
      memcpy (buffer, string, *buffer_size);
      g_free (string);
      break;

    case HYSCAN_DATA_SCHEMA_TYPE_ENUM:
      if (buffer == NULL)
        {
          *buffer_size = sizeof (gint64);
          break;
        }
      if (*buffer_size != sizeof (gint64))
        goto exit;
      if (use_default)
        {
          hyscan_data_schema_key_get_default_enum (schema, param_name, buffer);
          break;
        }
      *((gint64*)buffer) = g_key_file_get_int64 (priv->params, object_name, param_name, NULL);
      break;

    default:
      goto exit;
    }

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);
  g_free (schema_id);

  return status;
}
