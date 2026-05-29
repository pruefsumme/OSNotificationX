#include "osnx-drawer.h"

#define OSNX_DRAWER_WIDTH 336
#define OSNX_SHADOW_WIDTH 18
#define OSNX_WINDOW_WIDTH (OSNX_DRAWER_WIDTH + OSNX_SHADOW_WIDTH)
#define OSNX_ANIMATION_MS 260

typedef struct
{
  gchar *key;
  gchar *app_name;
  GIcon *icon;
  GPtrArray *entries;
} OsnxGroup;

struct _OsnxDrawer
{
  GtkWidget *window;
  GtkWidget *shell;
  GtkWidget *edge_shadow;
  GtkWidget *panel;
  GtkWidget *content_box;
  GtkWidget *alerts_switch;
  GtkWidget *alerts_subtitle;

  OsnxDrawerClosedFunc closed_callback;
  OsnxDrawerAlertsChangedFunc alerts_changed_callback;
  OsnxDrawerClearGroupFunc clear_group_callback;
  OsnxDrawerActivateEntryFunc activate_entry_callback;
  gpointer user_data;

  GdkRectangle geometry;
  guint tick_id;
  gint start_x;
  gint current_x;
  gint end_x;
  gint64 animation_start_us;
  gboolean visible;
  gboolean hiding;
  gboolean syncing_alerts_switch;
};

static void osnx_drawer_render (OsnxDrawer *drawer, GPtrArray *entries, gboolean log_available);

static void
osnx_group_free (OsnxGroup *group)
{
  if (group == NULL)
    return;

  g_free (group->key);
  g_free (group->app_name);
  g_clear_object (&group->icon);
  g_clear_pointer (&group->entries, g_ptr_array_unref);
  g_free (group);
}

static const gchar *
osnx_entry_app_name (const OsnxLogEntry *entry)
{
  if (entry->app_name != NULL && entry->app_name[0] != '\0')
    return entry->app_name;

  if (entry->app_id != NULL && entry->app_id[0] != '\0')
    return entry->app_id;

  return "Notifications";
}

static GtkWidget *
osnx_make_label (const gchar *text,
                 const gchar *css_class,
                 gfloat       xalign)
{
  GtkWidget *label = gtk_label_new (text != NULL ? text : "");
  GtkStyleContext *context = gtk_widget_get_style_context (label);

  gtk_label_set_xalign (GTK_LABEL (label), xalign);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 36);
  gtk_style_context_add_class (context, css_class);

  return label;
}

static void
osnx_container_clear (GtkWidget *container)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (container));
  GList *iter;

  for (iter = children; iter != NULL; iter = iter->next)
    gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (iter->data));

  g_list_free (children);
}

static void
osnx_drawer_position_for_anchor (OsnxDrawer *drawer,
                                 GtkWidget  *anchor)
{
  GdkDisplay *display = NULL;
  GdkMonitor *monitor = NULL;
  GdkWindow *anchor_window = NULL;

  display = gtk_widget_get_display (anchor);
  anchor_window = gtk_widget_get_window (anchor);

  if (display != NULL && anchor_window != NULL)
    monitor = gdk_display_get_monitor_at_window (display, anchor_window);
  if (display != NULL && monitor == NULL)
    monitor = gdk_display_get_primary_monitor (display);
  if (monitor != NULL)
    gdk_monitor_get_workarea (monitor, &drawer->geometry);
  else
    drawer->geometry = (GdkRectangle) { 0, 0, OSNX_DRAWER_WIDTH, 600 };

  gtk_window_resize (GTK_WINDOW (drawer->window), OSNX_WINDOW_WIDTH, drawer->geometry.height);
  gtk_widget_set_size_request (drawer->shell, OSNX_WINDOW_WIDTH, drawer->geometry.height);
  gtk_widget_set_size_request (drawer->edge_shadow, OSNX_SHADOW_WIDTH, drawer->geometry.height);
  gtk_widget_set_size_request (drawer->panel, OSNX_DRAWER_WIDTH, drawer->geometry.height);
}

