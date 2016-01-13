/**
 * \file hyscan-db-param-file.h
 *
 * \brief Заголовочный файл класса хранения параметров в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBParamFile HyScanDBParamFile класс для работы с группой параметров в INI фалах
 *
 * Описание функций класса совпадает с соответствующими функциями интерфейса \link HyScanDB \endlink.
 *
 * Создание объекта класса осуществляется при помощи функции hyscan_db_param_file_new.
 *
 * Конструктор класса имеет три параметра:
 *
 * - path - путь к каталогу с ini файлом (string);
 * - name - название группы параметров - ini файла (string).
 *
 */

#ifndef __HYSCAN_DB_PARAM_FILE_H__
#define __HYSCAN_DB_PARAM_FILE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_PARAM_FILE             (hyscan_db_param_file_get_type())
#define HYSCAN_DB_PARAM_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFile))
#define HYSCAN_IS_DB_PARAM_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))
#define HYSCAN_IS_DB_PARAM_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_PARAM_FILE))
#define HYSCAN_DB_PARAM_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_PARAM_FILE, HyScanDBParamFileClass))

typedef struct _HyScanDBParamFile HyScanDBParamFile;
typedef struct _HyScanDBParamFileClass HyScanDBParamFileClass;

struct _HyScanDBParamFileClass
{
  GObjectClass parent_class;
};

GType hyscan_db_param_file_get_type (void);

/* Функция создаёт новый объект HyScanDBParamFile. */
HyScanDBParamFile *hyscan_db_param_file_new    (const gchar           *path,
                                                const gchar           *name);

/* Функция возвращает список параметров. */
gchar    **hyscan_db_param_file_get_param_list (HyScanDBParamFile     *param);

/* Функция удаляет параметры по маске. */
gboolean   hyscan_db_param_file_remove_param   (HyScanDBParamFile     *param,
                                                const gchar           *mask);

/* Функция проверяет существование указанного параметра. */
gboolean   hyscan_db_param_file_has_param      (HyScanDBParamFile     *param,
                                                const gchar           *name);

/* Функция увеличивает значение параметра типа integer на единицу. */
gint64     hyscan_db_param_file_inc_integer    (HyScanDBParamFile     *param,
                                                const gchar           *name);

/* Функция устанавливает значение параметра типа integer. */
gboolean   hyscan_db_param_file_set_integer    (HyScanDBParamFile     *param,
                                                const gchar           *name,
                                                gint64                 value);

/* Функция устанавливает значение параметра типа double. */
gboolean   hyscan_db_param_file_set_double     (HyScanDBParamFile     *param,
                                                const gchar           *name,
                                                gdouble                value);

/* Функция устанавливает значение параметра типа boolean. */
gboolean   hyscan_db_param_file_set_boolean    (HyScanDBParamFile     *param,
                                                const gchar           *name,
                                                gboolean               value);

/* Функция устанавливает значение параметра типа string. */
gboolean   hyscan_db_param_file_set_string     (HyScanDBParamFile     *param,
                                                const gchar           *name,
                                                const gchar           *value);

/* Функция возвращает значение параметра типа integer. */
gint64     hyscan_db_param_file_get_integer    (HyScanDBParamFile     *param,
                                                const gchar           *name);

/* Функция возвращает значение параметра типа double. */
gdouble    hyscan_db_param_file_get_double     (HyScanDBParamFile     *param,
                                                const gchar           *name);

/* Функция возвращает значение параметра типа boolean. */
gboolean   hyscan_db_param_file_get_boolean    (HyScanDBParamFile     *param,
                                                const gchar           *name);

/* Функция возвращает значение параметра типа string. */
gchar     *hyscan_db_param_file_get_string     (HyScanDBParamFile     *param,
                                                const gchar           *name);

G_END_DECLS

#endif /* __HYSCAN_DB_PARAM_FILE_H__ */
