/* hyscan-db.c
 *
 * Copyright 2015-2020 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanDB.
 *
 * HyScanDB is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanDB имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanDB на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-db
 * @Short_description: интерфейс базы данных HyScan
 * @Title: HyScanDB
 *
 * Интерфейс #HyScanDB определяет модель взаимодействия с системой хранения
 * данных. Система реализует иерархическую структуру модели данных. Корневыми
 * элементами являются проекты, каждый из которых может хранить произвольное
 * число галсов. В каждом галсе могут храниться данные от множества источников
 * информации - каналов данных. С каждым из объектов (проектом, галсом или
 * данными) может быть связано произвольное число параметров вида ключ – значение.
 * Для идентификации объектов в процессе работы с системой хранения,
 * используются дескрипторы - значения больше нуля, возвращаемые функциями
 * открытия или создания объектов.
 *
 * Система может работать в двух режимах: локальном и сетевом. В локальном
 * режиме все операции выполняются в рамках процесса создавшего объект #HyScanDB
 * и права доступа к данным контролируются на уровне файловой системы. В случае
 * сетевого использования все операции транслируются на сервер, с которым
 * возможна параллельная работы нескольких клиентов. Права доступа в этом случае
 * контролируются сервером.
 *
 * Выбор режима работы, локальный или сетевой, осуществляется при подключении с
 * системе хранения данных. Функция #hyscan_db_get_uri возвращает строку с
 * текущим путём подключения к системе хранения.
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
 * Проверить существование проекта, галса или канала данных можно функцией
 * #hyscan_db_is_exist.
 *
 * При изменении объектов в базе данных меняются внутренние счётчики состояния
 * объектов. Эти счётчики можно использовать для слежения за изменениями в базе
 * без создания на неё дополнительной нагрузки. Для этого используется функция
 * #hyscan_db_get_mod_count.
 *
 * Все названия проектов, галсов, каналов данных и групп параметров должны быть
 * в кодировке UTF-8.
 *
 * При создании проектов и галсов передаётся описание схемы параметров в виде XML
 * данных. Описание формата схемы см. в #HyScanDataSchema. Если параметры в
 * проектах или галсах не используются, схему данных можно не передавать.
 *
 * Параметры проекта можно объединять в группы. Каждая группа может содержать
 * произвольное число объектов. При создании объекта необходимо указать
 * идентификатор схемы данных. Этот идентификатор должен содержаться  в
 * описании схемы данных, переданной при создании проекта.
 *
 * При создании галса и канала данных необходимо указать идентификатор схемы
 * данных. Этот идентификатор должен содержаться в описании схемы данных,
 * переданной при создании галса. В этом случае параметры создаются автоматически.
 * Для открытия этих параметров предназначены функции #hyscan_db_track_param_open
 * и #hyscan_db_channel_param_open. При чтении/записи этих параметров,
 * название объекта должно быть равно %NULL.
 *
 * Если пользователь явным образом не установил значение параметра, будет
 * использоваться значение по умолчанию, определённое в схеме.
 *
 * Названия параметров и типы значений должны подчиняться правилам
 * #HyScanDataSchema.
 *
 * Система хранения данных спроектирована таким образом, что не допускает
 * запись в уже существующий канал данных. Таким образом, запись данных
 * возможна ТОЛЬКО во вновь созданный канал - #hyscan_db_channel_create.
 * Аналогично, установка значений параметров, возможна ТОЛЬКО для вновь
 * созданного канала и только если параметры открыты из дескриптора имеющего
 * права на запись данных. Функция #hyscan_db_channel_finalize принудительно
 * переводит канал данных в режим только чтения. После вызова этой функции
 * запись новых данных и изменение параметров невозможно.
 *
 * Узнать в каком режиме находится канал данных (чтение или запись) можно
 * функцией #hyscan_db_channel_is_writable.
 *
 * Записываемые в канал данные обычно сохраняются на диске в виде файлов.
 * Слишком большой размер файлов создаёт проблемы с их копированием и хранением
 * в некоторых файловых системах. Функция #hyscan_db_channel_set_chunk_size
 * даёт указание системе хранения ограничить размер файлов указанным размером.
 * Если объём данных больше, чем максимальный размер файла, система должна
 * создать следующий файл, логически объединённый с предыдущим. По умолчанию
 * размер файла равен 1 Гб. Необходимо иметь в виду, что задаваемый параметр
 * только ограничивает размер файла. Действительный размер файла может быть
 * меньше.
 *
 * Необходимо иметь в виду, что размер файла одновременно накладывает
 * ограничения на максимальный размер одной записи в канале данных.
 *
 * При записи данных в канал имеется возможность ограничить объём или время
 * хранения данных. Для этого используются функции #hyscan_db_channel_set_save_time
 * и #hyscan_db_channel_set_save_size. Данные функции, дают указание системе
 * хранения удалять данные, которые старше указанного периода времени или
 * превышают определённый объём. Удаление данных допускается проводить блоками,
 * сразу по несколько записей. Таким образом, возможна ситуация временного
 * превышения лимитов хранения (данные хранятся дольше или данных записано
 * больше), но не наоборот.
 *
 * Основной объём информации записывается в каналы данных. Каналы данных
 * спроектированы таким образом, чтобы одновременно с хранением информации
 * хранить метку времени. Не допускается запись данных с меткой времени
 * равной или старше уже записанной. Каждому блоку данных при записи
 * присваивается уникальный (в рамках канала данных) номер - индекс. Чтение
 * данных возможно только по этому индексу. Для поиска необходимых данных
 * по времени предназначена функция #hyscan_db_channel_find_data, которая
 * возвращает индекс (или индексы при неточном совпадении метки времени).
 *
 * Метки времени должны задаваться в микросекундах, но в действительности
 * сохраняются как 64-х битное целое число со знаком. Это даёт возможность
 * клиенту интерпретировать моменты времени собственным способом.
 *
 * Если для канала данных задано ограничение по времени или объёму хранения,
 * возможна ситуация удаления ранних записей. Для того, чтобы определить
 * границы доступных записей предназначена функция #hyscan_db_channel_get_data_range.
 * Необходимо иметь в виду, что между моментом времени определения границ
 * записей и моментом чтения данных эти границы могут измениться
 *
 * В процессе работы любой из клиентов системы хранения может произвольно
 * создавать и удалять любые объекты (проекты, галсы, каналы данных, группы
 * параметров). Таким образом уже открытые объекты могут перестать
 * существовать. Признаком этого будет ошибка, при выполнении действий с
 * этим объектом.  Эти ошибки должны правильным образом обрабатываться
 * клиентом (закрытие объекта, исключение из дальнейшей обработки и т.п.).
 */

