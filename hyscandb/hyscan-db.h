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
 * данными) может быть связано произвольное число параметров вида ключ – значение. Для идентификации
 * объектов в процессе работы с системой хранения, используются дескрипторы - значения больше нуля,
 * возвращаемые функциями открытия или создания объектов.
 *
 * Система может работать в двух режимах: локальном и сетевом. В локальном режиме все операции
 * выполняются в рамках процесса создавшего объект \link HyScanDB \endlink и права доступа к данным
 * контролируются на уровне файловой системы. В случае сетевого использования все операции транслируются
 * на сервер, с которым возможна параллельная работы нескольких клиентов. Права доступа в этом случае
 * контролируются сервером.
 *
 * Выбор режима работы, локальный или сетевой, осуществляется при подключении с системе хранения данных.
 * Функция #hyscan_db_get_uri возвращает строку с путём подключения к системе хранения.
 *
 * Общая схема работы с системой хранения данных следующая:
 *
 * - подключение к системе хранения данных - #hyscan_db_new
 * - выбор используемого проекта и его параметров - #hyscan_db_project_list, #hyscan_db_project_param_list
 *   -# открытие существующего проекта - #hyscan_db_project_open
 *   -# создание нового проекта - #hyscan_db_project_create
 *   -# получение даты и времени создания проекта - #hyscan_db_project_get_ctime
 *   -# открытие (создание) группы параметров - #hyscan_db_project_param_open
 * - выбор используемого галса и его параметров - #hyscan_db_track_list
 *   -# открытие существующего галса - #hyscan_db_track_open
 *   -# создание нового галса - #hyscan_db_track_create
 *   -# получение даты и времени создания галса - #hyscan_db_track_get_ctime
 *   -# открытие группы параметров - #hyscan_db_track_param_open
 * - выбор используемого канала данных - #hyscan_db_channel_list
 *   -# открытие существующего канала данных - #hyscan_db_channel_open
 *   -# создание нового канала данных #hyscan_db_channel_create
 *   -# получение даты и времени создания канала данных - #hyscan_db_channel_get_ctime
 *   -# открытие группы параметров - #hyscan_db_channel_param_open
 * - работа с данными
 *   -# запись данных - #hyscan_db_channel_add_data
 *   -# чтение данных - #hyscan_db_channel_get_data
 *   -# чтение размера данных - #hyscan_db_channel_get_data_size
 *   -# чтение метки времени данных - #hyscan_db_channel_get_data_time
 *   -# поиск данных по времени - #hyscan_db_channel_find_data
 *   -# получение диапазона доступных данных - #hyscan_db_channel_get_data_range
 * - работа с параметрами
 *   -# получение списка объектов в группе параметров - #hyscan_db_param_object_list
 *   -# создание объекта в группе параметров - #hyscan_db_param_object_create
 *   -# удаление объектов в группе параметров - #hyscan_db_param_object_remove
 *   -# установка значения параметров - #hyscan_db_param_set
 *   -# чтение значения параметров - #hyscan_db_param_get
 * - удаление неиспользуемых объектов
 *   -# канала данных - #hyscan_db_channel_remove
 *   -# галса - #hyscan_db_track_remove
 *   -# проекта - #hyscan_db_project_remove
 *   -# группы параметров проекта - #hyscan_db_project_param_remove
 * - закрытие объектов системы хранения - #hyscan_db_close
 * - отключение от системы хранения данных осуществляется при удалении объекта функцией g_object_unref
 *
 * Проверить существование проекта, галса или канала данных можно функцией #hyscan_db_is_exist.
 *
 * При изменении объектов в базе данных меняются внутренние счётчики состояния объектов. Эти счётчики
 * можно использовать для слежения за изменениями в базе без создания на неё дополнительной нагрузки.
 * Для этого используется функция #hyscan_db_get_mod_count.
 *
 * Все названия проектов, галсов, каналов данных и групп параметров должны быть в кодировке UTF-8.
 *
 * При создании проектов и галсов передаётся описание схемы параметров в виде XML данных. Описание
 * формата схемы см. в \link HyScanDataSchema \endlink. Если параметры в проектах или галсах не
 * используются, схему данных можно не передавать.
 *
 * Параметры проекта можно объединять в группы. Каждая группа может содержать произвольное число объектов.
 * При создании объекта необходимо указать идентификатор схемы данных. Этот идентификатор должен содержаться
 * в описании схемы данных, переданной при создании проекта.
 *
 * При создании галса и канала данных необходимо указать идентификатор схемы данных. Этот идентификатор
 * должен содержаться в описании схемы данных, переданной при создании галса. В этом случае параметры
 * создаются автоматически. Для открытия этих параметров предназначены функции #hyscan_db_track_param_open
 * и #hyscan_db_channel_param_open. При чтении/записи этих параметров, название объекта должно быть равно NULL.
 *
 * Если пользователь явным образом не установил значение параметра, будет использоваться значение по
 * умолчанию, определённое в схеме.
 *
 * Названия параметров и типы значений должны подчиняться правилам \link HyScanDataSchema \endlink.
 *
 * Система хранения данных спроектирована таким образом, что не допускает запись в уже существующий канал данных.
 * Таким образом, запись данных возможна ТОЛЬКО во вновь созданный канал - #hyscan_db_channel_create. Аналогично,
 * установка значений параметров, возможна ТОЛЬКО для вновь созданного канала и только если параметры открыты
 * из дескриптора имеющего права на запись данных. Функция #hyscan_db_channel_finalize принудительно переводит
 * канал данных в режим только чтения. После вызова этой функции запись новых данных и изменение параметров невозможно.
 *
 * Узнать в каком режиме находится канал данных (чтение или запись) можно функцией #hyscan_db_channel_is_writable.
 *
 * Записываемые в канал данные обычно сохраняются на диске в виде файлов. Слишком большой размер файлов создаёт
 * проблемы с их копированием и хранением в некоторых файловых системах. Функция #hyscan_db_channel_set_chunk_size
 * даёт указание системе хранения ограничить размер файлов указанным размером. Если объём данных больше, чем
 * максимальный размер файла, система должна создать следующий файл, логически объединённый с предыдущим.
 * По умолчанию размер файла равен 1 Гб. Необходимо иметь в виду, что задаваемый параметр только ограничивает
 * размер файла. Действительный размер файла может быть меньше.
 *
 * Необходимо иметь в виду, что размер файла одновременно накладывает ограничения на максимальный размер
 * одной записи в канале данных.
 *
 * При записи данных в канал имеется возможность ограничить объём или время хранения данных. Для этого
 * используются функции #hyscan_db_channel_set_save_time и #hyscan_db_channel_set_save_size. Данные функции,
 * дают указание системе хранения удалять данные, которые старше указанного периода времени или превышают
 * определённый объём. Удаление данных допускается проводить блоками, сразу по несколько записей. Таким
 * образом, возможна ситуация временного превышения лимитов хранения (данные хранятся дольше или данных записано больше),
 * но не наоборот.
 *
 * Основной объём информации записывается в каналы данных. Каналы данных спроектированы таким образом, чтобы
 * одновременно с хранением информации хранить метку времени. Не допускается запись данных с меткой времени
 * равной или старше уже записанной. Каждому блоку данных при записи присваивается уникальный (в рамках канала данных)
 * номер - индекс. Чтение данных возможно только по этому индексу. Для поиска необходимых данных по времени предназначена
 * функция #hyscan_db_channel_find_data, которая возвращает индекс (или индексы при неточном совпадении метки времени).
 *
 * Метки времени должны задаваться в микросекундах, но в действительности сохраняются как 64-х битное целое число со знаком.
 * Это даёт возможность клиенту интерпретировать моменты времени собственным способом.
 *
 * Если для канала данных задано ограничение по времени или объёму хранения, возможна ситуация удаления ранних записей.
 * Для того, чтобы определить границы доступных записей предназначена функция #hyscan_db_channel_get_data_range.
 * Необходимо иметь в виду, что между моментом времени определения границ записей и моментом чтения данных
 * эти границы могут измениться
 *
 * В процессе работы любой из клиентов системы хранения может произвольно создавать и удалять любые объекты
 * (проекты, галсы, каналы данных, группы параметров). Таким образом уже открытые объекты могут перестать существовать.
 * Признаком этого будет ошибка, при выполнении действий с этим объектом.  Эти ошибки должны правильным образом
 * обрабатываться клиентом (закрытие объекта, исключение из дальнейшей обработки и т.п.).
 *
 */

