project('usb-tester-monitor', 'c',
    version: '0.1.0',
    license: 'GPL-3.0-or-later')

# Dependencies
gtk_dep = dependency('gtk4', version: '>= 4.12.0')
glib_dep = dependency('glib-2.0', version: '>= 2.78.0')
gio_dep = dependency('gio-2.0', version: '>= 2.78.0')
gobject_dep = dependency('gobject-2.0', version: '>= 2.78.0')
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.4.0')

# Compiler flags
add_project_arguments(
    '-DGETTEXT_PACKAGE="usb-tester-monitor"',
    '-DLOCALEDIR="' + get_option('localedir') + '"',
    language: 'c'
)

# Resources
resource_files = files(
    'data/org.gnome.USBTesterMonitor.gresource.xml',
    'data/org.gnome.USBTesterMonitor.desktop.in',
    'data/org.gnome.USBTesterMonitor.appdata.xml',
)

# GResource compilation
gresource = import('gnome')
gresource_data = gresource.compile_resources(
    'usb-tester-monitor',
    'data/org.gnome.USBTesterMonitor.gresource.xml',
    source_dir: meson.current_source_dir(),
    c_name: 'usb_tester_monitor'
)

# Executable
executable('usb-tester-monitor',
    sources: [
        'src/main.c',
        'src/application.c',
        'src/application.h',
        'src/main-window.c',
        'src/main-window.h',
        'src/device-manager.c',
        'src/device-manager.h',
        'src/device-protocol.c',
        'src/device-protocol.h',
        'src/qianli-a3.c',
        'src/qianli-a3.h',
        'src/data-display.c',
        'src/data-display.h',
        'src/chart-view.c',
        'src/chart-view.h',
        'src/data-logger.c',
        'src/data-logger.h',
    ],
    dependencies: [gtk_dep, glib_dep, gio_dep, gobject_dep, libadwaita_dep],
    install: true
)



# Install desktop and appdata files
desktop_file = configure_file(
    input: 'data/org.gnome.USBTesterMonitor.desktop.in',
    output: 'org.gnome.USBTesterMonitor.desktop',
    configuration: {'version': meson.project_version()},
    install: true,
    install_dir: get_option('datadir') / 'applications'
)

appdata_file = configure_file(
    input: 'data/org.gnome.USBTesterMonitor.appdata.xml',
    output: 'org.gnome.USBTesterMonitor.appdata.xml',
    configuration: {'version': meson.project_version()},
    install: true,
    install_dir: get_option('datadir') / 'appdata'
)

# Install icons
icons = [
    'data/icons/hicolor/16x16/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/32x32/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/48x48/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/64x64/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/128x128/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/256x256/apps/org.gnome.USBTesterMonitor.svg',
    'data/icons/hicolor/scalable/apps/org.gnome.USBTesterMonitor.svg',
]

foreach icon : icons
    install_data(icon, install_dir: get_option('datadir') / 'icons/hicolor/' / icon.split('/')[-3] + 'x' + icon.split('/')[-2] / 'apps')
endforeach