#include "hyscan-db.h"
#include <string.h>

G_DEFINE_INTERFACE (HyScanDB, hyscan_db, G_TYPE_OBJECT);

static void
hyscan_db_default_init (HyScanDBInterface *iface)
{
}

/**
 * hyscan_db_get_uri:
 * @db: указатель на #HyScanDB
 *
 * Функция возвращает строку uri с путём подключения к системе хранения.
 *
 * Returns: (nullable): Строка uri или %NULL в случае
 * ошибки. Для удаления #g_free.
 */
gchar *
hyscan_db_get_uri (HyScanDB *db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
 if (iface->get_uri != NULL)
    return iface->get_uri (db);

  return NULL;
}

/**
 * hyscan_db_get_mod_count:
 * @db: указатель на #HyScanDB
 * @id: дескриптор объекта или 0 для слежения за списком проектов
 *
 * Функция возвращает номер изменения в объекте (проекте, галсе, канале
 * данных или группе параметров). При любом изменении в объекте
 * увеличивается номер изменения. Например, при создании галса изменится
 * номер в проекте (если он открыт), при добавлении данных в канал
 * изменится номер в канале и т.п.
 *
 * Функцию полезно использовать для проверки наличия изменений в базе.
 * Если изменения есть, то можно считать из базы новые данные. Эта функция
 * значительно снижает нагрузку на базу данных, так как не требуется
 * постоянно считывать данные и анализировать их на предмет изменений.
 *
 * Программа не должна полагаться на значение номера изменения, важен
 * только факт смены номера по сравнению с предыдущим запросом.
 *
 * Returns: Номер изменения.
 */
