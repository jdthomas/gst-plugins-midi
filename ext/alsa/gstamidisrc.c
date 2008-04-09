/**
 * SECTION:element-plugin
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch -v -m audiotestsrc ! plugin ! fakesink silent=TRUE
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstamidisrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_amidisrc_debug);
#define GST_CAT_DEFAULT gst_amidisrc_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_SILENT
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-gst-midi, bufferlength=(fraction) 1024/44100")
    );

GST_BOILERPLATE (GstaMIDISrc, gst_amidisrc, GstPushSrc,
    GST_TYPE_PUSH_SRC);

static void gst_amidisrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_amidisrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_amidisrc_is_seekable (GstBaseSrc * push_src);
static gboolean gst_amidisrc_start (GstBaseSrc * bsrc);
static gboolean gst_amidisrc_stop (GstBaseSrc * bsrc);
static GstFlowReturn gst_amidisrc_create (GstPushSrc * psrc, GstBuffer ** outbuf);
static GstStateChangeReturn gst_amidisrc_change_state (GstElement * element, GstStateChange transition );


static void
gst_amidisrc_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
	  "MIDI ALSA Source",
	  "Src/Audio",
	  "Collects MIDI events in from an Alsa MIDI Port.",
	  "Jeff Thomas <jeffdthomas@gmail.com>"
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_set_details (element_class, &element_details);
}

/* initialize the plugin's class */
static void
gst_amidisrc_class_init (GstaMIDISrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstPushSrcClass *gstpushsrc_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;


  gobject_class->set_property = gst_amidisrc_set_property;
  gobject_class->get_property = gst_amidisrc_get_property;

  g_object_class_install_property (gobject_class, ARG_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  
  gstelement_class->change_state = gst_amidisrc_change_state;

  gstpushsrc_class->create = gst_amidisrc_create;

  gstbasesrc_class->start = gst_amidisrc_start;
  gstbasesrc_class->stop = gst_amidisrc_stop;
  gstbasesrc_class->is_seekable = gst_amidisrc_is_seekable;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_amidisrc_init (GstaMIDISrc * filter,
    GstaMIDISrcClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (filter);

  filter->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
}

static void
gst_amidisrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstaMIDISrc *filter = GST_AMIDISRC (object);

  switch (prop_id) {
    case ARG_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_amidisrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstaMIDISrc *filter = GST_AMIDISRC (object);

  switch (prop_id) {
    case ARG_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */
static gboolean gst_amidisrc_start (GstBaseSrc * bsrc)
{
	// TODO: start capturing midi events.
	return TRUE;
}
static gboolean gst_amidisrc_stop (GstBaseSrc * bsrc)
{
	// TODO: stop capturing midi events
	return TRUE;
}

static gboolean
gst_amidisrc_is_seekable (GstBaseSrc * push_src)
{
	return FALSE;
}


static GstFlowReturn gst_amidisrc_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
	//GstaMIDISrc *src = GST_AMIDISRC (psrc);

	/**
	*outbuf = gst_buffer_new ();
	GST_BUFFER_SIZE (*outbuf) = buffer->len;
	GST_BUFFER_MALLOCDATA (*outbuf) = buffer->data;
	GST_BUFFER_DATA (*outbuf) = GST_BUFFER_MALLOCDATA (*outbuf);
	GST_BUFFER_OFFSET (*outbuf) = src->read_offset;
	GST_BUFFER_OFFSET_END (*outbuf) =
	      src->read_offset + GST_BUFFER_SIZE (*outbuf);
	**/


	//TODO:
	g_print("***Here: %s:%d\n",__FILE__,__LINE__);
#if 0
	snd_seq_event_t *a_event=NULL;
	snd_seq_event_input(src->a_seq,&a_event);
	if(a_event){
		// convert to audio/x-gst-midi format and push on psrc.
	}
#endif
	return GST_FLOW_OK;
}
static GstStateChangeReturn
gst_amidisrc_change_state (GstElement * element, GstStateChange transition )
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  //GstaMIDISrc *src = GST_AMIDISRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
		 // TODO: Open our listen alsa ports
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
		return GST_STATE_CHANGE_NO_PREROLL;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
		 // TODO: close our listening alsa ports
      break;
	 case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		return GST_STATE_CHANGE_NO_PREROLL;
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
  GST_DEBUG_CATEGORY_INIT (gst_amidisrc_debug, "amidisrc",
      0, "Alsa MIDI Src");

  return gst_element_register (plugin, "amidisrc",
      GST_RANK_NONE, GST_TYPE_AMIDISRC);
}

/* this is the structure that gstreamer looks for to register plugins
 *
 * exchange the strings 'plugin' and 'Template plugin' with you plugin name and
 * description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"gstamidisrc",
		"MIDI from alsa port",
		plugin_init, 
		VERSION, 
		"LGPL", 
		GST_PACKAGE_NAME,
		GST_PACKAGE_ORIGIN
		)
