
plugin_LTLIBRARIES = libmoodbar.la

libmoodbar_la_SOURCES = gstfftwspectrum.c gstfftwunspectrum.c \
			gstspectrumeq.c gstmoodbar.c spectrum.c 

libmoodbar_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(FFTW_CFLAGS)
libmoodbar_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(FFTW_LIBS)
libmoodbar_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstfftwspectrum.h gstfftwunspectrum.h \
	         gstspectrumeq.h gstmoodbar.h spectrum.h