guint32
hyscan_db_get_mod_count (HyScanDB *db,
                         gint32    id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), 0);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->get_mod_count != NULL)
    return iface->get_mod_count (db, id);

  return 0;
}

/**
 * hyscan_db_is_exist:
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 * @track_name: (nullable): название проекта
 * @channel_name: (nullable): название проекта
 *
 * Функция проверяет существование проекта, галса или канала
 * данных в системе хранения.
 *
 * Если необходимо проверить существование проекта, в project_name
 * нужно передать название проекта, а track_name и channel_name нужно
 * установить в %NULL. Если необходимо проверить существования галса,
 * в дополнение к project_name в track_name нужно передать название
 * галса, а channel_name нужно установить в %NULL. Для проверки
 * существования канала данных необходимо передать все параметры.
 *
 * Returns: %TRUE - если объект существует, иначе %FALSE.
 */
gboolean
hyscan_db_is_exist (HyScanDB    *db,
                    const gchar *project_name,
                    const gchar *track_name,
                    const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->is_exist != NULL)
    return iface->is_exist (db, project_name, track_name, channel_name);

  return FALSE;
}

/**
 * hyscan_db_project_list:
 * @db: указатель на #HyScanDB
 *
 * Функция возвращает список проектов в системе хранения.
 *
 * Returns: (nullable): %NULL терминированный список проектов
 * или %NULL - если проектов нет. Для удаления #g_strfreev.
 */
gchar **
hyscan_db_project_list (HyScanDB * db)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_list != NULL)
    return iface->project_list (db);

  return NULL;
}

/**
 * hyscan_db_project_open:
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 *
 * Функция открывает проект.
 *
 * Функция возвращает идентификатор, который должен использоваться
 * в остальных функциях работы с проектами. По завершению работы с
 * проектом, его необходимо закрыть функцией #hyscan_db_close.
 *
 * Returns: Идентификатор проекта больше нуля или отрицательное
 * число в случае ошибки.
 */
gint32
hyscan_db_project_open (HyScanDB    *db,
                        const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_open != NULL)
    return iface->project_open (db, project_name);

  return -1;
}

/**
 * hyscan_db_project_create:
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 * @project_schema: (nullable): описание схемы параметров проекта в виде XML
 *
 * Функция создаёт новый проект и открывает его.
 *
 * Функция возвращает идентификатор, который должен использоваться
 * в остальных функциях работы с проектами. Если проект существует,
 * функция вернёт значение ноль. По завершению работы с проектом,
 * его необходимо закрыть функцией #hyscan_db_close.
 *
 * Если в проекте будут использоваться параметры, необходимо передать
 * их описание в виде XML данных. Если параметры использоваться не будут,
 * можно передать %NULL. Подробно формат схем описан в #HyScanDataSchema.
 *
 * Returns: Идентификатор проекта, ноль или отрицательное число в случае ошибки.
 */
gint32
hyscan_db_project_create (HyScanDB    *db,
                          const gchar *project_name,
                          const gchar *project_schema)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_create != NULL)
    return iface->project_create (db, project_name, project_schema);

  return -1;
}

/**
 * hyscan_db_project_remove:
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 *
 * Функция удаляет проект. Удаление возможно даже в случае
 * использования проекта или его объектов другими клиентами.
 *
 * Returns: %TRUE - если проект был удалён, иначе %FALSE.
 */
gboolean
hyscan_db_project_remove (HyScanDB    *db,
                          const gchar *project_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_remove != NULL)
    return iface->project_remove (db, project_name);

  return FALSE;
}

/**
 * hyscan_db_project_get_ctime:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 *
 * Функция возвращает информацию о дате и времени создания проекта.
 *
 * Returns: (nullable): Указатель на объект #GDateTime.
 * Для удаления #g_date_time_unref.
 */
GDateTime *
hyscan_db_project_get_ctime (HyScanDB *db,
                             gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_get_ctime != NULL)
    return iface->project_get_ctime (db, project_id);

  return NULL;
}

/**
 * hyscan_db_project_param_list:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 *
 * Функция возвращает список групп параметров проекта.
 *
 * Returns: (nullable): %NULL терминированный список групп параметров
 * или %NULL - если проектов нет. Для удаления #g_strfreev.
 */
