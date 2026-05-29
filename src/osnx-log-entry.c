#include "osnx-log-entry.h"

#include <gio/gdesktopappinfo.h>

static gchar *
osnx_dup_nonempty (const gchar *value)
{
  return value != NULL && value[0] != '\0' ? g_strdup (value) : NULL;
}

OsnxLogEntry *
osnx_log_entry_new_from_variant (GVariant *variant,
                                 GError  **error)
{
  const gchar *id = NULL;
  const gchar *tz_identifier = NULL;
  const gchar *app_id = NULL;
  const gchar *app_name = NULL;
  const gchar *icon_id = NULL;
  const gchar *summary = NULL;
  const gchar *body = NULL;
  GVariant *actions = NULL;
  gint64 timestamp = 0;
  gint expire_timeout = 0;
  gboolean is_read = FALSE;

  g_return_val_if_fail (variant != NULL, NULL);

  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE (OSNX_LOG_ENTRY_VARIANT_TYPE)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unexpected notification log entry type '%s'",
                   g_variant_get_type_string (variant));
      return NULL;
    }

  g_variant_get (variant,
                 "(&sx&s&s&s&s&s&s@a(ss)ib)",
                 &id,
                 &timestamp,
                 &tz_identifier,
                 &app_id,
                 &app_name,
                 &icon_id,
                 &summary,
                 &body,
                 &actions,
                 &expire_timeout,
                 &is_read);

  if (actions != NULL)
    g_variant_unref (actions);

  OsnxLogEntry *entry = g_new0 (OsnxLogEntry, 1);
  entry->id = g_strdup (id);
  entry->timestamp = timestamp;
  entry->tz_identifier = osnx_dup_nonempty (tz_identifier);
  entry->app_id = osnx_dup_nonempty (app_id);
  entry->app_name = osnx_dup_nonempty (app_name);
  entry->icon_id = osnx_dup_nonempty (icon_id);
  entry->summary = osnx_dup_nonempty (summary);
  entry->body = osnx_dup_nonempty (body);
  entry->expire_timeout = expire_timeout;
  entry->is_read = is_read;

  return entry;
}

GPtrArray *
osnx_log_entries_new_from_list_result (GVariant *result,
                                       GError  **error)
{
  GVariant *entries_variant = NULL;
  GVariantIter iter;
  GVariant *child = NULL;
  GPtrArray *entries = NULL;

  g_return_val_if_fail (result != NULL, NULL);

  if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(a" OSNX_LOG_ENTRY_VARIANT_TYPE ")")))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unexpected notification log list result type '%s'",
                   g_variant_get_type_string (result));
      return NULL;
    }

  g_variant_get (result, "(@a" OSNX_LOG_ENTRY_VARIANT_TYPE ")", &entries_variant);
  entries = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_log_entry_free);
  g_variant_iter_init (&iter, entries_variant);

  while ((child = g_variant_iter_next_value (&iter)) != NULL)
    {
      GError *child_error = NULL;
      OsnxLogEntry *entry = osnx_log_entry_new_from_variant (child, &child_error);

      if (entry != NULL)
        {
          g_ptr_array_add (entries, entry);
        }
      else
        {
          g_propagate_error (error, child_error);
          g_variant_unref (child);
          g_variant_unref (entries_variant);
          g_ptr_array_unref (entries);
          return NULL;
        }

      g_variant_unref (child);
    }

  g_variant_unref (entries_variant);
  return entries;
}

void
osnx_log_entry_free (OsnxLogEntry *entry)
{
  if (entry == NULL)
    return;

  g_free (entry->id);
  g_free (entry->tz_identifier);
  g_free (entry->app_id);
  g_free (entry->app_name);
  g_free (entry->icon_id);
  g_free (entry->summary);
  g_free (entry->body);
  g_free (entry);
}

gchar *
osnx_log_entry_app_key (const OsnxLogEntry *entry)
{
  g_return_val_if_fail (entry != NULL, g_strdup (""));

  if (entry->app_id != NULL && entry->app_id[0] != '\0')
    return g_strdup (entry->app_id);

  if (entry->app_name != NULL && entry->app_name[0] != '\0')
    return g_strdup (entry->app_name);

  return g_strdup ("unknown");
}

static gint64
osnx_timestamp_to_unix_seconds (gint64 timestamp)
{
  if (timestamp > 32503680000LL)
    return timestamp / G_USEC_PER_SEC;

  return timestamp;
}

