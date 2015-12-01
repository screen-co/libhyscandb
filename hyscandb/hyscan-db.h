/**
 * \file hyscan-db.h
 *
 * \brief Заголовочный файл интерфейса базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDB HyScanDB - интерфейс работы с системой хранения данных
 *
 * Интерфейс \link HyScanDB \endlink определяет модель взаимодействия с системой хранения данных.
 * Система реализует иерархическую структуру модели данных. Корневыми элементами являются проекты,
 * каждый из которых может хранить произвольное число галсов. В каждом галсе могут храниться данные
 * от множества источников информации - каналов данных. С каждым из объектов (проектом, галсом или
 * данными) может быть связано произвольное число параметров вида ключ – значение.
 *
 * Система может работать в двух режимах: локальном и сетевом. В локальном режиме все операции
 * выполняются в рамках процесса создавшего объект \link HyScanDB \endlink и права доступа к данным
 * контролируются на уровне файловой системы. В случае сетевого использования все операции транслируются
 * на сервер, с которым возможна параллельная работы нескольких клиентов. Права доступа в этом случае
 * контролируются сервером.
 *
 * Выбор режима работы, локальный или сетевой, осуществляется при подключении с системе хранения данных.
 *
 * Система допускает использование сторонних модулей, реализующих хранение данных в собственных
 * форматах, например XTF, SEG-Y и пр. Список доступных форматов хранения можно получить функцией
 * #hyscan_db_get_project_type_list. При создании проекта можно указать используемый формат.
 *
 * Функция #hyscan_db_get_uri возвращает строку с путём подключения к системе хранения.
 *
 * Общая схема работы с системой хранения данных следующая:
 *
 * - подключение к системе хранения данных - #hyscan_db_new
 * - выбор используемого проекта и его параметров - #hyscan_db_get_project_list, #hyscan_db_get_project_param_list
 *   -# открытие существующего проекта - #hyscan_db_open_project
 *   -# создание нового проекта - #hyscan_db_create_project
 *   -# получение даты и времени создания проекта - #hyscan_db_get_project_ctime
 *   -# открытие (создание) группы параметров - #hyscan_db_open_project_param
 * - выбор используемого галса и его параметров - #hyscan_db_get_track_list, #hyscan_db_get_track_param_list
 *   -# открытие существующего галса - #hyscan_db_open_track
 *   -# создание нового галса - #hyscan_db_create_track
 *   -# получение даты и времени создания галса - #hyscan_db_get_track_ctime
 *   -# открытие (создание) группы параметров - #hyscan_db_open_track_param
 * - выбор используемого канала данных - #hyscan_db_get_channel_list
 *   -# открытие существующего канала данных - #hyscan_db_open_channel
 *   -# создание нового канала данных #hyscan_db_create_channel
 *   -# открытие группы параметров - #hyscan_db_open_channel_param
 * - работа с данными
 *   -# запись данных - #hyscan_db_add_channel_data
 *   -# чтение данных - #hyscan_db_get_channel_data
 *   -# поиск данных по времени - #hyscan_db_find_channel_data
 *   -# получение диапазона доступных данных - #hyscan_db_get_channel_data_range
 * - работа с параметрами
 *   -# получение списка названий параметров в группе - #hyscan_db_get_param_list
 *   -# копирование параметров из одной группы в другую - #hyscan_db_copy_param
 *   -# удаление параметров в группе - #hyscan_db_remove_param
 *   -# увеличение значения параметра типа integer на единицу - #hyscan_db_inc_integer_param
 *   -# установка значения параметра типа integer - #hyscan_db_set_integer_param
 *   -# установка значения параметра типа double - #hyscan_db_set_double_param
 *   -# установка значения параметра типа boolean - #hyscan_db_set_boolean_param
 *   -# установка значения параметра типа string - #hyscan_db_set_string_param
 *   -# чтение значения параметра типа integer - #hyscan_db_get_integer_param
 *   -# чтение значения параметра типа double - #hyscan_db_get_double_param
 *   -# чтение значения параметра типа boolean - #hyscan_db_get_boolean_param
 *   -# чтение значения параметра типа string - #hyscan_db_get_string_param
 * - закрытие неиспользуемых объектов
 *   -# группы параметров - #hyscan_db_close_param
 *   -# канала данных - #hyscan_db_close_channel
 *   -# галса - #hyscan_db_close_track
 *   -# проекта - #hyscan_db_close_project
 * - удаление неиспользуемых объектов
 *   -# канала данных - #hyscan_db_remove_channel
 *   -# галса - #hyscan_db_remove_track
 *   -# проекта - #hyscan_db_remove_project
 *   -# группы параметров галса - #hyscan_db_remove_track_param
 *   -# группы параметров проекта - #hyscan_db_remove_project_param
 * - отключение от системы хранения данных осуществляется при удалении объекта функцией g_object_unref
 *
 * Все названия проектов, галсов, каналов данных и групп параметров должны быть в кодировке UTF-8.
 *
 * Необходимо иметь в виду, что параметры могут изменяться несколькими клиентами одновременно. Таким образом,
 * при одновременном изменении нескольких параметров может возникнуть ситуация, когда будет нарушена совокупная
 * целостность изменяемых параметров (другой клиент изменит часть параметров на свои значения). Чтобы избежать
 * этой проблемы, при необходимости атомарно изменить или считать несколько параметров, рекомендуется создать
 * отдельную группу парамтров и скопировать данные из неё или в неё функцией #hyscan_db_copy_param. Эта операция
 * будет произведена атомарно.
 *
 * Чтобы сигнализировать другим клиентам, что параметры изменились, можно использовать счётчик, изменившееся
 * значение которого укажет на это событие. Для атомарного увеличения значения параметра типа integer можно
 * использовать функцию #hyscan_db_inc_integer_param.
 *
 * Названия параметров должны иметь вид "подгруппа.имя" или "имя". Во втором случае автоматически используется
 * подгруппа "default". Названия параметров могут содержать цифры, буквы латинского алфавита, символы '-' и '_'.
 *
 * Система хранения данных спроектирована таким образом, что не допускает запись в уже существующий канал данных.
 * Таким образом, запись данных возможна ТОЛЬКО во вновь созданный функцией #hyscan_db_create_channel канал данных.
 * Аналогично, установка значений параметров канала данных, возможна ТОЛЬКО для вновь созданного. Функция
 * #hyscan_db_finalize_channel принудительно переводит канал данных в режим только чтения данных. После вызова этой
 * функции запись новых данных невозможна.
 *
 * Записываемые в канал данные обычно сохраняются на диске в виде файлов. Слишком большой размер файлов создаёт
 * проблемы с их копированием и хранением в некоторых файловых системах. Функция #hyscan_db_set_channel_chunk_size
 * даёт указание системе хранения ограничить размер файлов указанным размером. Если объём данных больше, чем
 * максимальный размер файла, система должна создать следующий файл, логически объединённый с предыдущим.
 * По умолчанию размер файла равен 1 Гб. Необходимо иметь в виду, что задаваемый параметр только ограничивает
 * размер файла. Действительный размер файла может быть меньше.
 *
 * Необходимо иметь в виду, что размер файла одновременно накладывает ограничения на максимальный размер
 * одной записи в канале данных.
 *
 * При записи данных в канал имеется возможность ограничить объём или время хранения данных. Для этого
 * используются функции #hyscan_db_set_channel_save_time и #hyscan_db_set_channel_save_size. Данные функции
 * дают указание системе хранения удалять данные которые старше указанного периода времени или превышают
 * определённый объём. Удаление данных допускается проводить блоками, сразу по несколько записей. Таким
 * образом, возможна ситуация временного превышения лимитов хранения (данные хранятся дольше или данных записано больше),
 * но не наоборот.
 *
 * Основной объём информации записывается в каналы данных. Каналы данных спроектированы таким образом, чтобы
 * одновременно с хранением информации хранить метку времени. Не допускается запись данных с меткой времени
 * равной или старше уже записанной. Каждому блоку данных при записи присваивается уникальный (в рамках канала данных)
 * номер - индекс. Чтение данных возможно только по этому индексу. Для поиска необходимых данных по времени предназначена
 * функция #hyscan_db_find_channel_data, которая возвращает индекс (или индексы при неточном совпадении метки времени).
 *
 * Метки времени должны задаваться в микросекундах, но в действительности сохраняются как 64-х битное целое число со знаком.
 * Это даёт возможность клиенту интерпретировать моменты времени собственным способом.
 *
 * Если для канала данных задано ограничение по времени или объёму хранения возможна ситуация удаления ранних записей.
 * Для того, чтобы определить границы доступных записей предназначена функция #hyscan_db_get_channel_data_range.
 * Необходимо иметь в виду, что между моментом времени определения границ записей и моментом чтения данных
 * эти границы могут измениться. При чтении данных с неизвестным индексом функция возвратит ошибку, которую
 * в данном случае следует игнорировать.
 *
 * В процессе работы любой из клиентов системы хранения может произвольно создавать и удалять
 * любые объекты (в рамках своих привелегий). Таким образом уже открытые объекты могут перестать существовать.
 * Признаком этого будет ошибка, при выполнении действий с этим объектом.  Эти ошибки должны правильным образом
 * обрабатываться клиентом (закрытие объекта, исключение из дальнейшей обработки).
 *
 */

