#ifndef __GST_AMIDISRC_H__
#define __GST_AMIDISRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_AMIDISRC \
  (gst_amidisrc_get_type())
#define GST_AMIDISRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMIDISRC,GstaMIDISrc))
#define GST_AMIDISRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMIDISRC,GstaMIDISrcClass))
#define GST_IS_AMIDISRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMIDISRC))
#define GST_IS_AMIDISRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMIDISRC))

typedef struct _GstaMIDISrc      GstaMIDISrc;
typedef struct _GstaMIDISrcClass GstaMIDISrcClass;

struct _GstaMIDISrc
{
  GstPushSrc element;

  gint port, client;
  gchar* device;
  gboolean silent;

  gint a_port,a_queue;
  snd_seq_t * a_seq;
};

struct _GstaMIDISrcClass 
{
  GstPushSrcClass parent_class;
};

GType gst_amidisrc_get_type (void);

G_END_DECLS

#endif /* __GST_AMIDISRC_H__ */