static gdouble
osnx_ease_out_cubic (gdouble t)
{
  gdouble p = CLAMP (t, 0.0, 1.0) - 1.0;
  return p * p * p + 1.0;
}

static gboolean
osnx_drawer_animation_tick (GtkWidget     *widget,
                            GdkFrameClock *frame_clock,
                            gpointer       user_data)
{
  OsnxDrawer *drawer = user_data;
  gint64 elapsed_us = gdk_frame_clock_get_frame_time (frame_clock) - drawer->animation_start_us;
  gdouble progress = elapsed_us / (OSNX_ANIMATION_MS * 1000.0);
  gdouble eased = osnx_ease_out_cubic (progress);

  drawer->current_x = drawer->start_x + (gint) ((drawer->end_x - drawer->start_x) * eased);
  gtk_window_move (GTK_WINDOW (widget), drawer->current_x, drawer->geometry.y);

  if (progress < 1.0)
    return G_SOURCE_CONTINUE;

  drawer->tick_id = 0;
  drawer->current_x = drawer->end_x;
  gtk_window_move (GTK_WINDOW (widget), drawer->current_x, drawer->geometry.y);

  if (drawer->hiding)
    {
      drawer->visible = FALSE;
      drawer->hiding = FALSE;
      gtk_widget_hide (drawer->window);
      if (drawer->closed_callback != NULL)
        drawer->closed_callback (drawer->user_data);
    }

  return G_SOURCE_REMOVE;
}

static void
osnx_drawer_start_animation (OsnxDrawer *drawer,
                             gint        start_x,
                             gint        end_x,
                             gboolean    hiding)
{
  if (drawer->tick_id != 0)
    gtk_widget_remove_tick_callback (drawer->window, drawer->tick_id);

  drawer->start_x = start_x;
  drawer->current_x = start_x;
  drawer->end_x = end_x;
  drawer->hiding = hiding;
  drawer->animation_start_us = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (drawer->window));
  gtk_window_move (GTK_WINDOW (drawer->window), drawer->current_x, drawer->geometry.y);
  drawer->tick_id = gtk_widget_add_tick_callback (drawer->window, osnx_drawer_animation_tick, drawer, NULL);
}

static gboolean
osnx_drawer_delete_event (GtkWidget *widget G_GNUC_UNUSED,
                          GdkEvent  *event G_GNUC_UNUSED,
                          gpointer   user_data)
{
  osnx_drawer_hide (user_data);
  return TRUE;
}

static gboolean
osnx_alerts_switch_state_set (GtkSwitch *toggle G_GNUC_UNUSED,
                              gboolean   state,
                              gpointer   user_data)
{
  OsnxDrawer *drawer = user_data;

  if (!drawer->syncing_alerts_switch && drawer->alerts_changed_callback != NULL)
    drawer->alerts_changed_callback (state, drawer->user_data);

  return FALSE;
}

static void
osnx_drawer_sync_alerts (OsnxDrawer *drawer,
                         gboolean    alerts_enabled)
{
  if (drawer->alerts_switch == NULL)
    return;

  drawer->syncing_alerts_switch = TRUE;
  gtk_switch_set_active (GTK_SWITCH (drawer->alerts_switch), alerts_enabled);
  if (drawer->alerts_subtitle != NULL)
    gtk_label_set_text (GTK_LABEL (drawer->alerts_subtitle),
                        alerts_enabled ? "Notifications are active" : "Will resume tomorrow");
  drawer->syncing_alerts_switch = FALSE;
}

static void
osnx_group_clear_clicked (GtkButton *button,
                          gpointer   user_data)
{
  OsnxDrawer *drawer = user_data;
  const gchar *key = g_object_get_data (G_OBJECT (button), "osnx-group-key");

  if (key != NULL && drawer->clear_group_callback != NULL)
    drawer->clear_group_callback (key, drawer->user_data);
}

