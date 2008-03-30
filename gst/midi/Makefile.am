plugindir = $(libdir)/gstreamer-$(GST_MAJORMINOR)

if USE_FLUID
FLUID_LIB = libgstfluidsynth.la
libgstfluidsynth_la_SOURCES =  gstfluidsynth.c

libgstfluidsynth_la_CFLAGS  = $(FLUID_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS) -I../../gst/midi
libgstfluidsynth_la_LIBADD  =                 $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR)
libgstfluidsynth_la_LDFLAGS = $(FLUID_LIBS)
else
FLUID_LIB=
endif

plugin_LTLIBRARIES = libgstmidi.la $(FLUID_LIB)

libgstmidi_la_SOURCES = gstmidibuffer.c gstsmfdec.c
libgstmidi_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS)
libgstmidi_la_LIBADD = $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR)
libgstmidi_la_LDFLAGS =$(PLUGIN_LIBS)

noinst_HEADERS = gstmidibuffer.h