#ifndef __HYSCAN_DB_H__
#define __HYSCAN_DB_H__

#include <glib-object.h>
#include <hyscan-db-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB            (hyscan_db_get_type ())
#define HYSCAN_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB, HyScanDB))
#define HYSCAN_IS_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB))
#define HYSCAN_DB_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_DB, HyScanDBInterface))

typedef struct _HyScanDB HyScanDB;

typedef struct _HyScanDBInterface HyScanDBInterface;

struct _HyScanDBInterface
{
  GTypeInterface g_iface;

  gchar **     (*get_project_type_list)        (HyScanDB              *db);
  gchar *      (*get_uri)                      (HyScanDB              *db);

  gchar **     (*get_project_list)             (HyScanDB              *db);
  gint32       (*open_project)                 (HyScanDB              *db,
                                                const gchar           *project_name);
  gint32       (*create_project)               (HyScanDB              *db,
                                                const gchar           *project_name,
                                                const gchar           *project_type);
  gboolean     (*remove_project)               (HyScanDB              *db,
                                                const gchar           *project_name);
  void         (*close_project)                (HyScanDB              *db,
                                                gint32                 project_id);
  GDateTime *  (*get_project_ctime)            (HyScanDB              *db,
                                                gint32                 project_id);

  gchar **     (*get_track_list)               (HyScanDB              *db,
                                                gint32                 project_id);
  gint32       (*open_track)                   (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);
  gint32       (*create_track)                 (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);
  gboolean     (*remove_track)                 (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);
  void         (*close_track)                  (HyScanDB              *db,
                                                gint32                 track_id);
  GDateTime *  (*get_track_ctime)              (HyScanDB              *db,
                                                gint32                 track_id);

