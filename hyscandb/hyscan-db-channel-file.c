/*
 * \file hyscan-db-channel-file.c
 *
 * \brief Исходный файл класса хранения данных канала в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-channel-file.h"

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#define INDEX_FILE_MAGIC       0x58495348              /* HSIX в виде строки. */
#define DATA_FILE_MAGIC        0x54445348              /* HSDT в виде строки. */
#define FILE_VERSION           0x31303731              /* 1701 в виде строки. */

#define MAX_PARTS              999999                  /* Максимальное число частей данных. */
#define CACHED_INDEXES         2048                    /* Число кэшированных индексов. */

#define INDEX_FILE_HEADER_SIZE (sizeof (HyScanDBChannelFileID) + sizeof (guint32)) /* Размер заголовка файла индексов. */
#define DATA_FILE_HEADER_SIZE  (sizeof (HyScanDBChannelFileID))                    /* Размер заголовка файла данных. */
#define FILE_HEADER_SIZE       (sizeof (HyScanDBChannelFileID))                    /* Размер общего заголовка файлов. */
#define INDEX_RECORD_SIZE      (sizeof (HyScanDBChannelFileIndexRec))              /* Размер индекса. */

#define MIN_DATA_FILE_SIZE     1*1024*1024             /* Минимально возможный размер файла части данных. */
#define MAX_DATA_FILE_SIZE     1024*1024*1024*1024LL   /* Максимально возможный размер файла части данных. */
#define DEFAULT_DATA_FILE_SIZE 1024*1024*1024          /* Размер файла части данных по умолчанию. */

enum
{
  PROP_O,
  PROP_PATH,
  PROP_NAME,
  PROP_READONLY
};

/* Заголовок файлов данных и индексов. */
typedef struct
{
  guint32              magic;                  /* Идентификатор файла. */
  guint32              version;                /* Версия API системы хранения. */
  gint64               ctime;                  /* Дата создания. */
} HyScanDBChannelFileID;

/* Информация о части списка данных. */
typedef struct
{
  guint64                      data_size;              /* Размер файла данных этой части. */

  gint64                       create_time;            /* Время создания этой части данных. */
  gint64                       last_append_time;       /* Время последней записи данных в эту часть. */

  guint32                      begin_index;            /* Начальный индекс данных в этой части. */
  guint32                      end_index;              /* Конечный индекс данных в этой части. */

  gint64                       begin_time;             /* Начальное время данных в этой части. */
  gint64                       end_time;               /* Конечное время данных в этой части. */

  GFile                       *fdi;                    /* Объект работы с файлом индексов. */
  GInputStream                *ifdi;                   /* Поток чтения файла индексов. */
  GOutputStream               *ofdi;                   /* Поток записи файла индексов. */

  GFile                       *fdd;                    /* Объект работы с файлом данных. */
  GInputStream                *ifdd;                   /* Поток чтения файла данных. */
  GOutputStream               *ofdd;                   /* Поток записи файла данных. */
} HyScanDBChannelFilePart;

/* Структура индексной записи в файле. */
typedef struct
{
  gint64                       time;                   /* Время приёма данных, в микросекундах. */
  guint64                      offset;                 /* Смещение до начала данных. */
  guint32                      size;                   /* Размер данных. */
  guint32                      pad;                    /* Выравнивание. */
} HyScanDBChannelFileIndexRec;

/* Информация о записи. */
typedef struct _HyScanDBChannelFileIndex HyScanDBChannelFileIndex;
struct _HyScanDBChannelFileIndex
{
  HyScanDBChannelFilePart     *part;                   /* Номер части с данными. */
  guint32                      index;                  /* Значение индекса. */

  gint64                       time;                   /* Время приёма данных, в микросекундах. */
  guint64                      offset;                 /* Смещение до начала данных. */
  guint32                      size;                   /* Размер данных. */

                                                       /* Структуры индексов связаны между собой
                                                          не в порядке их номеров.*/
  HyScanDBChannelFileIndex    *prev;                   /* Указатель на предыдущий индекс. */
  HyScanDBChannelFileIndex    *next;                   /* Указатель на следующий индекс.*/
};

/* Внутренние данные объекта. */
struct _HyScanDBChannelFilePrivate
{
  gchar                       *path;                   /* Путь к каталогу с файлами данных канала. */
  gchar                       *name;                   /* Название канала. */
  gint64                       ctime;                  /* Дата и время создания канала данных. */

  guint64                      max_data_file_size;     /* Максимальный размер файла части данных. */
  guint64                      save_size;              /* Максимальный объём всех сохраняемых данных. */
  gint64                       save_time;              /* Интервал времени для которого хранятся записываемые данные. */

  gboolean                     readonly;               /* Создавать или нет файлы при открытии канала. */
  gboolean                     fail;                   /* Признак ошибки в объекте. */

  guint64                      data_size;              /* Текущий объём хранимых данных. */

  gint32                       begin_index;            /* Начальный индекс данных. */
  gint32                       end_index;              /* Конечный индекс данных. */

  gint64                       begin_time;             /* Начальное время данных. */
  gint64                       end_time;               /* Конечное время данных. */

  HyScanDBChannelFilePart    **parts;                  /* Массив указателей на структуры с описанием части данных. */
  guint                        n_parts;                /* Число частей данных. */

  GHashTable                  *cached_indexes;         /* Таблица кэшированных индексов. */
  HyScanDBChannelFileIndex    *first_cached_index;     /* Первый индекс (недавно использовался). */
  HyScanDBChannelFileIndex    *last_cached_index;      /* Последний индекс (давно использовался). */

  GMutex                       lock;                   /* Блокировка многопоточного доступа. */
};

