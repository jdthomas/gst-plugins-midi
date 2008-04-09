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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#define GST_MIDI_EXTRA_CHECKS
#include "gstmidibuffer.h"

#define DEFAULT_BUFFER_SIZE (16)

GstMidiBuffer *	
gst_midi_buffer_new (GstClockTime timestamp, GstClockTime duration)
{
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (duration), NULL);

  GstBuffer *buf = gst_buffer_new_and_alloc (DEFAULT_BUFFER_SIZE);
  buf->size = 0;
  buf->timestamp = timestamp;
  buf->duration = duration;
  
  return buf;
}

/* FIXME: need len here or should this be parsed automagically? */
void
gst_midi_buffer_append (GstMidiBuffer *buf, GstClockTime time, 
    const guint8 *data, guint len)
{
  g_return_if_fail (GST_IS_BUFFER (buf));
  g_return_if_fail (data != NULL);
  g_return_if_fail (data[0] & 0x80);
  g_return_if_fail (len > 1);
  g_return_if_fail (buf->timestamp <= time);
  g_return_if_fail (time < buf->timestamp + buf->duration);

  gst_midi_buffer_append_with_status (buf, time, data[0], data + 1, len - 1);
}

void
gst_midi_buffer_append_with_status (GstMidiBuffer *buf, GstClockTime time,
    guint8 status, const guint8 *data, guint len)
{
  g_return_if_fail (GST_IS_BUFFER (buf));
  g_return_if_fail (status & 0x80);
  g_return_if_fail (data != NULL);
  g_return_if_fail (len > 0);
  g_return_if_fail (buf->timestamp <= time);
  g_return_if_fail (time < buf->timestamp + buf->duration);

  gint oldsize=buf->size;
  buf->size = buf->size + len + 9;
  if ( oldsize )
	  buf->data = g_realloc (buf->data,buf->size);
  else
	  buf->data = g_malloc (buf->size);

  GST_WRITE_UINT64_BE (buf->data + oldsize, time);
  buf->data[oldsize + 8] = status;
  memcpy (buf->data + oldsize + 9, data, len);
}

GstBuffer *
gst_midi_buffer_finish (GstMidiBuffer *buf)
{
  return buf;
}

/**
 * gst_midi_data_parse_varlen:
 * @data: data to parse from
 * @maxlen: maximum length of data
 * @len: value will be set to the length of the variable length that has been
 *	 parse, unless it's NULL, then it's ignored
 *
 * Parse a variable length midi value.
 *
 * Returns: The parsed value, -1 if not enough data was available, -2 when the
 *          varlen value was invalid
 **/
gint
gst_midi_data_parse_varlen (const guint8 *data, guint maxlen, guint *len)
{
  gint value;
  
  if (maxlen == 0) return -1;
  
  value = data[0] & 0x7f;
  if (data[0] & 0x80) {
    if (maxlen < 2) return -1;
    value = (value << 7) + (data[1] & 0x7f);
    if (data[1] & 0x80) {
      if (maxlen < 3) return -1;
      value = (value << 7) + (data[2] & 0x7f);
      if (data[2] & 0x80) {
	if (maxlen < 4) return -1;
	if (data[3] & 0x80) return -2;
	value = (value << 7) + (data[3] & 0x7f);
	if (len)
	  *len = 4;
      } else if (len) {
	*len = 3;
      }
    } else if (len) {
      *len = 2;
    }
  } else if (len) {
    *len = 1;
  }
  return value;
}

/**
 * gst_midi_data_get_length:
 * @data: data to parse
 * @maxlen: maximum length of data
 * @status: running status of midi stream or 0 if none
 *
 * Parses the given data expecting a raw midi stream. If it is a raw midi
 * stream, and the next midi event is contained in the data, the length of 
 * the next command will be returned. On failure, it returns 0.
 * If @status is 0, the midi data must contain its status. Otherwise
 * @status will be used if no status exists.
 *
 * Returns: Length of the data or 0 on failure
 **/
guint
gst_midi_data_get_length (const guint8 *data, guint maxlen, guint8 status)
{
  guint len, varlen_len;
  gint data_len;
  
  g_return_val_if_fail (data != NULL, 0);
  g_return_val_if_fail (status == 0 || (status & 0x80), 0);
  if (maxlen == 0) return 0;
  
  if (data[0] & 0x80) {
    status = data[0];
    len = 1;
    data++;
    maxlen--;
  } else if (status > 0) {
    len = 0;
  } else {
    /* no status present, error */
    return 0;
  }
  switch (status >> 4) {
    case 8:
    case 9:
    case 10:
    case 11:
    case 14:
      if (maxlen < 2)
	return 0;
      len += 2;
      break;
    case 12:
    case 13:
      if (maxlen < 1)
	return 0;
      len += 1;
      break;
    case 15:
      if (maxlen < 2)
	return 0;
      data++;
      maxlen--;
      data_len = gst_midi_data_parse_varlen (data, maxlen, &varlen_len);
      if (maxlen < (guint) data_len + varlen_len)
	return 0;
      len += (guint) data_len + varlen_len + 1;
      break;
    default:
      /* should be checked above */
      g_assert_not_reached();
      break;
  }
  return len;
}

