#ifndef OSNX_DRAWER_H
#define OSNX_DRAWER_H

#include "osnx-log-entry.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _OsnxDrawer OsnxDrawer;

typedef void (*OsnxDrawerClosedFunc) (gpointer user_data);
typedef void (*OsnxDrawerAlertsChangedFunc) (gboolean enabled, gpointer user_data);
typedef void (*OsnxDrawerClearGroupFunc) (const gchar *group_key, gpointer user_data);
typedef void (*OsnxDrawerActivateEntryFunc) (const gchar *entry_id, gpointer user_data);

OsnxDrawer *osnx_drawer_new (OsnxDrawerClosedFunc        closed_callback,
                             OsnxDrawerAlertsChangedFunc alerts_changed_callback,
                             OsnxDrawerClearGroupFunc    clear_group_callback,
                             OsnxDrawerActivateEntryFunc  activate_entry_callback,
                             gpointer                    user_data);
void osnx_drawer_free (OsnxDrawer *drawer);
void osnx_drawer_show (OsnxDrawer *drawer,
                       GtkWidget  *anchor,
                       GPtrArray  *entries,
                       gboolean    log_available,
                       gboolean    alerts_enabled);
void osnx_drawer_hide (OsnxDrawer *drawer);
void osnx_drawer_update (OsnxDrawer *drawer,
                         GPtrArray  *entries,
                         gboolean    log_available,
                         gboolean    alerts_enabled);
gboolean osnx_drawer_get_visible (OsnxDrawer *drawer);

G_END_DECLS

#endif