static gboolean
osnx_notification_button_release (GtkWidget      *widget,
                                  GdkEventButton *event,
                                  gpointer        user_data)
{
  OsnxDrawer *drawer = user_data;
  const gchar *entry_id = g_object_get_data (G_OBJECT (widget), "osnx-entry-id");

  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  if (entry_id != NULL && drawer->activate_entry_callback != NULL)
    drawer->activate_entry_callback (entry_id, drawer->user_data);

  return GDK_EVENT_STOP;
}

static GPtrArray *
osnx_build_groups (GPtrArray *entries)
{
  GPtrArray *groups = g_ptr_array_new_with_free_func ((GDestroyNotify) osnx_group_free);
  GHashTable *by_key = g_hash_table_new (g_str_hash, g_str_equal);
  guint i;

  if (entries == NULL)
    {
      g_hash_table_unref (by_key);
      return groups;
    }

  for (i = 0; i < entries->len; i++)
    {
      OsnxLogEntry *entry = g_ptr_array_index (entries, i);
      g_autofree gchar *key = osnx_log_entry_app_key (entry);
      OsnxGroup *group = g_hash_table_lookup (by_key, key);

      if (group == NULL)
        {
          group = g_new0 (OsnxGroup, 1);
          group->key = g_strdup (key);
          group->app_name = g_strdup (osnx_entry_app_name (entry));
          group->icon = osnx_log_entry_load_icon (entry);
          group->entries = g_ptr_array_new ();
          g_hash_table_insert (by_key, group->key, group);
          g_ptr_array_add (groups, group);
        }

      g_ptr_array_add (group->entries, entry);
    }

  g_hash_table_unref (by_key);
  return groups;
}

static GtkWidget *
osnx_create_group_header (OsnxDrawer *drawer,
                          OsnxGroup  *group)
{
  GtkWidget *header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 7);
  GtkWidget *image = gtk_image_new_from_gicon (group->icon, GTK_ICON_SIZE_MENU);
  GtkWidget *label = osnx_make_label (group->app_name, "osnx-group-label", 0.0f);
  GtkWidget *clear = gtk_button_new_with_label ("x");
  GtkStyleContext *context;

  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "osnx-group-header");
  context = gtk_widget_get_style_context (clear);
  gtk_style_context_add_class (context, "osnx-clear-button");
  gtk_button_set_relief (GTK_BUTTON (clear), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (clear, "Clear this application's notifications");
  g_object_set_data_full (G_OBJECT (clear), "osnx-group-key", g_strdup (group->key), g_free);
  g_signal_connect (clear, "clicked", G_CALLBACK (osnx_group_clear_clicked), drawer);

  gtk_box_pack_start (GTK_BOX (header), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (header), label, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (header), clear, FALSE, FALSE, 0);

  return header;
}

static GtkWidget *
osnx_create_notification_row (OsnxDrawer          *drawer,
                              const OsnxLogEntry *entry)
{
  GtkWidget *event_box = gtk_event_box_new ();
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *dot = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget *top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *title = osnx_make_label (entry->summary != NULL ? entry->summary : "(No summary)", "osnx-title", 0.0f);
  g_autofree gchar *time_text = osnx_log_entry_format_time (entry);
  GtkWidget *time = osnx_make_label (time_text, "osnx-time", 1.0f);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "osnx-notification-row");
  gtk_style_context_add_class (gtk_widget_get_style_context (dot), entry->is_read ? "osnx-read-space" : "osnx-unread-dot");
  gtk_widget_set_valign (dot, GTK_ALIGN_START);
  gtk_widget_set_margin_top (dot, 6);

  gtk_box_pack_start (GTK_BOX (top), title, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (top), time, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_box), top, FALSE, FALSE, 0);

  if (entry->body != NULL && entry->body[0] != '\0')
    {
      GtkWidget *body = osnx_make_label (entry->body, "osnx-body", 0.0f);
      gtk_label_set_line_wrap (GTK_LABEL (body), TRUE);
      gtk_label_set_line_wrap_mode (GTK_LABEL (body), PANGO_WRAP_WORD_CHAR);
      gtk_label_set_lines (GTK_LABEL (body), 3);
      gtk_box_pack_start (GTK_BOX (main_box), body, FALSE, FALSE, 0);
    }

  gtk_box_pack_start (GTK_BOX (row), dot, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (row), main_box, TRUE, TRUE, 0);

  gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
  gtk_widget_add_events (event_box, GDK_BUTTON_RELEASE_MASK);
  gtk_container_add (GTK_CONTAINER (event_box), row);
  if (entry->id != NULL)
    g_object_set_data_full (G_OBJECT (event_box), "osnx-entry-id", g_strdup (entry->id), g_free);
  g_signal_connect (event_box, "button-release-event", G_CALLBACK (osnx_notification_button_release), drawer);

  return event_box;
}

