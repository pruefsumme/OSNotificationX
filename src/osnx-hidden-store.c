#include "osnx-hidden-store.h"

#include <glib/gstdio.h>

#define OSNX_STATE_DIR "osnotificationx"
#define OSNX_STATE_FILE "state.ini"
#define OSNX_STATE_GROUP "notifications"
#define OSNX_HIDDEN_IDS_KEY "hidden-ids"

struct _OsnxHiddenStore
{
  GHashTable *ids;
  gchar *path;
};

static gchar *
osnx_hidden_store_default_path (void)
{
  return g_build_filename (g_get_user_config_dir (), OSNX_STATE_DIR, OSNX_STATE_FILE, NULL);
}

static void
osnx_hidden_store_load (OsnxHiddenStore *store)
{
  g_autoptr (GKeyFile) key_file = g_key_file_new ();
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) ids = NULL;
  gsize length = 0;
  gsize i;

  if (!g_file_test (store->path, G_FILE_TEST_EXISTS))
    return;

  if (!g_key_file_load_from_file (key_file, store->path, G_KEY_FILE_NONE, &error))
    {
      g_warning ("OSNotificationX: unable to load hidden notification state '%s': %s",
                 store->path,
                 error->message);
      return;
    }

  ids = g_key_file_get_string_list (key_file,
                                    OSNX_STATE_GROUP,
                                    OSNX_HIDDEN_IDS_KEY,
                                    &length,
                                    &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND) &&
          !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
        g_warning ("OSNotificationX: unable to read hidden notification state '%s': %s",
                   store->path,
                   error->message);
      return;
    }

  for (i = 0; i < length; i++)
    {
      if (ids[i] != NULL && ids[i][0] != '\0')
        g_hash_table_add (store->ids, g_strdup (ids[i]));
    }
}

OsnxHiddenStore *
osnx_hidden_store_new (const gchar *path)
{
  OsnxHiddenStore *store = g_new0 (OsnxHiddenStore, 1);

  store->ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  store->path = path != NULL ? g_strdup (path) : osnx_hidden_store_default_path ();
  osnx_hidden_store_load (store);

  return store;
}

void
osnx_hidden_store_free (OsnxHiddenStore *store)
{
  if (store == NULL)
    return;

  g_clear_pointer (&store->ids, g_hash_table_unref);
  g_free (store->path);
  g_free (store);
}

gboolean
osnx_hidden_store_contains (OsnxHiddenStore *store,
                            const gchar     *id)
{
  return store != NULL &&
         id != NULL &&
         g_hash_table_contains (store->ids, id);
}

gboolean
osnx_hidden_store_add (OsnxHiddenStore *store,
                       const gchar     *id)
{
  if (store == NULL || id == NULL || id[0] == '\0')
    return FALSE;

  return g_hash_table_add (store->ids, g_strdup (id));
}

gboolean
osnx_hidden_store_save (OsnxHiddenStore *store)
{
  g_autoptr (GKeyFile) key_file = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *directory = NULL;
  g_autofree gchar *contents = NULL;
  GList *ids = NULL;
  GList *iter;
  const gchar **values;
  gsize length;
  gsize i = 0;
  gboolean saved;

  g_return_val_if_fail (store != NULL, FALSE);

  directory = g_path_get_dirname (store->path);
  if (g_mkdir_with_parents (directory, 0700) != 0)
    {
      g_warning ("OSNotificationX: unable to create state directory '%s'", directory);
      return FALSE;
    }

  ids = g_hash_table_get_keys (store->ids);
  ids = g_list_sort (ids, (GCompareFunc) g_strcmp0);
  length = g_list_length (ids);
  values = g_new0 (const gchar *, length);

  for (iter = ids; iter != NULL; iter = iter->next)
    values[i++] = iter->data;

  key_file = g_key_file_new ();
  g_key_file_set_string_list (key_file,
                              OSNX_STATE_GROUP,
                              OSNX_HIDDEN_IDS_KEY,
                              values,
                              length);
  contents = g_key_file_to_data (key_file, NULL, NULL);
  saved = g_file_set_contents (store->path, contents, -1, &error);
  if (!saved)
    g_warning ("OSNotificationX: unable to save hidden notification state '%s': %s",
               store->path,
               error->message);

  g_free (values);
  g_list_free (ids);
  return saved;
}
