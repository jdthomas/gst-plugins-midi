#ifndef __GST_FLUIDSYNTH_H__
#define __GST_FLUIDSYNTH_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_FLUIDSYNTH (gst_fluidsynth_get_type())
#define GST_FLUIDSYNTH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FLUIDSYNTH,GstFluidsynth))
#define GST_FLUIDSYNTH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FLUIDSYNTH,GstFluidsynth))
#define GST_IS_FLUIDSYNTH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FLUIDSYNTH))
#define GST_IS_FLUIDSYNTH_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FLUIDSYNTH))

typedef struct _GstFluidsynth GstFluidsynth;
typedef struct _GstFluidsynthClass GstFluidsynthClass;

struct _GstFluidsynth 
{
  GstElement		element;

  GstPad *		sink;
  GstPad *		src;
  
  GstCaps *out_caps;


  fluid_synth_t *	synth;
  gchar *		soundfont;
  GstClockTime		expected;
};

struct _GstFluidsynthClass {
  GstElementClass	parent_class;
};

//static GstElementClass *parent_class = NULL;

GType gst_fluidsynth_get_type (void);

G_END_DECLS

#endif /* __GST_AMIDISINK_H__ */

