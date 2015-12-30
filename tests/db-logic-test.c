
#include "hyscan-db.h"

#include <glib/gstdio.h>
#include <string.h>

#define INT_INC              32
#define INT_VALUE(value)     (2 * value)
#define DOUBLE_VALUE(value)  (2 * value + 0.1)
#define BOOLEAN_VALUE(value) (value % 2 ? FALSE : TRUE)
#define STRING_VALUE         "VALUE IS %d"

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

  list = hyscan_db_get_project_list (db);
  if (list == NULL)
    g_error ("%s: can't get project list", error_prefix);

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

  list = hyscan_db_get_track_list (db, project_id);
  if (list == NULL)
    g_error ("%s: can't get project list", error_prefix);

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

  list = hyscan_db_get_channel_list (db, track_id);
  if (list == NULL)
    g_error ("%s: can't get channel list", error_prefix);

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

  list = hyscan_db_get_project_param_list (db, project_id);
  if (list == NULL)
    g_error ("%s: can't get project group parameter list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список групп параметров галса с контрольным списком. */
void
check_track_param_list (HyScanDB *db,
                        gchar    *error_prefix,
                        gchar   **orig,
                        guint32   track_id)
{
  gchar **list;

  list = hyscan_db_get_track_param_list (db, track_id);
  if (list == NULL)
    g_error ("%s: can't get track group parameter list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция сверяет список параметров группы с контрольным списком. */
void
check_param_list (HyScanDB *db,
                  gchar    *error_prefix,
                  gchar   **orig,
                  guint32   param_id)
{
  gchar **list;

  list = hyscan_db_get_param_list (db, param_id);
  if (list == NULL)
    g_error ("%s: can't get parameter list", error_prefix);

  check_list (error_prefix, orig, list);

  g_strfreev (list);
}

/* Функция устанавливает значения 4-х разных параметров в группе. */
void
set_parameters (HyScanDB *db,
                gchar    *error_prefix,
                gint      value,
                guint32   param_id)
{
  gint i;
  gint ivalue = INT_VALUE (value);
  gdouble dvalue = DOUBLE_VALUE (value);
  gboolean bvalue = BOOLEAN_VALUE (value);
  gchar *svalue = g_strdup_printf (STRING_VALUE, value);

  if (!hyscan_db_set_integer_param (db, param_id, "integer", ivalue))
    g_error ("%s: can't set integer parameter", error_prefix);

  if (!hyscan_db_set_double_param (db, param_id, "double", dvalue))
    g_error ("%s: can't set double parameter", error_prefix);

  if (!hyscan_db_set_boolean_param (db, param_id, "common.boolean", bvalue))
    g_error ("%s: can't set boolean parameter", error_prefix);

  if (!hyscan_db_set_string_param (db, param_id, "common.string", svalue))
    g_error ("%s: can't set string parameter", error_prefix);

  for (i = 0; i < INT_INC; i++)
    if (!hyscan_db_inc_integer_param (db, param_id, "integer"))
      g_error ("%s: can't increment integer parameter", error_prefix);

  g_free (svalue);
}

/* Функция проверяет значения 4-х разных параметров в группе. */
void
check_parameters (HyScanDB *db,
                  gchar    *error_prefix,
                  gint      value,
                  guint32   param_id)
{
  gint orig_ivalue = INT_VALUE (value) + INT_INC;
  gdouble orig_dvalue = DOUBLE_VALUE (value);
  gboolean orig_bvalue = BOOLEAN_VALUE (value);
  gchar *orig_svalue = g_strdup_printf (STRING_VALUE, value);

  gint ivalue = hyscan_db_get_integer_param (db, param_id, "integer");
  gdouble dvalue = hyscan_db_get_double_param (db, param_id, "double");
  gboolean bvalue = hyscan_db_get_boolean_param (db, param_id, "common.boolean");
  gchar *svalue = hyscan_db_get_string_param (db, param_id, "common.string");

  if (!hyscan_db_has_param (db, param_id, "integer"))
    g_error ("%s: integer parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "double"))
    g_error ("%s: double parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "common.boolean"))
    g_error ("%s: boolean parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "common.string"))
    g_error ("%s: string parameter doesn't exists", error_prefix);

  if (hyscan_db_has_param (db, param_id, "test1"))
    g_error ("%s: test1 parameter exists", error_prefix);
  if (hyscan_db_has_param (db, param_id, "test2"))
    g_error ("%s: test2 parameter exists", error_prefix);

  if (ivalue != orig_ivalue)
    g_error ("%s: error in integer parameter", error_prefix);
  if (ABS (dvalue - orig_dvalue) > 0.1)
    g_error ("%s: error in double parameter", error_prefix);
  if (bvalue != orig_bvalue)
    g_error ("%s: error in boolean parameter", error_prefix);
  if (g_strcmp0 (svalue, orig_svalue) != 0)
    g_error ("%s: error in string parameter", error_prefix);

  g_free (orig_svalue);
  g_free (svalue);
}


/* Функция копирует в группу параметров еще 4 дополнительных параметра. */
void
add_temporary_parameters (HyScanDB *db,
                          gchar    *error_prefix,
                          gint      value,
                          guint32   project_id,
                          guint32   param_id)
{
  gint src_param_id = hyscan_db_open_project_param (db, project_id, "src-temp");
  if (src_param_id < 0)
    g_error ("%s: can't create temporary parameters group", error_prefix);

  if (!hyscan_db_set_integer_param (db, src_param_id, "test1.param1", value))
    g_error ("%s: can't set temporary integer parameter", error_prefix);
  if (!hyscan_db_set_integer_param (db, src_param_id, "test1.param2", 2 * value))
    g_error ("%s: can't set temporary integer parameter", error_prefix);
  if (!hyscan_db_set_integer_param (db, src_param_id, "test2.param1", value))
    g_error ("%s: can't set temporary integer parameter", error_prefix);
  if (!hyscan_db_set_integer_param (db, src_param_id, "test2.param2", 2 * value))
    g_error ("%s: can't set temporary integer parameter", error_prefix);

  if (!hyscan_db_copy_param (db, src_param_id, param_id, "test1.*"))
    g_error ("%s: can't copy 'test1' parameters", error_prefix);
  if (!hyscan_db_copy_param (db, src_param_id, param_id, "test2.*"))
    g_error ("%s: can't copy 'test2' parameters", error_prefix);

  if (!hyscan_db_remove_project_param (db, project_id, "src-temp"))
    g_error ("%s: can't remove temporary parameters group", error_prefix);
}

/* Функция проверяет значения дополнительных параметров. */
void
check_temporary_parameters (HyScanDB *db,
                            gchar    *error_prefix,
                            gint      value,
                            guint32   param_id)
{
  if (!hyscan_db_has_param (db, param_id, "test1.param1"))
    g_error ("%s: test1.param1 parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "test1.param2"))
    g_error ("%s: test1.param2 parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "test2.param1"))
    g_error ("%s: test2.param1 parameter doesn't exists", error_prefix);
  if (!hyscan_db_has_param (db, param_id, "test2.param2"))
    g_error ("%s: test2.param2 parameter doesn't exists", error_prefix);

  if (hyscan_db_has_param (db, param_id, "test1"))
    g_error ("%s: test1 parameter exists", error_prefix);
  if (hyscan_db_has_param (db, param_id, "test2"))
    g_error ("%s: test2 parameter exists", error_prefix);

  if (hyscan_db_get_integer_param (db, param_id, "test1.param1") != value)
    g_error ("%s: error in temporary parameter test1.param1", error_prefix);
  if (hyscan_db_get_integer_param (db, param_id, "test1.param2") != 2 * value)
    g_error ("%s: error in temporary parameter test1.param2", error_prefix);
  if (hyscan_db_get_integer_param (db, param_id, "test2.param1") != value)
    g_error ("%s: error in temporary parameter test2.param1", error_prefix);
  if (hyscan_db_get_integer_param (db, param_id, "test2.param2") != 2 * value)
    g_error ("%s: error in temporary parameter test2.param2", error_prefix);
}


int
main (int argc, char **argv)
{
  /* Параметры программы. */
  gchar       *db_uri = NULL;          /* Путь к базе данных. */
  gint         n_projects = 8;         /* Число проектов в базе данных. */
  gint         n_tracks = 8;           /* Число галсов в каждом проекте. */
  gint         n_channels = 8;         /* Число каналов данных в каждом галсе. */
  gint         n_gparams = 8;          /* Число групп параметров в каждом проекте и галсе. */

  HyScanDB    *db;

  gchar      **projects;               /* Контрольный список проектов. */
  gchar      **tracks;                 /* Контрольный список галсов. */
  gchar      **channels;               /* Контрольный список каналов данных. */
  gchar      **gparams;                /* Контрольный список групп параметров. */

  gchar      **projects2;              /* Контрольный список половины проектов. */
  gchar      **tracks2;                /* Контрольный список половины галсов. */
  gchar      **channels2;              /* Контрольный список половины каналов данных. */
  gchar      **gparams2;               /* Контрольный список половины групп параметров. */

  /* Контрольный список параметров в группах. */
  gchar       *params[] =
    {
      "default.integer",
      "default.double",
      "common.boolean",
      "common.string",
      NULL
    };

  /* Контрольный список расширенных параметров в группах. */
  gchar       *params_ex[] = 
    {
      "default.integer",
      "default.double",
      "common.boolean",
      "common.string",
      "test1.param1",
      "test1.param2",
      "test2.param1",
      "test2.param2",
      NULL
  };

  gint        *project_id;
  gint       **track_id;
  gint      ***channel_id;
  gint       **project_param_id;
  gint      ***track_param_id;
  gint      ***channel_param_id;

  gint32       sample_size = 0;
  gchar       *sample1 = "THIS IS SAMPLE DATA";
  gchar       *buffer = NULL;

  gint i, j, k;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"projects", 'p', 0, G_OPTION_ARG_INT, &n_projects, "Number of projects", NULL},
      {"tracks", 't', 0, G_OPTION_ARG_INT, &n_tracks, "Number of tracks per project", NULL},
      {"channels", 'c', 0, G_OPTION_ARG_INT, &n_channels, "Number of channels per track", NULL},
      {"params", 'g', 0, G_OPTION_ARG_INT, &n_gparams, "Number of grouped parameters per project or track", NULL},
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

  /* Контрольные списки объектов. */
  projects = g_malloc ((n_projects + 1) * sizeof (gchar *));
  tracks = g_malloc ((n_tracks + 1) * sizeof (gchar *));
  channels = g_malloc ((n_channels + 1) * sizeof (gchar *));
  gparams = g_malloc ((n_gparams + 1) * sizeof (gchar *));

  for (i = 0; i < n_projects; i++)
    projects[i] = g_strdup_printf ("Project %d", i + 1);
  for (i = 0; i < n_tracks; i++)
    tracks[i] = g_strdup_printf ("Track %d", i + 1);
  for (i = 0; i < n_channels; i++)
    channels[i] = g_strdup_printf ("Channel %d", i + 1);
  for (i = 0; i < n_gparams; i++)
    gparams[i] = g_strdup_printf ("Parameters %d", i + 1);

  projects[n_projects] = NULL;
  tracks[n_tracks] = NULL;
  channels[n_channels] = NULL;
  gparams[n_gparams] = NULL;

  projects2 = g_malloc ((n_projects / 2 + 1) * sizeof (gchar *));
  tracks2 = g_malloc ((n_tracks / 2 + 1) * sizeof (gchar *));
  channels2 = g_malloc ((n_channels / 2 + 1) * sizeof (gchar *));
  gparams2 = g_malloc ((n_gparams / 2 + 1) * sizeof (gchar *));

  for (i = 1; i < n_projects; i += 2)
    projects2[i / 2] = g_strdup_printf ("Project %d", i + 1);
  for (i = 1; i < n_tracks; i += 2)
    tracks2[i / 2] = g_strdup_printf ("Track %d", i + 1);
  for (i = 1; i < n_channels; i += 2)
    channels2[i / 2] = g_strdup_printf ("Channel %d", i + 1);
  for (i = 1; i < n_gparams; i += 2)
    gparams2[i / 2] = g_strdup_printf ("Parameters %d", i + 1);

  projects2[n_projects / 2] = NULL;
  tracks2[n_tracks / 2] = NULL;
  channels2[n_channels / 2] = NULL;
  gparams2[n_gparams / 2] = NULL;


  /* Идентификаторы открытых объектов. */
  project_id = g_malloc (n_projects * sizeof (gint));
  project_param_id = g_malloc (n_projects * sizeof (gint *));
  track_id = g_malloc (n_projects * sizeof (gint *));
  track_param_id = g_malloc (n_projects * sizeof (gint *));
  channel_id = g_malloc (n_projects * sizeof (gint *));
  channel_param_id = g_malloc (n_projects * sizeof (gint *));

  for (i = 0; i < n_projects; i++)
    {
      project_param_id[i] = g_malloc (n_gparams * sizeof (gint));
      track_id[i] = g_malloc (n_tracks * sizeof (gint));
      track_param_id[i] = g_malloc (n_tracks * sizeof (gint *));
      channel_id[i] = g_malloc (n_tracks * sizeof (gint *));
      channel_param_id[i] = g_malloc (n_tracks * sizeof (gint *));

      for (j = 0; j < n_tracks; j++)
        {
          track_param_id[i][j] = g_malloc (n_gparams * sizeof (gint *));
          channel_id[i][j] = g_malloc (n_channels * sizeof (gint *));
          channel_param_id[i][j] = g_malloc (n_channels * sizeof (gint *));
        }
    }

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
  if (hyscan_db_get_project_list (db) != NULL)
    g_error ("projects list must be empty");

  /* Создаём проекты. */
  g_message ("creating projects");
  for (i = 0; i < n_projects; i++)
    if ((project_id[i] = hyscan_db_create_project (db, projects[i], NULL)) < 0)
      g_error ("can't create '%s'", projects[i]);

  /* Проверяем названия созданных проектов. */
  g_message ("checking projects names");
  check_project_list (db, "check_project_list", projects);

  /* Проверяем, что нет возможности создать проект с именем уже существующего проекта. */
  g_message ("checking projects duplication");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_create_project (db, projects[i], NULL) > 0)
      g_error ("'%s' duplicated", projects[i]);

  /* Проверяем, что при открытии уже открытых проектов возвращаются правильные идентификаторы. */
  g_message ("checking projects identifiers");
  for (i = 0; i < n_projects; i++)
    if (project_id[i] != hyscan_db_open_project (db, projects[i]))
      g_error ("wrong '%s' id", projects[i]);

  /* Проверяем, что список групп параметров в каждом проекте изначально пустой. */
  g_message ("checking project parameter groups list is empty");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_get_project_param_list (db, project_id[i]) != NULL)
      g_error ("project parameter groups list must be empty");

  /* Создаём группы параметров в каждом проекте. */
  g_message ("creating project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if ((project_param_id[i][j] = hyscan_db_open_project_param (db, project_id[i], gparams[j])) < 0)
        g_error ("can't create '%s.%s'", projects[i], gparams[j]);

  /* Проверяем названия созданных групп параметров. */
  g_message ("checking project parameters groups names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list( '%s' )", projects[i]);
      check_project_param_list (db, error_prefix, gparams, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что при открытии уже открытых групп параметров возвращаются правильные идентификаторы. */
  g_message ("checking project parameters identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if (project_param_id[i][j] != hyscan_db_open_project_param (db, project_id[i], gparams[j]))
        g_error ("wrong '%s.%s' id", projects[i], gparams[j]);

  /* Устанавливаем параметры. */
  g_message ("setting project parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("set_parameters( '%s.%s' )", projects[i], gparams[j]);
        set_parameters (db, error_prefix, j + 1, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем их правильность. */
  g_message ("checking project parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters( '%s.%s' )", projects[i], gparams[j]);
        check_parameters (db, error_prefix, j + 1, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем список параметров проектов. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_param_list( '%s.%s' )", projects[i], gparams[j]);
        check_param_list (db, error_prefix, params, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Создаём дополнительные параметры. */
  g_message ("adding temporary project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix =
          g_strdup_printf ("add_temporary_parameters( '%s.%s' )", projects[i], gparams[j]);
        add_temporary_parameters (db, error_prefix, j + 1, project_id[i], project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем дополнительные параметры. */
  g_message ("checking temporary project parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix =
          g_strdup_printf ("check_temporary_parameters( '%s.%s' )", projects[i], gparams[j]);
        check_temporary_parameters (db, error_prefix, j + 1, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем список параметров проектов. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_param_list( '%s.%s' )", projects[i], gparams[j]);
        check_param_list (db, error_prefix, params_ex, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Удаляем дополнительные параметры. */
  g_message ("removing temporary project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if (!hyscan_db_remove_param (db, project_param_id[i][j], "test*.param*"))
        g_error ("can't remove '%s.%s' test parameters", projects[i], gparams[j]);

  /* Проверяем список параметров проектов. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_param_list( '%s.%s' )", projects[i], gparams[j]);
        check_param_list (db, error_prefix, params, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Тесты уровня галсов. */
  g_message (" ");
  g_message ("checking tracks");

  /* Проверяем, что список галсов в каждом проекте изначально пустой. */
  g_message ("checking project tracks list is empty");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_get_track_list (db, project_id[i]) != NULL)
      g_error ("%s tracks list must be empty", projects[i]);

  /* Создаём галсы в каждом проекте. */
  g_message ("creating tracks");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if ((track_id[i][j] = hyscan_db_create_track (db, project_id[i], tracks[j])) < 0)
        g_error ("can't create '%s.%s'", projects[i], tracks[j]);

  /* Проверяем названия созданных галсов. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list( '%s' )", projects[i]);
      check_track_list (db, error_prefix, tracks, project_id[i]);
      g_free (error_prefix);
    }

  /* Проверяем, что нет возможности создать галс с именем уже существующего галса. */
  g_message ("checking tracks duplication");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_create_track (db, project_id[i], tracks[j]) > 0)
        g_error ("'%s.%s' duplicated", projects[i], tracks[j]);

  /* Проверяем, что при открытии уже открытых галсов возвращаются правильные идентификаторы. */
  g_message ("checking tracks identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (track_id[i][j] != hyscan_db_open_track (db, project_id[i], tracks[j]))
        g_error ("wrong '%s.%s' id", projects[i], tracks[j]);

  /* Проверяем, что список групп параметров в каждом галсе изначально пустой. */
  g_message ("checking track parameters list is empty");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_get_track_param_list (db, track_id[i][j]) != NULL)
        g_error ("track parameters list must be empty");

  /* Создаём группы параметров в каждом галсе. */
  g_message ("creating track parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        if ((track_param_id[i][j][k] = hyscan_db_open_track_param (db, track_id[i][j], gparams[k])) < 0)
          g_error ("can't create '%s.%s.%s'", projects[i], tracks[j], gparams[k]);

  /* Проверяем названия созданных групп параметров. */
  g_message ("checking track parameters groups names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_track_param_list( '%s.%s' )", projects[i], tracks[j]);
        check_track_param_list (db, error_prefix, gparams, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем, что при открытии уже открытых групп параметров возвращаются правильные идентификаторы. */
  g_message ("checking track parameters identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        if (track_param_id[i][j][k] != hyscan_db_open_track_param (db, track_id[i][j], gparams[k]))
          g_error ("wrong '%s.%s.%s' id", projects[i], tracks[j], gparams[k]);

  /* Устанавливаем параметры. */
  g_message ("setting track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("set_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          set_parameters (db, error_prefix, k + 1, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем их правильность. */
  g_message ("checking track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_parameters (db, error_prefix, k + 1, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем список параметров галса. */
  g_message ("checking track parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_param_list (db, error_prefix, params, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Создаём дополнительные параметры. */
  g_message ("adding temporary track parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("add_temporary_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          add_temporary_parameters (db, error_prefix, k + 1, project_id[i], track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем дополнительные параметры. */
  g_message ("checking temporary track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_temporary_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_temporary_parameters (db, error_prefix, k + 1, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем список параметров галса. */
  g_message ("checking track parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_param_list (db, error_prefix, params_ex, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Удаляем дополнительные параметры. */
  g_message ("removing temporary track parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        if (!hyscan_db_remove_param (db, track_param_id[i][j][k], "test*.param*"))
          g_error ("can't remove '%s.%s.%s' test parameters", projects[i], tracks[j], gparams[k]);

  /* Проверяем список параметров галса. */
  g_message ("checking track parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_param_list (db, error_prefix, params, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Тесты уровня каналов данных. */
  g_message (" ");
  g_message ("checking channels");

  /* Проверяем, что список каналов данных в каждом галсе изначально пустой. */
  g_message ("checking track channels list is empty");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_get_channel_list (db, track_id[i][j]) != NULL)
        g_error ("'%s.%s' channels list must be empty", projects[i], tracks[j]);

  /* Создаём каналы данных в каждом галсе. */
  g_message ("creating channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          if ((channel_id[i][j][k] = hyscan_db_create_channel (db, track_id[i][j], channels[k])) < 0)
            g_error ("can't create '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          if (!hyscan_db_add_channel_data (db, channel_id[i][j][k], 0, sample1, sample_size, NULL))
            g_error ("can't add data to '%s.%s.%s'", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем названия созданных каналов данных. */
  g_message ("checking channels names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_channel_list( '%s.%s' )", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем, что нет возможности создать канал данных с именем уже существующего канала. */
  g_message ("checking channels duplication");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_create_channel (db, track_id[i][j], channels[k]) > 0)
          g_error ("'%s.%s.%s' duplicated", projects[i], tracks[j], channels[k]);

  /* Проверяем, что при открытии уже открытых каналов данных возвращаются правильные идентификаторы. */
  g_message ("checking channels identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (channel_id[i][j][k] != hyscan_db_open_channel (db, track_id[i][j], channels[k]))
          g_error ("wrong '%s.%s.%s' id", projects[i], tracks[j], channels[k]);

  /* Проверяем, что данные записанные в канал совпадают с образцом. */
  g_message ("checking channels content");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gint readed_size = sample_size;
          if (!hyscan_db_get_channel_data (db, channel_id[i][j][k], 0, buffer, &sample_size, NULL))
            g_error ("can't read data from 'Project %d.Track %d.Channel %d'", i + 1, j + 1, k + 1);
          if (readed_size != sample_size || g_strcmp0 (sample1, buffer) != 0)
            g_error ("wrong 'Project %d.Track %d.Channel %d' data", i + 1, j + 1, k + 1);
        }

  /* Открываем группы параметров каналов данных. */
  g_message ("opening channel parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if ((channel_param_id[i][j][k] = hyscan_db_open_channel_param (db, channel_id[i][j][k])) < 0)
          g_error ("can't open '%s.%s.%s' parameters", projects[i], tracks[j], channels[k]);

  /* Проверяем, что при открытии уже открытых групп параметров возвращаются правильные идентификаторы. */
  g_message ("checking channel parameters identifiers");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (channel_param_id[i][j][k] != hyscan_db_open_channel_param (db, channel_id[i][j][k]))
          g_error ("wrong '%s.%s.%s' id", projects[i], tracks[j], channels[k]);

  /* Устанавливаем параметры канала данных. */
  g_message ("setting channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("set_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          set_parameters (db, error_prefix, k + 1, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем их правильность. */
  g_message ("checking channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, k + 1, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем, что параметры канала данных можно изменить. */
  g_message ("changing channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("set_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          set_parameters (db, error_prefix, 2 * (k + 1), channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем их правильность. */
  g_message ("checking channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, 2 * (k + 1), channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем список параметров канала данных. */
  g_message ("checking channel parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_param_list (db, error_prefix, params, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Создаём дополнительные параметры канала данных. */
  g_message ("adding temporary channel parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("add_temporary_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          add_temporary_parameters (db, error_prefix, k + 1, project_id[i], channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем дополнительные параметры канала данных. */
  g_message ("checking temporary channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_temporary_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_temporary_parameters (db, error_prefix, k + 1, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем список параметров канала данных. */
  g_message ("checking channel parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_param_list (db, error_prefix, params_ex, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Удаляем дополнительные параметры. */
  g_message ("removing temporary channel parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (!hyscan_db_remove_param (db, channel_param_id[i][j][k], "test*.param*"))
          g_error ("can't remove '%s.%s.%s' test parameters", params[i], tracks[j], channels[k]);

  /* Проверяем список параметров канала данных. */
  g_message ("checking channel parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_param_list (db, error_prefix, params, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

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
    if ((project_id[i] = hyscan_db_open_project (db, projects[i])) < 0)
      g_error ("can't open '%s'", projects[i]);

  /* Проверяем названия групп параметров. */
  g_message ("checking project parameters groups names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list( '%s' )", projects[i]);
      check_project_param_list (db, error_prefix, gparams, project_id[i]);
      g_free (error_prefix);
    }

  /* Открываем группы параметров. */
  g_message ("opening project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if ((project_param_id[i][j] = hyscan_db_open_project_param (db, project_id[i], gparams[j])) < 0)
        g_error ("can't open '%s.%s'", projects[i], gparams[j]);

  /* Проверяем список параметров проектов. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_param_list( '%s.%s' )", projects[i], gparams[j]);
        check_param_list (db, error_prefix, params, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем параметры проектов. */
  g_message ("checking project parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters( '%s.%s' )", projects[i], gparams[j]);
        check_parameters (db, error_prefix, j + 1, project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Изменяем параметры проектов. */
  g_message ("changing project parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("set_parameters( '%s.%s' )", projects[i], gparams[j]);
        set_parameters (db, error_prefix, 2 * (j + 1), project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Проверяем параметры проектов. */
  g_message ("checking project parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_parameters( '%s.%s' )", projects[i], gparams[j]);
        check_parameters (db, error_prefix, 2 * (j + 1), project_param_id[i][j]);
        g_free (error_prefix);
      }

  /* Тесты уровня галсов. */
  g_message (" ");
  g_message ("checking tracks");

  /* Проверяем список галсов. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list( '%s' )", projects[i]);
      check_track_list (db, error_prefix, tracks, project_id[i]);
      g_free (error_prefix);
    }

  /* Открываем все галсы. */
  g_message ("opening tracks");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if ((track_id[i][j] = hyscan_db_open_track (db, project_id[i], tracks[j])) < 0)
        g_error ("can't open '%s.%s'", projects[i], tracks[j]);

  /* Проверяем список групп параметров галсов. */
  g_message ("checking track parameters groups names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_track_param_list( '%s.%s' )", projects[i], tracks[j]);
        check_track_param_list (db, error_prefix, gparams, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Открываем группы параметров. */
  g_message ("opening track parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        if ((track_param_id[i][j][k] = hyscan_db_open_track_param (db, track_id[i][j], gparams[k])) < 0)
          g_error ("can't open '%s.%s.%s'", projects[i], tracks[j], gparams[k]);

  /* Проверяем список параметров проектов. */
  g_message ("checking track parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_param_list (db, error_prefix, params, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем правильность параметров галсов. */
  g_message ("checking track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_parameters (db, error_prefix, k + 1, track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Изменяем параметры галсов. */
  g_message ("changing track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("set_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          set_parameters (db, error_prefix, 2 * (k + 1), track_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем правильность параметров галсов. */
  g_message ("checking track parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], gparams[k]);
          check_parameters (db, error_prefix, 2 * (k + 1), track_param_id[i][j][k]);
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
        gchar *error_prefix = g_strdup_printf ("check_channel_list( '%s.%s' )", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Открываем все каналы данных. */
  g_message ("opening channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          if ((channel_id[i][j][k] = hyscan_db_open_channel (db, track_id[i][j], channels[k])) < 0)
            g_error ("can't open '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          if ((channel_param_id[i][j][k] = hyscan_db_open_channel_param (db, channel_id[i][j][k])) < 0)
            g_error ("can't open '%s.%s.%s' parameters", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем правильность записанных данных каналов. */
  g_message ("checking channels content");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gint readed_size = sample_size;
          if (!hyscan_db_get_channel_data (db, channel_id[i][j][k], 0, buffer, &sample_size, NULL))
            g_error ("can't read data from '%s.%s.%s'", projects[i], tracks[j], channels[k]);
          if (readed_size != sample_size || g_strcmp0 (sample1, buffer) != 0)
            g_error ("wrong '%s.%s.%s' data", projects[i], tracks[j], channels[k]);
        }

  /* Проверяем список параметров канала данных. */
  g_message ("checking channel parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_param_list( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_param_list (db, error_prefix, params, channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем правильность записанных параметров каналов. */
  g_message ("checking channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, 2 * (k + 1), channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Проверяем, что параметры каналов данных нельзя изменить. */
  g_message ("trying to change channel parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          if (hyscan_db_set_integer_param (db, channel_param_id[i][j][k], "integer", 0))
            g_error ("can set 'Project %d.Track %d.Channel %d' integer parameter", i + 1, j + 1, k + 1);
          if (hyscan_db_set_double_param (db, channel_param_id[i][j][k], "double", 0.0))
            g_error ("can set 'Project %d.Track %d.Channel %d' double parameter", i + 1, j + 1, k + 1);
          if (hyscan_db_set_boolean_param (db, channel_param_id[i][j][k], "common.boolean", FALSE))
            g_error ("can set 'Project %d.Track %d.Channel %d' boolean parameter", i + 1, j + 1, k + 1);
          if (hyscan_db_set_string_param (db, channel_param_id[i][j][k], "common.string", ""))
            g_error ("can set 'Project %d.Track %d.Channel %d' string parameter", i + 1, j + 1, k + 1);
        }

  /* Проверяем правильность записанных параметров каналов. */
  g_message ("checking channel parameters values");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        {
          gchar *error_prefix =
            g_strdup_printf ("check_parameters( '%s.%s.%s' )", projects[i], tracks[j], channels[k]);
          check_parameters (db, error_prefix, 2 * (k + 1), channel_param_id[i][j][k]);
          g_free (error_prefix);
        }

  /* Тесты удаления объектов. */
  g_message (" ");
  g_message ("removing objects");

  /* Удаляем половину каналов данных. */
  g_message ("removing half channels");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k += 2)
        if (!hyscan_db_remove_channel (db, track_id[i][j], channels[k]))
          g_error ("can't remove '%s.%s.%s'", projects[i], tracks[j], channels[k]);

  /* Проверяем изменившейся список. */
  g_message ("checking channels names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_channel_list( '%s.%s' )", projects[i], tracks[j]);
        check_channel_list (db, error_prefix, channels2, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Удаляем половину групп параметров галсов. */
  g_message ("removing half track parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k += 2)
        if (!hyscan_db_remove_track_param (db, track_id[i][j], gparams[k]))
          g_error ("can't remove '%s.%s.%s'", projects[i], tracks[j], gparams[k]);

  /* Проверяем изменившейся список. */
  g_message ("checking track parameters names");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      {
        gchar *error_prefix = g_strdup_printf ("check_track_param_list( '%s.%s' )", projects[i], tracks[j]);
        check_track_param_list (db, error_prefix, gparams2, track_id[i][j]);
        g_free (error_prefix);
      }

  /* Удаляем половину галсов. */
  g_message ("removing half tracks");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j += 2)
      if (!hyscan_db_remove_track (db, project_id[i], tracks[j]))
        g_error ("can't remove '%s.%s'", projects[i], tracks[j]);

  /* Проверяем изменившейся список. */
  g_message ("checking tracks names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_track_list( '%s' )", projects[i]);
      check_track_list (db, error_prefix, tracks2, project_id[i]);
      g_free (error_prefix);
    }

  /* Удаляем половину групп параметров проектов. */
  g_message ("removing half project parameters");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j += 2)
      if (!hyscan_db_remove_project_param (db, project_id[i], gparams[j]))
        g_error ("can't remove '%s.%s'", projects[i], gparams[j]);

  /* Проверяем изменившейся список. */
  g_message ("checking project parameters names");
  for (i = 0; i < n_projects; i++)
    {
      gchar *error_prefix = g_strdup_printf ("check_project_param_list( '%s' )", projects[i]);
      check_project_param_list (db, error_prefix, gparams2, project_id[i]);
      g_free (error_prefix);
    }

  /* Удаляем половину проектов. */
  g_message ("removing half projects");
  for (i = 0; i < n_projects; i += 2)
    if (!hyscan_db_remove_project (db, projects[i]))
      g_error ("can't remove '%s'", projects[i]);

  /* Проверяем изменившейся список. */
  g_message ("checking projects names");
  check_project_list (db, "check_project_list", projects2);

  /* Удаляем оставшиеся проекты. */
  g_message ("removing all projects");
  for (i = 1; i < n_projects; i += 2)
    if (!hyscan_db_remove_project (db, projects[i]))
      g_error ("can't delete '%s'", projects[i]);

  /* Проверяем, что список проектов пустой. */
  g_message ("checking db projects list is empty");
  if (hyscan_db_get_project_list (db) != NULL)
    g_error ("projects list must be empty");

  /* Проверяем, что все идентификаторы открытых объектов более не работоспособны. */
  g_message ("checking projects id validity");
  for (i = 0; i < n_projects; i++)
    if (hyscan_db_create_track (db, project_id[i], "Track") > 0)
      g_error ("'%s' still alive", projects[i]);

  g_message ("checking tracks id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      if (hyscan_db_create_channel (db, track_id[i][j], "Channel") > 0)
        g_error ("'%s.%s' is still alive", projects[i], tracks[j]);

  g_message ("checking channels id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_get_channel_data_range (db, channel_id[i][j][k], NULL, NULL))
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  g_message ("checking channel parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_channels; k++)
        if (hyscan_db_get_string_param (db, channel_param_id[i][j][k], "common.string") != NULL)
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], channels[i]);

  g_message ("checking track parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_tracks; j++)
      for (k = 0; k < n_gparams; k++)
        if (hyscan_db_get_string_param (db, track_param_id[i][j][k], "common.string") != NULL)
          g_error ("'%s.%s.%s' is still alive", projects[i], tracks[j], gparams[k]);

  g_message ("checking project parameters id validity");
  for (i = 0; i < n_projects; i++)
    for (j = 0; j < n_gparams; j++)
      if (hyscan_db_get_string_param (db, project_param_id[i][j], "common.string") != NULL)
        g_error ("'%s.%s' is still alive", projects[i], gparams[j]);

  /* Удаляем объект базы данных. */
  g_object_unref (db);

  /* Освобождаем память (для нормальной работы valgind). */
  g_strfreev (projects);
  g_strfreev (tracks);
  g_strfreev (channels);
  g_strfreev (gparams);

  g_strfreev (projects2);
  g_strfreev (tracks2);
  g_strfreev (channels2);
  g_strfreev (gparams2);

  for (i = 0; i < n_projects; i++)
    {
      for (j = 0; j < n_tracks; j++)
        {
          g_free (channel_param_id[i][j]);
          g_free (channel_id[i][j]);
          g_free (track_param_id[i][j]);
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

  return 0;

}
