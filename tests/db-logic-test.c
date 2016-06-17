
#include "hyscan-db.h"

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#define BOOLEAN_VALUE(value) (value % 2 ? TRUE : FALSE)
#define INTEGER_VALUE(value) (2 * value)
#define DOUBLE_VALUE(value)  (0.2 * value)
#define STRING_VALUE(value)  (g_strdup_printf ("%d", 2 * value))
#define ENUM_VALUE(value)    (((2 * value) % 5) + 1)

/* Функция сверяет два списка строк в произвольном порядке. */
void
check_list (gchar  *error_prefix,
            gchar **orig,
            gchar **list)
{
  gint64 sum = 0;
  guint n;

  /* Hash сумма всех строк контрольного списка. */
  for (n = 0; n < g_strv_length (orig); n++)
    sum += g_str_hash (orig[n]) % 1024;

  /* Сверяем число строк в списках. */
  if (g_strv_length (orig) != g_strv_length (list))
    g_error ("%s: wrong number of elements", error_prefix);

  /* Hash сумма всех строк проверяемого списка должна совпадать
     с hash суммой всех строк контрольного списка. */
  for (n = 0; n < g_strv_length (list); n++)
    sum -= g_str_hash (list[n]) % 1024;

  if (sum != 0)
    g_error ("%s: wrong list", error_prefix);
}