static void
osnx_drawer_render (OsnxDrawer *drawer,
                    GPtrArray  *entries,
                    gboolean    log_available)
{
  g_autoptr (GPtrArray) groups = NULL;
  guint i;

  osnx_container_clear (drawer->content_box);

  if (!log_available)
    {
      GtkWidget *label = osnx_make_label ("Notification log unavailable", "osnx-empty", 0.5f);
      gtk_box_pack_start (GTK_BOX (drawer->content_box), label, FALSE, FALSE, 0);
      gtk_widget_show_all (drawer->content_box);
      return;
    }

  if (entries == NULL || entries->len == 0)
    {
      GtkWidget *label = osnx_make_label ("No notifications", "osnx-empty", 0.5f);
      gtk_box_pack_start (GTK_BOX (drawer->content_box), label, FALSE, FALSE, 0);
      gtk_widget_show_all (drawer->content_box);
      return;
    }

  groups = osnx_build_groups (entries);
  for (i = 0; i < groups->len; i++)
    {
      OsnxGroup *group = g_ptr_array_index (groups, i);
      guint j;

      gtk_box_pack_start (GTK_BOX (drawer->content_box), osnx_create_group_header (drawer, group), FALSE, FALSE, 0);

      for (j = 0; j < group->entries->len; j++)
        {
          const OsnxLogEntry *entry = g_ptr_array_index (group->entries, j);
          gtk_box_pack_start (GTK_BOX (drawer->content_box), osnx_create_notification_row (drawer, entry), FALSE, FALSE, 0);
        }
    }

  gtk_widget_show_all (drawer->content_box);
}

