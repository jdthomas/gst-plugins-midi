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
#include <gst/base/gstadapter.h>
#include "gstmidibuffer.h"

#define GST_TYPE_SMFDEC (gst_smfdec_get_type())
#define GST_SMFDEC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SMFDEC,GstSmfdec))
#define GST_SMFDEC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SMFDEC,GstSmfdec))
#define GST_IS_SMFDEC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SMFDEC))
#define GST_IS_SMFDEC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SMFDEC))

typedef struct _GstSmfdec GstSmfdec;
typedef struct _GstSmfdecClass GstSmfdecClass;

typedef struct {
  guint32		fourcc;
  guint			length;
  const guint8 *	data;
  guint			available;
} Chunk;

struct _GstSmfdec {
  GstElement		element;

  GstPad *		sink;           /* Pad which data comes into us */
  GstPad *		src;            /* Pad which data goes out of us */

  GstAdapter *	  	adapter;
  Chunk			chunk;

  guint			format;		/* format as read in header + 1 */
  guint			tracks_missing;	/* tracks that haven't been parsed yet */

  guint8		status;		/* running status */
  
  guint			division;	/* division is read in the header */
  GstClockTime		tempo;		/* tempo as set by meta events in nanoseconds */
  guint			ticks;		/* number of ticks already processed */
  GstClockTime		start;		/* time at which we started */
  
  GstMidiBuffer *	buf;		/* current buffer */
  GstClockTime		buf_start;	/* time at which buffer sending starts */
  guint			buf_num;	/* numerator of buffer time */
  guint		  	buf_denom;	/* denominator of buffer time */
  guint			buf_sent;	/* number of buffers already sent */
};

struct _GstSmfdecClass {
  GstElementClass	parent_class;
};

static GstElementClass *parent_class = NULL;
static GType gst_smfdec_get_type (void);

static GstStaticPadTemplate gst_smfdec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/midi")
    );

static GstStaticPadTemplate gst_smfdec_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-gst-midi, bufferlength=(fraction) 1024/44100")
    );

static gboolean
get_next_chunk (GstAdapter *adapter, Chunk *chunk)
{
  const guint8 *data;
  guint length;
  
  data = gst_adapter_peek (adapter, 8);
  if (!data) return FALSE;
  length = GST_READ_UINT32_BE (data + 4);

  chunk->fourcc = GST_READ_UINT32_LE (data);
  chunk->length = length;
  gst_adapter_flush (adapter, 8);
  chunk->data = gst_adapter_peek (adapter, gst_adapter_available_fast (adapter));
  chunk->available = gst_adapter_available_fast (adapter);
  return TRUE;
}

static void
chunk_clear (Chunk *chunk)
{
  chunk->fourcc = 0;
  chunk->data = 0;
  chunk->length = 0;
  chunk->available = 0;
}

static void
chunk_skip (GstAdapter *adapter, Chunk *chunk, guint size)
{
  g_return_if_fail (size <= chunk->available);
  chunk->length -= size;
  chunk->available -= size;
  gst_adapter_flush (adapter, size);
  if (chunk->available) {
    chunk->data = gst_adapter_peek (adapter, chunk->available);
    g_assert (chunk->data);
  } else {
    chunk->data = NULL;
  }
  GST_LOG ("skipped %u bytes, %u of %u still available\n", size, 
      chunk->available, chunk->length);
}

static void
chunk_flush (GstAdapter *adapter, Chunk *chunk)
{
  g_return_if_fail (chunk->available == chunk->length);
  
  gst_adapter_flush (adapter, chunk->length);
  chunk_clear (chunk);
}