static void                      hyscan_db_channel_file_set_property        (GObject                     *object,
                                                                             guint                        prop_id,
                                                                             const GValue                *value,
                                                                             GParamSpec                  *pspec);
static void                      hyscan_db_channel_file_object_constructed  (GObject                     *object);
static void                      hyscan_db_channel_file_object_finalize     (GObject                     *object);

static gboolean                  hyscan_db_channel_file_add_part            (HyScanDBChannelFilePrivate  *priv);
static gboolean                  hyscan_db_channel_file_remove_old_part     (HyScanDBChannelFilePrivate  *priv);
static HyScanDBChannelFileIndex *hyscan_db_channel_file_read_index          (HyScanDBChannelFilePrivate  *priv,
                                                                             guint32                      index);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBChannelFile, hyscan_db_channel_file, G_TYPE_OBJECT);

static void
hyscan_db_channel_file_class_init (HyScanDBChannelFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_channel_file_set_property;

  object_class->constructed = hyscan_db_channel_file_object_constructed;
  object_class->finalize = hyscan_db_channel_file_object_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
                                   g_param_spec_string ("name", "Name", "Channel name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PATH,
                                   g_param_spec_string ("path", "Path", "Path to channel data", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_READONLY,
                                   g_param_spec_boolean ("readonly", "ReadOnly", "Read only mode", FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_db_channel_file_init (HyScanDBChannelFile *channel)
{
  channel->priv = hyscan_db_channel_file_get_instance_private (channel);
}

static void
hyscan_db_channel_file_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanDBChannelFile *channel = HYSCAN_DB_CHANNEL_FILE (object);
  HyScanDBChannelFilePrivate *priv = channel->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_PATH:
      priv->path = g_value_dup_string (value);
      break;

    case PROP_READONLY:
      priv->readonly = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_channel_file_object_constructed (GObject *object)
{
  HyScanDBChannelFile *channel = HYSCAN_DB_CHANNEL_FILE (object);
  HyScanDBChannelFilePrivate *priv = channel->priv;

  gint i;

  /* Начальные значения. */
  priv->max_data_file_size = DEFAULT_DATA_FILE_SIZE;
  priv->save_time = G_MAXINT64;
  priv->save_size = G_MAXINT64;
  priv->readonly = FALSE;
  priv->fail = FALSE;
  priv->data_size = 0;
  priv->begin_index = 0;
  priv->end_index = 0;
  priv->begin_time = 0;
  priv->end_time = 0;
  priv->parts = NULL;
  priv->n_parts = 0;

  g_mutex_init (&priv->lock);

  /* Кэш индексов.
     Кэш организован в виде hash таблицы, где номер индекса используется как ключ поиска.
     Всего возможно кэширование CACHED_INDEXES индексов. Информация об индексе хранится в
     структурах HyScanDBChannelFileIndex. Эти структуры связаны друг с другом в цепочку.
     При каждом доступе к индексу происходит поиск в hash таблице и если элемент находится
     в ней он переносится в начало цепочки. Если поиск в таблице не дал результата происходит
     чтение индекса с диска, запись в последний элемент цепочки и перенос этого элемента в
     начало цепочки. */
  {
    HyScanDBChannelFileIndex *prev_index = NULL;

    priv->cached_indexes = g_hash_table_new (g_direct_hash, g_direct_equal);
    for (i = 0; i < CACHED_INDEXES; i++)
      {
        HyScanDBChannelFileIndex *db_index = g_slice_new (HyScanDBChannelFileIndex);

        db_index->part = NULL;
        db_index->index = 0xffffffff;
        db_index->offset = 0;
        db_index->time = 0;
        db_index->size = 0;
        db_index->prev = NULL;
        db_index->next = NULL;

        if (prev_index != NULL)
          {
            prev_index->next = db_index;
            db_index->prev = prev_index;
          }

        if (i == 0)
          priv->first_cached_index = db_index;
        if (i == CACHED_INDEXES - 1)
          priv->last_cached_index = db_index;

        prev_index = db_index;
      }
  }

  /* Проверяем существующие данные и открываем их на чтение в случае существования. */
  while (TRUE)
    {
      HyScanDBChannelFilePart *fpart;
      HyScanDBChannelFileID id;

      gchar *fname;
      GFileInfo *finfo = NULL;

      GFile *fdi = NULL;
      GInputStream *ifdi = NULL;
      guint64 index_file_size;

      GFile *fdd = NULL;
      GInputStream *ifdd = NULL;
      guint64 data_file_size;

      guint64 begin_time;
      guint64 end_time;

      HyScanDBChannelFileIndexRec rec_index;
      guint32 begin_index;
      guint32 end_index;

      goffset offset;
      gssize iosize;

      /* Файл индексов. */
      fname = g_strdup_printf ("%s%s%s.%06d.i", priv->path, G_DIR_SEPARATOR_S, priv->name, priv->n_parts);
      fdi = g_file_new_for_path (fname);
      g_free (fname);

      /* Файл данных. */
      fname = g_strdup_printf ("%s%s%s.%06d.d", priv->path, G_DIR_SEPARATOR_S, priv->name, priv->n_parts);
      fdd = g_file_new_for_path (fname);
      g_free (fname);

      /* Если файлов нет - завершаем проверку. */
      if (!g_file_query_exists (fdi, NULL) && !g_file_query_exists (fdd, NULL))
        {
          g_object_unref (fdi);
          g_object_unref (fdd);
          break;
        }

      priv->readonly = TRUE;

      /* Поток чтения индексов. */
      ifdi = G_INPUT_STREAM (g_file_read (fdi, NULL, NULL));
      if (ifdi == NULL)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't open index file",
                      priv->name, priv->n_parts);
        }

      /* Поток чтения данных. */
      ifdd = G_INPUT_STREAM (g_file_read (fdd, NULL, NULL));
      if (ifdd == NULL)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't open data file",
                      priv->name, priv->n_parts);
        }

      /* Ошибка чтения существующих файлов. */
      if (ifdi == NULL || ifdd == NULL)
        goto break_open;

      /* Размер файла индексов. */
      finfo = g_file_query_info (fdi, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
      if (finfo == NULL)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't query index file size",
                      priv->name, priv->n_parts);
          goto break_open;
        }
      index_file_size = g_file_info_get_size (finfo);
      g_object_unref (finfo);

      /* Проверяем размер файла с индексами - должна быть как минимум одна запись и
       * размер должен быть кратен размеру структуры индекса. */
      if ((index_file_size < (INDEX_FILE_HEADER_SIZE + INDEX_RECORD_SIZE)) ||
          ((index_file_size - INDEX_FILE_HEADER_SIZE) % INDEX_RECORD_SIZE))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: invalid index file size",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Размер файла данных. */
      finfo = g_file_query_info (fdd, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
      if (finfo == NULL)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't query data file size",
                      priv->name, priv->n_parts);
          goto break_open;
        }
      data_file_size = g_file_info_get_size (finfo);
      g_object_unref (finfo);

      /* Считываем заголовок файла индексов. */
      iosize = FILE_HEADER_SIZE;
      if (g_input_stream_read (ifdi, &id, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't read index file header",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Проверяем заголовок файла индексов. */
      if ((GUINT32_FROM_LE (id.magic) != INDEX_FILE_MAGIC) ||
          (GUINT32_FROM_LE (id.version) != FILE_VERSION))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: unknown index file format",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Считываем заголовок файла данных. */
      iosize = FILE_HEADER_SIZE;
      if (g_input_stream_read (ifdd, &id, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't read data file header",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Проверяем заголовок файла данных. */
      if ((GUINT32_FROM_LE (id.magic) != DATA_FILE_MAGIC) ||
          (GUINT32_FROM_LE (id.version) != FILE_VERSION))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: unknown data file format",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Считываем начальный номер индекса части данных. */
      iosize = sizeof (guint32);
      if (g_input_stream_read (ifdi, &begin_index, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't query start index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Проверяем начальный номер индекса части данных. */
      begin_index = GUINT32_FROM_LE (begin_index);
      if ((priv->n_parts > 0) && (begin_index != priv->parts[priv->n_parts - 1]->end_index + 1))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: invalid index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      end_index = begin_index + ((index_file_size - INDEX_FILE_HEADER_SIZE) / INDEX_RECORD_SIZE) - 1;

      /* Считываем первый индекс части данных. */
      offset = INDEX_FILE_HEADER_SIZE;
      if (!g_seekable_seek (G_SEEKABLE (ifdi), offset, G_SEEK_SET, NULL, NULL))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't seek to start index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      iosize = INDEX_RECORD_SIZE;
      if (g_input_stream_read (ifdi, &rec_index, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't read start index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      begin_time = GINT64_FROM_LE (rec_index.time);

      /* Считываем последний индекс части данных. */
      offset = end_index - begin_index;
      offset *= INDEX_RECORD_SIZE;
      offset += INDEX_FILE_HEADER_SIZE;
      if (!g_seekable_seek (G_SEEKABLE (ifdi), offset, G_SEEK_SET, NULL, NULL))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't seek to end index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      iosize = INDEX_RECORD_SIZE;
      if (g_input_stream_read (ifdi, &rec_index, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't read end index",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      end_time = GINT64_FROM_LE (rec_index.time);

      /* Проверяем размер файла с данными - он должен быть равен смещению до данных
         по последнему индексу + размер данных. */
      if (data_file_size != (GUINT64_FROM_LE (rec_index.offset) + GUINT32_FROM_LE (rec_index.size)))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': part %d: invalid data file size",
                      priv->name, priv->n_parts);
          goto break_open;
        }

      /* Считываем информацию о части данных. */
      priv->n_parts += 1;
      priv->parts = g_realloc (priv->parts,
                               32 * (((priv->n_parts + 1) / 32) + 1) * sizeof (HyScanDBChannelFilePart *));

      fpart = g_new (HyScanDBChannelFilePart, 1);
      priv->parts[priv->n_parts - 1] = fpart;
      priv->parts[priv->n_parts] = NULL;

      if (priv->ctime == 0)
        priv->ctime = GUINT64_FROM_LE (id.ctime);

      fpart->fdi = fdi;
      fpart->fdd = fdd;
      fpart->ifdi = ifdi;
      fpart->ifdd = ifdd;
      fpart->ofdi = NULL;
      fpart->ofdd = NULL;

      fpart->create_time = 0;
      fpart->last_append_time = 0;

      fpart->begin_index = begin_index;
      fpart->end_index = end_index;
      fpart->begin_time = begin_time;
      fpart->end_time = end_time;

      fpart->data_size = data_file_size;
      priv->data_size += (data_file_size - DATA_FILE_HEADER_SIZE);

      /* Загружаем следующую часть. */
      if (priv->n_parts == MAX_PARTS)
        break;

      continue;

      /* Завершаем проверку. */
    break_open:
      /* Если не прочитали ни одной части - печалька... */
      if (priv->n_parts == 0)
        priv->fail = TRUE;
      g_clear_object (&ifdi);
      g_clear_object (&ifdd);
      g_clear_object (&fdi);
      g_clear_object (&fdd);
      break;
    }

  /* Если включен режим только чтения, а данных нет - ошибка. */
  if (priv->readonly && priv->n_parts == 0)
    priv->fail = TRUE;
}

static void
hyscan_db_channel_file_object_finalize (GObject *object)
{
  HyScanDBChannelFile *channel = HYSCAN_DB_CHANNEL_FILE (object);
  HyScanDBChannelFilePrivate *priv = channel->priv;

  guint i;

  /* Освобождаем структуры кэша индексов. */
  while (priv->first_cached_index != NULL)
    {
      HyScanDBChannelFileIndex *db_index = priv->first_cached_index;
      priv->first_cached_index = priv->first_cached_index->next;
      g_slice_free (HyScanDBChannelFileIndex, db_index);
    }
  g_hash_table_destroy (priv->cached_indexes);

  /* Освобождаем структуры с информацией о частях данных. */
  for (i = 0; i < priv->n_parts; i++)
    {
      g_clear_object (&priv->parts[i]->ofdi);
      g_clear_object (&priv->parts[i]->ofdd);
      g_clear_object (&priv->parts[i]->ifdi);
      g_clear_object (&priv->parts[i]->ifdd);
      g_clear_object (&priv->parts[i]->fdi);
      g_clear_object (&priv->parts[i]->fdd);
      g_free (priv->parts[i]);
    }
  g_free (priv->parts);

  g_mutex_clear (&priv->lock);

  g_free (priv->name);
  g_free (priv->path);

  G_OBJECT_CLASS (hyscan_db_channel_file_parent_class)->finalize (object);
}

/* Функция добавления новой части данных. */
static gboolean
hyscan_db_channel_file_add_part (HyScanDBChannelFilePrivate *priv)
{
  gchar *fname;
  HyScanDBChannelFilePart *fpart;
  HyScanDBChannelFileID id;

  guint32 begin_index = 0;
  gint64 ctime;

  gssize iosize;

  if (priv->readonly)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': read only mode", priv->name);
      return FALSE;
    }

  /* Закрываем потоки вывода текущей части. */
  if (priv->n_parts > 0)
    {
      g_clear_object (&priv->parts[priv->n_parts - 1]->ofdi);
      g_clear_object (&priv->parts[priv->n_parts - 1]->ofdd);
    }

  /* Всего частей может быть не более MAX_PARTS. */
  if (priv->n_parts == MAX_PARTS)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': too many parts", priv->name);
      return FALSE;
    }

  priv->n_parts += 1;
  priv->parts = g_realloc (priv->parts,
                           32 * (((priv->n_parts + 1) / 32) + 1) * sizeof (HyScanDBChannelFilePart *));

  fpart = g_new (HyScanDBChannelFilePart, 1);
  priv->parts[priv->n_parts - 1] = fpart;
  priv->parts[priv->n_parts] = NULL;

  /* Имя файла индексов. */
  fname = g_strdup_printf ("%s%s%s.%06d.i", priv->path, G_DIR_SEPARATOR_S, priv->name, priv->n_parts - 1);
  fpart->fdi = g_file_new_for_path (fname);
  g_free (fname);

  /* Имя файла данных. */
  fname = g_strdup_printf ("%s%s%s.%06d.d", priv->path, G_DIR_SEPARATOR_S, priv->name, priv->n_parts - 1);
  fpart->fdd = g_file_new_for_path (fname);
  g_free (fname);

  /* Создаём файл индексов и открываем его на запись. */
  fpart->ofdi = G_OUTPUT_STREAM (g_file_create (fpart->fdi, G_FILE_CREATE_NONE, NULL, NULL));
  if (fpart->ofdi == NULL)
    g_warning ("HyScanDBChannelFile: channel '%s': can't create index file", priv->name);

  /* Создаём файл данных и открываем его на запись. */
  fpart->ofdd = G_OUTPUT_STREAM (g_file_create (fpart->fdd, G_FILE_CREATE_NONE, NULL, NULL));
  if (fpart->ofdd == NULL)
    g_warning ("HyScanDBChannelFile: channel '%s': can't create data file", priv->name);

  /* Открываем файл индексов на чтение. */
  fpart->ifdi = G_INPUT_STREAM (g_file_read (fpart->fdi, NULL, NULL));
  if (fpart->ifdi == NULL)
    g_warning ("HyScanDBChannelFile: channel '%s': can't open index file", priv->name);

  /* Открываем файл данных на чтение. */
  fpart->ifdd = G_INPUT_STREAM (g_file_read (fpart->fdd, NULL, NULL));
  if (fpart->ifdd == NULL)
    g_warning ("HyScanDBChannelFile: channel '%s': can't open data file", priv->name);

  /* Ошибка при создании нового списка данных. */
  if (fpart->ofdi == NULL || fpart->ofdd == NULL || fpart->ifdi == NULL || fpart->ifdd == NULL)
    {
      priv->fail = TRUE;
      return FALSE;
    }

  if (priv->n_parts == 1)
    begin_index = 0;
  else
    begin_index = priv->parts[priv->n_parts - 2]->end_index + 1;

  fpart->create_time = g_get_monotonic_time ();

  fpart->begin_index = begin_index;
  fpart->end_index = begin_index;

  fpart->begin_time = 0;
  fpart->end_time = 0;

  /* Запись заголовка файла индексов. */
  ctime = g_get_real_time () / G_USEC_PER_SEC;
  id.magic = GUINT32_TO_LE (INDEX_FILE_MAGIC);
  id.version = GUINT32_TO_LE (FILE_VERSION);
  id.ctime = GUINT64_TO_LE (ctime);

  iosize = FILE_HEADER_SIZE;
  if (g_output_stream_write (fpart->ofdi, &id, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't write index header", priv->name);
      priv->fail = TRUE;
      return FALSE;
    }

  /* Запись значения начального индекса. */
  begin_index = GUINT32_TO_LE (begin_index);

  iosize = sizeof (guint32);
  if (g_output_stream_write (fpart->ofdi, &begin_index, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't write start index", priv->name);
      priv->fail = TRUE;
      return FALSE;
    }

  /* Запись заголовка файла данных. */
  ctime = g_get_real_time () / G_USEC_PER_SEC;
  id.magic = GUINT32_TO_LE (DATA_FILE_MAGIC);
  id.version = GUINT32_TO_LE (FILE_VERSION);
  id.ctime = GUINT64_TO_LE (ctime);

  iosize = FILE_HEADER_SIZE;
  if (g_output_stream_write (fpart->ofdd, &id, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't write data header", priv->name);
      priv->fail = TRUE;
      return FALSE;
    }

  fpart->data_size = DATA_FILE_HEADER_SIZE;

  return TRUE;
}

/* Функция удаляет старые части данных. */
static gboolean
hyscan_db_channel_file_remove_old_part (HyScanDBChannelFilePrivate *priv)
{
  HyScanDBChannelFilePart *fpart;
  guint i;

  if (priv->readonly)
    return TRUE;
  if (priv->n_parts < 2)
    return TRUE;

  fpart = priv->parts[0];

  /* Если с момента последней записи в первую часть прошло больше чем save_time времени или
     общий объём записанных данных без первой части больше чем save_size,
     удаляем эту часть. */
  if ((g_get_monotonic_time () - fpart->last_append_time > priv->save_time) ||
      ((priv->data_size - (fpart->data_size - DATA_FILE_HEADER_SIZE)) > priv->save_size))
    {
      /* Удаляем информацию о данных из списка. */
      priv->n_parts -= 1;
      for (i = 0; i < priv->n_parts; i++)
        priv->parts[i] = priv->parts[i + 1];
      priv->parts[priv->n_parts] = NULL;

      /* Закрываем потоки ввода, потоки вывода закрываются при добавлении новой части. */
      g_object_unref (fpart->ifdi);
      g_object_unref (fpart->ifdd);

      /* Удаляем файл индексов. */
      if (!g_file_delete (fpart->fdi, NULL, NULL))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': can't remove index file", priv->name);
          priv->fail = TRUE;
        }
      g_object_unref (fpart->fdi);

      /* Удаляем файл данных. */
      if (!g_file_delete (fpart->fdd, NULL, NULL))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': can't remove data file", priv->name);
          priv->fail = TRUE;
        }
      g_object_unref (fpart->fdd);

      /* Уменьшаем общий объём данных на размер удалённой части. */
      priv->data_size -= (fpart->data_size - DATA_FILE_HEADER_SIZE);

      g_free (fpart);

      /* Переименовываем файлы частей. */
      for (i = 0; i < priv->n_parts; i++)
        {
          GFile *fd;
          gchar *new_name;

          fpart = priv->parts[i];

          /* Переименовываем файл индексов. */
          new_name = g_strdup_printf ("%s.%06d.i", priv->name, i);
          fd = g_file_set_display_name (fpart->fdi, new_name, NULL, NULL);
          if (fd == NULL)
            {
              g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't rename index file", priv->name, i);
              priv->fail = TRUE;
            }
          else
            {
              g_object_unref (fpart->fdi);
              fpart->fdi = fd;
            }
          g_free (new_name);

          /* Переименовываем файл данных. */
          new_name = g_strdup_printf ("%s.%06d.d", priv->name, i);
          fd = g_file_set_display_name (fpart->fdd, new_name, NULL, NULL);
          if (fd == NULL)
            {
              g_warning ("HyScanDBChannelFile: channel '%s': part %d: can't rename data file", priv->name, i);
              priv->fail = TRUE;
            }
          else
            {
              g_object_unref (fpart->fdd);
              fpart->fdd = fd;
            }
          g_free (new_name);
        }
    }
  return TRUE;
}

/* Функция чтения индексов. Функция осуществляет поиск индекса в кэше и
   если не находит его производит чтение из файла.*/
static HyScanDBChannelFileIndex *
hyscan_db_channel_file_read_index (HyScanDBChannelFilePrivate *priv,
                                   guint32                     index)
{
  HyScanDBChannelFileIndex *db_index = g_hash_table_lookup (priv->cached_indexes, GUINT_TO_POINTER (index));

  HyScanDBChannelFileIndexRec rec_index;

  goffset offset;
  gssize iosize;
  guint i;

  /* Индекс найден в кэше. */
  if (db_index != NULL)
    {
      /* Индекс уже первый в цепочке. */
      if (priv->first_cached_index == db_index)
        return db_index;

      /* "Выдёргиваем" индекс из цепочки. */
      db_index->prev->next = db_index->next;

      /* Индекс последний в цепочке. */
      if (priv->last_cached_index == db_index)
        priv->last_cached_index = db_index->prev;
      else
        db_index->next->prev = db_index->prev;

      /* Помещаем в начало цепочки. */
      db_index->prev = NULL;
      db_index->next = priv->first_cached_index;
      db_index->next->prev = db_index;
      priv->first_cached_index = db_index;

      return db_index;
    }

  /* Индекс не найден в кэше. */

  /* Ищем часть содержащую требуемый индекс. */
  for (i = 0; i < priv->n_parts; i++)
    if (index >= priv->parts[i]->begin_index && index <= priv->parts[i]->end_index)
      break;

  /* Такого индекса нет. */
  if (i == priv->n_parts)
    return NULL;

  /* Считываем индекс. */
  iosize = INDEX_RECORD_SIZE;
  offset = index - priv->parts[i]->begin_index;
  offset *= iosize;
  offset += INDEX_FILE_HEADER_SIZE;

  if (!g_seekable_seek (G_SEEKABLE (priv->parts[i]->ifdi), offset, G_SEEK_SET, NULL, NULL))
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't seek to index", priv->name);
      priv->fail = TRUE;
      return NULL;
    }

  if (g_input_stream_read (priv->parts[i]->ifdi, &rec_index, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't read index", priv->name);
      priv->fail = TRUE;
      return FALSE;
    }

  /* Запоминаем индекс в кэше. */
  db_index = priv->last_cached_index;
  db_index->prev->next = NULL;
  priv->last_cached_index = db_index->prev;
  g_hash_table_remove (priv->cached_indexes, GUINT_TO_POINTER (db_index->index));
  g_hash_table_insert (priv->cached_indexes, GUINT_TO_POINTER (index), db_index);

  db_index->part = priv->parts[i];
  db_index->index = index;
  db_index->time = GINT64_FROM_LE (rec_index.time);
  db_index->offset = GUINT64_FROM_LE (rec_index.offset);
  db_index->size = GUINT32_FROM_LE (rec_index.size);

  /* Помещаем в начало цепочки. */
  db_index->prev = NULL;
  db_index->next = priv->first_cached_index;
  db_index->next->prev = db_index;
  priv->first_cached_index = db_index;

  return db_index;
}

/* Функция создаёт новый объект HyScanDBChannelFile. */
HyScanDBChannelFile *
hyscan_db_channel_file_new (const gchar *path,
                            const gchar *name,
                            gboolean     readonly)
{
  return g_object_new (HYSCAN_TYPE_DB_CHANNEL_FILE, "path", path, "name", name, "readonly", readonly, NULL);
}

/* Функция возвращает дату и время создания канала данных. */
gint64
hyscan_db_channel_file_get_ctime (HyScanDBChannelFile *channel)
{
  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), 0);

  return channel->priv->ctime;
}

/* Функция возвращает диапазон текущих значений индексов данных. */
gboolean
hyscan_db_channel_file_get_channel_data_range (HyScanDBChannelFile *channel,
                                               guint32             *first_index,
                                               guint32             *last_index)
{
  HyScanDBChannelFilePrivate *priv;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Нет данных. */
  if (priv->n_parts == 0)
    goto exit;

  if (first_index != NULL)
    *first_index = priv->parts[0]->begin_index;
  if (last_index != NULL)
    *last_index = priv->parts[priv->n_parts - 1]->end_index;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает новые данные. */
gboolean
hyscan_db_channel_file_add_channel_data (HyScanDBChannelFile *channel,
                                         gint64               time,
                                         gconstpointer        data,
                                         guint32              size,
                                         guint32             *index)
{
  HyScanDBChannelFilePrivate *priv;

  HyScanDBChannelFilePart *fpart;
  HyScanDBChannelFileIndex *db_index;
  HyScanDBChannelFileIndexRec rec_index;

  gboolean status = FALSE;
  gssize iosize;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  if (priv->readonly)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': read only mode", priv->name);
      return FALSE;
    }

  /* Время не может быть отрицательным. */
  if (time < 0)
    return FALSE;

  /* Проверяем, что записываемые данные меньше, чем максимальный размер файла. */
  if (size > priv->max_data_file_size - DATA_FILE_HEADER_SIZE)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Удаляем при необходимости старые части данных. */
  if (!hyscan_db_channel_file_remove_old_part (priv))
    {
      priv->fail = TRUE;
      goto exit;
    }

  /* Записанных данных еще нет. */
  if (priv->n_parts == 0)
    {
      if (hyscan_db_channel_file_add_part (priv))
        {
          fpart = priv->parts[priv->n_parts - 1];
          fpart->begin_time = time;
        }
      else
        {
          goto exit;
        }
    }
  /* Уже есть записанные данные. */
  else
    {
      /* Указатель на последнюю часть данных. */
      fpart = priv->parts[priv->n_parts - 1];

      /* Проверяем, что не превысили максимального числа записей. */
      if (fpart->end_index == G_MAXUINT32)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': too many records", priv->name);
          goto exit;
        }

      /* Проверяем записываемое время. */
      if (fpart->end_time >= time)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': current time %" G_GINT64_FORMAT ".%" G_GINT64_FORMAT
                     " is less or equal to previously written %" G_GINT64_FORMAT ".%" G_GINT64_FORMAT,
                     priv->name,
                     time / 1000000, time % 1000000,
                     fpart->end_time / 1000000, fpart->end_time % 1000000);

          goto exit;
        }

      /* Если при записи данных будет превышен максимальный размер файла или
         если в текущую часть идёт запись дольше чем интервал_времени_хранения/5 или
         размер записанных данных станет больше чем размер_сохраняемых_данных/5,
         создаём новую часть. */
      if ((fpart->data_size + size > priv->max_data_file_size - DATA_FILE_HEADER_SIZE) ||
          (g_get_monotonic_time () - fpart->create_time > (priv->save_time / 5)) ||
          (fpart->data_size + size > (priv->save_size / 5) - DATA_FILE_HEADER_SIZE))
        {
          if (hyscan_db_channel_file_add_part (priv))
            {
              fpart = priv->parts[priv->n_parts - 1];
              fpart->begin_time = time;
            }
          else
            {
              goto exit;
            }
        }
      /* Записываем данные в текущую часть.
         Увеличиваем счётчик индексов. */
      else
        {
          fpart->end_index += 1;
        }
    }

  /* Запоминаем записываемое время. */
  fpart->last_append_time = g_get_monotonic_time ();
  fpart->end_time = time;

  /* Структура нового индекса. */
  rec_index.time = GINT64_TO_LE (time);
  rec_index.offset = GUINT64_TO_LE (fpart->data_size);
  rec_index.size = GUINT32_TO_LE (size);

  /* Записываем индекс. */
  iosize = INDEX_RECORD_SIZE;
  if (g_output_stream_write (fpart->ofdi, &rec_index, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't write index", priv->name);
      priv->fail = TRUE;
      goto exit;
    }

  if (!g_output_stream_flush (fpart->ofdi, NULL, NULL))
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't flush index", priv->name);
      priv->fail = TRUE;
      goto exit;
    }

  /* Записываем данные. */
  iosize = size;
  if (g_output_stream_write (fpart->ofdd, data, iosize, NULL, NULL) != iosize)
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't write data", priv->name);
      priv->fail = TRUE;
      goto exit;
    }

  if (!g_output_stream_flush (fpart->ofdd, NULL, NULL))
    {
      g_warning ("HyScanDBChannelFile: channel '%s': can't flush data", priv->name);
      priv->fail = TRUE;
      goto exit;
    }

  /* Записанный индекс. */
  if (index != NULL)
    *index = fpart->end_index;

  /* Размер данных в этой части и общий объём данных. */
  fpart->data_size += size;
  priv->data_size += size;

  /* Запоминаем индекс в кэше. */
  db_index = priv->last_cached_index;
  db_index->prev->next = NULL;
  priv->last_cached_index = db_index->prev;
  g_hash_table_remove (priv->cached_indexes, GUINT_TO_POINTER (db_index->index));
  g_hash_table_insert (priv->cached_indexes, GUINT_TO_POINTER (fpart->end_index), db_index);

  db_index->part = fpart;
  db_index->index = fpart->end_index;
  db_index->time = GINT64_FROM_LE (rec_index.time);
  db_index->offset = GUINT64_FROM_LE (rec_index.offset);
  db_index->size = GUINT32_FROM_LE (rec_index.size);

  /* Помещаем в начало цепочки. */
  db_index->prev = NULL;
  db_index->next = priv->first_cached_index;
  db_index->next->prev = db_index;
  priv->first_cached_index = db_index;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция считывает данные. */
