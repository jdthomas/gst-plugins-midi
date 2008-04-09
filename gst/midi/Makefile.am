plugindir = $(libdir)/gstreamer-$(GST_MAJORMINOR)

plugin_LTLIBRARIES = libgstmidi.la

libgstmidi_la_SOURCES = gstmidibuffer.c gstsmfdec.c
libgstmidi_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS)
libgstmidi_la_LIBADD = $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR)
libgstmidi_la_LDFLAGS =$(PLUGIN_LIBS)

noinst_HEADERS = gstmidibuffer.h