gchar **
hyscan_db_project_param_list (HyScanDB *db,
                              gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_list != NULL)
    return iface->project_param_list (db, project_id);

  return NULL;
}

/**
 * hyscan_db_project_param_open:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 * @group_name: название группы параметров
 *
 * Функция открывает группу параметров проекта.
 *
 * Функция возвращает идентификатор, который должен использоваться
 * в остальных функциях работы с параметрами. Если указанной группы
 * параметров нет, она автоматически создаётся. По завершению
 * работы с группой параметров, её необходимо закрыть функцией
 * #hyscan_db_close.
 *
 * Returns: Идентификатор группы параметров проекта или отрицательное
 * число в случае ошибки.
 */
gint32
hyscan_db_project_param_open (HyScanDB    *db,
                              gint32       project_id,
                              const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_open != NULL)
    return iface->project_param_open (db, project_id, group_name);

  return -1;
}

/**
 * hyscan_db_project_param_remove:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 * @group_name: название группы параметров
 *
 * Функция удаляет группу параметров проекта.
 *
 * Returns: %TRUE - если группа параметров была удалёна, иначе %FALSE.
 */
gboolean
hyscan_db_project_param_remove (HyScanDB    *db,
                                gint32       project_id,
                                const gchar *group_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->project_param_remove != NULL)
    return iface->project_param_remove (db, project_id, group_name);

  return FALSE;
}

/**
 * hyscan_db_track_list:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 *
 * Функция возвращает список галсов в проекте.
 *
 * Returns: (nullable): %NULL терминированный список галсов
 * или %NULL - если галсов нет. Для удаления #g_strfreev.
 */
gchar **
hyscan_db_track_list (HyScanDB *db,
                      gint32    project_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_list != NULL)
    return iface->track_list (db, project_id);

  return NULL;
}

/**
 * hyscan_db_track_open:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 * @track_name: название галса
 *
 * Функция открывает галс. Функция возвращает идентификатор,
 * который должен использоваться в остальных функциях работы
 * с галсами. По завершению работы с галсом, его необходимо
 * закрыть функцией #hyscan_db_close.
 *
 * Returns: Идентификатор галса больше нуля или отрицательное
 * число в случае ошибки.
 */
gint32
hyscan_db_track_open (HyScanDB    *db,
                      gint32       project_id,
                      const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_open != NULL)
    return iface->track_open (db, project_id, track_name);

  return -1;
}

/**
 * hyscan_db_track_create:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 * @track_name: название галса
 * @track_schema: (nullable): описание схемы параметров галса в виде XML
 * @schema_id: (nullable): идентификатор схемы параметров галса
 *
 * Функция создаёт галс и открывает его. Функция возвращает
 * идентификатор, который должен использоваться в остальных
 * функциях работы с галсами. Если галс существует функция,
 * вернёт значение ноль. По завершению работы с галсом,
 * его необходимо закрыть функцией #hyscan_db_close.
 *
 * Если в галсе или каналах данных будут использоваться параметры,
 * необходимо передать их описание в виде XML данных. Если параметры
 * использоваться не будут, можно передать %NULL. Подробно формат
 * схем описан в #HyScanDataSchema.
 *
 * Returns: Идентификатор галса, ноль или отрицательное число в случае ошибки.
 */
gint32
hyscan_db_track_create (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name,
                        const gchar *track_schema,
                        const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_create != NULL)
    return iface->track_create (db, project_id, track_name, track_schema, schema_id);

  return -1;
}

/**
 * hyscan_db_track_remove:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор проекта
 * @track_name: название галса
 *
 * Функция удаляет галс. Удаление возможно даже в случае использования
 * галса или его объектов другими клиентами.
 *
 * Returns: %TRUE - если галс был удалён, иначе %FALSE.
 */
gboolean
hyscan_db_track_remove (HyScanDB    *db,
                        gint32       project_id,
                        const gchar *track_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_remove != NULL)
    return iface->track_remove (db, project_id, track_name);

  return FALSE;
}