gboolean
hyscan_db_channel_file_get_channel_data (HyScanDBChannelFile *channel,
                                         guint32              index,
                                         gpointer             buffer,
                                         guint32             *buffer_size,
                                         gint64              *time)
{
  HyScanDBChannelFilePrivate *priv;

  HyScanDBChannelFileIndex *db_index;

  gboolean status = FALSE;
  gssize iosize;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем требуемую запись. */
  db_index = hyscan_db_channel_file_read_index (priv, index);

  /* Такого индекса нет. */
  if (db_index == NULL)
    goto exit;

  /* Считываем данные. */
  if (buffer != NULL)
    {
      /* Позиционируем указатель на запись. */
      if (!g_seekable_seek (G_SEEKABLE (db_index->part->ifdd), db_index->offset, G_SEEK_SET, NULL, NULL))
        {
          g_warning ("HyScanDBChannelFile: channel '%s': can't seek to data", priv->name);
          priv->fail = TRUE;
          goto exit;
        }

      /* Считываем данные. */
      iosize = MIN (*buffer_size, db_index->size);
      if (g_input_stream_read (db_index->part->ifdd, buffer, iosize, NULL, NULL) != iosize)
        {
          g_warning ("HyScanDBChannelFile: channel '%s': can't read data", priv->name);
          priv->fail = TRUE;
          goto exit;
        }
    }
  else
    {
      iosize = db_index->size;
    }

  /* Метка времени данных. */
  if (time != NULL)
    *time = db_index->time;

  /* Размер данных. */
  *buffer_size = iosize;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}


