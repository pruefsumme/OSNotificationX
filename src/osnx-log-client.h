#ifndef OSNX_LOG_CLIENT_H
#define OSNX_LOG_CLIENT_H

#include "osnx-log-entry.h"

G_BEGIN_DECLS

typedef struct _OsnxLogClient OsnxLogClient;
typedef void (*OsnxLogClientChangedFunc) (OsnxLogClient *client, gpointer user_data);

OsnxLogClient *osnx_log_client_new (void);
OsnxLogClient *osnx_log_client_ref (OsnxLogClient *client);
void osnx_log_client_unref (OsnxLogClient *client);
void osnx_log_client_connect_async (OsnxLogClient *client);
void osnx_log_client_set_changed_callback (OsnxLogClient            *client,
                                           OsnxLogClientChangedFunc  callback,
                                           gpointer                  user_data);
gboolean osnx_log_client_is_available (OsnxLogClient *client);
gboolean osnx_log_client_has_unread (OsnxLogClient *client);
GPtrArray *osnx_log_client_get_entries (OsnxLogClient *client);
void osnx_log_client_refresh (OsnxLogClient *client);
void osnx_log_client_mark_visible_read (OsnxLogClient *client);
void osnx_log_client_hide_group (OsnxLogClient *client, const gchar *group_key);
gboolean osnx_log_client_activate_entry (OsnxLogClient *client, const gchar *entry_id);

G_END_DECLS

#endif
