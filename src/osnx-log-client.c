#include "osnx-log-client.h"
#include "osnx-hidden-store.h"

#include <gio/gdesktopappinfo.h>
#include <sqlite3.h>

#define OSNX_LOG_BUS_NAME "org.xfce.Notifyd"
#define OSNX_LOG_OBJECT_PATH "/org/xfce/Notifyd"
#define OSNX_LOG_INTERFACE "org.xfce.Notifyd.Log"
#define OSNX_LOG_LIMIT 100

struct _OsnxLogClient
{
  gint ref_count;
  GDBusProxy *proxy;
  GPtrArray *entries;
  GCancellable *cancellable;
  OsnxLogClientChangedFunc changed_callback;
  gpointer changed_user_data;
  gboolean has_unread;
  gboolean available;
  gboolean using_sqlite_fallback;
  OsnxHiddenStore *hidden_store;
};

static void osnx_log_client_emit_changed (OsnxLogClient *client);

static gboolean
osnx_log_client_has_visible_unread (OsnxLogClient *client)
{
  guint i;

  if (client == NULL || client->entries == NULL)
    return FALSE;

  for (i = 0; i < client->entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (client->entries, i);

      if (entry != NULL && !entry->is_read)
        return TRUE;
    }

  return FALSE;
}

static gboolean
osnx_log_client_entry_is_hidden (OsnxLogClient       *client,
                                 const OsnxLogEntry *entry)
{
  return entry != NULL &&
         osnx_hidden_store_contains (client->hidden_store, entry->id);
}

static void
osnx_log_client_replace_entries (OsnxLogClient *client,
                                 GPtrArray     *entries)
{
  GPtrArray *visible = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_log_entry_free);
  guint i;
  guint unread_count = 0;

  for (i = 0; entries != NULL && i < entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (entries, i);

      if (osnx_log_client_entry_is_hidden (client, entry))
        continue;

      if (!entry->is_read)
        unread_count++;

      g_ptr_array_add (visible, entry);
      g_ptr_array_index (entries, i) = NULL;
    }

  g_clear_pointer (&client->entries, g_ptr_array_unref);
  client->entries = visible;
  client->has_unread = unread_count > 0;

  if (entries != NULL)
    g_ptr_array_unref (entries);
}

static gchar *
osnx_log_client_sqlite_path (void)
{
  return g_build_filename (g_get_user_cache_dir (), "xfce4", "notifyd", "log.sqlite", NULL);
}

static gchar *
osnx_sqlite_column_dup (sqlite3_stmt *stmt,
                        gint          column)
{
  const guchar *text = sqlite3_column_text (stmt, column);

  return text != NULL && text[0] != '\0' ? g_strdup ((const gchar *) text) : NULL;
}

static gboolean
osnx_log_client_refresh_from_sqlite (OsnxLogClient *client)
{
  static const gchar *sql =
    "SELECT id, timestamp, tz_identifier, app_id, app_name, icon_id, summary, body, expire_timeout, is_read "
    "FROM notifications ORDER BY timestamp DESC LIMIT ?";
  g_autofree gchar *path = osnx_log_client_sqlite_path ();
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  GPtrArray *entries = NULL;
  gint rc;

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    return FALSE;

  rc = sqlite3_open_v2 (path, &db, SQLITE_OPEN_READONLY, NULL);
  if (rc != SQLITE_OK)
    {
      g_warning ("OSNotificationX: unable to open fallback log DB '%s': %s", path, sqlite3_errmsg (db));
      if (db != NULL)
        sqlite3_close (db);
      return FALSE;
    }

  sqlite3_busy_timeout (db, 250);

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    {
      g_warning ("OSNotificationX: unable to prepare fallback log query: %s", sqlite3_errmsg (db));
      sqlite3_close (db);
      return FALSE;
    }

  sqlite3_bind_int (stmt, 1, OSNX_LOG_LIMIT);
  entries = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_log_entry_free);

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      OsnxLogEntry *entry = g_new0 (OsnxLogEntry, 1);

      entry->id = osnx_sqlite_column_dup (stmt, 0);
      entry->timestamp = sqlite3_column_int64 (stmt, 1);
      entry->tz_identifier = osnx_sqlite_column_dup (stmt, 2);
      entry->app_id = osnx_sqlite_column_dup (stmt, 3);
      entry->app_name = osnx_sqlite_column_dup (stmt, 4);
      entry->icon_id = osnx_sqlite_column_dup (stmt, 5);
      entry->summary = osnx_sqlite_column_dup (stmt, 6);
      entry->body = osnx_sqlite_column_dup (stmt, 7);
      entry->expire_timeout = sqlite3_column_int (stmt, 8);
      entry->is_read = sqlite3_column_int (stmt, 9) != 0;

      g_ptr_array_add (entries, entry);
    }

  if (rc != SQLITE_DONE)
    {
      g_warning ("OSNotificationX: fallback log query failed: %s", sqlite3_errmsg (db));
      g_ptr_array_unref (entries);
      sqlite3_finalize (stmt);
      sqlite3_close (db);
      return FALSE;
    }

  sqlite3_finalize (stmt);
  sqlite3_close (db);

  osnx_log_client_replace_entries (client, entries);
  client->available = TRUE;
  client->using_sqlite_fallback = TRUE;
  return TRUE;
}

