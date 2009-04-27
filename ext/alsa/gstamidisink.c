/**
 * SECTION:element-plugin
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch -v -m filesrc location=smf_file.mid ! smfdec ! amidisink device=hw client=128 port=0
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstmidibuffer.h"
#include <alsa/asoundlib.h>

#include <gst/gst.h>

#include "gstamidisink.h"

GST_DEBUG_CATEGORY_STATIC (gst_amidisink_debug);
#define GST_CAT_DEFAULT gst_amidisink_debug

/* Filter signals and args */
enum
{
  ARG_0,
  ARG_PORT,
  ARG_CLIENT,
  ARG_DEVICE,
  ARG_DELAY
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-gst-midi, bufferlength=(fraction) 1024/44100")
    );

GST_BOILERPLATE (GstaMIDISink, gst_amidisink, GstElement, GST_TYPE_BASE_SINK);

static void gst_amidisink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_amidisink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_amidisink_render (GstBaseSink * bsink, GstBuffer * buf);
static GstStateChangeReturn gst_aplaymidi_change_state (GstElement * element, GstStateChange transition );

static void
gst_amidisink_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
	  "MIDI ALSA sink",
	  "Sink/Audio",
	  "Sends MIDI events out on an Alsa MIDI Port.",
	  "Jeff Thomas <jeffdthomas@gmail.com>"
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}

