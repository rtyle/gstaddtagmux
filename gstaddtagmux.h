/* GStreamer
 * Copyright (C) 2015 FIXME <fixme@example.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_ADD_TAG_MUX_H_
#define _GST_ADD_TAG_MUX_H_

#include <gst/gst.h>
#include <gst/gstpad.h>

G_BEGIN_DECLS

#define GST_TYPE_ADD_TAG_MUX_PAD	(gst_add_tag_mux_pad_get_type())
#define GST_ADD_TAG_MUX_PAD(o)		(G_TYPE_CHECK_INSTANCE_CAST((o),	GST_TYPE_ADD_TAG_MUX_PAD,GstAddTagMuxPad))
#define GST_ADD_TAG_MUX_PAD_CLASS(c)	(G_TYPE_CHECK_CLASS_CAST((c),		GST_TYPE_ADD_TAG_MUX_PAD,GstAddTagMuxPadClass))
#define GST_IS_ADD_TAG_MUX_PAD(o)	(G_TYPE_CHECK_INSTANCE_TYPE((o),	GST_TYPE_ADD_TAG_MUX_PAD))
#define GST_IS_ADD_TAG_MUX_PAD_CLASS(c)	(G_TYPE_CHECK_CLASS_TYPE((c),		GST_TYPE_ADD_TAG_MUX_PAD))

typedef struct _GstAddTagMuxPad	GstAddTagMuxPad;
typedef struct _GstAddTagMuxPadClass	GstAddTagMuxPadClass;

struct _GstAddTagMuxPad {
    GstPad		pad;
};

struct _GstAddTagMuxPadClass {
    GstPadClass		pad_class;
};

GType gst_add_tag_mux_get_type(void);

#define GST_TYPE_ADD_TAG_MUX		(gst_add_tag_mux_get_type())
#define GST_ADD_TAG_MUX(o)		(G_TYPE_CHECK_INSTANCE_CAST((o),	GST_TYPE_ADD_TAG_MUX,GstAddTagMux))
#define GST_ADD_TAG_MUX_CLASS(c)	(G_TYPE_CHECK_CLASS_CAST((c),		GST_TYPE_ADD_TAG_MUX,GstAddTagMuxClass))
#define GST_IS_ADD_TAG_MUX(o)		(G_TYPE_CHECK_INSTANCE_TYPE((o),	GST_TYPE_ADD_TAG_MUX))
#define GST_IS_ADD_TAG_MUX_CLASS(c)	(G_TYPE_CHECK_CLASS_TYPE((c),		GST_TYPE_ADD_TAG_MUX))

typedef struct _GstAddTagMux		GstAddTagMux;
typedef struct _GstAddTagMuxClass	GstAddTagMuxClass;

struct _GstAddTagMux {
    GstElement		element;	// we are a GstElement
    GstPad *		sink;		// our main sink
    GstPad *		src;		// our only src
    GMutex		mutex;		// lock on index and count changes
    gint		index;		// next index for image pad
    gint volatile	count;		// images pending
    GCond		cond;		// block on 0 == count condition
    GstTagList *	taglist;	//
};

struct _GstAddTagMuxClass {
    GstElementClass	element_class;
};

GType gst_add_tag_mux_get_type(void);

G_END_DECLS

#endif
