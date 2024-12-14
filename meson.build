project('paps', 'c', 'cpp',
        version: '0.8.0',
        default_options : ['cpp_std=c++17'],
        meson_version : '>= 0.55')

fmt_dep = dependency('fmt')

pkg = import('pkgconfig')
pango_dep = dependency('pangoft2')
cairo_dep = dependency('pangocairo')
glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

# C compiler. This is the cross compiler if we're cross-compiling
cc = meson.get_compiler('c')
libpaper = cc.find_library('paper')

cdata = configuration_data()

# Checks for library functions
foreach f : ['memmove', 'memset', 'strdup']
  cdata.set('HAVE_' + f.to_upper(), cc.has_function(f))
endforeach

# Checks for header files
# Some HAVE_FOO defines need to be defined to either 1 or 0, others need to
# be defined or undefined. The code base is a bit inconsistent there.
foreach h : ['stdlib.h', 'string.h', 'memory.h']
  cdata.set10('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach
foreach h : ['strings.h', 'sys/times.h']
  cdata.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach

# Not entirely correct, but sufficient for us. Should move away from this
# ancient define and just include individual headers based on individual defs
if cc.has_header('strings.h')
  # define to 1 or leave undefined otherwise, don't simplify
  cdata.set('STDC_HEADERS', 1)
endif

# This is available pretty much everywhere
cdata.set('HAVE_STRINGIZE', 1)

# Assume we have nls, since paps doesn't compile without it
cdata.set('ENABLE_NLS',1)

# Not really sure what this is used for, but paps doesn't compile without it.
cdata.set_quoted('DATADIR','.')

buildtype = get_option('buildtype')
if buildtype == 'debug' or buildtype == 'debugoptimized'
  cdata.set('DEBUG', 1)
endif

cdata.set_quoted('GETTEXT_PACKAGE', meson.project_name())
cdata.set_quoted('PACKAGE_STRING', 'paps-' + meson.project_version())

# write config.h
config_h = configure_file(output: 'config.h', configuration: cdata)

incs = include_directories('.', 'src')

# Install the src-to-paps script with its pango_markup.outlang
# dependancy.
install_data(['misc/pango_markup.outlang'],
             install_dir : 'share/paps')

install_data(['scripts/src-to-paps'],
             install_dir : 'bin')

subdir('src')