static gboolean
osnx_log_client_exec_sqlite (const gchar *sql,
                             const gchar *arg)
{
  g_autofree gchar *path = osnx_log_client_sqlite_path ();
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint rc;

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    return FALSE;

  rc = sqlite3_open_v2 (path, &db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK)
    {
      g_warning ("OSNotificationX: unable to open writable fallback log DB '%s': %s", path, sqlite3_errmsg (db));
      if (db != NULL)
        sqlite3_close (db);
      return FALSE;
    }

  sqlite3_busy_timeout (db, 250);

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    {
      g_warning ("OSNotificationX: unable to prepare fallback DB update: %s", sqlite3_errmsg (db));
      sqlite3_close (db);
      return FALSE;
    }

  if (arg != NULL)
    sqlite3_bind_text (stmt, 1, arg, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step (stmt);
  if (rc != SQLITE_DONE)
    {
      g_warning ("OSNotificationX: fallback DB update failed: %s", sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      sqlite3_close (db);
      return FALSE;
    }

  sqlite3_finalize (stmt);
  sqlite3_close (db);
  return TRUE;
}

OsnxLogClient *
osnx_log_client_new (void)
{
  OsnxLogClient *client = g_new0 (OsnxLogClient, 1);

  client->ref_count = 1;
  client->entries = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_log_entry_free);
  client->cancellable = g_cancellable_new ();
  client->hidden_store = osnx_hidden_store_new (NULL);

  return client;
}

OsnxLogClient *
osnx_log_client_ref (OsnxLogClient *client)
{
  g_return_val_if_fail (client != NULL, NULL);
  g_atomic_int_inc (&client->ref_count);
  return client;
}

void
osnx_log_client_unref (OsnxLogClient *client)
{
  if (client == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&client->ref_count))
    return;

  if (client->cancellable != NULL)
    g_cancellable_cancel (client->cancellable);

  g_clear_object (&client->cancellable);
  g_clear_object (&client->proxy);
  g_clear_pointer (&client->entries, g_ptr_array_unref);
  g_clear_pointer (&client->hidden_store, osnx_hidden_store_free);
  g_free (client);
}

static gboolean
osnx_log_client_call_no_reply (OsnxLogClient *client,
                               const gchar   *method,
                               GVariant      *parameters)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GError) error = NULL;

  if (client->proxy == NULL)
    return FALSE;

  result = g_dbus_proxy_call_sync (client->proxy,
                                   method,
                                   parameters,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   3000,
                                   NULL,
                                   &error);
  if (error != NULL)
    {
      g_warning ("OSNotificationX: %s failed: %s", method, error->message);
      return FALSE;
    }

  return TRUE;
}

static void
osnx_log_client_refresh_unread (OsnxLogClient *client)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GError) error = NULL;
  gboolean has_unread = FALSE;

  if (client->using_sqlite_fallback)
    {
      osnx_log_client_refresh_from_sqlite (client);
      return;
    }

  if (client->proxy == NULL)
    {
      if (!osnx_log_client_refresh_from_sqlite (client))
        client->has_unread = FALSE;
      return;
    }

  result = g_dbus_proxy_call_sync (client->proxy,
                                   "HasUnread",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   3000,
                                   NULL,
                                   &error);
  if (error != NULL)
    {
      g_warning ("OSNotificationX: HasUnread failed: %s", error->message);
      client->has_unread = FALSE;
      return;
    }

  g_variant_get (result, "(b)", &has_unread);
  client->has_unread = has_unread && osnx_log_client_has_visible_unread (client);
}

