project('plug', 'c',
    version: '0.1.0',
    license: 'GPL-3.0-or-later')

# Dependencies
gtk_dep = dependency('gtk4', version: '>= 4.12.0')
glib_dep = dependency('glib-2.0', version: '>= 2.78.0')
gio_dep = dependency('gio-2.0', version: '>= 2.78.0')
gobject_dep = dependency('gobject-2.0', version: '>= 2.78.0')
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.6.0')

# libm for round() used by the chart view
math_dep = meson.get_compiler('c').find_library('m', required: false)

# Compiler flags
add_project_arguments(
    '-DGETTEXT_PACKAGE="plug"',
    '-DLOCALEDIR="' + get_option('localedir') + '"',
    language: 'c'
)

# Resources
resource_files = files(
    'data/io.github.manuelrw.Plug.gresource.xml',
    'data/io.github.manuelrw.Plug.desktop.in',
    'data/io.github.manuelrw.Plug.appdata.xml',
)

# GResource compilation (commented out for now - UI files will be loaded at runtime)
# gresource = import('gnome')
# gresource_data = gresource.compile_resources(
#     'plug',
#     'data/io.github.manuelrw.Plug.gresource.xml',
#     source_dir: meson.current_source_dir(),
#     c_name: 'usb_tester_monitor'
# )

# Executable
executable('plug',
    sources: [
        'src/main.c',
        'src/application.c',
        'src/application.h',
        'src/main-window.c',
        'src/main-window.h',
        'src/device-manager.c',
        'src/device-manager.h',
        'src/device-permissions.c',
        'src/device-permissions.h',
        'src/device-protocol.c',
        'src/device-protocol.h',
        'src/qianli-a3.c',
        'src/qianli-a3.h',
        'src/fnirsi-fnb48.c',
        'src/fnirsi-fnb48.h',
        'src/powerz-km003c.c',
        'src/powerz-km003c.h',
        'src/data-display.c',
        'src/data-display.h',
        'src/stats-card.c',
        'src/stats-card.h',
        'src/chart-view.c',
        'src/chart-view.h',
        'src/data-logger.c',
        'src/data-logger.h',
    ],
    dependencies: [gtk_dep, glib_dep, gio_dep, gobject_dep, libadwaita_dep, math_dep],
    install: true
)



# Install desktop and metainfo files
desktop_file = configure_file(
    input: 'data/io.github.manuelrw.Plug.desktop.in',
    output: 'io.github.manuelrw.Plug.desktop',
    configuration: {'version': meson.project_version()},
    install: true,
    install_dir: get_option('datadir') / 'applications'
)

appdata_file = configure_file(
    input: 'data/io.github.manuelrw.Plug.appdata.xml',
    output: 'io.github.manuelrw.Plug.appdata.xml',
    configuration: {'version': meson.project_version()},
    install: true,
    install_dir: get_option('datadir') / 'metainfo'
)

# Install app icons
install_data(
    'data/icons/hicolor/scalable/apps/io.github.manuelrw.Plug.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
)

install_data(
    'data/icons/hicolor/symbolic/apps/io.github.manuelrw.Plug-symbolic.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/symbolic/apps',
)
