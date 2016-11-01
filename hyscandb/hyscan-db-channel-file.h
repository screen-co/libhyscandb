/*
 * \file hyscan-db-channel-file.h
 *
 * \brief Заголовочный файл класса хранения данных канала в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * HyScanDBChannelFile - класс работы с данными канала в формате HyScan версии 5
 *
 * Описание функций класса совпадает с соответствующими функциями интерфейса HyScanDB.
 *
 * Создание объекта класса осуществляется при помощи функции g_object_new.
 *
 * Конструктор класса имеет три параметра:
 *
 * - path - путь к каталогу с файлами данных. (string);
 * - name - название канала данных (string);
 * - readonly - признак работы в режиме только для чтения (boolean).
 *
 * Данные хранятся в двух основыных типах фалов: данных и индексов. Максимальный размер одного
 * файла ограничен константой MAX_DATA_FILE_SIZE и по умолчанию составляет 1 Гб. Минимальный
 * размер файла ограничен константой MIN_DATA_FILE_SIZE и по умолчанию составляет 1 Мб.
 *
 * Файлы данных и индексов имеют следующий формат имени: "<name>.XXXXXX.Y", где:
 * - name - название канала данных;
 * - XXXXXX - номер части данных от 0 до MAX_PARTS;
 * - Y - расширение имени файла, i - для файла индексов, d - для файла данных.
 *
 * Все служебные поля файла записываются в формате LITTLE ENDIAN.
 *
 * Каждый файл содержит в самом начале идентификатор типа размером 4 байта. Идентификаторы определены в константах:
 * - INDEX_FILE_MAGIC - для файлов индексов ("HSIX");
 * - DATA_FILE_MAGIC - для файлов данных ("HSDT").
 *
 * Следующие 4 байта занимает версия файла, константа FILE_VERSION ("1507").
 *
 * Константы определены как 32-х битное целое число таким образом, что бы при записи их в файл как
 * LITTLE ENDIAN 32-х битные значения и чтения их в виде строки, получались осмысленные аббревиатуры.
 *
 * Каждый файл индексов после идентификатора и версии файла содержит номер первого индекса - 32-х битное целое со знаком.
 *
 * Далее в файл индексов записываются индексы блоков данных, индекс описывается структурой HyScanDBChannelFileIndexRec.
 *
 * Для каждого индекса, в файл данных записывается информация размером соответствующим указанному в индексе. Смещение
 * до каждого записанного блока данных также указывается в индексе.
 *
 * Сами данные записываются без преобразований. Клиент должен знать формат записываемых данных.
 *
 * По достижении файлом данных определённого размера, создаётся новая часть данных, состоящая из пары файлов: индексов и данных.
 *
 * При включении ограничений по времени хранения данных или максимальному объёму, новая часть данных будет также создаваться
 * если запись в текущую часть длится дольше чем ( время хранения / 5 ) или размер текущей части превысил 1/5 часть от
 * сохраняемого объёма. Старые части данных (по времени или по объёму) будут периодически удаляться, а оставшиеся файлы
 * переименовываться, чтобы номер части данных всегда начинался с нуля.
 *
 */

#ifndef __HYSCAN_DB_CHANNEL_FILE_H__
#define __HYSCAN_DB_CHANNEL_FILE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_CHANNEL_FILE             (hyscan_db_channel_file_get_type ())
#define HYSCAN_DB_CHANNEL_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFile))
#define HYSCAN_IS_DB_CHANNEL_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE))
#define HYSCAN_DB_CHANNEL_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFileClass))
#define HYSCAN_IS_DB_CHANNEL_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_CHANNEL_FILE))
#define HYSCAN_DB_CHANNEL_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_CHANNEL_FILE, HyScanDBChannelFileClass))

typedef struct _HyScanDBChannelFile HyScanDBChannelFile;
typedef struct _HyScanDBChannelFilePrivate HyScanDBChannelFilePrivate;
typedef struct _HyScanDBChannelFileClass HyScanDBChannelFileClass;

struct _HyScanDBChannelFile
{
  GObject parent_instance;

  HyScanDBChannelFilePrivate *priv;
};

struct _HyScanDBChannelFileClass
{
  GObjectClass parent_class;
};

GType hyscan_db_channel_file_get_type (void);

/* Функция создаёт новый объект HyScanDBChannelFile. */
HyScanDBChannelFile *hyscan_db_channel_file_new            (const gchar         *path,
                                                            const gchar         *name,
                                                            gboolean             readonly);

/* Функция возвращает диапазон текущих значений индексов данных. */
gboolean   hyscan_db_channel_file_get_channel_data_range   (HyScanDBChannelFile *channel,
                                                            gint32              *first_index,
                                                            gint32              *last_index);


/* Функция записывает новые данные. */
gboolean   hyscan_db_channel_file_add_channel_data         (HyScanDBChannelFile *channel,
                                                            gint64               time,
                                                            gconstpointer        data,
                                                            guint32              size,
                                                            gint32              *index);

/* Функция считывает данные. */
gboolean   hyscan_db_channel_file_get_channel_data         (HyScanDBChannelFile *channel,
                                                            gint32               index,
                                                            gpointer             buffer,
                                                            guint32             *buffer_size,
                                                            gint64              *time);

/* Функция ищет данные по метке времени. */
gboolean   hyscan_db_channel_file_find_channel_data        (HyScanDBChannelFile *channel,
                                                            gint64               time,
                                                            gint32              *lindex,
                                                            gint32              *rindex,
                                                            gint64              *ltime,
                                                            gint64              *rtime);

/* Функция устанавливает максимальный размер файла данных. */
gboolean   hyscan_db_channel_file_set_channel_chunk_size   (HyScanDBChannelFile *channel,
                                                            guint64              chunk_size);

/* Функция устанавливает интервал времени в микросекундах для которого хранятся записываемые данные. */
gboolean   hyscan_db_channel_file_set_channel_save_time    (HyScanDBChannelFile *channel,
                                                            gint64               save_time);

/* Функция устанавливает максимальный объём сохраняемых данных. */
gboolean   hyscan_db_channel_file_set_channel_save_size    (HyScanDBChannelFile *channel,
                                                            guint64              save_size);

/* Функция завершает запись данных. */
void       hyscan_db_channel_file_finalize_channel         (HyScanDBChannelFile *channel);

/* Функция удаляет все файлы в каталоге path относящиеся к каналу name. */
gboolean   hyscan_db_channel_remove_channel_files          (const gchar         *path,
                                                            const gchar         *name);

G_END_DECLS

#endif /* __HYSCAN_DB_CHANNEL_FILE_H__ */
