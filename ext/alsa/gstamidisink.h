#ifndef __GST_AMIDISINK_H__
#define __GST_AMIDISINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_AMIDISINK \
  (gst_amidisink_get_type())
#define GST_AMIDISINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMIDISINK,GstaMIDISink))
#define GST_AMIDISINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMIDISINK,GstaMIDISinkClass))
#define GST_IS_AMIDISINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMIDISINK))
#define GST_IS_AMIDISINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMIDISINK))

typedef struct _GstaMIDISink      GstaMIDISink;
typedef struct _GstaMIDISinkClass GstaMIDISinkClass;

struct _GstaMIDISink
{
  GstBaseSink basesink;

  //GstPad *sinkpad;

  gint port, client, delay;
  gchar* device;

  gint a_port,a_queue;
  snd_seq_t * a_seq;
};

struct _GstaMIDISinkClass 
{
  GstBaseSinkClass parent_class;
};

GType gst_amidisink_get_type (void);

G_END_DECLS

#endif /* __GST_AMIDISINK_H__ */