#ifndef __HYSCAN_DB_H__
#define __HYSCAN_DB_H__

#include <hyscan-buffer.h>
#include <hyscan-param-list.h>
#include <hyscan-data-schema.h>

G_BEGIN_DECLS

/** \brief Статус поиска записи. */
typedef enum
{
  HYSCAN_DB_FIND_OK          = 0,              /**< Запись найдена. */
  HYSCAN_DB_FIND_FAIL        = 1,              /**< Ошибка поиска. */
  HYSCAN_DB_FIND_LESS        = 2,              /**< Запись не найдена, метка времени раньше записанных данных. */
  HYSCAN_DB_FIND_GREATER     = 3               /**< Запись не найдена, метка времени позже записанных данных. */
} HyScanDBFindStatus;

#define HYSCAN_TYPE_DB            (hyscan_db_get_type ())
#define HYSCAN_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB, HyScanDB))
#define HYSCAN_IS_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB))
#define HYSCAN_DB_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_DB, HyScanDBInterface))

typedef struct _HyScanDB HyScanDB;

typedef struct _HyScanDBInterface HyScanDBInterface;

struct _HyScanDBInterface
{
  GTypeInterface g_iface;

  gchar               *(*get_uri)                              (HyScanDB              *db);

  guint32              (*get_mod_count)                        (HyScanDB              *db,
                                                                gint32                 id);

