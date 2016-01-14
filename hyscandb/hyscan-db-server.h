/**
 * \file hyscan-db-server.h
 *
 * \brief Заголовочный файл RPC сервера базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBServer HyScanDBServer RPC сервер базы данных HyScan
 *
 * Класс реализует сервер базы данных HyScan. Для работы с сервером необходимо использовать
 * клиентскую реализацию интерфейса \link HyScanDB\endlink - \link HyScanDBClient \endlink.
 *
 * Рекомендуется использовать функцию \link hyscan_db_new \endlink, которая автоматически
 * выбирает реализацию системы хранения данных HyScan в зависимости от указанного адреса.
 *
 * Сервер базы данных транслирует все вызовы интерфейса \link HyScanDB \endlink от клиента
 * \link HyScanDBClient \endlink в объект db, указанный при создании сервера. Дополнительно
 * можно указать функцию, которая будет вызываться перед исполнением запроса от клиента
 * #hyscan_db_server_acl. Если функция вернёт TRUE сервер продолжит выполнения запроса,
 * иначе возвратит клиенту ошибку. Данная функция может использоваться для разграничения
 * доступа к базе данных.
 *
 * После создания сервера его необходимо запустить функцией #hyscan_db_server_start.
 *
 * При удалении объекта, сервер автоматически завершает свою работу.
 *
 */

#ifndef __HYSCAN_DB_SERVER_H__
#define __HYSCAN_DB_SERVER_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_SERVER             (hyscan_db_server_get_type ())
#define HYSCAN_DB_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_SERVER, HyScanDBServer))
#define HYSCAN_IS_DB_SERVER(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_SERVER))
#define HYSCAN_DB_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_SERVER, HyScanDBServerClass))
#define HYSCAN_IS_DB_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_SERVER))
#define HYSCAN_DB_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_SERVER, HyScanDBServerClass))

/**
 *
 * Прототип функции контроля доступа к серверу. В функцию передаётся название запрашиваемой
 * клиентом процедуры интерфейса \link HyScanDB \endlink и указатель на информацию о пользователе
 * (см. описание библиотеки uRPC). Функция должна вернуть значение TRUE, если пользователю разрешено
 * выполнение запроса или FALSE, если запрещено.
 *
 * \param function_name имя запрашиваемой процедуры \link HyScanDB \endlink;
 * \param key_data информация о пользователе.
 *
 * \return TRUE - если выполнение запроса разрешено, FALSE - если запрещено.
 *
 */
typedef gboolean       (*hyscan_db_server_acl)         (const gchar           *function_name,
                                                        gpointer               key_data);

typedef struct _HyScanDBServer HyScanDBServer;
typedef struct _HyScanDBServerClass HyScanDBServerClass;

struct _HyScanDBServerClass
{
  GObjectClass parent_class;
};

HYSCAN_DB_EXPORT
GType                  hyscan_db_server_get_type       (void);

/**
 *
 * Функция создаёт сервер базы данных.
 *
 * \param uri адрес сервера;
 * \param db объект в который транслируются запросы клиентов;
 * \param n_threads число потоков сервера;
 * \param n_clients максимальное число одновременно подключенных клиентов;
 * \param acl_fn функция проверки доступа к базе данных.
 *
 * \return Указатель на объект \link HyScanDBServer \endlink.
 *
 */
HYSCAN_DB_EXPORT
HyScanDBServer        *hyscan_db_server_new            (const gchar           *uri,
                                                        HyScanDB              *db,
                                                        guint                  n_threads,
                                                        guint                  n_clients,
                                                        hyscan_db_server_acl   acl_fn);

/**
 *
 * Функция запускает сервер базы данных в работу.
 *
 * \param server указатель на объект \link HyScanDBServer \endlink.
 *
 * \return TRUE - если сервер успешно запущен, FALSE - в случае ошибки.
 *
 */
HYSCAN_DB_EXPORT
gboolean               hyscan_db_server_start          (HyScanDBServer        *server);

G_END_DECLS

#endif /* __HYSCAN_DB_SERVER_H__ */
