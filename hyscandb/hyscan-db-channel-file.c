/**
 * \file hyscan-db-channel-file.c
 *
 * \brief Исходный файл класса хранения данных канала в файловой системе
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-channel-file.h"

#include <gio/gio.h>
#include <string.h>

#define INDEX_FILE_MAGIC        0x58495348       /* HSIX в виде строки. */
#define DATA_FILE_MAGIC         0x54445348       /* HSDT в виде строки. */
#define FILE_VERSION            0x37303531       /* 1507 в виде строки. */

#define MAX_PARTS               999999           /* Максимальное число частей данных. */
#define CACHED_INDEXES          2048             /* Число кэшированных индексов. */

#define INDEX_FILE_HEADER_SIZE  12               /* Размер заголовка файла индексов. */
#define DATA_FILE_HEADER_SIZE   8                /* Размер заголовка файла данных. */

#define MIN_DATA_FILE_SIZE      1*1024*1024      /* Минимально возможный размер файла части данных. */
#define MAX_DATA_FILE_SIZE      1024*1024*1024   /* Максимально возможный размер файла части данных. */

enum
{
  PROP_O,
  PROP_PATH,
  PROP_NAME,
  PROP_READONLY
};

/* Информация о части списка данных. */
typedef struct
{
  gint32                    data_size;           /* Размер файла данных этой части. */

  gint64                    create_time;         /* Время создания этой части данных. */
  gint64                    last_append_time;    /* Время последней записи данных в эту часть. */

  gint32                    begin_index;         /* Начальный индекс данных в этой части. */
  gint32                    end_index;           /* Конечный индекс данных в этой части. */

  gint64                    begin_time;          /* Начальное время данных в этой части. */
  gint64                    end_time;            /* Конечное время данных в этой части. */

  GFile                    *fdi;                 /* Объект работы с файлом индексов. */
  GInputStream             *ifdi;                /* Поток чтения файла индексов. */
  GOutputStream            *ofdi;                /* Поток записи файла индексов. */

  GFile                    *fdd;                 /* Объект работы с файлом данных. */
  GInputStream             *ifdd;                /* Поток чтения файла данных. */
  GOutputStream            *ofdd;                /* Поток записи файла данных. */
} HyScanDBChannelFilePart;

/* Структура индексной записи в файле. */
typedef struct
{
  gint64                    time;                /* Время приёма данных, в микросекундах. */
  gint32                    offset;              /* Смещение до начала данных. */
  gint32                    size;                /* Размер данных. */
} HyScanDBChannelFileIndexRec;

/* Информация о записи. */
typedef struct _HyScanDBChannelFileIndex HyScanDBChannelFileIndex;
struct _HyScanDBChannelFileIndex
{
  HyScanDBChannelFilePart  *part;                /* Номер части с данными. */
  gint32                    index;               /* Значение индекса. */

  gint64                    time;                /* Время приёма данных, в микросекундах. */
  gint32                    offset;              /* Смещение до начала данных. */
  gint32                    size;                /* Размер данных. */

                                                 /* Структуры индексов связаны между собой
                                                    не в порядке их номеров.*/
  HyScanDBChannelFileIndex *prev;                /* Указатель на предыдущий индекс. */
  HyScanDBChannelFileIndex *next;                /* Указатель на следующий индекс.*/
};

/* Внутренние данные объекта. */
struct _HyScanDBChannelFile
{
  GObject                   parent_instance;

  gchar                    *path;                /* Путь к каталогу с файлами данных канала. */
  gchar                    *name;                /* Название канала. */

  gint32                    max_data_file_size;  /* Максимальный размер файла части данных. */
  gint64                    save_time;           /* Интервал времени для которого хранятся записываемые данные. */
  gint64                    save_size;           /* Максимальный объём всех сохраняемых данных. */

  gboolean                  readonly;            /* Создавать или нет файлы при открытии канала. */
  gboolean                  fail;                /* Признак ошибки в объекте. */

  GMutex                    lock;                /* Блокировка многопоточного доступа. */

  gint64                    data_size;           /* Текущий объём хранимых данных. */

  gint32                    begin_index;         /* Начальный индекс данных. */
  gint32                    end_index;           /* Конечный индекс данных. */

  gint64                    begin_time;          /* Начальное время данных. */
  gint64                    end_time;            /* Конечное время данных. */

  HyScanDBChannelFilePart **parts;               /* Массив указателей на структуры с описанием части данных. */
  gint                      n_parts;             /* Число частей данных. */

  GHashTable               *cached_indexes;      /* Таблица кэшированных индексов. */
  HyScanDBChannelFileIndex *first_cached_index;  /* Первый индекс (недавно использовался). */
  HyScanDBChannelFileIndex *last_cached_index;   /* Последний индекс (давно использовался). */

};

static void                      hyscan_db_channel_file_set_property       (GObject             *object,
                                                                            guint                prop_id,
                                                                            const GValue        *value,
                                                                            GParamSpec          *pspec);
