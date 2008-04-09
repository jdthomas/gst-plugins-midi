plugindir = $(libdir)/gstreamer-$(GST_MAJORMINOR)
plugin_LTLIBRARIES = libgstfluidsynth.la
libgstfluidsynth_la_SOURCES =  gstfluidsynth.c

libgstfluidsynth_la_CFLAGS  = $(FLUID_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS) -I../../gst/midi/
libgstfluidsynth_la_LIBADD  =                 $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR) -lgstmidi
libgstfluidsynth_la_LDFLAGS = $(FLUID_LIBS) -L../../gst/midi
