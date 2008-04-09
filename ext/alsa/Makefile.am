plugindir = $(libdir)/gstreamer-$(GST_MAJORMINOR)
plugin_LTLIBRARIES = libgstamidisink.la libgstamidisrc.la
libgstamidisink_la_SOURCES =  gstamidisink.c

libgstamidisink_la_CFLAGS  = $(ALSA_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS) -I../../gst/midi/
libgstamidisink_la_LIBADD  =                 $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR) -lgstmidi
libgstamidisink_la_LDFLAGS = $(ALSA_LIBS) -L../../gst/midi

libgstamidisrc_la_SOURCES =  gstamidisrc.c

libgstamidisrc_la_CFLAGS  = $(ALSA_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(PLUGIN_CFLAGS) -I../../gst/midi/
libgstamidisrc_la_LIBADD  =                 $(GST_PLUGINS_BASE_LIBS)   $(GST_BASE_LIBS)   $(GST_LIBS) -lgsttag-$(GST_MAJORMINOR) -lgstmidi
libgstamidisrc_la_LDFLAGS = $(ALSA_LIBS) -L../../gst/midi
