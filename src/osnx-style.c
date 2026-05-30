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
    " background-image: linear-gradient(to right, rgba(0,0,0,0.34), rgba(0,0,0,0.16) 48%%, rgba(0,0,0,0.00));"
    "}\n"
    ".osnx-drawer-panel {"
    " background-color: #343d45;"
    " background-image: linear-gradient(rgba(72,84,94,0.38), rgba(44,52,61,0.34)), url('%s');"
    " background-repeat: repeat, repeat;"
    " background-size: auto, 430px 430px;"
    " color: #f2f2f2;"
    " font-family: 'Lucida Grande', 'Lucida Sans Unicode', 'Lucida Sans', 'DejaVu Sans', sans-serif;"
    " font-size: 11px;"
    " border-left: 1px solid rgba(0,0,0,0.66);"
    "}\n"
    ".osnx-topbar {"
    " padding: 8px 9px 8px 14px;"
    " background-image: linear-gradient(to bottom, rgba(255,255,255,0.12), rgba(255,255,255,0.03) 47%%, rgba(0,0,0,0.18) 48%%, rgba(0,0,0,0.34));"
    " border-top: 1px solid rgba(255,255,255,0.10);"
    " border-bottom: 1px solid rgba(0,0,0,0.60);"
    "}\n"
    ".osnx-topbar label { color: #fbfbfb; text-shadow: 0 1px rgba(0,0,0,0.98); font-weight: 700; }\n"
    ".osnx-subtitle { color: #e8e8e8; text-shadow: 0 1px rgba(0,0,0,0.75); font-size: 10px; }\n"
    ".osnx-topbar switch { min-width: 55px; min-height: 24px; border-radius: 13px; }\n"
    ".osnx-group-header {"
    " min-height: 21px;"
    " padding: 2px 6px 2px 8px;"
    " background-image: linear-gradient(to bottom, rgba(255,255,255,0.12), rgba(255,255,255,0.04) 45%%, rgba(0,0,0,0.18) 46%%, rgba(0,0,0,0.40));"
    " border-top: 1px solid rgba(255,255,255,0.12);"
    " border-bottom: 1px solid rgba(0,0,0,0.56);"
    " border-radius: 0;"
    "}\n"
    ".osnx-group-header image { -gtk-icon-shadow: 0 1px rgba(0,0,0,0.75); }\n"
    ".osnx-group-header label { color: #f7f7f7; text-shadow: 0 1px rgba(0,0,0,0.78); font-weight: 700; font-size: 12px; }\n"
    ".osnx-group-label { padding-top: 1px; }\n"
    ".osnx-clear-button { padding: 0; margin: 0; min-width: 16px; min-height: 16px; border-radius: 8px; color: #c9c9c9; font-weight: 700; font-size: 11px; background-image: linear-gradient(to bottom, #56585b, #242628 52%%, #111214); border: 1px solid rgba(0,0,0,0.78); box-shadow: inset 0 1px rgba(255,255,255,0.20), 0 1px rgba(255,255,255,0.08); text-shadow: 0 -1px rgba(0,0,0,0.86); transition: none; }\n"
    ".osnx-clear-button:hover, .osnx-clear-button:active, .osnx-clear-button:checked { background-image: linear-gradient(to bottom, #56585b, #242628 52%%, #111214); color: #ffffff; box-shadow: inset 0 1px rgba(255,255,255,0.20), 0 1px rgba(255,255,255,0.08); }\n"
    ".osnx-group-clearing { background-color: transparent; }\n"
    ".osnx-notification-row { padding: 7px 9px 7px 9px; border-bottom: 1px solid rgba(255,255,255,0.075); }\n"
    ".osnx-notification-row.osnx-row-active { background-image: linear-gradient(to bottom, rgba(45,158,255,0.92), rgba(3,96,207,0.94)); border-bottom-color: rgba(0,0,0,0.24); }\n"
    ".osnx-notification-row.osnx-row-active label { color: #ffffff; text-shadow: 0 1px rgba(0,42,102,0.92); }\n"
    ".osnx-title { color: #ffffff; font-weight: 700; text-shadow: 0 1px rgba(0,0,0,0.78); font-size: 11px; }\n"
    ".osnx-body { color: #f0f0f0; text-shadow: 0 1px rgba(0,0,0,0.72); font-size: 11px; }\n"
    ".osnx-time { color: #ffffff; font-weight: 700; text-shadow: 0 1px rgba(0,0,0,0.78); font-size: 10px; }\n"
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
