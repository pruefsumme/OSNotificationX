project(
  'osnotificationx',
  'c',
  version: '0.1.0',
  meson_version: '>=1.0.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
  ],
)

prefix = get_option('prefix')
datadir = get_option('datadir')
libdir = get_option('libdir')

osnx_data_dir = join_paths(prefix, datadir, 'osnotificationx')
osnx_panel_plugin_dir = join_paths(prefix, libdir, 'xfce4', 'panel', 'plugins')
osnx_panel_desktop_dir = join_paths(prefix, datadir, 'xfce4', 'panel', 'plugins')

config_h = configuration_data()
config_h.set_quoted('OSNX_DATA_DIR', osnx_data_dir)
config_h.set_quoted('OSNX_VERSION', meson.project_version())

configure_file(
  output: 'config.h',
  configuration: config_h,
)

subdir('src')
subdir('data')
subdir('tests')