OsnxDrawer *
osnx_drawer_new (OsnxDrawerClosedFunc        closed_callback,
                 OsnxDrawerAlertsChangedFunc alerts_changed_callback,
                 OsnxDrawerClearGroupFunc    clear_group_callback,
                 OsnxDrawerActivateEntryFunc  activate_entry_callback,
                 gpointer                    user_data)
{
  OsnxDrawer *drawer = g_new0 (OsnxDrawer, 1);
  GtkWidget *shell = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *edge_shadow = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *topbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *title = osnx_make_label ("Show Alerts and Banners", "osnx-top-title", 0.0f);
  GtkWidget *scrolled = gtk_scrolled_window_new (NULL, NULL);
  GdkScreen *screen = NULL;
  GdkVisual *visual = NULL;

  drawer->closed_callback = closed_callback;
  drawer->alerts_changed_callback = alerts_changed_callback;
  drawer->clear_group_callback = clear_group_callback;
  drawer->activate_entry_callback = activate_entry_callback;
  drawer->user_data = user_data;

  drawer->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  drawer->shell = shell;
  drawer->edge_shadow = edge_shadow;
  drawer->panel = outer;
  drawer->alerts_switch = gtk_switch_new ();
  drawer->alerts_subtitle = osnx_make_label ("Notifications are active", "osnx-subtitle", 0.0f);
  drawer->content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_window_set_decorated (GTK_WINDOW (drawer->window), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (drawer->window), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (drawer->window), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (drawer->window), TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (drawer->window), TRUE);
  gtk_window_set_accept_focus (GTK_WINDOW (drawer->window), TRUE);
  gtk_window_set_focus_on_map (GTK_WINDOW (drawer->window), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (drawer->window), GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_widget_set_app_paintable (drawer->window, TRUE);
  screen = gtk_widget_get_screen (drawer->window);
  visual = screen != NULL ? gdk_screen_get_rgba_visual (screen) : NULL;
  if (visual != NULL)
    gtk_widget_set_visual (drawer->window, visual);

  gtk_style_context_add_class (gtk_widget_get_style_context (drawer->shell), "osnx-window-shell");
  gtk_style_context_add_class (gtk_widget_get_style_context (drawer->edge_shadow), "osnx-edge-shadow");
  gtk_style_context_add_class (gtk_widget_get_style_context (drawer->panel), "osnx-drawer-panel");
  gtk_style_context_add_class (gtk_widget_get_style_context (topbar), "osnx-topbar");

  g_signal_connect (drawer->window, "delete-event", G_CALLBACK (osnx_drawer_delete_event), drawer);
  g_signal_connect (drawer->alerts_switch, "state-set", G_CALLBACK (osnx_alerts_switch_state_set), drawer);

  gtk_box_pack_start (GTK_BOX (label_box), title, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (label_box), drawer->alerts_subtitle, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (topbar), label_box, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (topbar), drawer->alerts_switch, FALSE, FALSE, 0);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (scrolled), drawer->content_box);

  gtk_box_pack_start (GTK_BOX (outer), topbar, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (outer), scrolled, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (shell), edge_shadow, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (shell), outer, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (drawer->window), shell);

  return drawer;
}

void
osnx_drawer_free (OsnxDrawer *drawer)
{
  if (drawer == NULL)
    return;

  if (drawer->tick_id != 0)
    gtk_widget_remove_tick_callback (drawer->window, drawer->tick_id);
  if (drawer->window != NULL)
    gtk_widget_destroy (drawer->window);

  g_free (drawer);
}

void
osnx_drawer_show (OsnxDrawer *drawer,
                  GtkWidget  *anchor,
                  GPtrArray  *entries,
                  gboolean    log_available,
                  gboolean    alerts_enabled)
{
  gint hidden_x;
  gint shown_x;

  g_return_if_fail (drawer != NULL);
  g_return_if_fail (GTK_IS_WIDGET (anchor));

  osnx_drawer_position_for_anchor (drawer, anchor);
  osnx_drawer_sync_alerts (drawer, alerts_enabled);
  osnx_drawer_render (drawer, entries, log_available);

  hidden_x = drawer->geometry.x + drawer->geometry.width;
  shown_x = drawer->geometry.x + drawer->geometry.width - OSNX_WINDOW_WIDTH;
  drawer->visible = TRUE;
  drawer->hiding = FALSE;
  gtk_window_move (GTK_WINDOW (drawer->window), hidden_x, drawer->geometry.y);
  gtk_widget_show_all (drawer->window);
  gtk_window_present (GTK_WINDOW (drawer->window));
  osnx_drawer_start_animation (drawer, hidden_x, shown_x, FALSE);
}

void
osnx_drawer_hide (OsnxDrawer *drawer)
{
  gint hidden_x;

  if (drawer == NULL || !drawer->visible || drawer->hiding)
    return;

  hidden_x = drawer->geometry.x + drawer->geometry.width;
  osnx_drawer_start_animation (drawer, drawer->current_x, hidden_x, TRUE);
}

void
osnx_drawer_update (OsnxDrawer *drawer,
                    GPtrArray  *entries,
                    gboolean    log_available,
                    gboolean    alerts_enabled)
{
  if (drawer == NULL)
    return;

  osnx_drawer_sync_alerts (drawer, alerts_enabled);
  osnx_drawer_render (drawer, entries, log_available);
}

gboolean
osnx_drawer_get_visible (OsnxDrawer *drawer)
{
  return drawer != NULL && drawer->visible && !drawer->hiding;
}