static gboolean
chunk_ensure (GstAdapter *adapter, Chunk *chunk, guint size)
{
  if (size == 0)
    size = MIN (gst_adapter_available (adapter), chunk->length);
  g_return_val_if_fail (size <= chunk->length, FALSE);

  chunk->data = gst_adapter_peek (adapter, size);
  if (chunk->data) {
    chunk->available = size;
    return TRUE;
  } else {
    chunk->available = 0;
    return FALSE;
  }
}
/******************************************************************************/
static void
gst_smfdec_set_tempo (GstSmfdec *dec, guint64 tempo)
{
  dec->start += dec->ticks * dec->tempo / dec->division;
  GST_DEBUG ("set tempo to %"G_GUINT64_FORMAT" and start offset to %"G_GUINT64_FORMAT" since %u ticks have been elapsed\n",
      tempo, dec->start, dec->ticks);
  dec->tempo = tempo * GST_USECOND;
  dec->ticks = 0;
}

static void
gst_smfdec_buffer_new (GstSmfdec *dec, GstClockTime time)
{
  g_assert (dec->buf == NULL);
  /**** FLOATING POINT PROBLEM HERE... dec->buf_* hasn't been inited yet ******/
  dec->buf_sent = (time - dec->buf_start) * dec->buf_denom / dec->buf_num / GST_SECOND;
  dec->buf = gst_midi_buffer_new (
      dec->buf_start + dec->buf_sent * GST_SECOND * dec->buf_num / dec->buf_denom,
      GST_SECOND * dec->buf_num / dec->buf_denom);
  dec->buf_sent++;
}

static void
gst_smfdec_buffer_push (GstSmfdec *dec)
{
  GstBuffer *buf;
  
  g_assert (dec->buf != NULL);
  
  buf = gst_midi_buffer_finish (dec->buf);
  dec->buf = NULL;
  gst_midi_buffer_dump (buf);
  //gst_pad_push (dec->src, GST_DATA (buf));
  gst_pad_push (dec->src, GST_BUFFER (buf));
}

static void
gst_smfdec_reset (GstSmfdec *dec)
{
  gst_adapter_clear (dec->adapter);
  chunk_clear (&dec->chunk);
  dec->format = 0;
  dec->start = 0;
  dec->status = 0;
  dec->division = 1;
  dec->ticks = 0;
  dec->buf_start = 0;
  dec->buf_num=1024;
  dec->buf_denom=44100;
  dec->buf_sent = 0;
  gst_smfdec_set_tempo (dec, 480000);
}

static void
gst_smfdec_buffer_append (GstSmfdec *dec, guint8 status, const guint8 *data, guint len)
{
  GstClockTime time = dec->start + dec->ticks * dec->tempo / dec->division;

  //g_print ("time is %"GST_TIME_FORMAT"\n", GST_TIME_ARGS (time));
  g_assert (!dec->buf || time >= dec->buf->timestamp);

  if (!dec->buf || time >= dec->buf->timestamp + dec->buf->duration) {
    if (dec->buf)
      gst_smfdec_buffer_push (dec);
    gst_smfdec_buffer_new (dec, time);
  }
  gst_midi_buffer_append_with_status (dec->buf, time, status, data, len);
}