/**
 * hyscan_db_track_get_ctime:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 *
 * Функция возвращает информацию о дате и времени создания галса.
 *
 * Returns: (nullable): Указатель на объект #GDateTime.
 * Для удаления #g_date_time_unref.
 */
GDateTime *
hyscan_db_track_get_ctime (HyScanDB *db,
                           gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_get_ctime != NULL)
    return iface->track_get_ctime (db, track_id);

  return NULL;
}

/**
 * hyscan_db_track_param_open:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 *
 * Функция открывает группу параметров галса. Функция возвращает
 * идентификатор, который должен использоваться в остальных
 * функциях работы с параметрами. Функция возвращает действительный
 * идентификатор даже если параметров галса нет. В этом случае
 * считать или записать значения параметров будет невозможно.
 *
 * Значения параметров можно изменять, если для их открытия
 * использовался идентификатор галса, полученный при его создании.
 * Если открывался уже созданный галс, изменить значения параметров
 * будет нельзя.
 *
 * Returns: Идентификатор группы параметров галса или отрицательное
 * число в случае ошибки.
 */
gint32
hyscan_db_track_param_open (HyScanDB *db,
                            gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->track_param_open != NULL)
    return iface->track_param_open (db, track_id);

  return -1;
}

/**
 * hyscan_db_channel_list:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 *
 * Функция возвращает список каналов данных в галсе.
 *
 * Returns: (nullable): %NULL терминированный список каналов данных
 * или %NULL - если каналов данных нет. Для удаления #g_strfreev.
 */
gchar **
hyscan_db_channel_list (HyScanDB *db,
                        gint32    track_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_list != NULL)
    return iface->channel_list (db, track_id);

  return NULL;
}

/**
 * hyscan_db_channel_open:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 * @channel_name: название канала данных
 *
 * Функция открывает канал данных. Функция возвращает идентификатор,
 * который должен использоваться в остальных функциях работы с
 * каналами данных. В канал данных, открытый этой функцией, нельзя
 * записывать данные. По завершению работы с каналом данных его
 * необходимо закрыть функцией #hyscan_db_close.
 *
 * Returns: Идентификатор канала данных больше нуля или отрицательное
 * число в случае ошибки.
 */
gint32
hyscan_db_channel_open (HyScanDB    *db,
                        gint32       track_id,
                        const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_open != NULL)
    return iface->channel_open (db, track_id, channel_name);

  return -1;
}

/**
 * hyscan_db_channel_create:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 * @channel_name: название канала данных
 * @schema_id: (nullable): идентификатор схемы параметров канала данных
 *
 * Функция создаёт новый канал данных и открывает его для работы.
 * Функция возвращает идентификатор, который должен использоваться
 * в остальных функциях работы с каналами данных. Если канал данных
 * существует, функция вернёт значение ноль. По завершению работы
 * с каналом данных, его необходимо закрыть функцией #hyscan_db_close.
 *
 * При создании канала данных можно указать идентификатор схемы параметров.
 *
 * Returns: Идентификатор канала данных, ноль или отрицательное число в
 * случае ошибки.
 */
gint32
hyscan_db_channel_create (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name,
                          const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_create != NULL)
    return iface->channel_create (db, track_id, channel_name, schema_id);

  return -1;
}

/**
 * hyscan_db_channel_remove:
 * @db: указатель на #HyScanDB
 * @track_id: идентификатор галса
 * @channel_name: название канала данных
 *
 * Функция удаляет канал данных. Удаление возможно даже в случае
 * использования канала данных или его параметров другими клиентами.
 *
 * Returns: %TRUE - если канал данных был удалён, иначе %FALSE.
 */
gboolean
hyscan_db_channel_remove (HyScanDB    *db,
                          gint32       track_id,
                          const gchar *channel_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_remove != NULL)
    return iface->channel_remove (db, track_id, channel_name);

  return FALSE;
}

/**
 * hyscan_db_channel_get_ctime:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 *
 * Функция возвращает информацию о дате и времени создания канала данных.
 *
 * Returns: (nullable): Указатель на объект #GDateTime.
 * Для удаления #g_date_time_unref.
 */