/* Функция ищет данные по метке времени. */
HyScanDBFindStatus
hyscan_db_channel_file_find_channel_data (HyScanDBChannelFile *channel,
                                          gint64               time,
                                          guint32             *lindex,
                                          guint32             *rindex,
                                          gint64              *ltime,
                                          gint64              *rtime)
{
  HyScanDBChannelFilePrivate *priv;

  gint64 begin_time;
  gint64 end_time;

  guint32 begin_index;
  guint32 end_index;
  guint32 new_index;

  HyScanDBChannelFileIndex *db_index;
  HyScanDBFindStatus status = HYSCAN_DB_FIND_FAIL;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return HYSCAN_DB_FIND_FAIL;

  g_mutex_lock (&priv->lock);

  /* Нет данных. */
  if (priv->n_parts == 0)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }

  /* Проверяем границу начала данных. */
  if (time < priv->parts[0]->begin_time)
    {
      status = HYSCAN_DB_FIND_LESS;
      goto exit;
    }

  /* Проверяем границу конца данных. */
  if (time > priv->parts[priv->n_parts - 1]->end_time)
    {
      status = HYSCAN_DB_FIND_GREATER;
      goto exit;
    }

  begin_time = priv->parts[0]->begin_time;
  end_time = priv->parts[priv->n_parts - 1]->end_time;

  begin_index = priv->parts[0]->begin_index;
  end_index = priv->parts[priv->n_parts - 1]->end_index;

  /* В цикле ищем метку времени. */
  while (TRUE)
    {
      /* Нашли точно нужный момент времени. */
      if (begin_time == time)
        {
          if (lindex != NULL)
            *lindex = begin_index;
          if (rindex != NULL)
            *rindex = begin_index;
          if (ltime != NULL)
            *ltime = begin_time;
          if (rtime != NULL)
            *rtime = begin_time;
          break;
        }

      /* Нашли точно нужный момент времени. */
      if (end_time == time)
        {
          if (lindex != NULL)
            *lindex = end_index;
          if (rindex != NULL)
            *rindex = end_index;
          if (ltime != NULL)
            *ltime = end_time;
          if (rtime != NULL)
            *rtime = end_time;
          break;
        }

      /* Интервал поиска стал меньше 2, выходим. */
      if (end_index - begin_index == 1)
        {
          if (lindex != NULL)
            *lindex = begin_index;
          if (rindex != NULL)
            *rindex = end_index;
          if (ltime != NULL)
            *ltime = begin_time;
          if (rtime != NULL)
            *rtime = end_time;
          break;
        }

      /* Делим отрезок. */
      new_index = begin_index + ((end_index - begin_index) / 2);
      db_index = hyscan_db_channel_file_read_index (priv, new_index);
      if (db_index == NULL)
        goto exit;

      /* Корректируем границы поиска. */
      if (db_index->time <= time)
        {
          begin_index = new_index;
          begin_time = db_index->time;
        }

      if (db_index->time > time)
        {
          end_index = new_index;
          end_time = db_index->time;
        }
    }

  status = HYSCAN_DB_FIND_OK;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает максимальный размер файла данных. */
