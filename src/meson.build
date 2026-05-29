glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
gtk_dep = dependency('gtk+-3.0')
xfce_panel_dep = dependency('libxfce4panel-2.0')
xfconf_dep = dependency('libxfconf-0')
sqlite_dep = dependency('sqlite3')

osnx_common_sources = [
  'osnx-log-entry.c',
  'osnx-log-client.c',
  'osnx-notify-settings.c',
  'osnx-style.c',
  'osnx-drawer.c',
]

osnx_common_deps = [
  glib_dep,
  gobject_dep,
  gio_dep,
  gio_unix_dep,
  gtk_dep,
  xfconf_dep,
  sqlite_dep,
]

osnx_common_inc = include_directories('.', '..')

osnx_common_lib = static_library(
  'osnx-common',
  osnx_common_sources,
  include_directories: osnx_common_inc,
  dependencies: osnx_common_deps,
)

shared_module(
  'osnotificationx',
  [
    'osnotificationx-plugin.c',
  ],
  include_directories: [
    osnx_common_inc,
  ],
  dependencies: [
    osnx_common_deps,
    xfce_panel_dep,
    xfconf_dep,
  ],
  link_with: osnx_common_lib,
  name_prefix: 'lib',
  install: true,
  install_dir: osnx_panel_plugin_dir,
)