/* Функция сверяет список проектов с контрольным списком. */
void
check_project_list (HyScanDB *db,
                    gchar    *error_prefix,
                    gchar   **orig)
{
  gchar **list;

  list = hyscan_db_project_list (db);
  if (list == NULL)
    g_error ("%s: can't get projects list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список галсов в проекте с контрольным списком. */
void
check_track_list (HyScanDB *db,
                  gchar    *error_prefix,
                  gchar   **orig,
                  guint32   project_id)
{
  gchar **list;

  list = hyscan_db_track_list (db, project_id);
  if (list == NULL)
    g_error ("%s: can't get tracks list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список каналов данных галса с контрольным списком. */
void
check_channel_list (HyScanDB *db,
                    gchar    *error_prefix,
                    gchar   **orig,
                    gint32    track_id)
{
  gchar **list;

  list = hyscan_db_channel_list (db, track_id);
  if (list == NULL)
    g_error ("%s: can't get channels list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список групп параметров проекта с контрольным списком. */
void
check_project_param_list (HyScanDB *db,
                          gchar    *error_prefix,
                          gchar   **orig,
                          guint32   project_id)
{
  gchar **list;

  list = hyscan_db_project_param_list (db, project_id);
  if (list == NULL)
    g_error ("%s: can't get objects list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список групп параметров проекта с контрольным списком. */
void
check_object_list (HyScanDB *db,
                   gchar    *error_prefix,
                   gchar   **orig,
                   guint32   param_id)
{
  gchar **list;

  list = hyscan_db_param_object_list (db, param_id);
  if (list == NULL)
    g_error ("%s: can't get object list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет схему параметров объекта с контрольной схемой. */
void
check_object_schema (HyScanDB         *db,
                     gchar            *error_prefix,
                     HyScanDataSchema *orig_schema,
                     guint32           param_id,
                     gchar            *object_name)
{
  HyScanDataSchema *schema;
  gchar **orig_list;
  gchar **list;

  schema = hyscan_db_param_object_get_schema (db, param_id, object_name);

  orig_list = hyscan_data_schema_list_keys (orig_schema);
  list = hyscan_data_schema_list_keys (schema);

  check_list (error_prefix, orig_list, list);

  g_strfreev (orig_list);
  g_strfreev (list);
}

/* Функция значения 4-х разных параметров в группе в значения по умолчанию. */
void
clear_parameters (HyScanDB *db,
                  gchar    *error_prefix,
                  guint32   param_id,
                  gchar    *object_name)
{
  hyscan_db_param_set (db, param_id, object_name, "/boolean", HYSCAN_DATA_SCHEMA_TYPE_BOOLEAN, NULL, 0);
  hyscan_db_param_set (db, param_id, object_name, "/integer", HYSCAN_DATA_SCHEMA_TYPE_INTEGER, NULL, 0);
  hyscan_db_param_set (db, param_id, object_name, "/double", HYSCAN_DATA_SCHEMA_TYPE_DOUBLE, NULL, 0);
  hyscan_db_param_set (db, param_id, object_name, "/string", HYSCAN_DATA_SCHEMA_TYPE_STRING, NULL, 0);
  hyscan_db_param_set (db, param_id, object_name, "/null", HYSCAN_DATA_SCHEMA_TYPE_STRING, NULL, 0);
  hyscan_db_param_set (db, param_id, object_name, "/enum", HYSCAN_DATA_SCHEMA_TYPE_ENUM, NULL, 0);
}

/* Функция устанавливает значения 4-х разных параметров в группе. */
void
set_parameters (HyScanDB *db,
                gchar    *error_prefix,
                gint      value,
                guint32   param_id,
                gchar    *object_name)
{
  gboolean bvalue = BOOLEAN_VALUE (value);
  gint64 ivalue = INTEGER_VALUE (value);
  gdouble dvalue = DOUBLE_VALUE (value);
  gchar *svalue = STRING_VALUE (value);
  gint64 evalue = ENUM_VALUE (value);

  hyscan_db_param_set_boolean (db, param_id, object_name, "/boolean", bvalue);
  hyscan_db_param_set_integer (db, param_id, object_name, "/integer", ivalue);
  hyscan_db_param_set_double (db, param_id, object_name, "/double", dvalue);
  hyscan_db_param_set_string (db, param_id, object_name, "/string", svalue);
  hyscan_db_param_set_string (db, param_id, object_name, "/null", svalue);
  hyscan_db_param_set_enum (db, param_id, object_name, "/enum", evalue);

  g_free (svalue);
}

/* Функция проверяет значения 4-х разных параметров в группе. */
void
check_parameters (HyScanDB *db,
                  gchar    *error_prefix,
                  gint      value,
                  guint32   param_id,
                  gchar    *object_name)
{
  gboolean orig_bvalue,  bvalue;
  gint64   orig_ivalue,  ivalue;
  gdouble  orig_dvalue,  dvalue;
  gchar   *orig_svalue, *svalue;
  gint64   orig_evalue,  evalue;

  orig_bvalue = BOOLEAN_VALUE (value);
  orig_ivalue = INTEGER_VALUE (value);
  orig_dvalue = DOUBLE_VALUE (value);
  orig_svalue = STRING_VALUE (value);
  orig_evalue = ENUM_VALUE (value);

  if (!hyscan_db_param_get_boolean (db, param_id, object_name, "/boolean", &bvalue))
    g_error ("%s: can't get /boolean parameter", error_prefix);
  if (!hyscan_db_param_get_integer (db, param_id, object_name, "/integer", &ivalue))
    g_error ("%s: can't get /integer parameter", error_prefix);
  if (!hyscan_db_param_get_double (db, param_id, object_name, "/double", &dvalue))
    g_error ("%s: can't get /double parameter", error_prefix);
  svalue = hyscan_db_param_get_string (db, param_id, object_name, "/string");
  if (svalue == NULL)
    g_error ("%s: can't get /string parameter", error_prefix);
  if (hyscan_db_param_get_string (db, param_id, object_name, "/null") != NULL)
    g_error ("%s: error in /null parameter", error_prefix);
  if (!hyscan_db_param_get_enum (db, param_id, object_name, "/enum", &evalue))
    g_error ("%s: can't get /enum parameter", error_prefix);

  if (bvalue != orig_bvalue)
    g_error ("%s: error in boolean parameter", error_prefix);
  if (ivalue != orig_ivalue)
    g_error ("%s: error in integer parameter", error_prefix);
  if (ABS (dvalue - orig_dvalue) > 0.000001)
    g_error ("%s: error in double parameter", error_prefix);
  if (g_strcmp0 (svalue, orig_svalue) != 0)
    g_error ("%s: error in string parameter", error_prefix);
  if (evalue != orig_evalue)
    g_error ("%s: error in enum parameter", error_prefix);

  g_free (orig_svalue);
  g_free (svalue);
}

int
main (int argc, char **argv)
{
  gchar               *db_uri                = NULL;           /* Путь к базе данных. */

  gint                 n_projects            = 8;              /* Число проектов в базе данных. */
  gint                 n_tracks              = 8;              /* Число галсов в каждом проекте. */
  gint                 n_channels            = 8;              /* Число каналов данных в каждом галсе. */
  gint                 n_gparams             = 8;              /* Число групп параметров в каждом проекте и галсе. */
  gint                 n_objects             = 32;             /* Число объектов в каждой группе параметров. */

  HyScanDB            *db                    = NULL;           /* База данных. */
  GBytes              *schema_data           = NULL;           /* Описание схемы параметров. */
  HyScanDataSchema    *schema                = NULL;           /* Схема объектов. */

  gchar              **projects              = NULL;           /* Контрольный список проектов. */
  gchar              **tracks                = NULL;           /* Контрольный список галсов. */
  gchar              **channels              = NULL;           /* Контрольный список каналов данных. */
  gchar              **gparams               = NULL;           /* Контрольный список групп параметров. */
  gchar              **objects               = NULL;           /* Контрольный список объектов в группах параметров. */

  gchar              **projects2             = NULL;           /* Контрольный список половины проектов. */
  gchar              **tracks2               = NULL;           /* Контрольный список половины галсов. */
  gchar              **channels2             = NULL;           /* Контрольный список половины каналов данных. */
  gchar              **gparams2              = NULL;           /* Контрольный список половины групп параметров. */
  gchar              **objects2              = NULL;           /* Контрольный список объектов в группах параметров. */

  gint32              *project_id            = NULL;
  gint32             **track_id              = NULL;
  gint32            ***channel_id            = NULL;
  gint32             **project_param_id      = NULL;
  gint32             **track_param_id        = NULL;
  gint32            ***channel_param_id      = NULL;

  gint32               sample_size           = 0;
  gchar               *sample1               = "THIS IS SAMPLE DATA";
  gchar               *buffer                = NULL;

  guint32              mod_count             = 0;

  gint                 i, j, k, l, m, n;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"projects", 'p', 0, G_OPTION_ARG_INT, &n_projects, "Number of projects", NULL},
      {"tracks", 't', 0, G_OPTION_ARG_INT, &n_tracks, "Number of tracks per project", NULL},
      {"channels", 'c', 0, G_OPTION_ARG_INT, &n_channels, "Number of channels per track", NULL},
      {"params", 'g', 0, G_OPTION_ARG_INT, &n_gparams, "Number of parameter groups", NULL},
      {"objects", 'o', 0, G_OPTION_ARG_INT, &n_objects, "Number of objects in parameter groups", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  if (n_projects < 2)
    n_projects = 2;
  if (n_tracks < 2)
    n_tracks = 2;
  if (n_channels < 2)
    n_channels = 2;
  if (n_gparams < 2)
    n_gparams = 2;
  if (n_objects < 2)
    n_objects = 2;

  /* Контрольные списки объектов. */
  projects = g_malloc ((n_projects + 1) * sizeof (gchar *));
  tracks = g_malloc ((n_tracks + 1) * sizeof (gchar *));
  channels = g_malloc ((n_channels + 1) * sizeof (gchar *));
  gparams = g_malloc ((n_gparams + 1) * sizeof (gchar *));
  objects = g_malloc ((n_objects + 1) * sizeof (gchar *));

  for (i = 0; i < n_projects; i++)
    projects[i] = g_strdup_printf ("Project%d", i + 1);
  for (i = 0; i < n_tracks; i++)
    tracks[i] = g_strdup_printf ("Track%d", i + 1);
  for (i = 0; i < n_channels; i++)
    channels[i] = g_strdup_printf ("Channel%d", i + 1);
  for (i = 0; i < n_gparams; i++)
    gparams[i] = g_strdup_printf ("Parameters%d", i + 1);
  for (i = 0; i < n_objects; i++)
    objects[i] = g_strdup_printf ("Object%d", i + 1);

  projects[n_projects] = NULL;
  tracks[n_tracks] = NULL;
  channels[n_channels] = NULL;
  gparams[n_gparams] = NULL;
  objects[n_objects] = NULL;

  projects2 = g_malloc ((n_projects / 2 + 1) * sizeof (gchar *));
  tracks2 = g_malloc ((n_tracks / 2 + 1) * sizeof (gchar *));
  channels2 = g_malloc ((n_channels / 2 + 1) * sizeof (gchar *));
  gparams2 = g_malloc ((n_gparams / 2 + 1) * sizeof (gchar *));
  objects2 = g_malloc ((n_objects / 2 + 1) * sizeof (gchar *));

  for (i = 1; i < n_projects; i += 2)
    projects2[i / 2] = g_strdup_printf ("Project%d", i + 1);
  for (i = 1; i < n_tracks; i += 2)
    tracks2[i / 2] = g_strdup_printf ("Track%d", i + 1);
  for (i = 1; i < n_channels; i += 2)
    channels2[i / 2] = g_strdup_printf ("Channel%d", i + 1);
  for (i = 1; i < n_gparams; i += 2)
    gparams2[i / 2] = g_strdup_printf ("Parameters%d", i + 1);
  for (i = 1; i < n_objects; i += 2)
    objects2[i / 2] = g_strdup_printf ("Object%d", i + 1);

  projects2[n_projects / 2] = NULL;
  tracks2[n_tracks / 2] = NULL;
  channels2[n_channels / 2] = NULL;
  gparams2[n_gparams / 2] = NULL;
  objects2[n_objects / 2] = NULL;

  /* Идентификаторы открытых объектов. */
  project_id = g_malloc0 (n_projects * sizeof (gint32));
  project_param_id = g_malloc0 (n_projects * sizeof (gint32 *));
  track_id = g_malloc0 (n_projects * sizeof (gint32 *));
  track_param_id = g_malloc0 (n_projects * sizeof (gint32 *));
  channel_id = g_malloc0 (n_projects * sizeof (gint32 *));
  channel_param_id = g_malloc0 (n_projects * sizeof (gint32 *));

  for (i = 0; i < n_projects; i++)
    {
      project_param_id[i] = g_malloc0 (n_gparams * sizeof (gint32));
      track_id[i] = g_malloc0 (n_tracks * sizeof (gint32));
      track_param_id[i] = g_malloc0 (n_tracks * sizeof (gint32));
      channel_id[i] = g_malloc0 (n_tracks * sizeof (gint32 *));
      channel_param_id[i] = g_malloc0 (n_tracks * sizeof (gint32 *));

      for (j = 0; j < n_tracks; j++)
        {
          channel_id[i][j] = g_malloc0 (n_channels * sizeof (gint32 *));
          channel_param_id[i][j] = g_malloc0 (n_channels * sizeof (gint32 *));
        }
    }

  /* Схемы данных. */
  schema_data = g_resources_lookup_data ("/org/hyscan/schemas/db-logic-schema.xml",
                                    G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (schema_data == NULL)
    g_error ("can't load db-logic schema");

  schema = hyscan_data_schema_new_from_string (g_bytes_get_data (schema_data, NULL), "complex");

  /* Буфер ввода/вывода. */
  sample_size = (gint32)strlen (sample1) + 1;
  buffer = g_malloc (sample_size);

  /* Создаём объект базы данных. */
  g_message ("db uri %s", db_uri);
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Тесты уровня проектов. */
  g_message (" ");
  g_message ("checking projects");

  /* Проверяем, что список проектов изначально пустой. */
  g_message ("checking projects list is empty");
  if (hyscan_db_project_list (db) != NULL)
    g_error ("projects list must be empty");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_is_exist (db, projects[i], NULL, NULL))
      g_error ("project '%s' exist", projects[i]);

  /* Создаём проекты. */
  g_message ("creating projects");
  mod_count = hyscan_db_get_mod_count (db, 0);
  for (i = 0; i < n_projects; i++)
    {
      project_id[i] = hyscan_db_project_create (db, projects[i], g_bytes_get_data (schema_data, NULL));
      if (project_id[i] < 0)
        g_error ("can't create '%s'", projects[i]);
      if (!hyscan_db_is_exist (db, projects[i], NULL, NULL))
        g_error ("project '%s' does not exist", projects[i]);
      if (mod_count == hyscan_db_get_mod_count (db, 0))
        g_error ("modification counter fail on create '%s'", projects[i]);
      mod_count = hyscan_db_get_mod_count (db, 0);
    }

  /* Проверяем названия созданных проектов. */
  g_message ("checking projects names");
  check_project_list (db, "check_project_list", projects);

  /* Проверяем, что нет возможности создать проект с именем уже существующего проекта. */
  g_message ("checking projects duplication");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_project_create (db, projects[i], NULL) > 0)
      g_error ("'%s' duplicated", projects[i]);

  /* Проверяем, что при открытии уже открытых проектов возвращаются уникальные идентификаторы. */
  g_message ("checking projects identifiers");
  for (i = 0; i < n_projects; i++)
    {
      gint32 nid;
      if ((nid = hyscan_db_project_open (db, projects[i])) < 0)
        g_error ("can't open '%s'", projects[i]);
      for (j=0; j < n_projects; j++)
        {
          if (nid == project_id[j])
            g_error ("duplicated '%s' id", projects[i]);
        }
      hyscan_db_close (db, project_id[i]);
      project_id[i] = nid;
    }

  /* Проверяем, что список групп параметров в каждом проекте изначально пустой. */
  g_message ("checking project parameter groups list is empty");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_project_param_list (db, project_id[i]) != NULL)
      g_error ("project parameter groups list must be empty");

  /* Создаём группы параметров в каждом проекте. */
  g_message ("creating project parameters");
  for (i = 0; i < n_projects; i++)
    {
      mod_count = hyscan_db_get_mod_count (db, project_id[i]);
      for (j = 0; j < n_gparams; j++)
        {
          if ((project_param_id[i][j] = hyscan_db_project_param_open (db, project_id[i], gparams[j])) < 0)
            g_error ("can't create '%s.%s'", projects[i], gparams[j]);
          if (mod_count == hyscan_db_get_mod_count (db, project_id[i]))
            g_error ("modification counter fail on create '%s.%s'", projects[i], gparams[j]);
          mod_count = hyscan_db_get_mod_count (db, project_id[i]);
        }
    }

  /* Проверяем названия созданных групп параметров. */
  g_message ("checking project parameters groups names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list ('%s')", projects[i]);
      check_project_param_list (db, error_prefix, gparams, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что при открытии уже открытых групп параметров возвращаются уникальные идентификаторы. */
  g_message ("checking project parameters identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gint32 nid;
        if((nid = hyscan_db_project_param_open (db, project_id[i], gparams[j])) < 0)
          g_error ("can't open '%s.%s'", projects[i], gparams[j]);
        for (k = 0; k < n_projects; k++)
          for (l = 0; l < n_gparams; l++)
            if (nid == project_param_id[k][l])
              g_error ("duplicated '%s.%s' id", projects[i], gparams[j]);
        hyscan_db_close (db, nid);
      }

  /* Проверяем, что список объектов в каждой группе параметров изначально пустой. */
  g_message ("checking objects list is empty");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if (hyscan_db_param_object_list (db, project_param_id[i][j]) != NULL)
        g_error ("objects list must be empty");

  /* Создаём объекты в каждой группе параметров. */
  g_message ("creating objects");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
        for (k = 0; k < n_objects; k++)
          {
            if (!hyscan_db_param_object_create (db, project_param_id[i][j], objects[k], "complex"))
              g_error ("can't create: %s.%s.%s", projects[i], gparams[j], objects[k]);
            if (mod_count == hyscan_db_get_mod_count (db, project_param_id[i][j]))
              g_error ("modification counter fail on create %s.%s.%s", projects[i], gparams[j], objects[k]);
            mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
          }
      }

  /* Проверяем названия созданных объектов. */
  g_message ("checking objects names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_object_list ('%s.%s')", projects[i], gparams[j]);
        check_object_list (db, error_prefix, objects, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем схему созданных объектов. */
  g_message ("checking objects schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_object_schema (db, error_prefix, schema, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Проверяем значения параметров по умолчанию. */
  g_message ("checking objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_parameters (db, error_prefix, 1, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Устанавливаем значения параметров. */
  g_message ("setting objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
        for (k = 0; k < n_objects; k++)
          {
            gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
            set_parameters (db, error_prefix, k + 2, project_param_id[i][j], objects[k]);
            if (mod_count == hyscan_db_get_mod_count (db, project_param_id[i][j]))
              g_error ("modification counter fail on %s", error_prefix);
            mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
            g_free (error_prefix);
          }
      }

  /* Проверяем новые значения параметров. */
  g_message ("checking objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_parameters (db, error_prefix, k + 2, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Закрываем параметры. */
  g_message ("closing projects parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      hyscan_db_close (db, project_param_id[i][j]);

  /* Тесты уровня галсов. */
  g_message (" ");
  g_message ("checking tracks");

  /* Проверяем, что список галсов в каждом проекте изначально пустой. */
  g_message ("checking project tracks list is empty");
  for (i = 0; i < n_projects; i++)
    {
      if (hyscan_db_track_list (db, project_id[i]) != NULL)
        g_error ("%s tracks list must be empty", projects[i]);
      for (j = 0; j < n_tracks; j++)
        if (hyscan_db_is_exist (db, projects[i], tracks[j], NULL))
          g_error ("track '%s.%s' exist", projects[i], tracks[j]);
    }

  /* Создаём галсы в каждом проекте. */
  g_message ("creating tracks");
  for (i = 0; i < n_projects; i++)
    {
      mod_count = hyscan_db_get_mod_count (db, project_id[i]);
      for (j = 0; j < n_tracks; j++)
        {
          track_id[i][j] = hyscan_db_track_create (db, project_id[i], tracks[j],
                                                   g_bytes_get_data (schema_data, NULL), "complex");
          if (track_id[i][j] < 0)
            g_error ("can't create '%s.%s'", projects[i], tracks[j]);
          if (!hyscan_db_is_exist (db, projects[i], tracks[j], NULL))
            g_error ("track '%s.%s' does not exist", projects[i], tracks[j]);
          if (mod_count == hyscan_db_get_mod_count (db, project_id[i]))
            g_error ("modification counter fail on create '%s.%s'", projects[i], tracks[j]);
          mod_count = hyscan_db_get_mod_count (db, project_id[i]);
        }
    }

  /* Проверяем названия созданных галсов. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list ('%s')", projects[i]);
      check_track_list (db, error_prefix, tracks, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что нет возможности создать галс с именем уже существующего галса. */
  g_message ("checking tracks duplication");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_track_create (db, project_id[i], tracks[j], NULL, NULL) > 0)
        g_error ("'%s.%s' duplicated", projects[i], tracks[j]);

  /* Тесты уровня каналов данных. */
  g_message (" ");
  g_message ("checking channels");

  /* Проверяем, что список каналов данных в каждом галсе изначально пустой. */
  g_message ("checking track channels list is empty");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        if (hyscan_db_channel_list (db, track_id[i][j]) != NULL)
          g_error ("'%s.%s' channels list must be empty", projects[i], tracks[j]);
        for (k = 0; k < n_channels; k++)
          if (hyscan_db_is_exist (db, projects[i], tracks[j], channels[k]))
            g_error ("channel '%s.%s.%s' exist", projects[i], tracks[j], channels[k]);
      }

  /* Создаём каналы данных в каждом галсе. */
  g_message ("creating channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        mod_count = hyscan_db_get_mod_count (db, track_id[i][j]);
        for (k = 0; k < n_channels; k++)
          {
            channel_id[i][j][k] = hyscan_db_channel_create (db, track_id[i][j], channels[k], "complex");
            if (channel_id[i][j][k] < 0)
              g_error ("can't create '%s.%s.%s'", projects[i], tracks[j], channels[k]);
            if (!hyscan_db_is_exist (db, projects[i], tracks[j], channels[k]))
              g_error ("channel '%s.%s.%s' does not exist", projects[i], tracks[j], channels[k]);
            if (mod_count == hyscan_db_get_mod_count (db, track_id[i][j]))
              g_error ("modification counter fail on create '%s.%s.%s'", projects[i], tracks[j], channels[k]);
            mod_count = hyscan_db_get_mod_count (db, track_id[i][j]);
          }
      }

  /* Проверяем режим доступа к каналам данных. */
  g_message ("checking channels mode");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (!hyscan_db_channel_is_writable (db, channel_id[i][j][k]))
          g_error ("channel is readonly '%s.%s.%s'", projects[i], tracks[j], channels[k]);

  /* Записываем контрольные данные в каналы. */
  g_message ("writing channels data");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          mod_count = hyscan_db_get_mod_count (db, channel_id[i][j][k]);
          if (!hyscan_db_channel_add_data (db, channel_id[i][j][k], 0, sample1, sample_size, NULL))
            g_error ("can't add data to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          if (mod_count == hyscan_db_get_mod_count (db, channel_id[i][j][k]))
            g_error ("modification counter fail on data add to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем названия созданных каналов данных. */
  g_message ("checking channels names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_channel_list ('%s.%s')", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем, что нет возможности создать канал данных с именем уже существующего канала. */
  g_message ("checking channels duplication");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_channel_create (db, track_id[i][j], channels[k], NULL) > 0)
          g_error ("'%s.%s.%s' duplicated", projects[i], tracks[j], channels[k]);

  /* Тесты уровня параметров галсов и каналов данных. */
  g_message (" ");
  g_message ("checking tracks and channels parameters");

  /* Открываем параметры галсов. */
  g_message ("opening tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        track_param_id[i][j] = hyscan_db_track_param_open (db, track_id[i][j]);
        if (track_param_id[i][j] < 0)
          g_error ("can't open '%s.%s' parameters", projects[i], tracks[j]);
      }

  /* Открываем параметры каналов данных. */
  g_message ("opening channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          channel_param_id[i][j][k] = hyscan_db_channel_param_open (db, channel_id[i][j][k]);
          if (channel_param_id[i][j][k] < 0)
            g_error ("can't open '%s.%s.%s' parameters", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем схему параметров галсов. */
  g_message ("checking tracks parameters schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s')", projects[i], tracks[j]);
        check_object_schema (db, error_prefix, schema, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Проверяем схему параметров каналов данных. */
  g_message ("checking channels parameters schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          check_object_schema (db, error_prefix, schema, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Проверяем значения параметров галсов по умолчанию. */
  g_message ("checking tracks parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s')", projects[i], tracks[j]);
        check_parameters (db, error_prefix, 1, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Проверяем значения параметров каналов данных по умолчанию. */
  g_message ("checking channels parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, 1, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Устанавливаем значения параметров галсов. */
  g_message ("setting tracks parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s')", projects[i], tracks[j]);
        mod_count = hyscan_db_get_mod_count (db, track_param_id[i][j]);
        set_parameters (db, error_prefix, j + 2, track_param_id[i][j], NULL);
        if (mod_count == hyscan_db_get_mod_count (db, track_param_id[i][j]))
          g_error ("modification counter fail on %s", error_prefix);
        g_free (error_prefix);
      }

  /* Устанавливаем значения параметров каналов данных. */
  g_message ("setting channels parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          mod_count = hyscan_db_get_mod_count (db, channel_param_id[i][j][k]);
          set_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          if (mod_count == hyscan_db_get_mod_count (db, channel_param_id[i][j][k]))
            g_error ("modification counter fail on %s", error_prefix);
          g_free (error_prefix);
        }

  /* Завершаем запись в канал данных. */
  g_message ("finalize channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        hyscan_db_channel_finalize (db, channel_id[i][j][k]);

  /* Проверяем режим доступа к каналам данных. */
  g_message ("checking channels mode");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_channel_is_writable (db, channel_id[i][j][k]))
          g_error ("channel is writable '%s.%s.%s'", projects[i], tracks[j], channels[k]);

  /* Проверяем, что нет возможности записать данные в каналы. */
  g_message ("trying add data to channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          mod_count = hyscan_db_get_mod_count (db, channel_id[i][j][k]);
          if (hyscan_db_channel_add_data (db, channel_id[i][j][k], 1, sample1, sample_size, NULL))
            g_error ("'%s.%s.%s' must be read only", projects[i], tracks[j], channels[k]);
          if (mod_count != hyscan_db_get_mod_count (db, channel_id[i][j][k]))
            g_error ("modification counter fail on data add to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем, что нет возможности изменить значения параметров каналов данных. */
  g_message ("trying to change channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          mod_count = hyscan_db_get_mod_count (db, channel_param_id[i][j][k]);
          set_parameters (db, error_prefix, k + 3, channel_param_id[i][j][k], NULL);
          check_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          if (mod_count != hyscan_db_get_mod_count (db, channel_param_id[i][j][k]))
            g_error ("modification counter fail on %s", error_prefix);
          g_free (error_prefix);
        }

  /* Закрываем параметры галсов. */
  g_message ("closing tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      hyscan_db_close (db, track_param_id[i][j]);

  /* Закрываем параметры каналов данных. */
  g_message ("closing channels parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        hyscan_db_close (db, channel_param_id[i][j][k]);

  /* Проверяем, что при открытии уже открытых галсов возвращаются уникальные идентификаторы. */
  g_message (" ");
  g_message ("re-opening tracks");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gint32 nid;
        if ((nid = hyscan_db_track_open (db, project_id[i], tracks[j])) < 0)
          g_error ("can't open '%s.%s'", projects[i], tracks[j]);
        for (k = 0; k < n_projects; k++)
          for (l = 0; l < n_tracks; l++)
            if (nid == track_id[k][l])
              g_error ("duplicated '%s.%s' id", projects[i], tracks[j]);
        hyscan_db_close (db, track_id[i][j]);
        track_id[i][j] = nid;
      }

  /* Проверяем, что при открытии уже открытых каналов данных возвращаются уникальные идентификаторы. */
  g_message ("re-opening channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gint32 nid;
          nid = hyscan_db_channel_open (db, track_id[i][j], channels[k]);
          if(nid < 0)
            g_error ("can't open '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          for (l = 0; l < n_projects; l++)
            for (m = 0; m < n_tracks; m++)
              for (n = 0; n < n_channels; n++)
                if (nid == channel_id[l][m][n])
                  g_error ("duplicated '%s.%s.%s' id", projects[i], tracks[j], channels[k]);
          hyscan_db_close (db, channel_id[i][j][k]);
          channel_id[i][j][k] = nid;
        }

  /* Проверяем, что данные записанные в канал совпадают с образцом. */
  g_message (" ");
  g_message ("checking channels content");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gint32 readed_size = sample_size;
          if (!hyscan_db_channel_get_data (db, channel_id[i][j][k], 0, buffer, &sample_size, NULL))
            g_error ("can't read data from 'Project %d.Track %d.Channel %d'", i + 1, j + 1, k + 1);
          if (readed_size != sample_size || g_strcmp0 (sample1, buffer) != 0)
            g_error ("wrong 'Project %d.Track %d.Channel %d' data", i + 1, j + 1, k + 1);
        }

  /* Проверяем, что нет возможности записать данные в каналы. */
  g_message ("trying add data to channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          mod_count = hyscan_db_get_mod_count (db, channel_id[i][j][k]);
          if (hyscan_db_channel_add_data (db, channel_id[i][j][k], 1, sample1, sample_size, NULL))
            g_error ("'%s.%s.%s' must be read only", projects[i], tracks[j], channels[k]);
          if (mod_count != hyscan_db_get_mod_count (db, channel_id[i][j][k]))
            g_error ("modification counter fail on data add to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
        }

  /* Открываем параметры галсов через идентификаторы только для чтения. */
  g_message ("opening tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        track_param_id[i][j] = hyscan_db_track_param_open (db, track_id[i][j]);
        if (track_param_id[i][j] < 0)
          g_error ("can't open '%s.%s' parameters", projects[i], tracks[j]);
      }

  /* Открываем параметры каналов данных через идентификаторы только для чтения. */
  g_message ("opening channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          channel_param_id[i][j][k] = hyscan_db_channel_param_open (db, channel_id[i][j][k]);
          if (channel_param_id[i][j][k] < 0)
            g_error ("can't open '%s.%s.%s' parameters", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем значения параметров галсов. */
  g_message ("checking tracks parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s')", projects[i], tracks[j]);
        check_parameters (db, error_prefix, j + 2, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Проверяем, что нет возможности изменить значения параметров галсов. */
  g_message ("trying to change tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s')", projects[i], tracks[j]);
        mod_count = hyscan_db_get_mod_count (db, track_param_id[i][j]);
        set_parameters (db, error_prefix, j + 3, track_param_id[i][j], NULL);
        check_parameters (db, error_prefix, j + 2, track_param_id[i][j], NULL);
        if (mod_count != hyscan_db_get_mod_count (db, track_param_id[i][j]))
          g_error ("modification counter fail on %s", error_prefix);
        g_free (error_prefix);
      }

  /* Проверяем значения параметров каналов данных. */
  g_message ("checking channels parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Проверяем, что нет возможности изменить значения параметров каналов данных. */
  g_message ("trying to change channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          mod_count = hyscan_db_get_mod_count (db, channel_param_id[i][j][k]);
          set_parameters (db, error_prefix, k + 3, channel_param_id[i][j][k], NULL);
          check_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          if (mod_count != hyscan_db_get_mod_count (db, channel_param_id[i][j][k]))
            g_error ("modification counter fail on %s", error_prefix);
          g_free (error_prefix);
        }

  /* Закрываем все объекты. */
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        hyscan_db_close (db, channel_param_id[i][j][k]);

  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        hyscan_db_close (db, channel_id[i][j][k]);

  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      hyscan_db_close (db, track_param_id[i][j]);

  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      hyscan_db_close (db, track_id[i][j]);

  for (i = 0; i < n_projects; i++)
    hyscan_db_close (db, project_id[i]);

  /* Удаляем объект базы данных. */
  g_object_unref (db);

  g_message (" ");
  g_message ("re-opening db");

  /* Пересоздаём объект базы данных с уже существующими проектами. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Тесты уровня проектов. */
  g_message (" ");
  g_message ("checking projects");

  /* Проверяем список проектов. */
  g_message ("checking projects names");
  check_project_list (db, "check_project_list", projects);

  /* Открываем все проекты. */
  g_message ("opening projects");
  for (i = 0; i < n_projects; i++)
    if ((project_id[i] = hyscan_db_project_open (db, projects[i])) < 0)
      g_error ("can't open '%s'", projects[i]);

  /* Проверяем названия групп параметров. */
  g_message ("checking project parameters groups names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list ('%s')", projects[i]);
      check_project_param_list (db, error_prefix, gparams, project_id[i]);
      g_free (error_prefix);
    }

  /* Открываем группы параметров. */
  g_message ("opening project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if ((project_param_id[i][j] = hyscan_db_project_param_open (db, project_id[i], gparams[j])) < 0)
        g_error ("can't open '%s.%s'", projects[i], gparams[j]);

  /* Проверяем названия объектов. */
  g_message ("checking objects names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_object_list ('%s.%s')", projects[i], gparams[j]);
        check_object_list (db, error_prefix, objects, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем схему объектов. */
  g_message ("checking objects schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_object_schema (db, error_prefix, schema, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Проверяем значения параметров. */
  g_message ("checking objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_parameters (db, error_prefix, k + 2, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Сбрасываем значения параметров в значения по умолчанию. */
  g_message ("clearing objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
        for (k = 0; k < n_objects; k++)
          {
            gchar *error_prefix = g_strdup_printf ("clear_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
            clear_parameters (db, error_prefix, project_param_id[i][j], objects[k]);
            if (mod_count == hyscan_db_get_mod_count (db, project_param_id[i][j]))
              g_error ("modification counter fail on %s", error_prefix);
            mod_count = hyscan_db_get_mod_count (db, project_param_id[i][j]);
            g_free (error_prefix);
          }
      }

  /* Проверяем значения параметров. */
  g_message ("checking objects values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], gparams[j], objects[k]);
          check_parameters (db, error_prefix, 1, project_param_id[i][j], objects[k]);
          g_free (error_prefix);
        }

  /* Тесты уровня галсов. */
  g_message (" ");
  g_message ("checking tracks");

  /* Проверяем список галсов. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list ('%s')", projects[i]);
      check_track_list (db, error_prefix, tracks, project_id[i]);
      g_free (error_prefix);
    }

  /* Открываем все галсы. */
  g_message ("opening tracks");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if ((track_id[i][j] = hyscan_db_track_open (db, project_id[i], tracks[j])) < 0)
        g_error ("can't open '%s.%s'", projects[i], tracks[j]);

  /* Открываем параметры галсов. */
  g_message ("opening tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        track_param_id[i][j] = hyscan_db_track_param_open (db, track_id[i][j]);
        if (track_param_id[i][j] < 0)
          g_error ("can't open '%s.%s' parameters", projects[i], tracks[j]);
      }

  /* Проверяем схему параметров галсов. */
  g_message ("checking tracks parameters schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s')", projects[i], tracks[j]);
        check_object_schema (db, error_prefix, schema, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Проверяем значения параметров галсов. */
  g_message ("checking tracks parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s')", projects[i], tracks[j]);
        check_parameters (db, error_prefix, j + 2, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Проверяем, что нет возможности изменить значения параметров галсов. */
  g_message ("trying to change tracks parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s')", projects[i], tracks[j]);
        set_parameters (db, error_prefix, j + 3, track_param_id[i][j], NULL);
        check_parameters (db, error_prefix, j + 2, track_param_id[i][j], NULL);
        g_free (error_prefix);
      }

  /* Тесты уровня каналов данных. */
  g_message (" ");
  g_message ("checking channels");

  /* Проверяем список каналов данных. */
  g_message ("checking channels names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_channel_list ('%s.%s')", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Открываем все каналы данных. */
  g_message ("opening channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if ((channel_id[i][j][k] = hyscan_db_channel_open (db, track_id[i][j], channels[k])) < 0)
          g_error ("can't open '%s.%s.%s'", projects[i], tracks[j], channels[k]);

  /* Проверяем правильность записанных данных каналов. */
  g_message ("checking channels content");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gint32 readed_size = sample_size;
          if (!hyscan_db_channel_get_data (db, channel_id[i][j][k], 0, buffer, &sample_size, NULL))
            g_error ("can't read data from '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          if (readed_size != sample_size || g_strcmp0 (sample1, buffer) != 0)
            g_error ("wrong '%s.%s.%s' data", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем, что нет возможности записать данные в каналы. */
  g_message ("trying add data to channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          mod_count = hyscan_db_get_mod_count (db, channel_id[i][j][k]);
          if (hyscan_db_channel_add_data (db, channel_id[i][j][k], 1, sample1, sample_size, NULL))
            g_error ("'%s.%s.%s' must be read only", projects[i], tracks[j], channels[k]);
          if (mod_count != hyscan_db_get_mod_count (db, channel_id[i][j][k]))
            g_error ("modification counter fail on data add to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
        }

  /* Открываем параметры каналов данных. */
  g_message ("opening channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          channel_param_id[i][j][k] = hyscan_db_channel_param_open (db, channel_id[i][j][k]);
          if (channel_param_id[i][j][k] < 0)
            g_error ("can't open '%s.%s.%s' parameters", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем схему параметров каналов данных. */
  g_message ("checking channels parameters schemas");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_object_schema ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          check_object_schema (db, error_prefix, schema, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Проверяем значения параметров каналов данных. */
  g_message ("checking channels parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("check_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Проверяем, что нет возможности изменить значения параметров галсов. */
  g_message ("trying to change channels parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix = g_strdup_printf ("set_parameters ('%s.%s.%s')", projects[i], tracks[j], channels[k]);
          set_parameters (db, error_prefix, k + 3, channel_param_id[i][j][k], NULL);
          check_parameters (db, error_prefix, k + 2, channel_param_id[i][j][k], NULL);
          g_free (error_prefix);
        }

  /* Тесты удаления объектов. */
  g_message (" ");
  g_message ("removing objects");

  /* Удаляем половину каналов данных. */
  g_message ("removing half channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        mod_count = hyscan_db_get_mod_count (db, track_id[i][j]);
        for (k = 0; k < n_channels; k += 2)
          {
            if (!hyscan_db_channel_remove (db, track_id[i][j], channels[k]))
              g_error ("can't remove '%s.%s.%s'", projects[i], tracks[j], channels[k]);
            if (mod_count == hyscan_db_get_mod_count (db, track_id[i][j]))
              {
                g_error ("modification counter fail on remove '%s.%s.%s'",
                         projects[i], tracks[j], channels[k]);
              }
            mod_count = hyscan_db_get_mod_count (db, track_id[i][j]);
          }
      }

  /* Проверяем изменившейся список. */
  g_message ("checking channels names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_channel_list ('%s.%s')", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels2, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем, что идентификаторы удалённых каналов данных не работоспсобны. */
  g_message ("checking removed channels id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k += 2)
        if (hyscan_db_channel_get_data_range (db, channel_id[i][j][k], NULL, NULL))
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  /* Проверяем, что идентификаторы параметров удалённых каналов данных не работоспсобны. */
  g_message ("checking removed channel parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k += 2)
        if (hyscan_db_param_object_get_schema (db, channel_param_id[i][j][k], NULL) != NULL)
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  /* Удаляем половину галсов. */
  g_message ("removing half tracks");
  for (i = 0; i < n_projects; i++)
    {
      mod_count = hyscan_db_get_mod_count (db, project_id[i]);
      for (j = 0; j < n_tracks; j += 2)
        {
          if (!hyscan_db_track_remove (db, project_id[i], tracks[j]))
            g_error ("can't remove '%s.%s'", projects[i], tracks[j]);
          if (mod_count == hyscan_db_get_mod_count (db, project_id[i]))
            g_error ("modification counter fail on remove '%s.%s'", projects[i], tracks[j]);
          mod_count = hyscan_db_get_mod_count (db, project_id[i]);
        }
    }

  /* Проверяем изменившейся список. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list ('%s')", projects[i]);
      check_track_list (db, error_prefix, tracks2, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что идентификаторы галсов не работоспсобны. */
  g_message ("checking tracks id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j += 2)
      if (hyscan_db_channel_create (db, track_id[i][j], "Channel", NULL) > 0)
        g_error ("'%s.%s' is still alive", projects[i], tracks[j]);

  /* Проверяем, что идентификаторы параметров удалённых галсов не работоспсобны. */
  g_message ("checking track parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j += 2)
      if (hyscan_db_param_object_get_schema (db, track_param_id[i][j], NULL) != NULL)
        g_error ("'%s.%s' is still alive", projects[i], tracks[j]);

  /* Удаляем половину групп параметров проектов. */
  g_message ("removing half project parameters");
  for (i = 0; i < n_projects; i++)
    {
      mod_count = hyscan_db_get_mod_count (db, project_id[i]);
      for (j = 0; j < n_gparams; j += 2)
        {
          if (!hyscan_db_project_param_remove (db, project_id[i], gparams[j]))
            g_error ("can't remove '%s.%s'", projects[i], gparams[j]);
          if (mod_count == hyscan_db_get_mod_count (db, project_id[i]))
            g_error ("modification counter fail on remove '%s.%s'", projects[i], gparams[j]);
          mod_count = hyscan_db_get_mod_count (db, project_id[i]);
        }
    }

  /* Проверяем изменившейся список. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list ('%s')", projects[i]);
      check_project_param_list (db, error_prefix, gparams2, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что идентификаторы удалённых групп параметров не работоспсобны. */
  g_message ("checking project parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j += 2)
      for (k = 0; k < n_objects; k++)
        if (hyscan_db_param_object_get_schema (db, project_param_id[i][j], objects[k]) != NULL)
            g_error ("'%s.%s.%s' is still alive", projects[i], gparams[j], objects[k]);

  /* Удаляем половину проектов. */
  g_message ("removing half projects");
  mod_count = hyscan_db_get_mod_count (db, 0);
  for (i = 0; i < n_projects; i += 2)
    {
      if (!hyscan_db_project_remove (db, projects[i]))
        g_error ("can't remove '%s'", projects[i]);
      if (mod_count == hyscan_db_get_mod_count (db, 0))
        g_error ("modification counter fail on remove '%s'", projects[i]);
    }

  /* Проверяем изменившейся список. */
  g_message ("checking projects names");
  check_project_list (db, "check_project_list", projects2);

  /* Удаляем оставшиеся проекты. */
  g_message ("removing all projects");
  mod_count = hyscan_db_get_mod_count (db, 0);
  for (i = 1; i < n_projects; i += 2)
    {
      if (!hyscan_db_project_remove (db, projects[i]))
        g_error ("can't delete '%s'", projects[i]);
      if (mod_count == hyscan_db_get_mod_count (db, 0))
        g_error ("modification counter fail on remove '%s'", projects[i]);
    }

  /* Проверяем, что список проектов пустой. */
  g_message ("checking db projects list is empty");
  if (hyscan_db_project_list (db) != NULL)
    g_error ("projects list must be empty");

  /* Проверяем, что все идентификаторы открытых объектов более не работоспособны. */
  g_message ("checking project parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      for (k = 0; k < n_objects; k++)
        if (hyscan_db_param_object_get_schema (db, project_param_id[i][j], objects[k]) != NULL)
            g_error ("'%s.%s.%s' is still alive", projects[i], gparams[j], objects[k]);

  g_message ("checking projects id validity");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_track_create (db, project_id[i], "Track", NULL, NULL) > 0)
      g_error ("'%s' still alive", projects[i]);

  g_message ("checking track parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_param_object_get_schema (db, track_param_id[i][j], NULL) != NULL)
        g_error ("'%s.%s' is still alive", projects[i], tracks[j]);

  g_message ("checking tracks id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_channel_create (db, track_id[i][j], "Channel", NULL) > 0)
        g_error ("'%s.%s' is still alive", projects[i], tracks[j]);

  g_message ("checking channel parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_param_object_get_schema (db, channel_param_id[i][j][k], NULL) != NULL)
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  g_message ("checking channels id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_channel_get_data_range (db, channel_id[i][j][k], NULL, NULL))
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  /* Удаляем объект базы данных. */
  g_object_unref (db);

  /* Освобождаем память (для нормальной работы valgind). */
  g_strfreev (projects);
  g_strfreev (tracks);
  g_strfreev (channels);
  g_strfreev (gparams);
  g_strfreev (objects);

  g_strfreev (projects2);
  g_strfreev (tracks2);
  g_strfreev (channels2);
  g_strfreev (gparams2);
  g_strfreev (objects2);

  for (i = 0; i < n_projects; i++)
    {
      for (j = 0; j < n_tracks; j++)
        {
          g_free (channel_param_id[i][j]);
          g_free (channel_id[i][j]);
        }
      g_free (channel_param_id[i]);
      g_free (channel_id[i]);
      g_free (track_param_id[i]);
      g_free (track_id[i]);
      g_free (project_param_id[i]);
    }

  g_free (channel_param_id);
  g_free (channel_id);
  g_free (track_param_id);
  g_free (track_id);
  g_free (project_param_id);
  g_free (project_id);

  g_free (buffer);

  g_free (db_uri);

  g_bytes_unref (schema_data);
  g_object_unref (schema);

  return 0;
}