  gchar **     (*get_channel_list)             (HyScanDB              *db,
                                                gint32                 track_id);
  gint32       (*open_channel)                 (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);
  gboolean     (*create_channel)               (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);
  gboolean     (*remove_channel)               (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);
  void         (*close_channel)                (HyScanDB              *db,
                                                gint32                 channel_id);
  gint32       (*open_channel_param)           (HyScanDB             *db,
                                                gint32                 channel_id);

  gboolean     (*set_channel_chunk_size)       (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                 chunk_size);
  gboolean     (*set_channel_save_time)        (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 save_time);
  gboolean     (*set_channel_save_size)        (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 save_size);
  void         (*finalize_channel)             (HyScanDB              *db,
                                                gint32                 channel_id);

  gboolean     (*get_channel_data_range)       (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                *first_index,
                                                gint32                *last_index);
  gboolean     (*add_channel_data)             (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 time,
                                                gpointer               data,
                                                gint32                 size,
                                                gint32                *index);
  gboolean     (*get_channel_data)             (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                 index,
                                                gpointer               buffer,
                                                gint32                *buffer_size,
                                                gint64                *time);
  gboolean     (*find_channel_data)            (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 time,
                                                gint32                *lindex,
                                                gint32                *rindex,
                                                gint64                *ltime,
                                                gint64                *rtime);