gchar *
osnx_log_entry_format_time (const OsnxLogEntry *entry)
{
  gint64 then_s;
  gint64 now_s;
  gint64 delta;
  GDateTime *then = NULL;
  gchar *formatted = NULL;

  g_return_val_if_fail (entry != NULL, g_strdup (""));

  then_s = osnx_timestamp_to_unix_seconds (entry->timestamp);
  now_s = g_get_real_time () / G_USEC_PER_SEC;
  delta = MAX (0, now_s - then_s);

  if (delta < 60)
    return g_strdup_printf ("%" G_GINT64_FORMAT "s ago", MAX ((gint64) 1, delta));
  if (delta < 3600)
    return g_strdup_printf ("%" G_GINT64_FORMAT "m ago", delta / 60);
  if (delta < 86400)
    return g_strdup_printf ("%" G_GINT64_FORMAT "h ago", delta / 3600);
  if (delta < 172800)
    return g_strdup ("Yesterday");
  if (delta < 604800)
    return g_strdup_printf ("%" G_GINT64_FORMAT " days ago", delta / 86400);

  then = g_date_time_new_from_unix_local (then_s);
  formatted = then != NULL ? g_date_time_format (then, "%b %-d") : g_strdup ("");
  g_clear_pointer (&then, g_date_time_unref);
  return formatted;
}

static GIcon *
osnx_icon_from_desktop_id (const gchar *app_id)
{
  g_autofree gchar *desktop_id = NULL;
  g_autoptr (GDesktopAppInfo) app_info = NULL;
  GIcon *icon = NULL;

  if (app_id == NULL || app_id[0] == '\0')
    return NULL;

  desktop_id = g_str_has_suffix (app_id, ".desktop") ? g_strdup (app_id) : g_strdup_printf ("%s.desktop", app_id);
  app_info = g_desktop_app_info_new (desktop_id);
  if (app_info == NULL)
    return NULL;

  icon = g_app_info_get_icon (G_APP_INFO (app_info));
  return icon != NULL ? g_object_ref (icon) : NULL;
}

static GIcon *
osnx_icon_from_app_hint (const OsnxLogEntry *entry)
{
  g_autofree gchar *hint = NULL;

  if (entry->app_name != NULL && entry->app_name[0] != '\0')
    hint = g_utf8_strdown (entry->app_name, -1);
  else if (entry->app_id != NULL && entry->app_id[0] != '\0')
    hint = g_utf8_strdown (entry->app_id, -1);

  if (hint == NULL)
    return NULL;

  if (strstr (hint, "message") != NULL || strstr (hint, "chat") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("internet-chat");

  if (strstr (hint, "mail") != NULL || strstr (hint, "thunderbird") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("mail-message-new");

  if (strstr (hint, "calendar") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("x-office-calendar");

  if (strstr (hint, "backup") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("drive-harddisk");

  if (strstr (hint, "battery") != NULL || strstr (hint, "power") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("battery");

  if (strstr (hint, "system") != NULL)
    return g_themed_icon_new_with_default_fallbacks ("preferences-system");

  return NULL;
}

GIcon *
osnx_log_entry_load_icon (const OsnxLogEntry *entry)
{
  g_autofree gchar *cached_name = NULL;
  g_autofree gchar *cached_path = NULL;

  g_return_val_if_fail (entry != NULL, g_themed_icon_new ("application-x-executable"));

  if (entry->icon_id != NULL && g_path_is_absolute (entry->icon_id) && g_file_test (entry->icon_id, G_FILE_TEST_EXISTS))
    {
      g_autoptr (GFile) file = g_file_new_for_path (entry->icon_id);
      return G_ICON (g_file_icon_new (file));
    }

  if (entry->icon_id != NULL && entry->icon_id[0] != '\0')
    {
      cached_name = g_strconcat (entry->icon_id, ".png", NULL);
      cached_path = g_build_filename (g_get_user_cache_dir (), "xfce4", "notifyd", "icons", cached_name, NULL);
      if (g_file_test (cached_path, G_FILE_TEST_EXISTS))
        {
          g_autoptr (GFile) file = g_file_new_for_path (cached_path);
          return G_ICON (g_file_icon_new (file));
        }
    }

  if (entry->app_id != NULL)
    {
      GIcon *icon = osnx_icon_from_desktop_id (entry->app_id);
      if (icon != NULL)
        return icon;
    }

  {
    GIcon *icon = osnx_icon_from_app_hint (entry);
    if (icon != NULL)
      return icon;
  }

  if (entry->icon_id != NULL && entry->icon_id[0] != '\0')
    return g_themed_icon_new_with_default_fallbacks (entry->icon_id);

  return g_themed_icon_new ("application-x-executable");
}