GDateTime *
hyscan_db_channel_get_ctime (HyScanDB *db,
                             gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_ctime != NULL)
    return iface->channel_get_ctime (db, channel_id);

  return NULL;
}

/**
 * hyscan_db_channel_finalize:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 *
 * Функция переводит канал данных в режим только чтения. После
 * вызова этой функции записывать новые данные в канал и изменять
 * его параметры нельзя.
 */
void
hyscan_db_channel_finalize (HyScanDB *db,
                            gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_finalize != NULL)
    iface->channel_finalize (db, channel_id);
}

/**
 * hyscan_db_channel_is_writable:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 *
 * Функция возвращает режим доступа к каналу данных. Функция
 * проверяет режим доступа глобально в рамках системы хранения,
 * а не для идентификатора канала данных.
 *
 * Returns: %TRUE - если возможна запись данных, иначе %FALSE.
 */
gboolean
hyscan_db_channel_is_writable (HyScanDB *db,
                               gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_is_writable != NULL)
    return iface->channel_is_writable (db, channel_id);

  return FALSE;
}

/**
 * hyscan_db_channel_param_open:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 *
 * Функция открывает группу параметров канала данных. Функция
 * возвращает идентификатор, который должен использоваться в
 * остальных функциях работы с параметрами. Функция возвращает
 * действительный идентификатор даже если параметров канала данных
 * нет. В этом случае считать или записать значения параметров
 * будет невозможно.
 *
 * Значения параметров можно изменять, если для их открытии
 * использовался идентификатор канала данных, полученный при
 * его создании. Если открывался уже созданный канал данных,
 * изменить значения параметров будет нельзя.
 *
 * Returns: Идентификатор группы параметров канала данных или
 * отрицательное число в случае ошибки.
 */
gint32
hyscan_db_channel_param_open (HyScanDB *db,
                              gint32    channel_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), -1);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_param_open != NULL)
    return iface->channel_param_open (db, channel_id);

  return -1;
}

/**
 * hyscan_db_channel_set_chunk_size:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @chunk_size: максимальный размер файла в байтах
 *
 * Функция задаёт максимальный размер файлов, хранящих данные
 * канала, которые может создавать система. Если для максимального
 * размера указан ноль, будет восстановлено значение по умолчанию.
 *
 * Returns: %TRUE - если максимальный размер файлов изменён, иначе %FALSE.
 */
gboolean
hyscan_db_channel_set_chunk_size (HyScanDB *db,
                                  gint32    channel_id,
                                  guint64   chunk_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_chunk_size != NULL)
    return iface->channel_set_chunk_size (db, channel_id, chunk_size);

  return FALSE;
}

/**
 * hyscan_db_channel_set_save_time:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @save_time: время хранения данных в микросекундах
 *
 * Функция задаёт интервал времени, для которого сохраняются
 * записываемые данные. Если данные были записаны ранее
 * "текущего времени" - "интервал хранения" они удаляются.
 * Подробнее об этом можно прочитать в описании интерфейса #HyScanDB.
 * Если для времени указан ноль, будет восстановлено значение по умолчанию.
 *
 * Returns: %TRUE - если время хранения данных изменено, иначе %FALSE.
 */
gboolean
hyscan_db_channel_set_save_time (HyScanDB *db,
                                 gint32    channel_id,
                                 gint64    save_time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_save_time != NULL)
    return iface->channel_set_save_time (db, channel_id, save_time);

  return FALSE;
}

/**
 * hyscan_db_channel_set_save_size:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @save_size: объём сохраняемых данных в байтах
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём
 * данных превышает этот предел, старые данные удаляются. Подробнее
 * об этом можно прочитать в описании интерфейса #HyScanDB.
 * Если для объёма указан ноль, будет восстановлено значение по умолчанию.
 *
 * Returns: %TRUE - если объём сохраняемых данных изменён, иначе %FALSE.
 */
gboolean
hyscan_db_channel_set_save_size (HyScanDB *db,
                                 gint32    channel_id,
                                 guint64   save_size)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_set_save_size != NULL)
    return iface->channel_set_save_size (db, channel_id, save_size);

  return FALSE;
}

