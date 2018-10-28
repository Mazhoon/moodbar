project('moodbar', 'c', version : '0.1.5')
add_global_arguments('-DHAVE_CONFIG_H', language: 'c')

build_cflags = ['-Wall']

buildtype = get_option('buildtype')
if buildtype == 'debug' or buildtype == 'debugoptimized'
    build_cflags += ['-g', '-Werror']
endif

gst_major = '1.0'
gst_required = '1.0.0'

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
configure_file(output : 'config.h', configuration : conf)

fftw = dependency('fftw3f', version: '>= 3.0', required: true)

gstreamer = dependency('-'.join(['gstreamer', gst_major]),
    version: ''.join(['>=', gst_required]), required: true)

gstbase = dependency('-'.join(['gstreamer-base', gst_major]),
    version: ''.join(['>=', gst_required]), required: true)

gstplugindir = get_option('plugindir')
if gstplugindir == ''
    gstpluginsdir = gstreamer.get_pkgconfig_variable('pluginsdir')
endif

plugin_sources = [
    'plugin/gstfftwspectrum.c',
    'plugin/gstfftwunspectrum.c',
    'plugin/gstspectrumeq.c',
    'plugin/gstmoodbar.c',
    'plugin/spectrum.c'
]

plugin_deps = [gstreamer, gstbase, fftw]
top_inc = include_directories('.')

shared_library('moodbar', plugin_sources, dependencies: plugin_deps,
    install: true, install_dir: gstpluginsdir, c_args: build_cflags,
    link_args: '-lm', include_directories : top_inc)

moodbar_installdir = join_paths([get_option('prefix'), get_option('bindir')])
executable('moodbar', sources: ['analyzer/main.c'], dependencies: gstreamer,
    install: true, install_dir: moodbar_installdir, c_args: build_cflags)
