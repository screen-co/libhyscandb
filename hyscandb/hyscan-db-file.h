/*
 * \file hyscan-db-file.h
 *
 * \brief Заголовочный файл класса файловой базы данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * HyScanDBFile класс хранения данных в формате HyScan 5
 *
 * Класс реализует интерфейс HyScanDB и хранит данные с использованием
 * HyScanDBChannelFile и HyScanDBParamFile.
 *
 */

#ifndef __HYSCAN_DB_FILE_H__
#define __HYSCAN_DB_FILE_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_FILE             (hyscan_db_file_get_type ())
#define HYSCAN_DB_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_FILE, HyScanDBFile))
#define HYSCAN_IS_DB_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_FILE))
#define HYSCAN_DB_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_FILE, HyScanDBFileClass))
#define HYSCAN_IS_DB_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_FILE))
#define HYSCAN_DB_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_FILE, HyScanDBFileClass))

typedef struct _HyScanDBFile HyScanDBFile;
typedef struct _HyScanDBFilePrivate HyScanDBFilePrivate;
typedef struct _HyScanDBFileClass HyScanDBFileClass;

struct _HyScanDBFile
{
  GObject parent_instance;

  HyScanDBFilePrivate *priv;
};

struct _HyScanDBFileClass
{
  GObjectClass parent_class;
};

GType          hyscan_db_file_get_type (void);

HyScanDBFile  *hyscan_db_file_new      (const gchar   *path);

G_END_DECLS

#endif /* __HYSCAN_DB_FILE_H__ */
