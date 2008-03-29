/* 
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

#include <gst/gst.h>

#ifndef __GST_MIDIBUFFER_H__
#define __GST_MIDIBUFFER_H__

G_BEGIN_DECLS


typedef GstBuffer GstMidiBuffer;
typedef guint8 GstMidiEvent;
typedef struct _GstMidiIter GstMidiIter;

struct _GstMidiIter {
  GstBuffer *		buf;
  const guint8 *	data;
};

typedef enum {
  GST_MIDI_INVALID = 0x0,
  GST_MIDI_NOTE_OFF = 0x8,
  GST_MIDI_NOTE_ON = 0x9,
  GST_MIDI_KEY_PRESSURE = 0xA,
  GST_MIDI_CONTROL_CHANGE = 0xB,
  GST_MIDI_PROGRAM_CHANGE = 0xC,
  GST_MIDI_CHANNEL_PRESSURE = 0xD,
  GST_MIDI_PITCH_BEND = 0xE,
  GST_MIDI_SYSTEM = 0xF
} GstMidiEventType;


/* general support functions */
gint		gst_midi_data_parse_varlen	(const guint8 *		data,
						 guint			maxlen,
						 guint *		len);
guint		gst_midi_data_get_length      	(const guint8 *		data,
						 guint			maxlen,
						 guint8			status);

/* writing midi events into a buffer */
GstMidiBuffer *	gst_midi_buffer_new		(GstClockTime		timestamp,
						 GstClockTime		duration);
/* FIXME: need len here or should this be parsed automagically? */
void		gst_midi_buffer_append		(GstMidiBuffer *	buf,
						 GstClockTime		time,
						 const guint8 *		data,
						 guint			len);
void		gst_midi_buffer_append_with_status
						(GstMidiBuffer *	buf,
						 GstClockTime		time,
						 guint8			status,
						 const guint8 *		data,
						 guint			len);
GstBuffer *	gst_midi_buffer_finish		(GstMidiBuffer *	buf);

/* reading midi events from a buffer */
void		gst_midi_iter_init		(GstMidiIter *		iter,
						 GstBuffer *		buf);
GstClockTime	gst_midi_iter_get_time		(GstMidiIter *		iter);
const GstMidiEvent *
		gst_midi_iter_get_event		(GstMidiIter *		iter);
gboolean	gst_midi_iter_next		(GstMidiIter *		iter);

/* midi events */
guint		gst_midi_event_get_length	(const GstMidiEvent *	event);
#ifdef GST_MIDI_EXTRA_CHECKS
GstMidiEventType
		gst_midi_event_get_type		(const GstMidiEvent *	event);
gint		gst_midi_event_get_channel	(const GstMidiEvent *	event);
gint		gst_midi_event_get_byte1	(const GstMidiEvent *	event);
gint		gst_midi_event_get_byte2	(const GstMidiEvent *	event);
#else
#define gst_midi_event_get_type(event) (event[0] >> 4)
#define gst_midi_event_get_channel(event) ((gint) (event[0] & 0xF))
#define gst_midi_event_get_byte1(event) ((gint) (event[1]))
#define gst_midi_event_get_byte2(event) ((gint) (event[2]))
#endif

/* debugging */
void		gst_midi_event_dump		(const GstMidiEvent *	data);
void		gst_midi_buffer_dump		(GstBuffer *		buf);


G_END_DECLS

#endif /* __GST_MIDIBUFFER_H__ */
