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

  gboolean             new_file;       /* Признак нового файла. */
  GFile               *fd;             /* Объект работы с файлом параметров. */
  GOutputStream       *ofd;            /* Поток записи файла параметров. */

  GMutex               lock;           /* Блокировка многопоточного доступа. */
};

static void                    hyscan_db_param_file_set_property       (GObject                 *object,
                                                                        guint                    prop_id,
                                                                        const GValue            *value,
                                                                        GParamSpec              *pspec);
static void                    hyscan_db_param_file_object_constructed (GObject                 *object);
static void                    hyscan_db_param_file_object_finalize    (GObject                 *object);

static HyScanDataSchema       *hyscan_db_param_file_schema_lookup      (GHashTable              *schemas,
                                                                        const gchar             *schema_file,
                                                                        const gchar             *schema_id);

static gboolean                hyscan_db_param_file_compare_types      (HyScanDataSchemaKeyType  param_type,
                                                                        GVariantClass            value_type);

static gboolean                hyscan_db_param_file_params_flush       (GKeyFile                *params,
                                                                        GOutputStream           *ofd);

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
          g_warning ("HyScanDBParamFile: can't load parameters file '%s'", priv->param_file);
          g_clear_pointer (&priv->params, g_key_file_unref);
        }
      else
        {
          priv->new_file = TRUE;
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

  schema = hyscan_data_schema_new_from_file (schema_file, schema_id);
  if (schema != NULL)
    g_hash_table_insert (schemas, g_strdup (schema_id), schema);

  return schema;
}

/* Функция сравнивает тип параметра схемы и тип значения GVariant. */
static gboolean
hyscan_db_param_file_compare_types (HyScanDataSchemaKeyType param_type,
                                    GVariantClass           value_type)
{
  if (param_type == HYSCAN_DATA_SCHEMA_KEY_INVALID)
    return FALSE;

  switch (param_type)
    {
    case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
      if (value_type == G_VARIANT_CLASS_BOOLEAN)
        return TRUE;
      break;

    case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
      if (value_type == G_VARIANT_CLASS_INT64)
        return TRUE;
      break;

    case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
      if (value_type == G_VARIANT_CLASS_DOUBLE)
        return TRUE;
      break;

    case HYSCAN_DATA_SCHEMA_KEY_STRING:
      if (value_type == G_VARIANT_CLASS_STRING)
        return TRUE;
      break;

    case HYSCAN_DATA_SCHEMA_KEY_ENUM:
      if (value_type == G_VARIANT_CLASS_INT64)
        return TRUE;
      break;

    default:
      break;
    }

  return FALSE;
}