/* initialize the plugin's class */
static void
gst_amidisink_class_init (GstaMIDISinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *b_class;


  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  b_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_amidisink_set_property;
  gobject_class->get_property = gst_amidisink_get_property;
  gstelement_class->change_state = gst_aplaymidi_change_state;

  g_object_class_install_property (gobject_class, ARG_PORT,
      g_param_spec_int ("port", "Port", "Alsa MIDI Port to connect to.",
        G_MININT, G_MAXINT,0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_CLIENT,
      g_param_spec_int ("client", "Client", "Alsa MIDI Client to connect to.",
        G_MININT, G_MAXINT,128, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_DEVICE,
      g_param_spec_string ("device", "Device", "Alsa MIDI Device to connect to.",
        "default", G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_DELAY,
      g_param_spec_int ("delay", "Delay", "Delay the MIDI by this amount.",
        G_MININT, G_MAXINT,0, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_amidisink_init (GstaMIDISink * sink,
    GstaMIDISinkClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (sink);
  GstBaseSinkClass *gstbasesink_class;

  sink->device = g_strdup("default");
  sink->client = 128;
  sink->port   = 0;

  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_amidisink_render);
}

static void
gst_amidisink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstaMIDISink *sink = GST_AMIDISINK (object);
  switch (prop_id) {
    case ARG_PORT:
      sink->port = g_value_get_int (value);
      break;
    case ARG_CLIENT:
      sink->client = g_value_get_int (value);
      break;
    case ARG_DEVICE:
      if( sink->device ) g_free(sink->device);
      sink->device = g_strdup(g_value_get_string (value));
      break;
    case ARG_DELAY:
      sink->delay = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_amidisink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstaMIDISink *sink= GST_AMIDISINK (object);

  switch (prop_id) {
    case ARG_PORT:
      g_value_set_int (value, sink->port);
      break;
    case ARG_CLIENT:
      g_value_set_int (value, sink->client);
      break;
    case ARG_DEVICE:
      g_value_set_string (value, sink->device);
      break;
    case ARG_DELAY:
      g_value_set_int (value, sink->delay);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_amidisink_render (GstBaseSink * bsink, GstBuffer * buf)
{
  GstaMIDISink *sink;
  sink = GST_AMIDISINK ( bsink );

  GstMidiIter iter;
  GstBuffer *in = GST_BUFFER (buf);
  gboolean events_left = TRUE;
  GstClockTime last;
  const guint8* event;
  gst_midi_iter_init (&iter, in);
  snd_seq_event_t *a_event;

  last = in->timestamp + in->duration;

  // Covert audio/x-gst-midi to alsa's midi structures
  while (events_left && gst_midi_iter_get_time (&iter) < last) {
    event = gst_midi_iter_get_event (&iter);
    a_event=NULL;

    switch (gst_midi_event_get_type (event)) {
      case GST_MIDI_NOTE_ON:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_NOTEON;
        a_event->data.note.channel     = gst_midi_event_get_channel(event);
        a_event->data.note.note        = gst_midi_event_get_byte1(event);
        a_event->data.note.velocity    = gst_midi_event_get_byte2 (event);
        a_event->data.note.off_velocity= 0;
        break;
      case GST_MIDI_NOTE_OFF:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_NOTEOFF;
        a_event->data.note.channel     = gst_midi_event_get_channel(event);
        a_event->data.note.note        = gst_midi_event_get_byte1(event);
        a_event->data.note.velocity    = gst_midi_event_get_byte2 (event);
        a_event->data.note.off_velocity= 0;
        break;
      case GST_MIDI_PITCH_BEND:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_PITCHBEND;
        a_event->data.control.channel = gst_midi_event_get_channel(event);
        a_event->data.control.value = (gst_midi_event_get_byte1(event) | (gst_midi_event_get_byte2(event)<< 7)) - 0x2000;
        break;
      case GST_MIDI_CONTROL_CHANGE:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_CONTROLLER;
        a_event->data.control.channel = gst_midi_event_get_channel(event);
        a_event->data.control.param = gst_midi_event_get_byte1(event);
        a_event->data.control.value = gst_midi_event_get_byte2 (event);
        break;
      case GST_MIDI_PROGRAM_CHANGE:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_PGMCHANGE;
        a_event->data.control.channel = gst_midi_event_get_channel(event);
        a_event->data.control.value = gst_midi_event_get_byte1 (event);
        break;
      case GST_MIDI_SYSTEM:
        a_event = g_malloc(sizeof(snd_seq_event_t));
        if ( !a_event ) goto err;
        snd_seq_ev_clear(a_event);
        snd_seq_ev_set_fixed(a_event);
        a_event->type = SND_SEQ_EVENT_SYSEX;
        //snd_seq_ev_set_variable(event, Len, Buff);
        break;
      default:
        gst_midi_event_dump (event);
        break;
    }
    if( a_event ) {
      // Push a_event to the alsa subsystem
      // TODO: Add/subtract delay as specified by property: delay=
      snd_seq_ev_set_direct(a_event);
      snd_seq_ev_set_source(a_event, sink->a_port);
      snd_seq_ev_set_subs(a_event);
      snd_seq_event_output_direct(sink->a_seq, a_event);
      snd_seq_drain_output(sink->a_seq);
#if 0
      snd_seq_real_time_t CurTime={0,0};//=(gst_midi_iter_get_time (&iter))
      snd_seq_ev_set_source(a_event, sink->a_port);
      snd_seq_ev_set_subs(a_event);
      snd_seq_ev_schedule_real(a_event, sink->a_queue, 1, &CurTime );
      snd_seq_event_output(sink->a_seq, a_event);
      snd_seq_drain_output(sink->a_seq);
#endif

      // Done with a_event, free it.
      g_free(a_event);
      a_event=NULL;
    }
    events_left = gst_midi_iter_next (&iter);
  }
  return GST_FLOW_OK;
err:
  gst_midi_event_dump (event);
  return GST_FLOW_ERROR;
}


static GstStateChangeReturn
gst_aplaymidi_change_state (GstElement * element, GstStateChange transition )
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstaMIDISink *sink = GST_AMIDISINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      // connect to midi port
      if ((snd_seq_open(&sink->a_seq, sink->device, SND_SEQ_OPEN_OUTPUT, 0)) < 0)
        return GST_STATE_CHANGE_FAILURE;
      if( sink->a_seq == NULL )
        return GST_STATE_CHANGE_FAILURE;
      snd_seq_set_client_name(sink->a_seq, "gstreamer");
      if ((sink->a_port = snd_seq_create_simple_port(sink->a_seq, "Out",
              SND_SEQ_PORT_CAP_READ |
              SND_SEQ_PORT_CAP_SUBS_READ,
              SND_SEQ_PORT_TYPE_MIDI_GENERIC)) < 0)
      {
        return GST_STATE_CHANGE_FAILURE;
      }
      sink->a_queue = snd_seq_alloc_queue(sink->a_seq);
      if( sink->a_queue < 0 )
        return GST_STATE_CHANGE_FAILURE;
      if( snd_seq_connect_to(sink->a_seq, sink->a_port, sink->client, sink->port) < 0 )
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      // disconnect from midi port
      if( sink->a_queue >= 0 ){
        if( snd_seq_free_queue( sink->a_seq, sink->a_queue ) < 0 )
          return GST_STATE_CHANGE_FAILURE;
      }
      if( sink->a_port >= 0 && sink->a_seq != NULL){
        if( snd_seq_delete_simple_port(sink->a_seq,sink->a_port) < 0 ){
          return GST_STATE_CHANGE_FAILURE;
        }
      }
      if( snd_seq_close( sink->a_seq ) < 0 )
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  /* exchange the strings 'plugin' and 'Template plugin' with your
   * plugin name and description */
  GST_DEBUG_CATEGORY_INIT (gst_amidisink_debug, "amidisink",
      0, "Alsa MIDI Sink");

  return gst_element_register (plugin, "amidisink",
      GST_RANK_NONE, GST_TYPE_AMIDISINK);
}

/* this is the structure that gstreamer looks for to register plugins
 *
 * exchange the strings 'plugin' and 'Template plugin' with you plugin name and
 * description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"gstamidisink",
		"MIDI to alsa port",
		plugin_init, 
		VERSION, 
		"LGPL", 
		GST_PACKAGE_NAME,
		GST_PACKAGE_ORIGIN
		)