void
osnx_log_client_refresh (OsnxLogClient *client)
{
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GPtrArray) entries = NULL;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (client != NULL);

  if (client->proxy == NULL)
    {
      if (!osnx_log_client_refresh_from_sqlite (client))
        client->available = FALSE;
      osnx_log_client_emit_changed (client);
      return;
    }

  result = g_dbus_proxy_call_sync (client->proxy,
                                   "List",
                                   g_variant_new ("(sub)", "", (guint32) OSNX_LOG_LIMIT, FALSE),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   3000,
                                   NULL,
                                   &error);
  if (error != NULL)
    {
      g_warning ("OSNotificationX: List failed: %s", error->message);
      if (!osnx_log_client_refresh_from_sqlite (client))
        {
          client->available = FALSE;
          g_clear_pointer (&client->entries, g_ptr_array_unref);
          client->entries = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_log_entry_free);
        }
      osnx_log_client_refresh_unread (client);
      osnx_log_client_emit_changed (client);
      return;
    }

  entries = osnx_log_entries_new_from_list_result (result, &error);
  if (error != NULL)
    {
      g_warning ("OSNotificationX: unable to parse notification log: %s", error->message);
      if (!osnx_log_client_refresh_from_sqlite (client))
        client->available = FALSE;
    }
  else
    {
      client->available = TRUE;
      client->using_sqlite_fallback = FALSE;
      osnx_log_client_replace_entries (client, g_steal_pointer (&entries));
    }

  osnx_log_client_refresh_unread (client);
  osnx_log_client_emit_changed (client);
}

static void
osnx_log_client_proxy_signal (GDBusProxy *proxy G_GNUC_UNUSED,
                              gchar      *sender_name G_GNUC_UNUSED,
                              gchar      *signal_name,
                              GVariant   *parameters G_GNUC_UNUSED,
                              gpointer    user_data)
{
  OsnxLogClient *client = user_data;

  if (g_strcmp0 (signal_name, "RowAdded") == 0 ||
      g_strcmp0 (signal_name, "RowChanged") == 0 ||
      g_strcmp0 (signal_name, "RowDeleted") == 0 ||
      g_strcmp0 (signal_name, "Truncated") == 0 ||
      g_strcmp0 (signal_name, "Cleared") == 0)
    osnx_log_client_refresh (client);
}

static void
osnx_log_client_proxy_ready (GObject      *source G_GNUC_UNUSED,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  OsnxLogClient *client = user_data;
  g_autoptr (GError) error = NULL;

  client->proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      osnx_log_client_unref (client);
      return;
    }

  if (error != NULL)
    {
      g_warning ("OSNotificationX: could not connect to xfce4-notifyd log: %s", error->message);
      osnx_log_client_refresh_from_sqlite (client);
      osnx_log_client_emit_changed (client);
      osnx_log_client_unref (client);
      return;
    }

  g_dbus_proxy_set_default_timeout (client->proxy, 3000);
  g_signal_connect (client->proxy, "g-signal", G_CALLBACK (osnx_log_client_proxy_signal), client);
  osnx_log_client_refresh (client);
  osnx_log_client_unref (client);
}

void
osnx_log_client_connect_async (OsnxLogClient *client)
{
  g_return_if_fail (client != NULL);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            OSNX_LOG_BUS_NAME,
                            OSNX_LOG_OBJECT_PATH,
                            OSNX_LOG_INTERFACE,
                            client->cancellable,
                            osnx_log_client_proxy_ready,
                            osnx_log_client_ref (client));
}

void
osnx_log_client_set_changed_callback (OsnxLogClient            *client,
                                      OsnxLogClientChangedFunc  callback,
                                      gpointer                  user_data)
{
  g_return_if_fail (client != NULL);

  client->changed_callback = callback;
  client->changed_user_data = user_data;
}

gboolean
osnx_log_client_is_available (OsnxLogClient *client)
{
  return client != NULL && client->available;
}

