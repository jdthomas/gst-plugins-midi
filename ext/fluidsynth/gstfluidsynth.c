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

#include <fluidsynth.h>
#include "gstmidibuffer.h"

#include <gst/gst.h>

#include "gstfluidsynth.h"

/* Filter signals and args */
enum {
  ARG_0,
  ARG_SOUNDFONT
};

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
	 GST_STATIC_CAPS (
		 "audio/x-raw-float, "
			 "rate = (int) 44100, "
			 "channels = (int) 2, "
			 "endianness = (int) BYTE_ORDER, "
			 "width = (int) 32 "
/**
		 " ; "
		 "audio/x-raw-int, "
			 "rate = (int) 44100, "
			 "channels = (int) 2, "
			 "endianness = (int) BYTE_ORDER, "
			 "width = (int) 16, "
			 "depth = (int) 16, "
			 "signed = (boolean) true"
**/
		 )
    );

GST_BOILERPLATE (GstFluidsynth, gst_fluidsynth, GstElement, GST_TYPE_ELEMENT);

static void gst_fluidsynth_set_property (GObject *object, guint prop_id, 
		const GValue *value, GParamSpec *pspec);
static void gst_fluidsynth_get_property (GObject *object, guint prop_id, 
		GValue *value, GParamSpec *pspec);

static GstFlowReturn gst_fluidsynth_chain (GstPad * pad, GstBuffer * data);
static GstStateChangeReturn gst_fluidsynth_change_state (GstElement * element,
		GstStateChange transition );
static gboolean gst_fluidsynth_process_event (fluid_synth_t *synth, 
		const guint8* event);

static void gst_fluidsynth_start (GstFluidsynth *synth);
static void gst_fluidsynth_end (GstFluidsynth *synth);
static void gst_fluidsynth_dispose (GObject *object);

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

static void
gst_fluidsynth_class_init (GstFluidsynthClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  //parent_class = g_type_class_peek_parent (g_class);

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
gst_fluidsynth_init (GstFluidsynth * fluidsynth,
    GstFluidsynthClass * gclass)
{
	GstElementClass *klass = GST_ELEMENT_GET_CLASS (fluidsynth);

	/* Sink pad setup */
  fluidsynth->sink = gst_pad_new_from_template
	  (gst_element_class_get_pad_template (klass, "sink"), "sink");
  gst_pad_use_fixed_caps (fluidsynth->sink);
	gst_pad_set_setcaps_function (fluidsynth->sink,
			GST_DEBUG_FUNCPTR(gst_pad_set_caps));
	gst_pad_set_chain_function (fluidsynth->sink,
			GST_DEBUG_FUNCPTR(gst_fluidsynth_chain));
	gst_element_add_pad (GST_ELEMENT (fluidsynth), fluidsynth->sink);

	/* Src pad setup */
	fluidsynth->src = gst_pad_new_from_template
		(gst_element_class_get_pad_template (klass, "src"), "src");
  gst_pad_use_fixed_caps (fluidsynth->src);
	gst_pad_set_setcaps_function (fluidsynth->src,
			GST_DEBUG_FUNCPTR(gst_pad_set_caps));
	gst_element_add_pad (GST_ELEMENT (fluidsynth), fluidsynth->src);
}

/**
 * Sets up the fluidsynth stuff. Loads a soundfont if one was specified. 
 */
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

/**
 * Tears down the fluidsynth library objects. 
 */
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
	GstFlowReturn ret;

	g_assert (synth->synth);
	gst_midi_iter_init (&iter, in);
	last = in->timestamp;
	while ((GstClockTimeDiff) last - synth->expected > GST_USECOND) {
		/* empty buffers */
		/* buffer size = sample size * channels * samples per buffer */
		ret = gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, 1024*2*sizeof(float), GST_PAD_CAPS (synth->src), &out);
		if( ret != GST_FLOW_OK ){
			return ret;
		}

		out->timestamp = synth->expected;
		out->duration = GST_SECOND * 1024 / 44100;
		if (fluid_synth_write_float (synth->synth, 1024, 
					out->data, 0, 2, out->data, 1, 2) != 0){
			g_print ("ERROR ERROR ERROR");
			return GST_FLOW_ERROR;
		}
    gst_buffer_set_caps (GST_BUFFER (out), synth->out_caps);
		gst_pad_push (synth->src, GST_BUFFER (out));
		synth->expected += GST_SECOND * 1024 / 44100;
	}
	/* buffer size = sample size * channels * samples per buffer */
	ret = gst_pad_alloc_buffer (synth->src, GST_BUFFER_OFFSET_NONE, 1024*2*sizeof(float), GST_PAD_CAPS (synth->src), &out);
	if( ret != GST_FLOW_OK ){
		return ret;
	}
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
	gst_buffer_unref (data);
	//g_print ("pushing %llu\n", GST_BUFFER_TIMESTAMP (out));
  gst_buffer_set_caps (GST_BUFFER (out), synth->out_caps);
	gst_pad_push (synth->src, GST_BUFFER (out));
	gst_object_unref(synth);

	return GST_FLOW_OK;
}

static GstStateChangeReturn
gst_fluidsynth_change_state (GstElement * element, GstStateChange transition )
{
	GstFluidsynth *fluidsynth = GST_FLUIDSYNTH (element);
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
      fluidsynth->out_caps =
        gst_caps_copy (gst_pad_get_pad_template_caps (fluidsynth->src));
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			gst_fluidsynth_start (fluidsynth);
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;
	switch (transition) {
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			gst_fluidsynth_end (fluidsynth);
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
      gst_caps_unref (fluidsynth->out_caps);
			break;
		default:
			break;
	}

	return ret;
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

static gboolean
plugin_init (GstPlugin * plugin)
{
	if (!gst_element_register (plugin, "fluidsynth", GST_RANK_SECONDARY,
				GST_TYPE_FLUIDSYNTH))
		return FALSE;

	return TRUE;
}

GST_PLUGIN_DEFINE (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"gstfluidsynth",
		"midi to raw audio converter using fluidsynth", plugin_init, VERSION, 
		"LGPL",
		GST_PACKAGE_NAME,
		GST_PACKAGE_ORIGIN
		)