void
gst_midi_iter_init (GstMidiIter *iter, GstBuffer *buf)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GST_IS_BUFFER (buf));

  iter->buf = buf;
  iter->data = buf->data;
}

GstClockTime
gst_midi_iter_get_time (GstMidiIter *iter)
{
  g_return_val_if_fail (iter != NULL, 0);

  return GST_READ_UINT64_BE (iter->data);
}

const guint8 *
gst_midi_iter_get_event (GstMidiIter *iter)
{
  g_return_val_if_fail (iter != NULL, NULL);

  return iter->data + 8;
}

gboolean
gst_midi_iter_next (GstMidiIter *iter)
{
  guint len;

  g_return_val_if_fail (iter != NULL, FALSE);

  iter->data += 8;
  len = gst_midi_data_get_length (iter->data, 
      iter->buf->size +iter->buf->data - iter->data, 0);
  if (len == 0) {
    g_warning ("invaalid data in midi buffer");
    return FALSE;
  }
  iter->data += len;
  return iter->data < iter->buf->size +iter->buf->data;
}

GstMidiEventType
gst_midi_event_get_type (const GstMidiEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event[0] >= 0x80, 0);
  
  return event[0] >> 4;
}

gint
gst_midi_event_get_channel (const GstMidiEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event[0] >= 0x80 && event[0] < 0xF0, 0);

  return event[0] & 0xF;
}

gint
gst_midi_event_get_byte1 (const GstMidiEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event[0] >= 0x80 && event[0] < 0xF0, 0);

  return event[1];
}

gint
gst_midi_event_get_byte2 (const GstMidiEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail ((event[0] >= 0x80 && event[0] < 0xC0) ||
      (event[0] >= 0xE0 && event[0] < 0xEF), 0);

  return event[2];
}

guint
gst_midi_event_get_length (const GstMidiEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);

  switch (gst_midi_event_get_type (event)) {
    case 8:
    case 9:
    case 10:
    case 11:
    case 14:
      return 3;
    case 12:
    case 13:
      return 2;
    case 15:
    default:
      g_return_val_if_reached (0);
  }
}

void
gst_midi_event_dump (const GstMidiEvent *data)
{
  switch (gst_midi_event_get_type (data)) {
    case GST_MIDI_NOTE_ON:
      g_print ("note on       %2d %3d %3d\n", (int) data[0] & 0xF, (int) data[1], (int) data[2]);
      break;
    case GST_MIDI_NOTE_OFF:
      g_print ("note off      %2d %3d %3d\n", (int) data[0] & 0xF, (int) data[1], (int) data[2]);
      break;
    case GST_MIDI_KEY_PRESSURE:
      g_print ("key pressure  %2d %3d %3d\n", (int) data[0] & 0xF, (int) data[1], (int) data[2]);
      break;
    case GST_MIDI_CONTROL_CHANGE:
      g_print ("ctrl change   %2d %3d %3d\n", (int) data[0] & 0xF, (int) data[1], (int) data[2]);
      break;
    case GST_MIDI_PROGRAM_CHANGE:
      g_print ("prog change   %2d %3d\n", (int) data[0] & 0xF, (int) data[1]);
      break;
    case GST_MIDI_CHANNEL_PRESSURE:
      g_print ("channel press %2d %3d\n", (int) data[0] & 0xF, (int) data[1]);
      break;
    case GST_MIDI_PITCH_BEND:
      g_print ("pitch bend    %2d %3d %3d\n", (int) data[0] & 0xF, (int) data[1], (int) data[2]);
      break;
    case GST_MIDI_SYSTEM:
      g_print ("system\n");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

void
gst_midi_buffer_dump (GstBuffer *buf)
{
  GstMidiIter iter;
  GstClockTime time;

  gst_midi_iter_init (&iter, buf);
  do {
    time = gst_midi_iter_get_time (&iter);
    g_print ("%"GST_TIME_FORMAT"   ", GST_TIME_ARGS (time));
    gst_midi_event_dump (gst_midi_iter_get_event (&iter));
  } while (gst_midi_iter_next (&iter));
}