  gchar **     (*get_project_param_list)       (HyScanDB              *db,
                                                gint32                 project_id );
  gint32       (*open_project_param)           (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *group_name);
  gboolean     (*remove_project_param)         (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *group_name);

  gchar **     (*get_track_param_list)         (HyScanDB              *db,
                                                gint32                 track_id );
  gint32       (*open_track_param)             (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *group_name);
  gboolean     (*remove_track_param)           (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *group_name);

  gchar **     (*get_param_list)               (HyScanDB              *db,
                                                gint32                 param_id);
  gboolean     (*copy_param)                   (HyScanDB              *db,
                                                gint32                 src_param_id,
                                                gint32                 dst_param_id,
                                                const gchar           *mask);
  gboolean     (*remove_param)                 (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *mask);
  void         (*close_param)                  (HyScanDB              *db,
                                                gint32                 param_id);
  gboolean     (*has_param)                    (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

  gint64       (*inc_integer_param)            (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);
  gboolean     (*set_integer_param)            (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gint64                 value);
  gboolean     (*set_double_param)             (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gdouble                value);
  gboolean     (*set_boolean_param)            (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gboolean               value);
  gboolean     (*set_string_param)             (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                const gchar           *value);
  gint64       (*get_integer_param)            (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);
  gdouble      (*get_double_param)             (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);
  gboolean     (*get_boolean_param)            (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);
  gchar*       (*get_string_param)             (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

};

HYSCAN_DB_EXPORT
GType          hyscan_db_get_type              (void);

/* Общие функции. */

/**
 *
 * Функция подключается к системе хранения данных.
 *
 * Параметры подключения определяются строкой uri. Формат строки следующий:
 * "type://<user>:<password>@path/", где:
 * - type - тип подключения к системе хранения ( file, tcp, shm );
 * - user - имя пользователя;
 * - password - пароль пользователя;
 * - path - путь к каталогу с проектами (для типа подключения file) или адрес сервера (для tcp и shm).
 *
 * Параметры user и password являются опциональными и требуют задания только при подключении к серверу
 * на котором используется система аутентификации.
 *
 * Объект HyScanDB необходимо удалить функцией g_object_unref после окончания работы.
 *
 * \param uri адрес подключения.
 *
 * \return Указатель на интерфейс \link HyScanDB \endlink.
 *
 */
HYSCAN_DB_EXPORT
HyScanDB      *hyscan_db_new                   (const gchar           *uri);

/**
 *
 * Функция возвращает список доступных форматов хранения.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return NULL терминированный список форматов хранения, NULL - если доступен только формат по умолчанию.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_project_type_list (HyScanDB              *db);

/**
 *
 * Функция возвращает строку uri с путём подключения к системе хранения.
 *
 * Память выделенная под строку должна быть освобождена после использования (см. g_free).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Строка uri или NULL в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gchar         *hyscan_db_get_uri               (HyScanDB              *db);

/* Функции работы с проектами. */

/**
 *
 * Функция возвращает список проектов существующих в системе хранения.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return NULL терминированный список проектов или NULL - если проектов нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_project_list      (HyScanDB              *db);

/**
 *
 * Функция открывает существующий проект для работы.
 *
 * Функция возвращает идентификатор открытого проекта, который должен использоваться в остальных функциях работы с проектами.
 *
 * По завершению работы с проектом его необходимо закрыть функцией #hyscan_db_close_project.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта.
 *
 * \return Идентификатор открытого проекта или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_project          (HyScanDB              *db,
                                                const gchar           *project_name);

/**
 *
 * Функция создаёт новый проект и открывает его для работы.
 *
 * При создании проекта можно указать тип используемого формата храненияданных , из списка возвращаемого функцией
 * #hyscan_db_get_project_type_list. Если project_type == NULL используется формат хранения по умолчанию.
 *
 * Функция возвращает идентификатор открытого проекта, который должен использоваться в остальных функциях работы с проектами.
 *
 * По завершению работы с проектом его необходимо закрыть функцией #hyscan_db_close_project.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param project_type тип модуля системы хранения или NULL.
 *
 * \return Идентификатор нового, открытого проекта или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_create_project        (HyScanDB              *db,
                                                const gchar           *project_name,
                                                const gchar           *project_type);

/**
 *
 * Функция удаляет существующий проект.
 *
 * Удаление возможно даже в случае использования проекта или его объектов другими клиентами. В этом
 * случае функции работы с этим проектом и его объектами будут возвращать ошибки.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта.
 *
 * \return TRUE - если проект был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_project        (HyScanDB              *db,
                                                const gchar           *project_name);

/**
 *
 * Функция закрывает открытый ранее проект.
 *
 * Закрытие проекта не влияет на открытые в этом проекте объекты. Их использование может быть продолжено.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_DB_EXPORT
void           hyscan_db_close_project         (HyScanDB              *db,
                                                gint32                 project_id);

/**
 *
 * Функция возвращает информацию о дате и времени создания проекта.
 *
 * Память выделенная под объект GDateTime должна быть освобождена после использования (см. g_date_time_unref).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта.
 *
 * \return Указатель на объект GDateTime или NULL в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
GDateTime     *hyscan_db_get_project_ctime     (HyScanDB              *db,
                                                gint32                 project_id);

/* Функции работы с галсами. */

/**
 *
 * Функция возвращает список галсов существующих в открытом проекте.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта.
 *
 * \return NULL терминированный список галсов или NULL - если галсов нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_track_list        (HyScanDB              *db,
                                                gint32                 project_id);

/**
 *
 * Функция открывает существующий галс для работы.
 *
 * Функция возвращает идентификатор открытого галса, который должен использоваться в остальных функциях работы с галсами.
 *
 * По завершению работы с галсом его необходимо закрыть функцией #hyscan_db_close_track.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта;
 * \param track_name название галса.
 *
 * \return Идентификатор открытого галса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_track            (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);

/**
 *
 * Функция создаёт новый галс и открывает его для работы.
 *
 * Функция возвращает идентификатор открытого галса, который должен использоваться в остальных функциях работы с галсами.
 *
 * По завершению работы с галсом его необходимо закрыть функцией #hyscan_db_close_track.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта;
 * \param track_name название галса.
 *
 * \return Идентификатор нового, открытого галса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_create_track          (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);

/**
 *
 * Функция удаляет существующий галс.
 *
 * Удаление возможно даже в случае использования галса или его объектов другими клиентами. В этом
 * случае функции работы с этим галсом и его объектами будут возвращать ошибки.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта;
 * \param track_name название галса.
 *
 * \return TRUE - если галс был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_track          (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *track_name);

/**
 *
 * Функция закрывает открытый ранее галс.
 *
 * Закрытие галса не влияет на открытые в этом галсе объекты. Их использование может быть продолжено.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_DB_EXPORT
void           hyscan_db_close_track           (HyScanDB              *db,
                                                gint32                 track_id);

/**
 *
 * Функция возвращает информацию о дате и времени создания галса.
 *
 * Память выделенная под объект GDateTime должна быть освобождена после использования (см. g_date_time_unref).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса.
 *
 * \return Указатель на объект GDateTime или NULL в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
GDateTime     *hyscan_db_get_track_ctime       (HyScanDB              *db,
                                                gint32                 track_id);

/* Функции работы с каналами данных. */

/**
 *
 * Функция возвращает список каналов данных существующих в открытом галсе.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса.
 *
 * \return NULL терминированный список каналов данных или NULL - если каналов данных нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_channel_list      (HyScanDB              *db,
                                                gint32                 track_id);

/**
 *
 * Функция открывает существующий канал данных для работы.
 *
 * Функция возвращает идентификатор открытого канала данных, который должен использоваться в остальных функциях работы с каналами данных.
 *
 * В открытый этой функцией канал данных нельзя записывать данные.
 *
 * По завершению работы с каналом данных его необходимо закрыть функцией #hyscan_db_close_channel.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса;
 * \param channel_name название канала данных.
 *
 * \return Идентификатор открытого канала данных или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_channel          (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);

/**
 *
 * Функция создаёт новый канал данных и открывает его для работы.
 *
 * Функция возвращает идентификатор открытого канала данных, который должен использоваться в остальных функциях работы с каналами данных.
 *
 * По завершению работы с каналом данных его необходимо закрыть функцией #hyscan_db_close_channel.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса;
 * \param channel_name название канала данных.
 *
 * \return Идентификатор нового, открытого канала данных или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_create_channel        (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);

/**
 *
 * Функция удаляет существующий канал данных.
 *
 * Удаление возможно даже в случае использования канала данных или его параметров другими клиентами. В этом
 * случае функции работы с этим каналом данных и его параметрами будут возвращать ошибки.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса;
 * \param channel_name название канала данных.
 *
 * \return TRUE - если канал данных был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_channel        (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *channel_name);

/**
 *
 * Функция закрывает открытый ранее канал данных.
 *
 * Закрытие канала данных не влияет на открытые параметры этого канала данных. Их использование может быть продолжено.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_DB_EXPORT
void           hyscan_db_close_channel         (HyScanDB              *db,
                                                gint32                 channel_id);

/**
 *
 * Функция открывает группу параметров канала для работы.
 *
 * Функция возвращает идентификатор открытой группы параметров, который должен использоваться в остальных функциях работы с параметрами.
 *
 * По завершению работы с группой параметров её необходимо закрыть функцией #hyscan_db_close_param
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных.
 *
 * \return Идентификатор открытой группы параметров или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_channel_param    (HyScanDB              *db,
                                                gint32                 channel_id);

/**
 *
 * Функция задаёт максимальный размер файлов, хранящих данные канала, которые может создавать система.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param chunk_size максимальный размер файла в байтах.
 *
 * \return TRUE - если максимальный размер файлов изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_channel_chunk_size(HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                 chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные. Если данные
 * были записаны ранее "текущего времени" - "интервал хранения" они удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param save_time время хранения данных в микросекундах.
 *
 * \return TRUE - если время хранения данных изменено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_channel_save_time (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных превышает этот предел,
 * старые данные удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param save_size объём сохраняемых данных в байтах.
 *
 * \return TRUE - если объём сохраняемых данных изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_channel_save_size (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 save_size);

/**
 *
 * Функция переводит канал данных в режим только чтения. После вызова этой функции
 * записывать новые данные в канал нельзя.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_DB_EXPORT
void           hyscan_db_finalize_channel      (HyScanDB              *db,
                                                gint32                 channel_id);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_get_channel_data_range(HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                *first_index,
                                                gint32                *last_index);

/**
 *
 * Функция записывает новые данные в канал.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param time метка времени в микросекундах;
 * \param data указатель на записываемые данные;
 * \param size размер записываемых данных;
 * \param index указатель на переменную для сохранения индекса данных или NULL.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_add_channel_data      (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 time,
                                                gpointer               data,
                                                gint32                 size,
                                                gint32                *index);

/**
 *
 * Функция считывает записанные данные по номеру индекса.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер считанных данных.
 * Размер считанных данных может быть ограничен размером буфера. Для того чтобы определить размер данных без
 * их чтения необходимо вызвать функцию с переменной buffer = NULL.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные или NULL;
 * \param buffer_size указатель на переменную с размером области памяти для данных;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_get_channel_data      (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint32                 index,
                                                gpointer               buffer,
                                                gint32                *buffer_size,
                                                gint64                *time);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * Если найдены данные, метка времени которых точно совпадает с указанной, значения lindex и rindex, а также ltime и rtime будут одинаковые.
 *
 * Если найдены данные, метка времени которых находится в переделах записанных данных, значения lindex и ltime будут указывать на
 * индекс и время соответственно, данных, записанных перед искомым моментом времени. А rindex и ltime будут указывать на индекс и время
 * соответственно, данных, записанных после искомого момента времени.
 *
 * Если lindex == G_MININT32 или ltime == G_MININT64, то момент времени раньше чем первая запись данных.
 *
 * Если rindex == G_MAXINT32 или rtime == G_MAXINT64, то момент времени позже, чем последняя запись данных.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор открытого канала данных;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_find_channel_data     (HyScanDB              *db,
                                                gint32                 channel_id,
                                                gint64                 time,
                                                gint32                *lindex,
                                                gint32                *rindex,
                                                gint64                *ltime,
                                                gint64                *rtime);

/* Функции работы с параметрами. */

/**
 *
 * Функция возвращает список групп параметров открытого проекта.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта.
 *
 * \return NULL терминированный список групп параметров или NULL - если параметров нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_project_param_list(HyScanDB              *db,
                                                gint32                 project_id);

/**
 *
 * Функция открывает указанную группу параметров проекта для работы.
 *
 * Функция возвращает идентификатор открытой группы параметров, который должен использоваться в остальных функциях работы с параметрами.
 *
 * По завершению работы с группой параметров её необходимо закрыть функцией #hyscan_db_close_param
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта;
 * \param group_name название группы параметров.
 *
 * \return Идентификатор открытой группы параметров или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_project_param    (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *group_name);

/**
 *
 * Функция удаляет указанную группу параметров проекта.
 *
 * Удаление возможно даже в случае использования группы параметров другими клиентами. В этом
 * случае функции работы с этими параметрами будут возвращать ошибки.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор открытого проекта;
 * \param group_name название группы параметров.
 *
 * \return TRUE - если группа параметров была удалена, FALSE - в случае ошибки.
 *
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_project_param  (HyScanDB              *db,
                                                gint32                 project_id,
                                                const gchar           *group_name);

/**
 *
 * Функция возвращает список групп параметров открытого галса.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса.
 *
 * \return NULL терминированный список групп параметров или NULL - если параметров нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_track_param_list  (HyScanDB              *db,
                                                gint32                 track_id);

/**
 *
 * Функция открывает указанную группу параметров галса для работы.
 *
 * Функция возвращает идентификатор открытой группы параметров, который должен использоваться в остальных функциях работы с параметрами.
 *
 * По завершению работы с группой параметров её необходимо закрыть функцией #hyscan_db_close_param
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса;
 * \param group_name название группы параметров.
 *
 * \return Идентификатор открытой группы параметров или отрицательное число в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint32         hyscan_db_open_track_param      (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *group_name);

/**
 *
 * Функция удаляет указанную группу параметров галса.
 *
 * Удаление возможно даже в случае использования группы параметров другими клиентами. В этом
 * случае функции работы с этими параметрами будут возвращать ошибки.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор открытого галса;
 * \param group_name название группы параметров.
 *
 * \return TRUE - если группа параметров была удалена, FALSE - в случае ошибки.
 *
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_track_param    (HyScanDB              *db,
                                                gint32                 track_id,
                                                const gchar           *group_name);

/**
 *
 * Функция возвращает список названий параметров в открытой группе.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров.
 *
 * \return NULL терминированный список названий параметров или NULL - если параметров нет.
 *
 */
HYSCAN_DB_EXPORT
gchar        **hyscan_db_get_param_list        (HyScanDB              *db,
                                                gint32                 param_id);

/**
 *
 * Функция копирует параметры из одной открытой группы в другую.
 *
 * Копирование возможно только в пределах одного проекта.
 *
 * При копировании можно задать маску имён, которая будет учтена при копировании параметров (см. GPatternSpec).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param src_param_id идентификатор открытой группы параметров источника;
 * \param dst_param_id идентификатор открытой группы параметров приёмника;
 * \param mask маска имён.
 *
 * \return TRUE - если копирование успешно выполнено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_copy_param            (HyScanDB              *db,
                                                gint32                 src_param_id,
                                                gint32                 dst_param_id,
                                                const gchar           *mask);

/**
 *
 * Функция удалаяет параметры в открытой группе.
 *
 * При удалении можно задать маску имён, которая будет учтена при удалении параметров (см. GPatternSpec).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param mask маска имён.
 *
 * \return TRUE - если удаление успешно выполнено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_remove_param          (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *mask);

/**
 *
 * Функция закрывает открытую ранее группу параметров.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_DB_EXPORT
void           hyscan_db_close_param           (HyScanDB              *db,
                                                gint32                 param_id);

/**
 *
 * Функция проверяет существование указанного параметра.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return TRUE - если параметр существует, FALSE - если нет.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_has_param             (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

/**
 *
 * Функция атомарно увеличивает значение параметра на единицу.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return Новое значение параметра.
 *
 */
HYSCAN_DB_EXPORT
gint64         hyscan_db_inc_integer_param     (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

/**
 *
 * Функция устанавливает значение параметра типа integer.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра;
 * \param value значение параметра.
 *
 * \return TRUE - если значение параметра успешно установлено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_integer_param     (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gint64                 value);

/**
 *
 * Функция устанавливает значение параметра типа double.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра;
 * \param value значение параметра.
 *
 * \return TRUE - если значение параметра успешно установлено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_double_param      (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gdouble                value);

/**
 *
 * Функция устанавливает значение параметра типа boolean.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра;
 * \param value значение параметра.
 *
 * \return TRUE - если значение параметра успешно установлено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_boolean_param     (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                gboolean               value);

/**
 *
 * Функция устанавливает значение параметра типа string.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра;
 * \param value значение параметра - строка с нулём на конце.
 *
 * \return TRUE - если значение параметра успешно установлено, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_set_string_param      (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name,
                                                const gchar           *value);

/**
 *
 * Функция считывает и возвращает значение параметра типа integer.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return Значение параметра или 0 - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gint64         hyscan_db_get_integer_param     (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

/**
 *
 * Функция считывает и возвращает значение параметра типа double.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return Значение параметра или 0.0 - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gdouble        hyscan_db_get_double_param      (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

/**
 *
 * Функция считывает и возвращает значение параметра типа boolean.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return Значение параметра или FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean       hyscan_db_get_boolean_param     (HyScanDB              *db,
                                                gint32                 param_id,
                                                const gchar           *name);

/**
 *
 * Функция считывает и возвращает значение параметра типа string.
 *
 * Память выделенная под строку должна быть освобождена после использования (см. g_free).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор открытой группы параметров;
 * \param name название параметра.
 *
 * \return Значение параметра или NULL - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gchar         *hyscan_db_get_string_param     (HyScanDB               *db,
                                               gint32                  param_id,
                                               const gchar            *name);

G_END_DECLS

#endif /* __HYSCAN_DB_H__ */
