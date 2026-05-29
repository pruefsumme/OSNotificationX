#ifndef OSNX_NOTIFY_SETTINGS_H
#define OSNX_NOTIFY_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _OsnxNotifySettings OsnxNotifySettings;
typedef void (*OsnxNotifySettingsChangedFunc) (OsnxNotifySettings *settings, gpointer user_data);

OsnxNotifySettings *osnx_notify_settings_new (void);
void osnx_notify_settings_free (OsnxNotifySettings *settings);
void osnx_notify_settings_set_changed_callback (OsnxNotifySettings            *settings,
                                                OsnxNotifySettingsChangedFunc  callback,
                                                gpointer                       user_data);
gboolean osnx_notify_settings_get_alerts_enabled (OsnxNotifySettings *settings);
void osnx_notify_settings_set_alerts_enabled (OsnxNotifySettings *settings, gboolean enabled);
void osnx_notify_settings_ensure_logging (OsnxNotifySettings *settings);

G_END_DECLS

#endif