/* Функция записывает значения параметров в INI файл. */
static gboolean
hyscan_db_param_file_params_flush (GKeyFile      *params,
                                   GOutputStream *ofd)
{
  gchar *data;
  gsize dsize;
  gssize wsize;
  gboolean status = FALSE;

  data = g_key_file_to_data (params, &dsize, NULL);

  /* Обнуляем файл. */
  if (!g_seekable_truncate (G_SEEKABLE (ofd), 0, NULL, NULL))
    {
      g_warning ("HyScanDBParamFile: can't update parameters");
      goto exit;
    }

  /* Записываем в него новые данные. */
  wsize = dsize;
  if (g_output_stream_write (ofd, data, dsize, NULL, NULL) != wsize)
    {
      g_warning ("HyScanDBParamFile: can't write parameters");
      goto exit;
    }

  /* Очищаем IO буфер. */
  if (!g_output_stream_flush (ofd, NULL, NULL))
    {
      g_warning ("HyScanDBParamFile: can't flush parameters");
      goto exit;
    }

  status = TRUE;

exit:
  g_free (data);

  return status;
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

/* Функция проверяет была создан новый файл параметров или использовался существующий. */
gboolean
hyscan_db_param_file_is_new (HyScanDBParamFile *param)
{
  g_return_val_if_fail (HYSCAN_IS_DB_PARAM_FILE (param), FALSE);

  return param->priv->new_file;
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
hyscan_db_param_file_set (HyScanDBParamFile *param,
                          const gchar       *object_name,
                          HyScanParamList   *param_list)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;
  HyScanDataSchemaKeyType param_type;
  GVariantClass value_type;
  gboolean status = FALSE;

  const gchar * const *param_names;
  GVariant **param_values = NULL;
  guint n_param_names;
  guint i;

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

  /* Список устанавливаемых параметров. */
  param_names = hyscan_param_list_params (param_list);
  if (param_names == NULL)
    goto exit;

  /* Список значений параметров. */
  n_param_names = g_strv_length ((gchar **)param_names);
  param_values = g_new0 (GVariant *, n_param_names);

  /* Проверяем параметы. */
  for (i = 0; i < n_param_names; i++)
    {
      HyScanDataSchemaKeyAccess access;

      /* Доступность на запись. */
      access = hyscan_data_schema_key_get_access (schema, param_names[i]);
      if (access == HYSCAN_DATA_SCHEMA_ACCESS_READONLY)
        goto exit;

      /* Новое значение параметра. */
      param_values[i]= hyscan_param_list_get (param_list, param_names[i]);

      /* Установка значения по умолчанию. */
      if (param_values[i] == NULL)
        continue;

      /* Ошибка в типе нового значения параметра. */
      param_type = hyscan_data_schema_key_get_type (schema, param_names[i]);
      value_type = g_variant_classify (param_values[i]);
      if (!hyscan_db_param_file_compare_types (param_type, value_type))
        goto exit;

      /* Недопустимое значение параметра. */
      if (!hyscan_data_schema_key_check (schema, param_names[i], param_values[i]))
        goto exit;
    }

  /* Изменяем параметы. */
  for (i = 0; i < n_param_names; i++)
    {
      /* Сбрасываем значение параметра до состояния по умолчанию. */
      if (param_values[i] == NULL)
        {
          g_key_file_remove_key (priv->params, object_name, param_names[i], NULL);
          continue;
        }

      value_type = g_variant_classify (param_values[i]);
      switch (value_type)
        {
        case G_VARIANT_CLASS_BOOLEAN:
          g_key_file_set_boolean (priv->params, object_name, param_names[i],
                                  g_variant_get_boolean (param_values[i]));
          break;

        case G_VARIANT_CLASS_INT64:
          g_key_file_set_int64 (priv->params, object_name, param_names[i],
                                g_variant_get_int64 (param_values[i]));
          break;

        case G_VARIANT_CLASS_DOUBLE:
          g_key_file_set_double (priv->params, object_name, param_names[i],
                                 g_variant_get_double (param_values[i]));
          break;

        case G_VARIANT_CLASS_STRING:
          g_key_file_set_string (priv->params, object_name, param_names[i],
                                 g_variant_get_string (param_values[i], NULL));
          break;

        default:
          break;
        }

      g_variant_unref (param_values[i]);
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

  if (!status && (param_values != NULL))
    for (i = 0; i < n_param_names; i++)
      g_clear_pointer (&param_values[i], g_variant_unref);

  g_free (param_values);
  g_free (schema_id);

  return status;
}

/* Функция считывает значение параметра. */
gboolean
hyscan_db_param_file_get (HyScanDBParamFile *param,
                          const gchar       *object_name,
                          HyScanParamList   *param_list)
{
  HyScanDBParamFilePrivate *priv;

  gchar *schema_id = NULL;
  HyScanDataSchema *schema = NULL;
  gboolean status = FALSE;

  const gchar * const *param_names;
  guint n_param_names;
  guint i;

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

  /* Список устанавливаемых параметров. */
  param_names = hyscan_param_list_params (param_list);
  if (param_names == NULL)
    goto exit;

  /* Проверяем параметы. */
  n_param_names = g_strv_length ((gchar **)param_names);
  for (i = 0; i < n_param_names; i++)
    {
      HyScanDataSchemaKeyAccess access;

      /* Доступность на чтение. */
      access = hyscan_data_schema_key_get_access (schema, param_names[i]);
      if (access == HYSCAN_DATA_SCHEMA_ACCESS_WRITEONLY)
        goto exit;

      if (!hyscan_data_schema_has_key (schema, param_names[i]))
        goto exit;
    }

  /* Считываем значения параметров. */
  for (i = 0; param_names[i] != NULL; i++)
    {
      HyScanDataSchemaKeyType param_type;
      HyScanDataSchemaKeyAccess access;
      GVariant *param_value;

      /* Для параметров "только для чтения" или если значение не установлено
       * возвращаем значение по умолчанию. */
      access = hyscan_data_schema_key_get_access (schema, param_names[i]);
      if ((access == HYSCAN_DATA_SCHEMA_ACCESS_READONLY) ||
          !g_key_file_has_key (priv->params, object_name, param_names[i], NULL))
        {
          param_value = hyscan_data_schema_key_get_default (schema, param_names[i]);
          hyscan_param_list_set (param_list, param_names[i], param_value);
          g_clear_pointer (&param_value, g_variant_unref);
        }
      else
        {
          param_type = hyscan_data_schema_key_get_type (schema, param_names[i]);
          switch (param_type)
            {
            case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
              param_value = g_variant_new_boolean (g_key_file_get_boolean (priv->params, object_name, param_names[i], NULL));
              break;

            case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
            case HYSCAN_DATA_SCHEMA_KEY_ENUM:
              param_value = g_variant_new_int64 (g_key_file_get_int64 (priv->params, object_name, param_names[i], NULL));
              break;

            case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
              param_value = g_variant_new_double (g_key_file_get_double (priv->params, object_name, param_names[i], NULL));
              break;

            case HYSCAN_DATA_SCHEMA_KEY_STRING:
              param_value = g_variant_new_take_string (g_key_file_get_string (priv->params, object_name, param_names[i], NULL));
              break;

            default:
              param_value = NULL;
              break;
            }

          hyscan_param_list_set (param_list, param_names[i], param_value);
        }
    }

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);
  g_free (schema_id);

  return status;
}