  gboolean             (*is_exist)                             (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                const gchar           *channel_name);

  gchar              **(*project_list)                         (HyScanDB              *db);

  gint32               (*project_open)                         (HyScanDB              *db,
                                                                const gchar           *project_name);

  gint32               (*project_create)                       (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *project_schema);

  gboolean             (*project_remove)                       (HyScanDB              *db,
                                                                const gchar           *project_name);

  GDateTime           *(*project_get_ctime)                    (HyScanDB              *db,
                                                                gint32                 project_id);

  gchar              **(*project_param_list)                   (HyScanDB              *db,
                                                                gint32                 project_id);

  gint32               (*project_param_open)                   (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

  gboolean             (*project_param_remove)                 (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

  gchar              **(*track_list)                           (HyScanDB              *db,
                                                                gint32                 project_id);

  gint32               (*track_open)                           (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

  gint32               (*track_create)                         (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name,
                                                                const gchar           *track_schema,
                                                                const gchar           *schema_id);

  gboolean             (*track_remove)                         (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

  GDateTime           *(*track_get_ctime)                      (HyScanDB              *db,
                                                                gint32                 track_id);

  gint32               (*track_param_open)                     (HyScanDB              *db,
                                                                gint32                 track_id);

  gchar              **(*channel_list)                         (HyScanDB              *db,
                                                                gint32                 track_id);

  gint32               (*channel_open)                         (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

  gint32               (*channel_create)                       (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name,
                                                                const gchar           *schema_id);

  gboolean             (*channel_remove)                       (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

  GDateTime           *(*channel_get_ctime)                    (HyScanDB              *db,
                                                                gint32                 channel_id);

  void                 (*channel_finalize)                     (HyScanDB              *db,
                                                                gint32                 channel_id);

  gboolean             (*channel_is_writable)                  (HyScanDB              *db,
                                                                gint32                 channel_id);

  gint32               (*channel_param_open)                   (HyScanDB              *db,
                                                                gint32                 channel_id);

  gboolean             (*channel_set_chunk_size)               (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                chunk_size);

  gboolean             (*channel_set_save_time)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 save_time);

  gboolean             (*channel_set_save_size)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                save_size);

  gboolean             (*channel_get_data_range)               (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

  gboolean             (*channel_add_data)                     (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                HyScanBuffer          *buffer,
                                                                guint32               *index);

  gboolean             (*channel_get_data)                     (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index,
                                                                HyScanBuffer          *buffer,
                                                                gint64                *time);

  guint32              (*channel_get_data_size)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

  gint64               (*channel_get_data_time)                (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

  gint32               (*channel_find_data)                    (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

  gchar              **(*param_object_list)                    (HyScanDB              *db,
                                                                gint32                 param_id);

  gboolean             (*param_object_create)                  (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

  gboolean             (*param_object_remove)                  (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

  HyScanDataSchema    *(*param_object_get_schema)              (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

  gboolean             (*param_set)                            (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

  gboolean             (*param_get)                            (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

  void                 (*close)                                (HyScanDB              *db,
                                                                gint32                 id);
};

HYSCAN_API
GType          hyscan_db_get_type              (void);

/* Общие функции. */

/**
 *
 * Функция подключается к системе хранения данных.
 *
 * Параметры подключения определяются строкой uri. Формат строки следующий:
 * "type://<user>:<password>@path", где:
 * - type - тип подключения к системе хранения (file, tcp, shm);
 * - user - имя пользователя;
 * - password - пароль пользователя;
 * - path - путь к каталогу с проектами (для типа подключения file) или адрес сервера (для tcp и shm).
 *
 * Параметры user и password являются опциональными и требуют задания только при подключении к серверу
 * на котором используется система аутентификации.
 *
 * \param uri адрес подключения.
 *
 * \return Указатель на интерфейс \link HyScanDB \endlink или NULL.
 *
 */
HYSCAN_API
HyScanDB              *hyscan_db_new                           (const gchar           *uri);

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
HYSCAN_API
gchar                 *hyscan_db_get_uri                       (HyScanDB              *db);

/**
 *
 * Функция возвращает номер изменения в объекте (проекте, галсе, канале данных или группе параметров).
 * При любом изменении в объекте увеличивается номер изменения. Например, при создании галса изменится
 * номер в проекте (если он открыт), при добавлении данных в канал изменится номер в канале и т.п.
 *
 * Функцию полезно использовать для проверки наличия изменений в базе. Если изменения есть, то можно
 * считать из базы новые данные. Эта функция значительно снижает нагрузку на базу данных, так как
 * не требуется постоянно считывать данные и анализировать их на предмет изменений.
 *
 * Программа не должна полагаться на значение номера изменения, важен только факт смены номера по
 * сравнению с предыдущим запросом.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param id дескриптор объекта или 0 для слежения за списком проектов.
 *
 * \return Номер изменения.
 *
 */
HYSCAN_API
guint32                hyscan_db_get_mod_count                 (HyScanDB              *db,
                                                                gint32                 id);

/**
 *
 * Функция проверяет существование проекта, галса или канала данных в системе хранения.
 *
 * Если необходимо проверить существование проекта, в project_name нужно передать название проекта,
 * а track_name и channel_name нужно установить в NULL. Если необходимо проверить существования галса,
 * в дополнение к project_name в track_name нужно передать название галса, а channel_name нужно
 * установить в NULL. Для проверки существования канала данных необходимо передать все параметры.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название проекта или NULL;
 * \param channel_name название проекта или NULL.
 *
 * \return TRUE - если объект существует, FALSE - если объекта нет.
 *
 */
HYSCAN_API
gboolean               hyscan_db_is_exist                      (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                const gchar           *channel_name);

/* Функции работы с проектами. */

/**
 *
 * Функция возвращает список проектов в системе хранения.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return NULL терминированный список проектов или NULL - если проектов нет.
 *
 */
HYSCAN_API
gchar                **hyscan_db_project_list                  (HyScanDB              *db);

/**
 *
 * Функция открывает проект.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с проектами. По завершению работы с проектом, его необходимо закрыть функцией #hyscan_db_close.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта.
 *
 * \return Идентификатор проекта больше нуля или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_project_open                  (HyScanDB              *db,
                                                                const gchar           *project_name);

/**
 *
 * Функция создаёт новый проект и открывает его.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с проектами. Если проект существует, функция вернёт значение ноль. По завершению работы с проектом,
 * его необходимо закрыть функцией #hyscan_db_close.
 *
 * Если в проекте будут использоваться параметры, необходимо передать их описание в виде XML данных.
 * Если параметры использоваться не будут, можно передать NULL. Подробно формат схем описан в
 * \link HyScanDataSchema \endlink.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param project_schema описание схемы параметров проекта в виде XML или NULL.
 *
 * \return Идентификатор проекта, ноль или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_project_create                (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *project_schema);

/**
 *
 * Функция удаляет проект.
 *
 * Удаление возможно даже в случае использования проекта или его объектов другими клиентами.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_name название проекта.
 *
 * \return TRUE - если проект был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_project_remove                (HyScanDB              *db,
                                                                const gchar           *project_name);

/**
 *
 * Функция возвращает информацию о дате и времени создания проекта.
 *
 * Память выделенная под объект GDateTime должна быть освобождена после использования (см. g_date_time_unref).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта.
 *
 * \return Указатель на объект GDateTime или NULL в случае ошибки.
 *
 */
HYSCAN_API
GDateTime             *hyscan_db_project_get_ctime             (HyScanDB              *db,
                                                                gint32                 project_id);

/**
 *
 * Функция возвращает список групп параметров проекта.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта.
 *
 * \return NULL терминированный список групп параметров или NULL - если параметров нет.
 *
 */
HYSCAN_API
gchar                **hyscan_db_project_param_list            (HyScanDB              *db,
                                                                gint32                 project_id);

/**
 *
 * Функция открывает группу параметров проекта.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с параметрами. Если указанной группы параметров нет, она автоматически создаётся. По завершению
 * работы с группой параметров, её необходимо закрыть функцией #hyscan_db_close.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта;
 * \param group_name название группы параметров.
 *
 * \return Идентификатор группы параметров проекта или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_project_param_open            (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

/**
 *
 * Функция удаляет группу параметров проекта.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта;
 * \param group_name название группы параметров.
 *
 * \return TRUE - если группа параметров была удалёна, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_project_param_remove          (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *group_name);

/* Функции работы с галсами. */

/**
 *
 * Функция возвращает список галсов в проекте.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта.
 *
 * \return NULL терминированный список галсов или NULL - если галсов нет.
 *
 */
HYSCAN_API
gchar                **hyscan_db_track_list                    (HyScanDB              *db,
                                                                gint32                 project_id);

/**
 *
 * Функция открывает галс.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с галсами. По завершению работы с галсом, его необходимо закрыть функцией #hyscan_db_close.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта;
 * \param track_name название галса.
 *
 * \return Идентификатор галса больше нуля или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_track_open                    (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

/**
 *
 * Функция создаёт галс и открывает его.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с галсами. Если галс существует функция, вернёт значение ноль. По завершению работы с галсом,
 * его необходимо закрыть функцией #hyscan_db_close.
 *
 * Если в галсе или каналах данных будут использоваться параметры, необходимо передать
 * их описание в виде XML данных. Если параметры использоваться не будут, можно передать NULL.
 * Подробно формат схем описан в \link HyScanDataSchema \endlink.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта;
 * \param track_name название галса;
 * \param track_schema описание схемы параметров галса в виде XML или NULL;
 * \param schema_id идентификатор схемы параметров галса или NULL.
 *
 * \return Идентификатор галса, ноль или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_track_create                  (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name,
                                                                const gchar           *track_schema,
                                                                const gchar           *schema_id);

/**
 *
 * Функция удаляет галс.
 *
 * Удаление возможно даже в случае использования галса или его объектов другими клиентами.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param project_id идентификатор проекта;
 * \param track_name название галса.
 *
 * \return TRUE - если галс был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_track_remove                  (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

/**
 *
 * Функция возвращает информацию о дате и времени создания галса.
 *
 * Память выделенная под объект GDateTime должна быть освобождена после использования (см. g_date_time_unref).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса.
 *
 * \return Указатель на объект GDateTime или NULL в случае ошибки.
 *
 */
HYSCAN_API
GDateTime             *hyscan_db_track_get_ctime               (HyScanDB              *db,
                                                                gint32                 track_id);

/**
 *
 * Функция открывает группу параметров галса.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с параметрами. Функция возвращает действительный идентификатор даже если параметров галса нет.
 * В этом случае считать или записать значения параметров будет невозможно.
 *
 * Значения параметров можно изменять, если для их открытии использовался идентификатор галса,
 * полученный при его создании. Если открывался уже созданный галс, изменить значения параметров
 * будет нельзя.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса.
 *
 * \return Идентификатор группы параметров галса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_track_param_open              (HyScanDB              *db,
                                                                gint32                 track_id);

/* Функции работы с каналами данных. */

/**
 *
 * Функция возвращает список каналов данных в галсе.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса.
 *
 * \return NULL терминированный список каналов данных или NULL - если каналов данных нет.
 *
 */
HYSCAN_API
gchar                **hyscan_db_channel_list                  (HyScanDB              *db,
                                                                gint32                 track_id);

/**
 *
 * Функция открывает канал данных.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с каналами данных. В канал данных, открытый этой функцией, нельзя записывать данные. По завершению
 * работы с каналом данных его необходимо закрыть функцией #hyscan_db_close.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса;
 * \param channel_name название канала данных.
 *
 * \return Идентификатор канала данных больше нуля или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_channel_open                  (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

/**
 *
 * Функция создаёт новый канал данных и открывает его для работы.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с каналами данных. Если канал данных существует, функция вернёт значение ноль. По завершению работы
 * с каналом данных, его необходимо закрыть функцией #hyscan_db_close.
 *
 * При создании канала данных можно указать идентификатор схемы параметров.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса;
 * \param channel_name название канала данных;
 * \param schema_id идентификатор схемы параметров канала данных или NULL.
 *
 * \return Идентификатор канала данных, ноль или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_channel_create                (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name,
                                                                const gchar           *schema_id);

/**
 *
 * Функция удаляет канал данных.
 *
 * Удаление возможно даже в случае использования канала данных или его параметров другими клиентами.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param track_id идентификатор галса;
 * \param channel_name название канала данных.
 *
 * \return TRUE - если канал данных был удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_remove                (HyScanDB              *db,
                                                                gint32                 track_id,
                                                                const gchar           *channel_name);

/**
 *
 * Функция возвращает информацию о дате и времени создания канала данных.
 *
 * Память выделенная под объект GDateTime должна быть освобождена после использования (см. g_date_time_unref).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных.
 *
 * \return Указатель на объект GDateTime или NULL в случае ошибки.
 *
 */
HYSCAN_API
GDateTime             *hyscan_db_channel_get_ctime             (HyScanDB              *db,
                                                                gint32                 channel_id);

/**
 *
 * Функция переводит канал данных в режим только чтения. После вызова этой функции
 * записывать новые данные в канал и изменять его параметры нельзя.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_API
void                   hyscan_db_channel_finalize              (HyScanDB              *db,
                                                                gint32                 channel_id);

/**
 *
 * Функция возвращает режим доступа к каналу данных. Функция проверяет режим
 * доступа глобально в рамках системы хранения, а не для идентификатора канала данных.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных.
 *
 * \return TRUE - если возможна запись данных, FALSE - если канал в режиме только чтения.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_is_writable           (HyScanDB              *db,
                                                                gint32                 channel_id);

/**
 *
 * Функция открывает группу параметров канала данных.
 *
 * Функция возвращает идентификатор, который должен использоваться в остальных функциях работы
 * с параметрами. Функция возвращает действительный идентификатор даже если параметров канала данных нет.
 * В этом случае считать или записать значения параметров будет невозможно.
 *
 * Значения параметров можно изменять, если для их открытии использовался идентификатор канала
 * данных, полученный при его создании. Если открывался уже созданный канал данных, изменить
 * значения параметров будет нельзя.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных.
 *
 * \return Идентификатор группы параметров канала данных или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint32                 hyscan_db_channel_param_open            (HyScanDB              *db,
                                                                gint32                 channel_id);

/**
 *
 * Функция задаёт максимальный размер файлов, хранящих данные канала, которые может создавать система.
 *
 * Если для максимального размера указан ноль, будет восстановлено значение по умолчанию.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param chunk_size максимальный размер файла в байтах.
 *
 * \return TRUE - если максимальный размер файлов изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_set_chunk_size        (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные. Если данные
 * были записаны ранее "текущего времени" - "интервал хранения" они удаляются. Подробнее об этом
 * можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * Если для времени указан ноль, будет восстановлено значение по умолчанию.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param save_time время хранения данных в микросекундах.
 *
 * \return TRUE - если время хранения данных изменено, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_set_save_time         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных превышает этот предел,
 * старые данные удаляются. Подробнее об этом можно прочитать в описании интерфейса
 * \link HyScanDB \endlink.
 *
 * Если для объёма указан ноль, будет восстановлено значение по умолчанию.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param save_size объём сохраняемых данных в байтах.
 *
 * \return TRUE - если объём сохраняемых данных изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_set_save_size         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint64                save_size);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей. Функция вернёт FALSE если записей в канале нет.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_get_data_range        (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

/**
 *
 * Функция записывает новые данные в канал.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param time метка времени в микросекундах;
 * \param buffer указатель на буфер данных;
 * \param index указатель на переменную для сохранения индекса данных или NULL.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_add_data              (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                HyScanBuffer          *buffer,
                                                                guint32               *index);

/**
 *
 * Функция считывает записанные данные по номеру индекса.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param index индекс считываемых данных;
 * \param buffer указатель на буфер данных;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_channel_get_data              (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index,
                                                                HyScanBuffer          *buffer,
                                                                gint64                *time);

/**
 *
 * Функция считывает размер данных по номеру индекса.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param index индекс данных.
 *
 * \return Размер данных или ноль.
 *
 */
HYSCAN_API
guint32                hyscan_db_channel_get_data_size         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

/**
 *
 * Функция считывает метку времени данных по номеру индекса.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param index индекс данных.
 *
 * \return Метка времени или отрицательное число.
 *
 */
HYSCAN_API
gint64                 hyscan_db_channel_get_data_time         (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                guint32                index);

/**
 *
 * Функция ищет индекс данных для указанного момента времени. Функция возвращает статус поиска
 * индекса данных \link HyScanDBFindStatus \endlink.
 *
 * Если найдены данные, метка времени которых точно совпадает с указанной, значения lindex и rindex,
 * а также ltime и rtime будут одинаковые.
 *
 * Если найдены данные, метка времени которых находится в переделах записанных данных, значения
 * lindex и ltime будут указывать на индекс и время соответственно, данных, записанных перед искомым
 * моментом времени. А rindex и ltime будут указывать на индекс и время соответственно, данных, записанных
 * после искомого момента времени.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param channel_id идентификатор канала данных;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return Статус поиска индекса данных.
 *
 */
HYSCAN_API
HyScanDBFindStatus     hyscan_db_channel_find_data             (HyScanDB              *db,
                                                                gint32                 channel_id,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

/* Функции работы с параметрами. */

/**
 *
 * Функция возвращает список объектов в группе параметров проекта.
 *
 * Память выделенная под список должна быть освобождена после использования (см. g_strfreev).
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров.
 *
 * \return NULL терминированный список групп параметров или NULL - если параметров нет.
 *
 */
HYSCAN_API
gchar                **hyscan_db_param_object_list             (HyScanDB              *db,
                                                                gint32                 param_id);

/**
 *
 * Функция создаёт объект в группе параметров проекта. Если объект с указанным
 * именем уже существует, функция вернёт FALSE.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров;
 * \param object_name название объекта;
 * \param schema_id схема данных объекта.
 *
 * \return TRUE - если объект успешно создан, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_param_object_create           (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                const gchar           *schema_id);

/**
 *
 * Функция удаляет объект из группы параметров проекта.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров;
 * \param object_name название объекта.
 *
 * \return TRUE - если объект успешно удалён, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_param_object_remove           (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

/**
 *
 * Функция возвращает описание схемы данных для объекта группы параметров.
 *
 * Для описание схемы данных используется класс \link HyScanDataSchema \endlink.
 * По окончании использования пользователь должен освободить возвращаемый
 * объект функцией g_object_unref.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров;
 * \param object_name название объекта.
 *
 * \return Указатель на объект \link HyScanDataSchema \endlink или NULL если объекта нет.
 *
 */
HYSCAN_API
HyScanDataSchema      *hyscan_db_param_object_get_schema       (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name);

/**
 *
 * Функция устанавливает значения параметров объекта. Установка значений
 * нескольких параметров производится атомарно.
 *
 * При установке значений параметров галсов или каналов данных, название объекта
 * должно быть NULL.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров;
 * \param object_name название объекта;
 * \param param_list список параметров для записи.
 *
 * \return TRUE - если значения установлены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_param_set                     (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

/**
 *
 * Функция считывает значение параметра объекта. Чтение значений
 * нескольких параметров производится атомарно.
 *
 * При считывании значений параметров галсов или каналов данных, название объекта
 * должно быть NULL.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param param_id идентификатор группы параметров;
 * \param object_name название объекта;
 * \param param_list список параметров для чтения.
 *
 * \return TRUE - если значения считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_db_param_get                     (HyScanDB              *db,
                                                                gint32                 param_id,
                                                                const gchar           *object_name,
                                                                HyScanParamList       *param_list);

/**
 *
 * Функция закрывает объект системы хранения. Данная функция используется для закрытия
 * любых объектов системы хранения, открытых ранее.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink;
 * \param id идентификатор объекта.
 *
 * \return Функция не возвращает значений.
 *
 */
HYSCAN_API
void                   hyscan_db_close                         (HyScanDB              *db,
                                                                gint32                 id);

G_END_DECLS

#endif /* __HYSCAN_DB_H__ */