/**
 * hyscan_db_channel_get_data_range:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @first_index: (out) (nullable): начальный индекс данных
 * @last_index: (out) (nullable): конечный индекс данных
 *
 * Функция возвращает диапазон значений индексов записанных данных.
 * Функция вернёт значения начального и конечного индекса записей.
 * Функция вернёт FALSE если записей в канале нет.
 *
 * Returns: %TRUE - если границы записей определены, иначе %FALSE.
 */
gboolean
hyscan_db_channel_get_data_range (HyScanDB *db,
                                  gint32    channel_id,
                                  guint32  *first_index,
                                  guint32  *last_index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data_range != NULL)
    return iface->channel_get_data_range (db, channel_id, first_index, last_index);

  return FALSE;
}

/**
 * hyscan_db_channel_add_data:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @time: метка времени в микросекундах
 * @buffer: указатель на буфер данных
 * @index: (out) (nullable): индекса данных
 *
 * Функция записывает новые данные в канал.
 *
 * Returns: %TRUE - если данные успешно записаны, иначе %FALSE.
 */
gboolean
hyscan_db_channel_add_data (HyScanDB     *db,
                            gint32        channel_id,
                            gint64        time,
                            HyScanBuffer *buffer,
                            guint32      *index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_add_data != NULL)
    return iface->channel_add_data (db, channel_id, time, buffer, index);

  return FALSE;
}

/**
 * hyscan_db_channel_get_data:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @index: индекс считываемых данных
 * @buffer: буфер данных
 * @time: (out) (nullable): метка времени считанных данных
 *
 * Функция считывает записанные данные по номеру индекса.
 *
 * Returns: %TRUE - если данные успешно считаны, иначе %FALSE.
 */
gboolean
hyscan_db_channel_get_data (HyScanDB     *db,
                            gint32        channel_id,
                            guint32       index,
                            HyScanBuffer *buffer,
                            gint64       *time)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data != NULL)
    return iface->channel_get_data (db, channel_id, index, buffer, time);

  return FALSE;
}

/**
 * hyscan_db_channel_get_data_size:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @index: индекс считываемых данных
 *
 * Функция считывает размер данных по номеру индекса.
 *
 * Returns: Размер данных или ноль.
 */
guint32
hyscan_db_channel_get_data_size (HyScanDB     *db,
                                 gint32        channel_id,
                                 guint32       index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data_size != NULL)
    return iface->channel_get_data_size (db, channel_id, index);

  return 0;
}

/**
 * hyscan_db_channel_get_data_time:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @index: индекс считываемых данных
 *
 * Функция считывает метку времени данных по номеру индекса.
 *
 * Returns: Метка времени или отрицательное число.
 */
gint64
hyscan_db_channel_get_data_time (HyScanDB     *db,
                                 gint32        channel_id,
                                 guint32       index)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_get_data_time != NULL)
    return iface->channel_get_data_time (db, channel_id, index);

  return -1;
}

/**
 * hyscan_db_channel_find_data:
 * @db: указатель на #HyScanDB
 * @channel_id: идентификатор канала данных
 * @time: искомый момент времени
 * @lindex: (out) (nullable): "левый" индекс данных
 * @rindex: (out) (nullable): "правый" индекс данных
 * @ltime: (out) (nullable): "левая" метка времени данных
 * @rtime: (out) (nullable): "правая" метка времени данных
 *
 * Функция ищет индекс данных для указанного момента времени. Если
 * найдены данные, метка времени которых точно совпадает с указанной,
 * значения lindex и rindex, а также ltime и rtime будут одинаковые.
 *
 * Если найдены данные, метка времени которых находится в переделах
 * записанных данных, значения lindex и ltime будут указывать на индекс
 * и время соответственно, данных, записанных перед искомым моментом
 * времени. А rindex и ltime будут указывать на индекс и время
 * соответственно, данных, записанных после искомого момента времени.
 *
 * Returns: Статус поиска индекса данных.
 */