static void                      hyscan_db_channel_file_object_constructed (GObject             *object);
static void                      hyscan_db_channel_file_object_finalize    (GObject             *object);

static gboolean                  hyscan_db_channel_file_add_part           (HyScanDBChannelFile *channel);
static gboolean                  hyscan_db_channel_file_remove_old_part    (HyScanDBChannelFile *channel);
static HyScanDBChannelFileIndex *hyscan_db_channel_file_read_index         (HyScanDBChannelFile *channel,
                                                                            gint32               index);

G_DEFINE_TYPE (HyScanDBChannelFile, hyscan_db_channel_file, G_TYPE_OBJECT);

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
}

static void
hyscan_db_channel_file_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanDBChannelFile *channel = HYSCAN_DB_CHANNEL_FILE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      channel->name = g_value_dup_string (value);
      break;

    case PROP_PATH:
      channel->path = g_value_dup_string (value);
      break;

    case PROP_READONLY:
      channel->readonly = g_value_get_boolean (value);
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

  gint i;

  /* Начальные значения. */
  channel->max_data_file_size = MAX_DATA_FILE_SIZE;
  channel->save_time = G_MAXINT64;
  channel->save_size = G_MAXINT64;
  channel->readonly = FALSE;
  channel->fail = FALSE;
  channel->data_size = 0;
  channel->begin_index = 0;
  channel->end_index = 0;
  channel->begin_time = 0;
  channel->end_time = 0;
  channel->parts = NULL;
  channel->n_parts = 0;

  g_mutex_init (&channel->lock);

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

    channel->cached_indexes = g_hash_table_new (g_direct_hash, g_direct_equal);
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
          channel->first_cached_index = db_index;
        if (i == CACHED_INDEXES - 1)
          channel->last_cached_index = db_index;

        prev_index = db_index;
      }
  }

  /* Проверяем существующие данные и открываем их на чтение в случае существования. */
  while (TRUE)
    {
      HyScanDBChannelFilePart *fpart;

      gchar *fname;
      GFileInfo *finfo = NULL;

      GFile *fdi = NULL;
      GInputStream *ifdi = NULL;
      goffset index_file_size;

      GFile *fdd = NULL;
      GInputStream *ifdd = NULL;
      goffset data_file_size;

      guint64 begin_time;
      guint64 end_time;

      HyScanDBChannelFileIndexRec rec_index;
      gint32 begin_index;
      gint32 end_index;

      GError *error;
      goffset offset;
      gsize iosize;
      gint32 value;

      /* Файл индексов. */
      fname = g_strdup_printf ("%s%s%s.%06d.i", channel->path, G_DIR_SEPARATOR_S, channel->name, channel->n_parts);
      fdi = g_file_new_for_path (fname);
      g_free (fname);

      /* Файл данных. */
      fname = g_strdup_printf ("%s%s%s.%06d.d", channel->path, G_DIR_SEPARATOR_S, channel->name, channel->n_parts);
      fdd = g_file_new_for_path (fname);
      g_free (fname);

      /* Если файлов нет - завершаем проверку. */
      if (!g_file_query_exists (fdi, NULL) && !g_file_query_exists (fdd, NULL))
        {
          g_object_unref (fdi);
          g_object_unref (fdd);
          break;
        }

      channel->readonly = TRUE;

      /* Поток чтение индексов. */
      error = NULL;
      ifdi = G_INPUT_STREAM (g_file_read (fdi, NULL, &error));
      if (error != NULL)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
        }

      /* Поток чтения данных. */
      error = NULL;
      ifdd = G_INPUT_STREAM (g_file_read (fdd, NULL, &error));
      if (error != NULL)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
        }

      /* Ошибка чтения существующих файлов. */
      if (ifdi == NULL || ifdd == NULL)
        goto break_open;

      /* Размер файла индексов. */
      error = NULL;
      finfo = g_file_query_info (fdi, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &error);
      if (error != NULL)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }
      index_file_size = g_file_info_get_size (finfo);
      g_object_unref (finfo);

      /* Проверяем размер файла с индексами - должна быть как минимум одна запись. */
      if (index_file_size < (INDEX_FILE_HEADER_SIZE + sizeof (HyScanDBChannelFileIndexRec)))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid index file size",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Проверяем размер файла с индексами - размер должен быть кратен размеру структуры индекса. */
      if ((index_file_size - INDEX_FILE_HEADER_SIZE) % sizeof (HyScanDBChannelFileIndexRec))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid index file size",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Размер файла данных. */
      error = NULL;
      finfo = g_file_query_info (fdd, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &error);
      if (error != NULL)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }
      data_file_size = g_file_info_get_size (finfo);
      g_object_unref (finfo);

      /* Считываем идентификатор файла индексов. */
      error = NULL;
      iosize = sizeof (gint32);
      if (g_input_stream_read (ifdi, &value, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      /* Проверяем идентификатор файла индексов. */
      value = GINT32_FROM_LE (value);
      if (value != INDEX_FILE_MAGIC)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: unknown file format",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Считываем версию файла индексов. */
      error = NULL;
      iosize = sizeof (gint32);
      if (g_input_stream_read (ifdi, &value, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      /* Проверяем версию файла индексов. */
      value = GINT32_FROM_LE (value);
      if (value != FILE_VERSION)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: unknown file format",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Считываем идентификатор файла данных. */
      error = NULL;
      iosize = sizeof (gint32);
      if (g_input_stream_read (ifdd, &value, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      /* Проверяем идентификатор файла данных. */
      value = GINT32_FROM_LE (value);
      if (value != DATA_FILE_MAGIC)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid file version",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Считываем версию файла данных. */
      error = NULL;
      iosize = sizeof (gint32);
      if (g_input_stream_read (ifdd, &value, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      /* Проверяем версию файла данных. */
      value = GINT32_FROM_LE (value);
      if (value != FILE_VERSION)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid file version",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Считываем начальный номер индекса части данных. */
      error = NULL;
      iosize = sizeof (gint32);
      if (g_input_stream_read (ifdi, &value, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      /* Проверяем начальный номер индекса части данных. */
      value = GINT32_FROM_LE (value);
      if (value < 0 || (channel->n_parts > 0 && value != channel->parts[channel->n_parts - 1]->end_index + 1))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid index",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      begin_index = value;
      end_index =
        begin_index + ((index_file_size - INDEX_FILE_HEADER_SIZE) / sizeof (HyScanDBChannelFileIndexRec)) - 1;

      /* Считываем первый индекс части данных. */
      iosize = sizeof (HyScanDBChannelFileIndexRec);
      offset = INDEX_FILE_HEADER_SIZE;

      error = NULL;
      if (!g_seekable_seek (G_SEEKABLE (ifdi), offset, G_SEEK_SET, NULL, &error))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      error = NULL;
      if (g_input_stream_read (ifdi, &rec_index, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      begin_time = GINT64_FROM_LE (rec_index.time);

      /* Считываем последний индекс части данных. */
      iosize = sizeof (HyScanDBChannelFileIndexRec);
      offset = end_index - begin_index;
      offset *= iosize;
      offset += INDEX_FILE_HEADER_SIZE;

      error = NULL;
      if (!g_seekable_seek (G_SEEKABLE (ifdi), offset, G_SEEK_SET, NULL, &error))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      error = NULL;
      if (g_input_stream_read (ifdi, &rec_index, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: %s",
                      channel->name, channel->n_parts, error->message);
          g_error_free (error);
          goto break_open;
        }

      end_time = GINT64_FROM_LE (rec_index.time);

      /* Проверяем размер файла с данными - он должен быть равен смещению до данных
         по последнему индексу + размер данных. */
      if (data_file_size != (GINT32_FROM_LE (rec_index.offset) + GINT32_FROM_LE (rec_index.size)))
        {
          g_critical ("hyscan_db_channel_file_constructor: channel '%s': part %d: invalid data file size",
                      channel->name, channel->n_parts);
          goto break_open;
        }

      /* Считываем информацию о части данных. */
      channel->n_parts += 1;
      channel->parts =
        g_realloc (channel->parts, 32 * (((channel->n_parts + 1) / 32) + 1) * sizeof (HyScanDBChannelFilePart *));

      fpart = g_malloc (sizeof (HyScanDBChannelFilePart));
      channel->parts[channel->n_parts - 1] = fpart;
      channel->parts[channel->n_parts] = NULL;

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
      channel->data_size += (data_file_size - DATA_FILE_HEADER_SIZE);

      /* Загружаем следующую часть. */
      if (channel->n_parts == MAX_PARTS)
        break;

      continue;

      /* Завершаем проверку. */
    break_open:
      /* Если не прочитали ни одной части - печалька... */
      if (channel->n_parts == 0)
        channel->fail = TRUE;
      if (ifdi != NULL)
        g_object_unref (ifdi);
      if (ifdd != NULL)
        g_object_unref (ifdd);
      if (fdi != NULL)
        g_object_unref (fdi);
      if (fdd != NULL)
        g_object_unref (fdd);
      break;
    }

  /* Если включен режим только чтения, а данных нет - ошибка. */
  if (channel->readonly && channel->n_parts == 0)
    channel->fail = TRUE;
}

static void
hyscan_db_channel_file_object_finalize (GObject *object)
{
  HyScanDBChannelFile *channel = HYSCAN_DB_CHANNEL_FILE (object);

  gint i;

  /* Освобождаем структуры кэша индексов. */
  while (channel->first_cached_index != NULL)
    {
      HyScanDBChannelFileIndex *db_index = channel->first_cached_index;
      channel->first_cached_index = channel->first_cached_index->next;
      g_slice_free (HyScanDBChannelFileIndex, db_index);
    }
  g_hash_table_destroy (channel->cached_indexes);

  /* Освобождаем структуры с информацией о частях данных. */
  for (i = 0; i < channel->n_parts; i++)
    {
      if (channel->parts[i]->ofdi != NULL)
        g_object_unref (channel->parts[i]->ofdi);
      if (channel->parts[i]->ofdd != NULL)
        g_object_unref (channel->parts[i]->ofdd);
      if (channel->parts[i]->ifdi != NULL)
        g_object_unref (channel->parts[i]->ifdi);
      if (channel->parts[i]->ifdd != NULL)
        g_object_unref (channel->parts[i]->ifdd);
      if (channel->parts[i]->fdi != NULL)
        g_object_unref (channel->parts[i]->fdi);
      if (channel->parts[i]->fdd != NULL)
        g_object_unref (channel->parts[i]->fdd);
      g_free (channel->parts[i]);
    }
  g_free (channel->parts);

  g_mutex_clear (&channel->lock);

  g_free (channel->name);
  g_free (channel->path);

  G_OBJECT_CLASS (hyscan_db_channel_file_parent_class)->finalize (object);
}

/* Функция добавления новой части данных. */
static gboolean
hyscan_db_channel_file_add_part (HyScanDBChannelFile *channel)
{
  gchar *fname;
  HyScanDBChannelFilePart *fpart;

  gint32 begin_index = 0;

  GError *error;
  gsize iosize;
  gint32 value;

  if (channel->readonly)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': read only mode", channel->name);
      return FALSE;
    }

  /* Закрываем потоки вывода текущей части. */
  if (channel->n_parts > 0)
    {
      g_object_unref (channel->parts[channel->n_parts - 1]->ofdi);
      g_object_unref (channel->parts[channel->n_parts - 1]->ofdd);
      channel->parts[channel->n_parts - 1]->ofdi = NULL;
      channel->parts[channel->n_parts - 1]->ofdd = NULL;
    }

  /* Всего частей может быть не более MAX_PARTS. */
  if (channel->n_parts == MAX_PARTS)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': too many parts", channel->name);
      return FALSE;
    }

  channel->n_parts += 1;
  channel->parts =
    g_realloc (channel->parts, 32 * (((channel->n_parts + 1) / 32) + 1) * sizeof (HyScanDBChannelFilePart *));

  fpart = g_malloc (sizeof (HyScanDBChannelFilePart));
  channel->parts[channel->n_parts - 1] = fpart;
  channel->parts[channel->n_parts] = NULL;

  /* Имя файла индексов. */
  fname = g_strdup_printf ("%s%s%s.%06d.i", channel->path, G_DIR_SEPARATOR_S, channel->name, channel->n_parts - 1);
  fpart->fdi = g_file_new_for_path (fname);
  g_free (fname);

  /* Имя файла данных. */
  fname = g_strdup_printf ("%s%s%s.%06d.d", channel->path, G_DIR_SEPARATOR_S, channel->name, channel->n_parts - 1);
  fpart->fdd = g_file_new_for_path (fname);
  g_free (fname);

  /* Создаём файл индексов и открываем его на запись. */
  error = NULL;
  fpart->ofdi = G_OUTPUT_STREAM (g_file_create (fpart->fdi, G_FILE_CREATE_NONE, NULL, &error));
  if (error != NULL)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
    }

  /* Создаём файл данных и открываем его на запись. */
  error = NULL;
  fpart->ofdd = G_OUTPUT_STREAM (g_file_create (fpart->fdd, G_FILE_CREATE_NONE, NULL, &error));
  if (error != NULL)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
    }

  /* Открываем файл индексов на чтение. */
  error = NULL;
  fpart->ifdi = G_INPUT_STREAM (g_file_read (fpart->fdi, NULL, &error));
  if (error != NULL)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
    }

  /* Открываем файл данных на чтение. */
  error = NULL;
  fpart->ifdd = G_INPUT_STREAM (g_file_read (fpart->fdd, NULL, &error));
  if (error != NULL)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
    }

  /* Ошибка при создании нового списка данных. */
  if (fpart->ofdi == NULL || fpart->ofdd == NULL || fpart->ifdi == NULL || fpart->ifdd == NULL)
    {
      channel->fail = TRUE;
      return FALSE;
    }

  if (channel->n_parts == 1)
    begin_index = 0;
  else
    begin_index = channel->parts[channel->n_parts - 2]->end_index + 1;

  fpart->create_time = g_get_monotonic_time ();

  fpart->begin_index = begin_index;
  fpart->end_index = begin_index;

  fpart->begin_time = 0;
  fpart->end_time = 0;

  iosize = sizeof (gint32);

  /* Запись идентификатора файла индексов. */
  error = NULL;
  value = GINT32_TO_LE (INDEX_FILE_MAGIC);
  if (g_output_stream_write (fpart->ofdi, &value, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': can't write index magic header (%s)",
                  channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Запись версии файла индексов. */
  error = NULL;
  value = GINT32_TO_LE (FILE_VERSION);
  if (g_output_stream_write (fpart->ofdi, &value, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': can't write index file version (%s)",
                  channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Запись значения начального индекса. */
  error = NULL;
  value = GINT32_TO_LE (begin_index);
  if (g_output_stream_write (fpart->ofdi, &value, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': can't write start index value (%s)",
                  channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Запись идентификатора файла данных. */
  error = NULL;
  value = GINT32_TO_LE (DATA_FILE_MAGIC);
  if (g_output_stream_write (fpart->ofdd, &value, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': can't write data magic header (%s)",
                  channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Запись версии файла данных. */
  error = NULL;
  value = GINT32_TO_LE (FILE_VERSION);
  if (g_output_stream_write (fpart->ofdd, &value, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add_part: channel '%s': can't write data magic header (%s)",
                  channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  fpart->data_size = DATA_FILE_HEADER_SIZE;

  return TRUE;
}

/* Функция удаляет старые части данных. */
static gboolean
hyscan_db_channel_file_remove_old_part (HyScanDBChannelFile *channel)
{
  HyScanDBChannelFilePart *fpart;

  GError *error;
  gint i;

  if (channel->readonly)
    return TRUE;
  if (channel->n_parts < 2)
    return TRUE;

  fpart = channel->parts[0];

  /* Если с момента последней записи в первую часть прошло больше чем save_time времени или
     общий объём записанных данных без первой части больше чем save_size,
     удаляем эту часть. */
  if ((g_get_monotonic_time () - fpart->last_append_time > channel->save_time) ||
      ((channel->data_size - (fpart->data_size - DATA_FILE_HEADER_SIZE)) > channel->save_size))
    {
      /* Удаляем информацию о данных из списка. */
      channel->n_parts -= 1;
      for (i = 0; i < channel->n_parts; i++)
        channel->parts[i] = channel->parts[i + 1];
      channel->parts[channel->n_parts] = NULL;

      /* Закрываем потоки ввода, потоки вывода закрываются при добавлении новой части. */
      g_object_unref (fpart->ifdi);
      g_object_unref (fpart->ifdd);

      /* Удаляем файл индексов. */
      error = NULL;
      if (!g_file_delete (fpart->fdi, NULL, &error))
        {
          g_critical ("hyscan_db_channel_file_remove_old_part: channel '%s': can't remove index file (%s)",
                      channel->name, error->message);
          g_error_free (error);
          channel->fail = TRUE;
        }
      g_object_unref (fpart->fdi);

      /* Удаляем файл данных. */
      error = NULL;
      if (!g_file_delete (fpart->fdd, NULL, &error))
        {
          g_critical ("hyscan_db_channel_file_remove_old_part: channel '%s': can't remove data file (%s)",
                      channel->name, error->message);
          g_error_free (error);
          channel->fail = TRUE;
        }
      g_object_unref (fpart->fdd);

      /* Уменьшаем общий объём данных на размер удалённой части. */
      channel->data_size -= (fpart->data_size - DATA_FILE_HEADER_SIZE);

      g_free (fpart);

      /* Переименовываем файлы частей. */
      for (i = 0; i < channel->n_parts; i++)
        {
          GFile *fd;
          gchar *new_name;

          fpart = channel->parts[i];

          /* Переименовываем файл индексов. */
          new_name = g_strdup_printf ("%s.%06d.i", channel->name, i);
          error = NULL;
          fd = g_file_set_display_name (fpart->fdi, new_name, NULL, &error);
          if (fd == NULL)
            {
              g_critical
                ("hyscan_db_channel_file_remove_old_part: channel '%s': part %d: can't rename index file (%s)",
                 channel->name, i, error->message);
              g_error_free (error);
              channel->fail = TRUE;
            }
          else
            {
              g_object_unref (fpart->fdi);
              fpart->fdi = fd;
            }
          g_free (new_name);

          /* Переименовываем файл данных. */
          new_name = g_strdup_printf ("%s.%06d.d", channel->name, i);
          error = NULL;
          fd = g_file_set_display_name (fpart->fdd, new_name, NULL, &error);
          if (fd == NULL)
            {
              g_critical
                ("hyscan_db_channel_file_remove_old_part: channel '%s': part %d: can't rename data file (%s)",
                 channel->name, i, error->message);
              g_error_free (error);
              channel->fail = TRUE;
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
hyscan_db_channel_file_read_index (HyScanDBChannelFile *channel,
                                   gint32               index)
{
  HyScanDBChannelFileIndex *db_index = g_hash_table_lookup (channel->cached_indexes, GINT_TO_POINTER (index));

  HyScanDBChannelFileIndexRec rec_index;

  GError *error;
  goffset offset;
  gsize iosize;
  gint i;

  /* Индекс найден в кэше. */
  if (db_index != NULL)
    {
      /* Индекс уже первый в цепочке. */
      if (channel->first_cached_index == db_index)
        return db_index;

      /* "Выдёргиваем" индекс из цепочки. */
      db_index->prev->next = db_index->next;

      /* Индекс последний в цепочке. */
      if (channel->last_cached_index == db_index)
        channel->last_cached_index = db_index->prev;
      else
        db_index->next->prev = db_index->prev;

      /* Помещаем в начало цепочки. */
      db_index->prev = NULL;
      db_index->next = channel->first_cached_index;
      db_index->next->prev = db_index;
      channel->first_cached_index = db_index;

      return db_index;
    }

  /* Индекс не найден в кэше. */

  /* Ищем часть содержащую требуемый индекс. */
  for (i = 0; i < channel->n_parts; i++)
    if (index >= channel->parts[i]->begin_index && index <= channel->parts[i]->end_index)
      break;

  /* Такого индекса нет. */
  if (i == channel->n_parts)
    return NULL;

  /* Считываем индекс. */
  iosize = sizeof (HyScanDBChannelFileIndexRec);
  offset = index - channel->parts[i]->begin_index;
  offset *= iosize;
  offset += INDEX_FILE_HEADER_SIZE;

  error = NULL;
  if (!g_seekable_seek (G_SEEKABLE (channel->parts[i]->ifdi), offset, G_SEEK_SET, NULL, &error))
    {
      g_critical ("hyscan_db_channel_file_read_index: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return NULL;
    }

  error = NULL;
  if (g_input_stream_read (channel->parts[i]->ifdi, &rec_index, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_read_index: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Запоминаем индекс в кэше. */
  db_index = channel->last_cached_index;
  db_index->prev->next = NULL;
  channel->last_cached_index = db_index->prev;
  g_hash_table_remove (channel->cached_indexes, GUINT_TO_POINTER (db_index->index));
  g_hash_table_insert (channel->cached_indexes, GUINT_TO_POINTER (index), db_index);

  db_index->part = channel->parts[i];
  db_index->index = index;
  db_index->time = GINT64_FROM_LE (rec_index.time);
  db_index->offset = GINT32_FROM_LE (rec_index.offset);
  db_index->size = GINT32_FROM_LE (rec_index.size);

  /* Помещаем в начало цепочки. */
  db_index->prev = NULL;
  db_index->next = channel->first_cached_index;
  db_index->next->prev = db_index;
  channel->first_cached_index = db_index;

  return db_index;
}

/* Функция возвращает диапазон текущих значений индексов данных. */
gboolean
hyscan_db_channel_file_get_channel_data_range (HyScanDBChannelFile *channel,
                                               gint32              *first_index,
                                               gint32              *last_index)
{
  g_mutex_lock (&channel->lock);

  /* Нет данных. */
  if (channel->n_parts == 0)
    {
      g_mutex_unlock (&channel->lock);
      return FALSE;
    }

  if (first_index != NULL)
    *first_index = channel->parts[0]->begin_index;
  if (last_index != NULL)
    *last_index = channel->parts[channel->n_parts - 1]->end_index;

  g_mutex_unlock (&channel->lock);

  return TRUE;
}

/* Функция записывает новые данные. */
gboolean
hyscan_db_channel_file_add_channel_data (HyScanDBChannelFile *channel,
                                         gint64               time,
                                         gpointer             data,
                                         gint32               size,
                                         gint32              *index)
{
  HyScanDBChannelFilePart *fpart;
  HyScanDBChannelFileIndex *db_index;
  HyScanDBChannelFileIndexRec rec_index;

  GError *error;
  gsize iosize;

  if (channel->fail)
    return FALSE;

  if (channel->readonly)
    {
      g_critical ("hyscan_db_channel_file_add: channel '%s': read only mode", channel->name);
      return FALSE;
    }

  /* Время не может быть отрицательным. */
  if (time < 0)
    return FALSE;

  /* Проверяем, что записываемые данные меньше, чем максимальный размер файла. */
  if (size > channel->max_data_file_size - DATA_FILE_HEADER_SIZE)
    return FALSE;

  g_mutex_lock (&channel->lock);

  /* Удаляем при необходимости старые части данных. */
  if (!hyscan_db_channel_file_remove_old_part (channel))
    {
      channel->fail = TRUE;
      g_mutex_unlock (&channel->lock);
      return FALSE;
    }

  /* Записанных данных еще нет. */
  if (channel->n_parts == 0)
    {
      if (hyscan_db_channel_file_add_part (channel))
        {
          fpart = channel->parts[channel->n_parts - 1];
          fpart->begin_time = time;
        }
      else
        {
          g_mutex_unlock (&channel->lock);
          return FALSE;
        }
    }
  /* Уже есть записанные данные. */
  else
    {
      /* Указатель на последнюю часть данных. */
      fpart = channel->parts[channel->n_parts - 1];

      /* Проверяем, что не превысили максимального числа записей. */
      if (fpart->end_index == G_MAXINT32)
        {
          g_warning ("hyscan_db_channel_file_add: channel '%s': too many records", channel->name);
          g_mutex_unlock (&channel->lock);
          return FALSE;
        }

      /* Проверяем записываемое время. */
      if (fpart->end_time >= time)
        {
          g_warning
            ("hyscan_db_channel_file_add: channel '%s': current time %" G_GINT64_FORMAT
             ".% " G_GINT64_FORMAT " is less than previously written",
             channel->name, time / 1000000, time % 1000000);
          g_mutex_unlock (&channel->lock);
          return FALSE;
        }

      /* Если при записи данных будет превышен максимальный размер файла или
         если в текущую часть идёт запись дольше чем ( интервал времени хранения / 5 ),
         создаём новую часть. */
      if ((fpart->data_size + size > channel->max_data_file_size - DATA_FILE_HEADER_SIZE) ||
          (g_get_monotonic_time () - fpart->create_time > (channel->save_time / 5)) ||
          (fpart->data_size + size > (channel->save_size / 5) - DATA_FILE_HEADER_SIZE))
        {
          if (hyscan_db_channel_file_add_part (channel))
            {
              fpart = channel->parts[channel->n_parts - 1];
              fpart->begin_time = time;
            }
          else
            {
              g_mutex_unlock (&channel->lock);
              return FALSE;
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
  rec_index.offset = GINT32_TO_LE (fpart->data_size);
  rec_index.size = GINT32_TO_LE (size);

  /* Записываем индекс. */
  error = NULL;
  iosize = sizeof (HyScanDBChannelFileIndexRec);
  if (g_output_stream_write (fpart->ofdi, &rec_index, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      g_mutex_unlock (&channel->lock);
      channel->fail = TRUE;
      return FALSE;
    }

  if (!g_output_stream_flush (fpart->ofdi, NULL, &error))
    {
      g_critical ("hyscan_db_channel_file_add: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      g_mutex_unlock (&channel->lock);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Записываем данные. */
  error = NULL;
  iosize = size;
  if (g_output_stream_write (fpart->ofdd, data, iosize, NULL, &error) != iosize)
    {
      g_critical ("hyscan_db_channel_file_add: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      g_mutex_unlock (&channel->lock);
      channel->fail = TRUE;
      return FALSE;
    }

  if (!g_output_stream_flush (fpart->ofdd, NULL, &error))
    {
      g_critical ("hyscan_db_channel_file_add: channel '%s': %s", channel->name, error->message);
      g_error_free (error);
      g_mutex_unlock (&channel->lock);
      channel->fail = TRUE;
      return FALSE;
    }

  /* Записанный индекс. */
  if (index != NULL)
    *index = fpart->end_index;

  /* Размер данных в этой части и общий объём данных. */
  fpart->data_size += size;
  channel->data_size += size;

  /* Запоминаем индекс в кэше. */
  db_index = channel->last_cached_index;
  db_index->prev->next = NULL;
  channel->last_cached_index = db_index->prev;
  g_hash_table_remove (channel->cached_indexes, GUINT_TO_POINTER (db_index->index));
  g_hash_table_insert (channel->cached_indexes, GUINT_TO_POINTER (fpart->end_index), db_index);

  db_index->part = fpart;
  db_index->index = fpart->end_index;
  db_index->time = GINT64_FROM_LE (rec_index.time);
  db_index->offset = GINT32_FROM_LE (rec_index.offset);
  db_index->size = GINT32_FROM_LE (rec_index.size);

  /* Помещаем в начало цепочки. */
  db_index->prev = NULL;
  db_index->next = channel->first_cached_index;
  db_index->next->prev = db_index;
  channel->first_cached_index = db_index;

  g_mutex_unlock (&channel->lock);

  return TRUE;
}

/* Функция считывает данные. */
gboolean
hyscan_db_channel_file_get_channel_data (HyScanDBChannelFile *channel,
                                         gint32               index,
                                         gpointer             buffer,
                                         gint32              *buffer_size,
                                         gint64              *time)
{
  HyScanDBChannelFileIndex *db_index;

  GError *error;
  gsize iosize;

  if (channel->fail)
    return FALSE;

  g_mutex_lock (&channel->lock);

  /* Ищем требуемую запись. */
  db_index = hyscan_db_channel_file_read_index (channel, index);

  /* Такого индекса нет. */
  if (db_index == NULL)
    {
      g_mutex_unlock (&channel->lock);
      return FALSE;
    }

  /* Считываем данные. */
  if (buffer != NULL)
    {
      /* Позиционируем указатель на запись. */
      error = NULL;
      if (!g_seekable_seek (G_SEEKABLE (db_index->part->ifdd), db_index->offset, G_SEEK_SET, NULL, &error))
        {
          g_critical ("hyscan_db_channel_file_get: channel '%s': %s", channel->name, error->message);
          g_error_free (error);
          g_mutex_unlock (&channel->lock);
          channel->fail = TRUE;
          return FALSE;
        }

      /* Считываем данные. */
      error = NULL;
      iosize = MIN (*buffer_size, db_index->size);
      if (g_input_stream_read (db_index->part->ifdd, buffer, iosize, NULL, &error) != iosize)
        {
          g_critical ("hyscan_db_channel_file_get: channel '%s': %s", channel->name, error->message);
          g_error_free (error);
          g_mutex_unlock (&channel->lock);
          channel->fail = TRUE;
          return FALSE;
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

  g_mutex_unlock (&channel->lock);

  return TRUE;
}


/* Функция ищет данные по метке времени. */
gboolean
hyscan_db_channel_file_find_channel_data (HyScanDBChannelFile *channel,
                                          gint64               time,
                                          gint32              *lindex,
                                          gint32              *rindex,
                                          gint64              *ltime,
                                          gint64              *rtime)
{
  gint64 begin_time;
  gint64 end_time;

  gint32 begin_index;
  gint32 end_index;
  gint32 new_index;

  HyScanDBChannelFileIndex *db_index;

  if (channel->fail)
    return FALSE;

  g_mutex_lock (&channel->lock);

  /* Нет данных. */
  if (channel->n_parts == 0)
    {
      g_mutex_unlock (&channel->lock);
      return FALSE;
    }

  /* Проверяем границу начала данных. */
  if (time < channel->parts[0]->begin_time)
    {
      if (lindex != NULL)
        *lindex = G_MININT32;
      if (rindex != NULL)
        *rindex = channel->parts[0]->begin_index;
      if (ltime != NULL)
        *ltime = G_MININT64;
      if (rtime != NULL)
        *rtime = channel->parts[0]->begin_time;
      g_mutex_unlock (&channel->lock);
      return TRUE;
    }

  /* Проверяем границу конца данных. */
  if (time > channel->parts[channel->n_parts - 1]->end_time)
    {
      if (lindex != NULL)
        *lindex = channel->parts[channel->n_parts - 1]->end_index;
      if (rindex != NULL)
        *rindex = G_MAXINT32;
      if (ltime != NULL)
        *ltime = channel->parts[channel->n_parts - 1]->end_time;
      if (rtime != NULL)
        *rtime = G_MAXINT64;
      g_mutex_unlock (&channel->lock);
      return TRUE;
    }

  begin_time = channel->parts[0]->begin_time;
  end_time = channel->parts[channel->n_parts - 1]->end_time;

  begin_index = channel->parts[0]->begin_index;
  end_index = channel->parts[channel->n_parts - 1]->end_index;

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
      new_index = begin_index + (0.5 * (end_index - begin_index));
      db_index = hyscan_db_channel_file_read_index (channel, new_index);
      if (db_index == NULL)
        {
          g_mutex_unlock (&channel->lock);
          return FALSE;
        }

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

  g_mutex_unlock (&channel->lock);

  return TRUE;
}

/* Функция устанавливает максимальный размер файла данных. */
gboolean
hyscan_db_channel_file_set_channel_chunk_size (HyScanDBChannelFile *channel,
                                               gint32               chunk_size)
{
  /* Проверяем новый размер файла. */
  if (chunk_size < MIN_DATA_FILE_SIZE || chunk_size > MAX_DATA_FILE_SIZE)
    return FALSE;

  /* Устанавливаем новый размер. */
  g_mutex_lock (&channel->lock);
  channel->max_data_file_size = chunk_size;
  g_mutex_unlock (&channel->lock);

  return TRUE;
}

/* Функция устанавливает интервал времени в микросекундах для которого хранятся записываемые данные. */
gboolean
hyscan_db_channel_file_set_channel_save_time (HyScanDBChannelFile *channel,
                                              gint64               save_time)
{
  /* Проверяем новый интервал времени. */
  if (save_time < 5000000)
    return FALSE;

  /* Устанавливаем новый интервал времени. */
  g_mutex_lock (&channel->lock);
  channel->save_time = save_time;
  g_mutex_unlock (&channel->lock);

  return TRUE;
}

/* Функция устанавливает максимальный объём сохраняемых данных. */
gboolean
hyscan_db_channel_file_set_channel_save_size (HyScanDBChannelFile *channel,
                                              gint64               save_size)
{
  /* Проверяем новый максимальный размер. */
  if (save_size < MIN_DATA_FILE_SIZE)
    return FALSE;

  /* Устанавливаем новый максимальный размер. */
  g_mutex_lock (&channel->lock);
  channel->save_size = save_size;
  g_mutex_unlock (&channel->lock);

  return TRUE;
}


/* Функция завершает запись данных. */
void
hyscan_db_channel_file_finalize_channel (HyScanDBChannelFile *channel)
{
  gint i;

  /* Закрываем все потоки записи данных. */
  g_mutex_lock (&channel->lock);

  for (i = 0; i < channel->n_parts; i++)
    {
      if (channel->parts[i]->ofdi != NULL)
        g_object_unref (channel->parts[i]->ofdi);
      if (channel->parts[i]->ofdd != NULL)
        g_object_unref (channel->parts[i]->ofdd);
      channel->parts[i]->ofdi = NULL;
      channel->parts[i]->ofdd = NULL;
    }

  channel->readonly = TRUE;

  g_mutex_unlock (&channel->lock);
}
