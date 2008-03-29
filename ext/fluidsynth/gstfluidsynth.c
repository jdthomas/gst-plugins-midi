/* GStreamer
 * Copyright (C) 2005 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <fluidsynth.h>
#include "gstmidibuffer.h"

#define GST_TYPE_FLUIDSYNTH (gst_fluidsynth_get_type())
#define GST_FLUIDSYNTH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FLUIDSYNTH,GstFluidsynth))
#define GST_FLUIDSYNTH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FLUIDSYNTH,GstFluidsynth))
#define GST_IS_FLUIDSYNTH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FLUIDSYNTH))
#define GST_IS_FLUIDSYNTH_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FLUIDSYNTH))

typedef struct _GstFluidsynth GstFluidsynth;
typedef struct _GstFluidsynthClass GstFluidsynthClass;

struct _GstFluidsynth {
  GstElement		element;

  GstPad *		sink;
  GstPad *		src;

  fluid_synth_t *	synth;
  gchar *		soundfont;
  GstClockTime		expected;
};

struct _GstFluidsynthClass {
  GstElementClass	parent_class;
};

static GstElementClass *parent_class = NULL;
static GType gst_fluidsynth_get_type (void);

static GstStaticPadTemplate gst_fluidsynth_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-gst-midi, bufferlength=(fraction) 1024/44100")
    );

static GstStaticPadTemplate gst_fluidsynth_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, endianness=(int)byte_order, buffer-frames=1024, width=32, rate=44100, channels=2")
    );

enum {
  ARG_0,
  ARG_SOUNDFONT
};

static void
gst_fluidsynth_start (GstFluidsynth *synth)
{
  fluid_settings_t *settings;
  
  g_assert (synth->synth == NULL);
  settings = new_fluid_settings ();
  synth->synth = new_fluid_synth (settings);
  if (synth->soundfont) {
    if (fluid_synth_sfload (synth->synth, synth->soundfont, 1) < 0) {
      g_free (synth->soundfont);
      synth->soundfont = NULL;
      g_object_notify (G_OBJECT (synth), "soundfont");
    }
  }
  synth->expected = 0;
}

static void
gst_fluidsynth_end (GstFluidsynth *synth)
{
  fluid_settings_t *settings;
  
  g_assert (synth->synth != NULL);
  settings = fluid_synth_get_settings (synth->synth);
  delete_fluid_synth (synth->synth);
  synth->synth = NULL;
  delete_fluid_settings (settings);
}

static gboolean
gst_fluidsynth_process_event (fluid_synth_t *synth, const guint8* event)
{
  switch (gst_midi_event_get_type (event)) {
    case GST_MIDI_NOTE_ON:
      if (fluid_synth_noteon (synth, gst_midi_event_get_channel (event),
	      gst_midi_event_get_byte1 (event), gst_midi_event_get_byte2 (event)) != 0)
	goto err;
      break;
    case GST_MIDI_NOTE_OFF:
      if (fluid_synth_noteoff (synth, gst_midi_event_get_channel (event),
	  gst_midi_event_get_byte1 (event)) != 0)
	goto err;
      break;
    case GST_MIDI_PITCH_BEND:
      if (fluid_synth_pitch_bend (synth, gst_midi_event_get_channel (event),
		      gst_midi_event_get_byte1 (event)) != 0)
	goto err;
      break;
    case GST_MIDI_CONTROL_CHANGE:
      if (fluid_synth_cc (synth, gst_midi_event_get_channel (event),
	      gst_midi_event_get_byte1 (event), gst_midi_event_get_byte2 (event)) != 0)
	goto err;
      break;
    case GST_MIDI_PROGRAM_CHANGE:
      if (fluid_synth_program_change (synth, gst_midi_event_get_channel (event),
	      gst_midi_event_get_byte1 (event)) != 0)
	goto err;
      break;
    default:
      gst_midi_event_dump (event);
      break;
  }
  return TRUE;

err:
  gst_midi_event_dump (event);
  return FALSE;
}

static GstFlowReturn
gst_fluidsynth_chain (GstPad * pad, GstBuffer * data)
{
  GstClockTime last;
  GstMidiIter iter;
  GstBuffer *out, *in = GST_BUFFER (data);
  GstFluidsynth *synth = GST_FLUIDSYNTH (gst_pad_get_parent (pad));
  gboolean events_left = TRUE;
  guint i;

  g_assert (synth->synth);
  gst_midi_iter_init (&iter, in);
  last = in->timestamp;
  while ((GstClockTimeDiff) last - synth->expected > GST_USECOND) {
    /* empty buffers */
    /* buffer size = sample size * channels * samples per buffer */
    //out = gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, sizeof (float) * 2 * 1024);
    gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, 1024,
	  GST_PAD_CAPS (synth->src), &out);

    out->timestamp = synth->expected;
    out->duration = GST_SECOND * 1024 / 44100;
    if (fluid_synth_write_float (synth->synth, 1024, 
	    out->data, 0, 2, out->data, 1, 2) != 0){
       g_print ("ERROR ERROR ERROR");
       return GST_FLOW_ERROR;
    }
    gst_pad_push (synth->src, GST_BUFFER (out));
    synth->expected += GST_SECOND * 1024 / 44100;
  }
  /* buffer size = sample size * channels * samples per buffer */
  //out = gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, sizeof (float) * 2 * 1024);
    gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, 1024,
	  GST_PAD_CAPS (synth->src), &out);
  //gst_buffer_stamp (out, in);
  gst_buffer_copy_metadata (out, in,GST_BUFFER_COPY_TIMESTAMPS);
  for (i = 0; i < 1024 / 64; i++) {
    last += in->duration * (i + 1) * 64 / 1024;
    while (events_left && gst_midi_iter_get_time (&iter) < last) {
      gst_fluidsynth_process_event (synth->synth, gst_midi_iter_get_event (&iter));
      events_left = gst_midi_iter_next (&iter);
    }
    if (fluid_synth_write_float (synth->synth, 64, 
	    out->data, 0 + 64 * 2 * i, 2, 
	    out->data, 1 + 64 * 2 * i, 2) != 0){
       g_print ("ERROR ERROR ERROR");
       return GST_FLOW_ERROR;
    }
  }
  synth->expected = in->timestamp + in->duration;
  //gst_data_unref (data);
  gst_buffer_unref (data);
  //g_print ("pushing %llu\n", GST_BUFFER_TIMESTAMP (out));
  gst_pad_push (synth->src, GST_BUFFER (out));
  return GST_FLOW_OK;;
}