HyScanDBFindStatus
hyscan_db_channel_find_data (HyScanDB *db,
                             gint32    channel_id,
                             gint64    time,
                             guint32  *lindex,
                             guint32  *rindex,
                             gint64   *ltime,
                             gint64   *rtime)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), HYSCAN_DB_FIND_FAIL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->channel_find_data != NULL)
    return iface->channel_find_data (db, channel_id, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

/**
 * hyscan_db_param_object_list:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 *
 * Функция возвращает список объектов в группе параметров проекта.
 *
 * Returns: (nullable): %NULL терминированный список объектов
 * или %NULL - если объектов нет. Для удаления #g_strfreev.
 */
gchar **
hyscan_db_param_object_list (HyScanDB *db,
                             gint32    param_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_list != NULL)
    return iface->param_object_list (db, param_id);

  return NULL;
}

/**
 * hyscan_db_param_object_create:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 * @object_name: название объекта
 * @schema_id: схема данных объекта
 *
 * Функция создаёт объект в группе параметров проекта.
 *
 * Returns: %TRUE - если объект успешно создан, иначе %FALSE.
 */
gboolean
hyscan_db_param_object_create (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *object_name,
                               const gchar *schema_id)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_create != NULL)
    return iface->param_object_create (db, param_id, object_name, schema_id);

  return FALSE;
}

/**
 * hyscan_db_param_object_remove:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 * @object_name: название объекта
 *
 * Функция удаляет объект из группы параметров проекта.
 *
 * Returns: %TRUE - если объект успешно удалён, иначе %FALSE.
 */
gboolean
hyscan_db_param_object_remove (HyScanDB    *db,
                               gint32       param_id,
                               const gchar *object_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_remove != NULL)
    return iface->param_object_remove (db, param_id, object_name);

  return FALSE;
}

/**
 * hyscan_db_param_object_get_schema:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 * @object_name: название объекта
 *
 * Функция возвращает описание схемы данных для объекта группы параметров.
 *
 * Returns: (transfer full) (nullable): #HyScanDataSchema или %NULL
 * если объекта нет. Для удаления #g_object_unref.
 */
HyScanDataSchema *
hyscan_db_param_object_get_schema (HyScanDB    *db,
                                   gint32       param_id,
                                   const gchar *object_name)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), NULL);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_object_get_schema != NULL)
    return iface->param_object_get_schema (db, param_id, object_name);

  return NULL;
}

/**
 * hyscan_db_param_set:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 * @object_name: название объекта
 * @param_list: список параметров для записи
 *
 * Функция устанавливает значения параметров объекта. Установка значений
 * нескольких параметров производится атомарно. При установке значений
 * параметров галсов или каналов данных, название объекта должно быть %NULL.
 *
 * Returns: %TRUE - если значения установлены, иначе %FALSE.
 */
gboolean
hyscan_db_param_set (HyScanDB        *db,
                     gint32           param_id,
                     const gchar     *object_name,
                     HyScanParamList *param_list)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_set != NULL)
    return iface->param_set (db, param_id, object_name, param_list);

  return FALSE;
}

/**
 * hyscan_db_param_get:
 * @db: указатель на #HyScanDB
 * @param_id: идентификатор группы параметров
 * @object_name: название объекта
 * @param_list: список параметров для записи
 *
 * Функция считывает значение параметра объекта. Чтение значений
 * нескольких параметров производится атомарно. При считывании значений
 * параметров галсов или каналов данных, название объекта должно быть %NULL.
 *
 * Returns: %TRUE - если значения считаны, иначе %FALSE.
 */
gboolean
hyscan_db_param_get (HyScanDB        *db,
                     gint32           param_id,
                     const gchar     *object_name,
                     HyScanParamList *param_list)
{
  HyScanDBInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->param_get != NULL)
    return iface->param_get (db, param_id, object_name, param_list);

  return FALSE;
}

/**
 * hyscan_db_close:
 * @db: указатель на #HyScanDB
 * @id: идентификатор объекта
 *
 * Функция закрывает объект системы хранения. Данная функция
 * используется для закрытия любых объектов системы хранения,
 * открытых ранее.
 */
void
hyscan_db_close (HyScanDB *db,
                 gint32    id)
{
  HyScanDBInterface *iface;

  g_return_if_fail (HYSCAN_IS_DB (db));

  iface = HYSCAN_DB_GET_IFACE (db);
  if (iface->close != NULL)
    iface->close (db, id);
}
