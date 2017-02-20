/*
 * \file hyscan-db-param-file.h
 *
 * \brief Заголовочный файл класса хранения параметров в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * HyScanDBParamFile класс для работы с группой параметров в INI фалах
 *
 * Описание функций класса совпадает с соответствующими функциями интерфейса HyScanDB.
 *
 * Создание объекта класса осуществляется при помощи функции hyscan_db_param_file_new.
 *
 * Конструктор класса имеет два параметра:
 *
 * - param_file - путь к файлу со значениями параметров;
 * - schema_file - путь к файлу со схемой данных.
 *
 */

#ifndef __HYSCAN_DB_PARAM_FILE_H__
#define __HYSCAN_DB_PARAM_FILE_H__

#include <hyscan-data-schema.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_PARAM_FILE             (hyscan_db_param_file_get_type())
#define HYSCAN_DB_PARAM_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFile))
#define HYSCAN_IS_DB_PARAM_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))
#define HYSCAN_IS_DB_PARAM_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))

typedef struct _HyScanDBParamFile HyScanDBParamFile;
typedef struct _HyScanDBParamFilePrivate HyScanDBParamFilePrivate;
typedef struct _HyScanDBParamFileClass HyScanDBParamFileClass;

struct _HyScanDBParamFile
{
  GObject parent_instance;

  HyScanDBParamFilePrivate *priv;
};

struct _HyScanDBParamFileClass
{
  GObjectClass parent_class;
};

GType hyscan_db_param_file_get_type (void);

/* Функция создаёт новый объект HyScanDBParamFile. */
HyScanDBParamFile     *hyscan_db_param_file_new                (const gchar           *param_file,
                                                                const gchar           *schema_file);

/* Функция проверяет была создан новый файл параметров или использовался существующий. */
gboolean               hyscan_db_param_file_is_new             (HyScanDBParamFile     *param);

/* Функция возвращает список объектов. */
gchar                **hyscan_db_param_file_object_list        (HyScanDBParamFile     *param);

/* Функция возвращает указатель на объект HyScanDataSchema для объекта.
 * Объект принадлежит HyScanDBParamFile. */
HyScanDataSchema      *hyscan_db_param_file_object_get_schema  (HyScanDBParamFile     *param,
                                                                const gchar           *object_name);

/* Функция создаёт объект с указанным идентификатором схемы. */
gboolean               hyscan_db_param_file_object_create      (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

/* Функция удаляет объект. */
gboolean               hyscan_db_param_file_object_remove      (HyScanDBParamFile     *param,
                                                                const gchar           *object_name);


/* Функция устанавливает значение параметра. */
gboolean               hyscan_db_param_file_set                (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                const gchar *const    *param_names,
                                                                GVariant             **param_values);

/* Функция считывает значение параметра. */
gboolean               hyscan_db_param_file_get                (HyScanDBParamFile     *param,
                                                                const gchar           *object_name,
                                                                const gchar *const    *param_names,
                                                                GVariant             **param_values);

G_END_DECLS

#endif /* __HYSCAN_DB_PARAM_FILE_H__ */