gboolean
hyscan_db_channel_file_set_channel_chunk_size (HyScanDBChannelFile *channel,
                                               guint64              chunk_size)
{
  HyScanDBChannelFilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  /* При нулевом значении устаналиваем размер по умолчанию. */
  if (chunk_size == 0)
    chunk_size = DEFAULT_DATA_FILE_SIZE;

  /* Проверяем новый размер файла. */
  if ((chunk_size < MIN_DATA_FILE_SIZE) || (chunk_size > MAX_DATA_FILE_SIZE))
    return FALSE;

  /* Устанавливаем новый размер. */
  g_mutex_lock (&priv->lock);
  priv->max_data_file_size = chunk_size;
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает интервал времени в микросекундах для которого хранятся записываемые данные. */
gboolean
hyscan_db_channel_file_set_channel_save_time (HyScanDBChannelFile *channel,
                                              gint64               save_time)
{
  HyScanDBChannelFilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  /* При нулевом значении устаналиваем время по умолчанию. */
  if (save_time == 0)
    save_time = G_MAXINT64;

  /* Проверяем новый интервал времени. */
  if (save_time < 5000000)
    return FALSE;

  /* Устанавливаем новый интервал времени. */
  g_mutex_lock (&priv->lock);
  priv->save_time = save_time;
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает максимальный объём сохраняемых данных. */
gboolean
hyscan_db_channel_file_set_channel_save_size (HyScanDBChannelFile *channel,
                                              guint64              save_size)
{
  HyScanDBChannelFilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel), FALSE);

  priv = channel->priv;

  if (priv->fail)
    return FALSE;

  /* При нулевом значении устаналиваем объём по умолчанию. */
  if (save_size == 0)
    save_size = G_MAXUINT64;

  /* Проверяем новый максимальный размер. */
  if (save_size < MIN_DATA_FILE_SIZE)
    return FALSE;

  /* Устанавливаем новый максимальный размер. */
  g_mutex_lock (&priv->lock);
  priv->save_size = save_size;
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция завершает запись данных. */
void
hyscan_db_channel_file_finalize_channel (HyScanDBChannelFile *channel)
{
  HyScanDBChannelFilePrivate *priv;

  guint i;

  g_return_if_fail (HYSCAN_IS_DB_CHANNEL_FILE (channel));

  priv = channel->priv;

  /* Закрываем все потоки записи данных. */
  g_mutex_lock (&priv->lock);

  for (i = 0; i < priv->n_parts; i++)
    {
      g_clear_object (&priv->parts[i]->ofdi);
      g_clear_object (&priv->parts[i]->ofdd);
    }

  priv->readonly = TRUE;

  g_mutex_unlock (&priv->lock);
}

/* Функция удаляет все файлы в каталоге path относящиеся к каналу name. */
gboolean
hyscan_db_channel_remove_channel_files (const gchar *path,
                                        const gchar *name)
{
  gboolean status = TRUE;
  gchar *channel_file = NULL;
  gint i;

  /* Удаляем файлы индексов. */
  for (i = 0; i < MAX_PARTS; i++)
    {
      channel_file = g_strdup_printf ("%s%s%s.%06d.i", path, G_DIR_SEPARATOR_S, name, i);

      /* Если файлы закончились - выходим. */
      if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
        {
          g_free (channel_file);
          break;
        }

      if (g_unlink (channel_file) != 0)
        {
          g_warning ("HyScanDBFile: can't remove file %s", channel_file);
          status = FALSE;
        }
      g_free (channel_file);

      if (!status)
        return status;
    }

  /* Удаляем файлы данных. */
  for (i = 0; i < MAX_PARTS; i++)
    {
      channel_file = g_strdup_printf ("%s%s%s.%06d.d", path, G_DIR_SEPARATOR_S, name, i);

      /* Если файлы закончились - выходим. */
      if (!g_file_test (channel_file, G_FILE_TEST_IS_REGULAR))
        {
          g_free (channel_file);
          break;
        }

      if (g_unlink (channel_file) != 0)
        {
          g_warning ("HyScanDBFile: can't remove file %s", channel_file);
          status = FALSE;
        }
      g_free (channel_file);

      if (!status)
        return status;
    }

  return status;
}