static gboolean
gst_smfdec_src_setcaps (GstPad * pad, GstCaps * caps)
{
  GstSmfdec *dec = GST_SMFDEC (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  const GValue *value = gst_structure_get_value (structure, "bufferlength");

  g_assert (value);

  if (dec->buf_denom)
    dec->buf_start += dec->buf_sent * GST_SECOND * dec->buf_num / dec->buf_denom;
#if 0
  dec->buf_num = gst_value_get_fraction_numerator (value);
  dec->buf_denom = gst_value_get_fraction_denominator (value);
#endif
  dec->buf_sent = 0;
  gst_object_unref(dec);
  return TRUE;
}

static gboolean
gst_smfdec_meta_event (GstSmfdec *dec, const guint8 *data, guint len)
{
  guint check, skip, type;
  
  type = data[0];
  check = gst_midi_data_parse_varlen (data + 1, len, &skip);
  data += skip + 1; 
  len -= (skip + 1);
  g_assert (len == check);
  
  switch (type) {
    case 0x01:
      /* comment */
      g_print ("comment: %*s\n", len, data);
      break;
    case 0x02:
      /* copyright */
      g_print ("copyright: %*s\n", len, data);
      break;
    case 0x03:
      /* track name */
      g_print ("track name: %*s\n", len, data);
      break;
    case 0x51:
      /* tempo change */
      if (len != 3) {
	GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
	    ("tempo meta event not 3 bytes long, but %u", len));
	return FALSE;
      }
      g_print ("got a tempo change\n");
      gst_smfdec_set_tempo (dec, ((data[0] << 16) + (data[1] << 8) + data[2]));
      break;
    case 0x58:
      /* time signature */
      if (len != 4) {
	GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
	    ("time signature meta event not 4 bytes long, but %u", len));
	return FALSE;
      }
      g_print ("this is a %d/%d song\n", (int) data[0], 1 << data[1]);
      break;
    case 0x59:
      /* key signature */
      if (len != 2) {
	GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
	    ("key signature meta event not 2 bytes long, but %u", len));
	return FALSE;
      }
      g_print ("this song is keyed as %d %s %s\n", 
	  (int) data[0] - (data[0] > 127 ? 255 : 0), data[0] <= 7 ? "sharp" : "flat",
	  data[1] ? "minor" : "major");
      break;
    default:
      GST_WARNING ("meta event 0x%02X not yet handled", (int) type);
      break;
  }
  return TRUE;
}

static GstFlowReturn
gst_smfdec_chain (GstPad * pad, GstBuffer * data)
{
	GstSmfdec *dec = GST_SMFDEC (gst_pad_get_parent (pad));
	gint i, ticks;
	guint len, midi_len;

	/* we must be negotiated */
	g_assert ( dec != NULL );
	g_assert (dec->buf_denom > 0);

	gst_adapter_push (dec->adapter, GST_BUFFER (data));
	if (dec->chunk.fourcc) {
		chunk_ensure (dec->adapter, &dec->chunk, 0);
	}

	while (dec->chunk.fourcc || get_next_chunk (dec->adapter, &dec->chunk)) {
		switch (dec->chunk.fourcc) {
			case GST_MAKE_FOURCC ('M', 'T', 'h', 'd'):
				if (dec->format) {
					GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL), 
							("got a header chunk while already initialized"));
					goto error;
				}
				if (dec->chunk.length != 6) {
					GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL), 
							("header chunk %d bytes long, not 6", (int) dec->chunk.length));
					goto error;
				}
				if (!chunk_ensure (dec->adapter, &dec->chunk, 6))
					goto out;
				/* we add one, so we can use 0 for uninitialized */
				dec->format = GST_READ_UINT16_BE (dec->chunk.data) + 1;
				if (dec->format > 2)
					g_warning ("I have no clue if midi format %u works", dec->format - 1);
				dec->tracks_missing = GST_READ_UINT16_BE (dec->chunk.data + 2);
				//g_assert (dec->tracks_missing == 1);
				/* ignore the number of track atoms */
				i = (gint16) GST_READ_UINT16_BE (dec->chunk.data + 4);
				if (i < 0) {
					GST_ELEMENT_ERROR (dec, STREAM, NOT_IMPLEMENTED, (NULL), 
							("can't handle smpte timing"));
					goto error;
				} else {
					dec->start = 0;
					dec->division = i;
				}
				chunk_flush (dec->adapter, &dec->chunk);
				break;
			case GST_MAKE_FOURCC ('M', 'T', 'r', 'k'):
				if (dec->format == 0) {
					GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL), 
							("got a track chunk while not yet initialized"));
					goto error;
				}
				/* figure out if we have enough data */
				ticks = gst_midi_data_parse_varlen (dec->chunk.data, dec->chunk.available, &len);
				if (ticks == -1) goto out;
				if (ticks == -2) {
					GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL), 
							("invalid delta time value"));
					goto error;
				}
				midi_len = gst_midi_data_get_length (dec->chunk.data + len, 
						dec->chunk.available - len, dec->status);
				if (midi_len == 0) goto out;
				/* we have enough data, process */
				dec->ticks += ticks;
				//g_print ("got %u ticks, now %u\n", ticks, dec->ticks);
				if (dec->chunk.data[len] & 0x80) {
					dec->status = dec->chunk.data[len];
					len++;
					midi_len--;
				}
				if (dec->status == 0xFF) {
					if (!gst_smfdec_meta_event (dec, dec->chunk.data + len, midi_len))
						goto error;
				} else {
					gst_smfdec_buffer_append (dec, dec->status, dec->chunk.data + len, midi_len);
				}
				chunk_skip (dec->adapter, &dec->chunk, len + midi_len);
				if (dec->chunk.length == 0)
					chunk_clear (&dec->chunk);
				break;
			default:
				goto out;
		}
	}