static GstStateChangeReturn
gst_fluidsynth_change_state (GstElement * element, GstStateChange transition )
{
  GstFluidsynth *fluidsynth = GST_FLUIDSYNTH (element);

  switch ( transition ) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_fluidsynth_start (fluidsynth);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_fluidsynth_end (fluidsynth);
      break;
    default:
      break;
  }

  if (parent_class->change_state)
    return parent_class->change_state (element,transition);

  return GST_STATE_CHANGE_SUCCESS;
}

static void
gst_fluidsynth_dispose (GObject *object)
{
  GstFluidsynth *synth = GST_FLUIDSYNTH (object);

  g_assert (synth->synth == NULL);
  g_free (synth->soundfont);
  synth->soundfont = NULL;
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_fluidsynth_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstFluidsynth *synth = GST_FLUIDSYNTH (object);

  switch (prop_id) {
    case ARG_SOUNDFONT:
      synth->soundfont = g_value_dup_string (value);
      if (synth->synth && synth->soundfont) {
	if (fluid_synth_sfload (synth->synth, synth->soundfont, 1) < 0) {
	  g_free (synth->soundfont);
	  synth->soundfont = NULL;
	}
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_fluidsynth_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstFluidsynth *synth = GST_FLUIDSYNTH (object);

  switch (prop_id) {
    case ARG_SOUNDFONT:
      g_value_set_string (value, synth->soundfont);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_fluidsynth_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  parent_class = g_type_class_peek_parent (g_class);

  gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_fluidsynth_change_state);

  object_class->set_property = gst_fluidsynth_set_property;
  object_class->get_property = gst_fluidsynth_get_property;
  object_class->dispose = gst_fluidsynth_dispose;
  
  g_object_class_install_property (object_class, ARG_SOUNDFONT,
      g_param_spec_string ("soundfont", "soundfont",
	  "path to soundfont to be used",
	  NULL, G_PARAM_READWRITE));
}

static void
gst_fluidsynth_init (GstFluidsynth * fluidsynth)
{
  fluidsynth->sink =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&gst_fluidsynth_sink_template), "sink");
  gst_element_add_pad (GST_ELEMENT (fluidsynth), fluidsynth->sink);
  gst_pad_set_chain_function (fluidsynth->sink, GST_DEBUG_FUNCPTR(gst_fluidsynth_chain));
  
  fluidsynth->src = gst_pad_new_from_template (gst_static_pad_template_get (
	&gst_fluidsynth_src_template), "src");
  gst_element_add_pad (GST_ELEMENT (fluidsynth), fluidsynth->src);
}

static void
gst_fluidsynth_base_init (gpointer g_class)
{
  static GstElementDetails gst_fluidsynth_details =
  GST_ELEMENT_DETAILS ("standard midi file to midi converter",
      "Codec/Demuxer/Audio",
      "Convert a midi file to GStreamer midi representation",
      "Benjamin Otte <otte@gnome.org>");

  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_fluidsynth_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_fluidsynth_src_template));
  gst_element_class_set_details (element_class, &gst_fluidsynth_details);

}

static GType
gst_fluidsynth_get_type (void)
{
  static GType fluidsynth_type = 0;

  if (!fluidsynth_type) {
    static const GTypeInfo fluidsynth_info = {
      sizeof (GstFluidsynthClass),
      gst_fluidsynth_base_init,
      NULL,
      (GClassInitFunc) gst_fluidsynth_class_init,
      NULL,
      NULL,
      sizeof (GstFluidsynth),
      0,
      (GInstanceInitFunc) gst_fluidsynth_init,
    };

    fluidsynth_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstFluidsynth", &fluidsynth_info,
        0);
  }
  return fluidsynth_type;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "fluidsynth", GST_RANK_SECONDARY,
          GST_TYPE_FLUIDSYNTH))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gstfluidsynth",
    "midi to raw audio converter using fluidsynth", plugin_init, VERSION, "LGPL",
    GST_PACKAGE_NAME,GST_PACKAGE_ORIGIN)