gboolean
osnx_log_client_has_unread (OsnxLogClient *client)
{
  return client != NULL && client->has_unread;
}

GPtrArray *
osnx_log_client_get_entries (OsnxLogClient *client)
{
  return client != NULL ? client->entries : NULL;
}

void
osnx_log_client_mark_visible_read (OsnxLogClient *client)
{
  guint i;

  if (client == NULL || client->proxy == NULL || client->entries == NULL)
    {
      if (client != NULL && client->using_sqlite_fallback)
        {
          osnx_log_client_exec_sqlite ("UPDATE notifications SET is_read = TRUE", NULL);
          osnx_log_client_refresh (client);
        }
      return;
    }

  for (i = 0; i < client->entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (client->entries, i);
      GVariantBuilder ids;

      if (!entry->is_read && entry->id != NULL)
        {
          g_variant_builder_init (&ids, G_VARIANT_TYPE ("as"));
          g_variant_builder_add (&ids, "s", entry->id);
          osnx_log_client_call_no_reply (client, "MarkRead", g_variant_new ("(as)", &ids));
        }
    }

  osnx_log_client_refresh (client);
}

void
osnx_log_client_hide_group (OsnxLogClient *client,
                            const gchar   *group_key)
{
  guint i;
  gboolean changed = FALSE;

  if (client == NULL || client->entries == NULL || group_key == NULL)
      return;

  for (i = 0; i < client->entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (client->entries, i);
      g_autofree gchar *entry_key = osnx_log_entry_app_key (entry);

      if (entry->id != NULL && g_strcmp0 (entry_key, group_key) == 0)
        changed |= osnx_hidden_store_add (client->hidden_store, entry->id);
    }

  if (changed)
    osnx_hidden_store_save (client->hidden_store);

  osnx_log_client_refresh (client);
}

static GAppInfo *
osnx_log_client_find_app_info (const OsnxLogEntry *entry)
{
  g_autofree gchar *desktop_id = NULL;
  GAppInfo *match = NULL;
  GList *apps = NULL;
  GList *iter;

  if (entry->app_id != NULL && entry->app_id[0] != '\0')
    {
      desktop_id = g_str_has_suffix (entry->app_id, ".desktop") ? g_strdup (entry->app_id) : g_strdup_printf ("%s.desktop", entry->app_id);
      match = G_APP_INFO (g_desktop_app_info_new (desktop_id));
      if (match != NULL)
        return match;
    }

  apps = g_app_info_get_all ();
  for (iter = apps; iter != NULL; iter = iter->next)
    {
      GAppInfo *app = iter->data;
      const gchar *id = g_app_info_get_id (app);
      const gchar *name = g_app_info_get_name (app);
      gboolean id_matches = entry->app_id != NULL && id != NULL && g_strrstr (id, entry->app_id) != NULL;
      gboolean name_matches = entry->app_name != NULL && name != NULL && g_ascii_strcasecmp (name, entry->app_name) == 0;

      if (id_matches || name_matches)
        {
          match = g_object_ref (app);
          break;
        }
    }

  g_list_free_full (apps, g_object_unref);
  return match;
}

gboolean
osnx_log_client_activate_entry (OsnxLogClient *client,
                                const gchar   *entry_id)
{
  guint i;

  if (client == NULL || client->entries == NULL || entry_id == NULL)
    return FALSE;

  for (i = 0; i < client->entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (client->entries, i);
      g_autoptr (GAppInfo) app_info = NULL;
      g_autoptr (GError) error = NULL;

      if (g_strcmp0 (entry->id, entry_id) != 0)
        continue;

      app_info = osnx_log_client_find_app_info (entry);
      if (app_info == NULL)
        {
          g_warning ("OSNotificationX: no launchable app found for notification source '%s'",
                     entry->app_id != NULL ? entry->app_id : (entry->app_name != NULL ? entry->app_name : "unknown"));
          return FALSE;
        }

      if (!g_app_info_launch (app_info, NULL, NULL, &error))
        {
          g_warning ("OSNotificationX: unable to launch notification source: %s", error->message);
          return FALSE;
        }

      return TRUE;
    }

  return FALSE;
}

static void
osnx_log_client_emit_changed (OsnxLogClient *client)
{
  if (client->changed_callback != NULL)
    client->changed_callback (client, client->changed_user_data);
}