out:
	gst_object_unref(dec);
	return GST_FLOW_OK;;

error:
	gst_smfdec_reset (dec);
	return GST_FLOW_ERROR;
}

static GstStateChangeReturn
gst_smfdec_change_state (GstElement * element, GstStateChange transition)
{
  GstSmfdec *smfdec = GST_SMFDEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_smfdec_reset (smfdec);
      break;
    default:
      break;
  }

  if (parent_class->change_state)
    return parent_class->change_state (element,transition);

  return GST_STATE_CHANGE_SUCCESS;
}

static void
gst_smfdec_dispose (GObject *object)
{
  GstSmfdec *dec = GST_SMFDEC (object);

  g_object_unref (dec->adapter);
  dec->adapter = NULL;
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_smfdec_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  parent_class = g_type_class_peek_parent (g_class);

  gstelement_class->change_state = gst_smfdec_change_state;

  object_class->dispose = gst_smfdec_dispose;
}

static void
gst_smfdec_init (GstSmfdec * smfdec)
{
	smfdec->sink = gst_pad_new_from_template (
			gst_static_pad_template_get(&gst_smfdec_sink_template), "sink" );
	
	gst_element_add_pad (GST_ELEMENT (smfdec), smfdec->sink);

	gst_pad_set_chain_function (smfdec->sink, GST_DEBUG_FUNCPTR(gst_smfdec_chain) );

	smfdec->src = gst_pad_new_from_template (
			gst_static_pad_template_get ( &gst_smfdec_src_template), "src");

	gst_pad_set_setcaps_function ( smfdec->src, GST_DEBUG_FUNCPTR(gst_smfdec_src_setcaps));

	gst_element_add_pad (GST_ELEMENT (smfdec), smfdec->src);

	smfdec->adapter = gst_adapter_new ();
}

static void
gst_smfdec_base_init (gpointer g_class)
{
  static GstElementDetails gst_smfdec_details =
  GST_ELEMENT_DETAILS ("standard midi file to midi converter",
      "Demuxer/Decoder/Audio",
      "Convert a midi file to GStreamer midi representation",
      "Benjamin Otte <otte@gnome.org>");

  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_smfdec_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_smfdec_src_template));
  gst_element_class_set_details (element_class, &gst_smfdec_details);

}

static GType
gst_smfdec_get_type (void)
{
  static GType smfdec_type = 0;

  if (!smfdec_type) {
    static const GTypeInfo smfdec_info = {
      sizeof (GstSmfdecClass),
      gst_smfdec_base_init,
      NULL,
      (GClassInitFunc) gst_smfdec_class_init,
      NULL,
      NULL,
      sizeof (GstSmfdec),
      0,
      (GInstanceInitFunc) gst_smfdec_init,
    };

    smfdec_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstSmfdec", &smfdec_info,
        0);
  }
  return smfdec_type;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
#if 0
  if (!gst_plugin_load ("gstbytestream"))
    return FALSE;
#endif
  return gst_element_register (plugin, "smfdec", GST_RANK_SECONDARY,
	GST_TYPE_SMFDEC);
}

GST_PLUGIN_DEFINE (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "gstmidi",
      "various midi plugins", plugin_init, VERSION, "LGPL", 
      GST_PACKAGE_NAME,
      GST_PACKAGE_ORIGIN
      )
