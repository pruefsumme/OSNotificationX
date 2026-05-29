#include "osnx-notify-settings.h"

#include <xfconf/xfconf.h>

struct _OsnxNotifySettings
{
  XfconfChannel *channel;
  OsnxNotifySettingsChangedFunc changed_callback;
  gpointer changed_user_data;
};

static void
osnx_notify_settings_property_changed (XfconfChannel *channel G_GNUC_UNUSED,
                                       const gchar   *property,
                                       const GValue  *value G_GNUC_UNUSED,
                                       gpointer       user_data)
{
  OsnxNotifySettings *settings = user_data;

  if (g_strcmp0 (property, "/do-not-disturb") == 0 &&
      settings->changed_callback != NULL)
    settings->changed_callback (settings, settings->changed_user_data);
}

OsnxNotifySettings *
osnx_notify_settings_new (void)
{
  OsnxNotifySettings *settings = g_new0 (OsnxNotifySettings, 1);

  if (!xfconf_init (NULL))
    {
      g_warning ("OSNotificationX: xfconf_init failed; notification settings are unavailable");
      return settings;
    }

  settings->channel = xfconf_channel_new ("xfce4-notifyd");
  g_signal_connect (settings->channel,
                    "property-changed",
                    G_CALLBACK (osnx_notify_settings_property_changed),
                    settings);
  osnx_notify_settings_ensure_logging (settings);

  return settings;
}

void
osnx_notify_settings_free (OsnxNotifySettings *settings)
{
  if (settings == NULL)
    return;

  g_clear_object (&settings->channel);
  g_free (settings);
}

void
osnx_notify_settings_set_changed_callback (OsnxNotifySettings            *settings,
                                           OsnxNotifySettingsChangedFunc  callback,
                                           gpointer                       user_data)
{
  g_return_if_fail (settings != NULL);

  settings->changed_callback = callback;
  settings->changed_user_data = user_data;
}

gboolean
osnx_notify_settings_get_alerts_enabled (OsnxNotifySettings *settings)
{
  if (settings == NULL || settings->channel == NULL)
    return TRUE;

  return !xfconf_channel_get_bool (settings->channel, "/do-not-disturb", FALSE);
}

void
osnx_notify_settings_set_alerts_enabled (OsnxNotifySettings *settings,
                                         gboolean            enabled)
{
  if (settings == NULL || settings->channel == NULL)
    return;

  xfconf_channel_set_bool (settings->channel, "/do-not-disturb", !enabled);
}

void
osnx_notify_settings_ensure_logging (OsnxNotifySettings *settings)
{
  if (settings == NULL || settings->channel == NULL)
    return;

  xfconf_channel_set_bool (settings->channel, "/notification-log", TRUE);
  xfconf_channel_set_string (settings->channel, "/log-level", "always");
  xfconf_channel_set_string (settings->channel, "/log-level-apps", "except-blocked");
}
