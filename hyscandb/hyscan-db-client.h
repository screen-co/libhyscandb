/**
 * \file hyscan-db-client.h
 *
 * \brief Заголовочный файл RPC клиента базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBClient HyScanDBClient RPC клиент базы данных HyScan
 *
 * Класс реализует интерфейс \link HyScanDB \endlink и обеспечивает взаимодействие с
 * сервером системы хранения HyScan через библиотеку uRPC. Для взаимодействия доступны
 * протоколы TCP и SHM из состава библиотеки uRPC.
 *
 * Функция #hyscan_db_client_new создаёт клиента базы данных и производит подключение по
 * указанному адресу.
 *
 * Рекомендуется использовать функцию \link hyscan_db_new \endlink, которая автоматически
 * выбирает реализацию системы хранения данных HyScan в зависимости от указанного адреса.
 *
 */

#ifndef __HYSCAN_DB_CLIENT_H__
#define __HYSCAN_DB_CLIENT_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_CLIENT             (hyscan_db_client_get_type ())
#define HYSCAN_DB_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_CLIENT, HyScanDBClient))
#define HYSCAN_IS_DB_CLIENT(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_CLIENT))
#define HYSCAN_DB_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_CLIENT, HyScanDBClientClass))
#define HYSCAN_IS_DB_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_CLIENT))
#define HYSCAN_DB_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_CLIENT, HyScanDBClientClass))

typedef struct _HyScanDBClient HyScanDBClient;
typedef struct _HyScanDBClientClass HyScanDBClientClass;

struct _HyScanDBClientClass
{
  GObjectClass parent_class;
};

HYSCAN_DB_EXPORT
GType                  hyscan_db_client_get_type       (void);

/**
 *
 * Функция создаёт объект \link HyScanDBClient \endlink, совместимый с интерфейсом
 * \link HyScanDB \endlink, и производит подключение к серверу базы данных по указанному
 * адресу. Адрес указывается в формате uRPC. Поддерживаются протоколы TCP и SHM.
 *
 * \param uri адрес сервера базы данных.
 *
 * \return Указатель на объект \link HyScanDBClient \endlink.
 *
 */
HYSCAN_DB_EXPORT
HyScanDBClient        *hyscan_db_client_new            (const gchar           *uri);

G_END_DECLS

#endif /* __HYSCAN_DB_CLIENT_H__ */
