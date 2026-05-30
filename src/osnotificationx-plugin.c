#include "config.h"
#include "osnx-drawer.h"
#include "osnx-log-client.h"
#include "osnx-notify-settings.h"
#include "osnx-style.h"

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

typedef struct _OsnxPlugin
{
  XfcePanelPlugin parent;

  GtkWidget *button;
  GtkWidget *button_image;
  GtkWidget *button_overlay;
  GtkWidget *button_unread_dot;

  OsnxDrawer *drawer;
  OsnxLogClient *log_client;
  OsnxNotifySettings *settings;
  guint refresh_timeout_id;
} OsnxPlugin;

typedef struct _OsnxPluginClass
{
  XfcePanelPluginClass parent_class;
} OsnxPluginClass;

static void osnx_plugin_construct (XfcePanelPlugin *plugin);
static void osnx_plugin_free_data (XfcePanelPlugin *plugin);
static gboolean osnx_plugin_size_changed (XfcePanelPlugin *plugin, gint size);

XFCE_PANEL_DEFINE_PLUGIN (OsnxPlugin, osnx_plugin)

static void
osnx_plugin_set_button_icon (OsnxPlugin *self)
{
  gint size;
  gint icon_size;
  g_autofree gchar *icon_path = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GtkIconTheme *theme = NULL;
  const gchar *fallback_icon;

  if (self->button_image == NULL)
    return;

  size = xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (self));
  icon_size = MAX (16, size - XFCE_PANEL_PLUGIN_ICON_PADDING);
  icon_path = g_build_filename (OSNX_DATA_DIR, "applet_icon.png", NULL);

  if (g_file_test (icon_path, G_FILE_TEST_EXISTS))
    {
      pixbuf = gdk_pixbuf_new_from_file_at_scale (icon_path, icon_size, icon_size, TRUE, &error);
      if (pixbuf != NULL)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (self->button_image), pixbuf);
          return;
        }

      g_warning ("OSNotificationX: unable to load applet icon: %s", error->message);
    }

  theme = gtk_icon_theme_get_default ();
  fallback_icon = gtk_icon_theme_has_icon (theme, "xsi-view-list-bullet-symbolic")
                    ? "xsi-view-list-bullet-symbolic"
                    : "notifications-symbolic";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->button_image), fallback_icon, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (self->button_image), icon_size);
}

static void
osnx_plugin_update_button (OsnxPlugin *self)
{
  gboolean has_unread = osnx_log_client_has_unread (self->log_client);

  osnx_plugin_set_button_icon (self);

  if (self->button_unread_dot != NULL)
    gtk_widget_set_visible (self->button_unread_dot, has_unread);

  gtk_widget_set_tooltip_text (self->button,
                               has_unread ? "OSNotificationX - unread notifications" : "OSNotificationX");
}

static void
osnx_plugin_update_drawer (OsnxPlugin *self)
{
  if (!osnx_drawer_get_visible (self->drawer))
    return;

  osnx_drawer_update (self->drawer,
                      osnx_log_client_get_entries (self->log_client),
                      osnx_log_client_is_available (self->log_client),
                      osnx_notify_settings_get_alerts_enabled (self->settings));
}

static void
osnx_log_client_changed (OsnxLogClient *client G_GNUC_UNUSED,
                         gpointer       user_data)
{
  OsnxPlugin *self = user_data;

  osnx_plugin_update_button (self);
  osnx_plugin_update_drawer (self);
}

static void
osnx_settings_changed (OsnxNotifySettings *settings G_GNUC_UNUSED,
                       gpointer            user_data)
{
  OsnxPlugin *self = user_data;
  osnx_plugin_update_drawer (self);
}

