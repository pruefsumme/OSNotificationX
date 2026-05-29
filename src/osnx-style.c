#include "config.h"
#include "osnx-style.h"

void
osnx_style_apply (void)
{
  static gboolean applied = FALSE;
  g_autoptr (GtkCssProvider) provider = NULL;
  g_autofree gchar *asset_path = NULL;
  g_autofree gchar *asset_uri = NULL;
  g_autofree gchar *css = NULL;
  g_autoptr (GError) error = NULL;
  GdkScreen *screen = NULL;

  if (applied)
    return;

  asset_path = g_build_filename (OSNX_DATA_DIR, "cloth_bg.png", NULL);
  asset_uri = g_filename_to_uri (asset_path, NULL, NULL);
  if (asset_uri == NULL)
    asset_uri = g_strdup ("");

  css = g_strdup_printf (
    ".osnx-panel-button { padding: 0; border-radius: 0; }\n"
    ".osnx-panel-unread-dot {"
    " min-width: 7px; min-height: 7px; border-radius: 4px;"
    " background-image: radial-gradient(circle, #6ec4ff 0%%, #0877dd 72%%, #064c99 100%%);"
    " box-shadow: 0 0 2px rgba(96,184,255,0.95);"
    " margin: 1px;"
    "}\n"
    ".osnx-window-shell { background-color: transparent; }\n"
    ".osnx-edge-shadow {"
    " background-image: linear-gradient(to right, rgba(0,0,0,0.00), rgba(0,0,0,0.18) 18%%, rgba(0,0,0,0.58) 68%%, rgba(0,0,0,0.96));"
    "}\n"
    ".osnx-drawer-panel {"
    " background-color: #111316;"
    " background-image: linear-gradient(rgba(0,0,0,0.58), rgba(0,0,0,0.58)), url('%s');"
    " background-repeat: repeat, repeat;"
    " color: #f2f2f2;"
    " font-family: 'Lucida Grande', 'Lucida Sans Unicode', 'Lucida Sans', 'DejaVu Sans', sans-serif;"
    " font-size: 11px;"
    " border-left: 1px solid rgba(255,255,255,0.08);"
    "}\n"
    ".osnx-topbar {"
    " padding: 8px 9px 8px 14px;"
    " background-image: linear-gradient(to bottom, rgba(255,255,255,0.14), rgba(255,255,255,0.04) 48%%, rgba(0,0,0,0.32) 49%%, rgba(0,0,0,0.46));"
    " border-top: 1px solid rgba(255,255,255,0.12);"
    " border-bottom: 1px solid rgba(0,0,0,0.86);"
    "}\n"
    ".osnx-topbar label { color: #fbfbfb; text-shadow: 0 1px rgba(0,0,0,0.98); font-weight: 700; }\n"
    ".osnx-subtitle { color: #e8e8e8; text-shadow: 0 1px rgba(0,0,0,0.95); font-size: 10px; }\n"
    ".osnx-topbar switch { min-width: 55px; min-height: 24px; border-radius: 13px; }\n"
    ".osnx-group-header {"
    " min-height: 23px;"
    " padding: 2px 6px 2px 10px;"
    " background-image: linear-gradient(to bottom, rgba(255,255,255,0.22), rgba(255,255,255,0.10) 45%%, rgba(0,0,0,0.24) 46%%, rgba(0,0,0,0.54));"
    " border-top: 1px solid rgba(255,255,255,0.18);"
    " border-bottom: 1px solid rgba(0,0,0,0.84);"
    "}\n"
    ".osnx-group-header image { -gtk-icon-shadow: 0 1px rgba(0,0,0,0.75); }\n"
    ".osnx-group-header label { color: #f7f7f7; text-shadow: 0 1px rgba(0,0,0,0.98); font-weight: 700; font-size: 12px; }\n"
    ".osnx-group-label { padding-top: 1px; }\n"
    ".osnx-clear-button { padding: 0; min-width: 17px; min-height: 17px; border-radius: 9px; color: #4b4b4b; font-weight: 700; font-size: 11px; background-image: linear-gradient(to bottom, #d8d8d8, #a8a8a8); border: 1px solid rgba(0,0,0,0.45); box-shadow: inset 0 1px rgba(255,255,255,0.58), 0 1px rgba(0,0,0,0.35); }\n"
    ".osnx-notification-row { padding: 7px 9px 7px 9px; border-bottom: 1px solid rgba(255,255,255,0.045); }\n"
    ".osnx-title { color: #ffffff; font-weight: 700; text-shadow: 0 1px rgba(0,0,0,0.98); font-size: 11px; }\n"
    ".osnx-body { color: #ececec; text-shadow: 0 1px rgba(0,0,0,0.96); font-size: 11px; }\n"
    ".osnx-time { color: #ffffff; font-weight: 700; text-shadow: 0 1px rgba(0,0,0,0.98); font-size: 10px; }\n"
    ".osnx-unread-dot {"
    " min-width: 9px; min-height: 9px; border-radius: 5px;"
    " background-image: radial-gradient(circle, #62c1ff 0%%, #0877dd 70%%, #063f89 100%%);"
    " box-shadow: 0 0 2px rgba(96,184,255,0.95);"
    "}\n"
    ".osnx-read-space { min-width: 9px; min-height: 9px; }\n"
    ".osnx-empty { padding: 20px 14px; color: #dddddd; text-shadow: 0 1px rgba(0,0,0,0.9); }\n",
    asset_uri);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, &error);
  if (error != NULL)
    g_warning ("OSNotificationX: unable to load CSS: %s", error->message);

  screen = gdk_screen_get_default ();
  if (screen != NULL)
    gtk_style_context_add_provider_for_screen (screen,
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  applied = TRUE;
}
