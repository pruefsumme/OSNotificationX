#ifndef OSNX_LOG_ENTRY_H
#define OSNX_LOG_ENTRY_H

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSNX_LOG_ENTRY_VARIANT_TYPE "(sxssssssa(ss)ib)"

typedef struct
{
  gchar *id;
  gint64 timestamp;
  gchar *tz_identifier;
  gchar *app_id;
  gchar *app_name;
  gchar *icon_id;
  gchar *summary;
  gchar *body;
  gint expire_timeout;
  gboolean is_read;
} OsnxLogEntry;

OsnxLogEntry *osnx_log_entry_new_from_variant (GVariant *variant, GError **error);
GPtrArray *osnx_log_entries_new_from_list_result (GVariant *result, GError **error);
void osnx_log_entry_free (OsnxLogEntry *entry);
gchar *osnx_log_entry_app_key (const OsnxLogEntry *entry);
gchar *osnx_log_entry_format_time (const OsnxLogEntry *entry);
GIcon *osnx_log_entry_load_icon (const OsnxLogEntry *entry);

G_END_DECLS

#endif