static void
osnx_drawer_closed (gpointer user_data)
{
  OsnxPlugin *self = user_data;

  if (self->refresh_timeout_id != 0)
    {
      g_source_remove (self->refresh_timeout_id);
      self->refresh_timeout_id = 0;
    }

  if (self->button != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
}

static void
osnx_drawer_alerts_changed (gboolean enabled,
                            gpointer user_data)
{
  OsnxPlugin *self = user_data;

  osnx_notify_settings_set_alerts_enabled (self->settings, enabled);
  osnx_plugin_update_drawer (self);
}

static void
osnx_drawer_clear_group (const gchar *group_key,
                         gpointer     user_data)
{
  OsnxPlugin *self = user_data;

  osnx_log_client_hide_group (self->log_client, group_key);
}

static void
osnx_drawer_activate_entry (const gchar *entry_id,
                            gpointer     user_data)
{
  OsnxPlugin *self = user_data;

  osnx_log_client_activate_entry (self->log_client, entry_id);
}

static gboolean
osnx_plugin_refresh_while_open (gpointer user_data)
{
  OsnxPlugin *self = user_data;

  if (!osnx_drawer_get_visible (self->drawer))
    {
      self->refresh_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  osnx_log_client_refresh (self->log_client);
  return G_SOURCE_CONTINUE;
}

static void
osnx_plugin_show_drawer (OsnxPlugin *self)
{
  osnx_notify_settings_ensure_logging (self->settings);
  osnx_log_client_refresh (self->log_client);
  osnx_drawer_show (self->drawer,
                    GTK_WIDGET (self),
                    osnx_log_client_get_entries (self->log_client),
                    osnx_log_client_is_available (self->log_client),
                    osnx_notify_settings_get_alerts_enabled (self->settings));

  if (self->refresh_timeout_id == 0)
    self->refresh_timeout_id = g_timeout_add_seconds (1, osnx_plugin_refresh_while_open, self);
}

static void
osnx_button_toggled (GtkToggleButton *button,
                     gpointer         user_data)
{
  OsnxPlugin *self = user_data;

  if (gtk_toggle_button_get_active (button))
    osnx_plugin_show_drawer (self);
  else
    osnx_drawer_hide (self->drawer);
}

static void
osnx_plugin_build_button (OsnxPlugin *self,
                          XfcePanelPlugin *plugin)
{
  GtkStyleContext *context;

  self->button = gtk_toggle_button_new ();
  self->button_overlay = gtk_overlay_new ();
  self->button_image = gtk_image_new ();
  self->button_unread_dot = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  context = gtk_widget_get_style_context (self->button);
  gtk_style_context_add_class (context, "osnx-panel-button");

  context = gtk_widget_get_style_context (self->button_unread_dot);
  gtk_style_context_add_class (context, "osnx-panel-unread-dot");
  gtk_widget_set_halign (self->button_unread_dot, GTK_ALIGN_END);
  gtk_widget_set_valign (self->button_unread_dot, GTK_ALIGN_START);
  gtk_widget_set_no_show_all (self->button_unread_dot, TRUE);

  gtk_container_add (GTK_CONTAINER (self->button_overlay), self->button_image);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->button_overlay), self->button_unread_dot);
  gtk_container_add (GTK_CONTAINER (self->button), self->button_overlay);
  xfce_panel_plugin_add_action_widget (plugin, self->button);
  gtk_container_add (GTK_CONTAINER (plugin), self->button);

  g_signal_connect (self->button, "toggled", G_CALLBACK (osnx_button_toggled), self);

  osnx_plugin_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
  osnx_plugin_update_button (self);
  gtk_widget_show_all (self->button);
}

static void
osnx_plugin_construct (XfcePanelPlugin *plugin)
{
  OsnxPlugin *self = (OsnxPlugin *) plugin;

  osnx_style_apply ();
  xfce_panel_plugin_set_small (plugin, TRUE);

  self->settings = osnx_notify_settings_new ();
  self->log_client = osnx_log_client_new ();
  self->drawer = osnx_drawer_new (osnx_drawer_closed,
                                  osnx_drawer_alerts_changed,
                                  osnx_drawer_clear_group,
                                  osnx_drawer_activate_entry,
                                  self);

  osnx_log_client_set_changed_callback (self->log_client, osnx_log_client_changed, self);
  osnx_notify_settings_set_changed_callback (self->settings, osnx_settings_changed, self);

  osnx_plugin_build_button (self, plugin);
  osnx_log_client_connect_async (self->log_client);
}

static void
osnx_plugin_free_data (XfcePanelPlugin *plugin)
{
  OsnxPlugin *self = (OsnxPlugin *) plugin;

  g_clear_pointer (&self->drawer, osnx_drawer_free);
  g_clear_pointer (&self->log_client, osnx_log_client_unref);
  g_clear_pointer (&self->settings, osnx_notify_settings_free);

  if (self->refresh_timeout_id != 0)
    {
      g_source_remove (self->refresh_timeout_id);
      self->refresh_timeout_id = 0;
    }
}

static gboolean
osnx_plugin_size_changed (XfcePanelPlugin *plugin,
                          gint             size)
{
  OsnxPlugin *self = (OsnxPlugin *) plugin;

  if (self->button != NULL)
    gtk_widget_set_size_request (self->button, size, size);

  if (self->button_image != NULL)
    osnx_plugin_set_button_icon (self);

  return TRUE;
}

static void
osnx_plugin_class_init (OsnxPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);

  plugin_class->construct = osnx_plugin_construct;
  plugin_class->free_data = osnx_plugin_free_data;
  plugin_class->size_changed = osnx_plugin_size_changed;
}

static void
osnx_plugin_init (OsnxPlugin *self G_GNUC_UNUSED)
{
}
