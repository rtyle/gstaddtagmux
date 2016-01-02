/* GStreamer
 * Copyright (C) 2015 Ross Tyler
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstaddtagmux
 *
 * The addtagmux element multiplexes content from additional streams as tags.
 * Inserting an addtagmux element in a pipeline by itself does nothing.
 * When additional streams are added to its sinks, it will block the main stream
 * until end of stream is reached on all the others.
 * During this time, the additional stream content is gathered and converted
 * to tags.
 * When all additional streams have ended, these tags are pushed downstream
 * and flow of the main stream is unblocked.
 *
 * Currently, only image/jpeg, image/png and txt/uri-list additional
 * content is recognized.
 * Each buffer of such is turned into an image tag, the type of which
 * may be specified by an image-type field of an upstream capsfilter element.
 * Use a jpegparse element upstream to send a complete image in each buffer.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=0.flac \
 *     ! addtagmux name=addtagmux \
 *         filesrc location=folder.jpg ! jpegparse ! jpegdec \
 *             ! videoscale ! video/x-raw,width=300,height=300 ! jpegenc \
 *             ! image/jpeg,image-type=front-cover ! addtagmux. \
 *     addtagmux. \
 * ! flacparse ! flacdec ! audioconvert ! vorbisenc ! oggmux ! filesink location=0.ogg
 * ]|
 * Removing the middle lines (leaving only the first and last) is a recipe
 * for converting a flac audio encoding to ogg/vorbis.
 * Using only the first two and last two lines does nothing more than insert
 * an addtagmux element with no additional streams so the result is the same.
 * The middle lines specify an additional stream that takes a jpeg image
 * from folder.jpg, scales it to 300x300, says it is the front-cover
 * and feeds it to addtagmux to add such a tag to the output.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gstpad.h>
#include <gst/base/gsttypefindhelper.h>
#include <gst/tag/tag.h>

#include "gstaddtagmux.h"

GST_DEBUG_CATEGORY_STATIC(gst_add_tag_mux_debug_category);
#define GST_CAT_DEFAULT gst_add_tag_mux_debug_category

/// No properties supported yet
enum {
    PROP_0
};

G_DEFINE_TYPE_WITH_CODE (
    GstAddTagMuxPad,
    gst_add_tag_mux_pad,
    GST_TYPE_PAD,
    GST_DEBUG_CATEGORY_INIT(
	gst_add_tag_mux_debug_category,
	"addtagmux",
	0,
	"debug category for addtagmux pad"
    )
);

/// GObject disposal for GstAddTagMuxPad
/// https://developer.gnome.org/gobject/stable/howto-gobject-destruction.html
static void
gst_add_tag_mux_pad_dispose(
    GObject *		object)
{
    GST_TRACE_OBJECT(object, ">");
    G_OBJECT_CLASS(gst_add_tag_mux_pad_parent_class)->dispose(object);
    GST_TRACE_OBJECT(object, "<");
}

/// GObject finalization for GstAddTagMuxPad
/// https://developer.gnome.org/gobject/stable/howto-gobject-destruction.html
static void
gst_add_tag_mux_pad_finalize(
    GObject *		object)
{
    GST_TRACE_OBJECT(object, ">");
    G_OBJECT_CLASS(gst_add_tag_mux_pad_parent_class)->finalize(object);
    GST_TRACE("<");
}

static void
gst_add_tag_mux_pad_class_init(
    GstAddTagMuxPadClass *	klass)
{
    GST_TRACE("<");
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose	= gst_add_tag_mux_pad_dispose;
    gobject_class->finalize	= gst_add_tag_mux_pad_finalize;
    GST_TRACE("<");
}

static GstFlowReturn
gst_add_tag_mux_pad_sink_chain_eos(
    GstPad *		pad,
    GstObject *		parent,
    GstBuffer *		buffer)
{
    GST_TRACE_OBJECT(pad, ">");
    GST_WARNING_OBJECT(pad, "EOS");
    GST_TRACE_OBJECT(pad, "< EOS");
    return GST_FLOW_EOS;
}

static GstFlowReturn
gst_add_tag_mux_pad_sink_chain(
    GstPad *		pad,
    GstObject *		parent,
    GstBuffer *		buffer)
{
    GST_TRACE_OBJECT(pad, ">");

    // guidance taken from
    // gst-plugins-base/gst-libs/gst/tag/tags.c
    // gst_tag_image_data_to_image_sample

    GstCaps * caps
        = gst_type_find_helper_for_buffer(GST_OBJECT(pad), buffer, NULL);
    if (caps) {
	GST_DEBUG_OBJECT(pad, "caps buffer %" GST_PTR_FORMAT, caps);

	gchar const * name
	    = gst_structure_get_name(gst_caps_get_structure(caps, 0));
	if (!g_str_has_prefix (name, "image/") &&
		!g_str_equal (name, "text/uri-list")) {
	    GST_WARNING_OBJECT(pad, "caps buffer not supported %"
		GST_PTR_FORMAT, caps);
	    gst_caps_unref(caps);
	    gst_buffer_unref(buffer);
	    GST_TRACE_OBJECT(pad, "< NOT SUPPORTED");
	    return GST_FLOW_NOT_SUPPORTED;
	}

	GstTagImageType image_type = GST_TAG_IMAGE_TYPE_FRONT_COVER;
	// change the default image_type from the current caps.
	// this can be set with a capsfilter element with an image-type
	// field whose string value can be converted (by name or nickname)
	// to a GstTagImageType.
	// For example, by inserting ...
	//	image/jpeg,image-type=front-cover
	// in the pipeline before us.
	{
	    GstCaps * c = gst_pad_get_current_caps(pad);
	    GST_DEBUG_OBJECT(pad, "caps current %" GST_PTR_FORMAT, c);
	    guint i = gst_caps_get_size(c);
	    while (i--) {
		gchar * n;
		if (gst_structure_get(gst_caps_get_structure(c, i),
			"image-type", G_TYPE_STRING, &n,
			NULL)) {
		    GEnumClass * e = g_type_class_ref(GST_TYPE_TAG_IMAGE_TYPE);
		    GEnumValue * v;
		    if (0
			    || (v = g_enum_get_value_by_name(e, n))
			    || (v = g_enum_get_value_by_nick(e, n))) {
			GST_DEBUG_OBJECT(pad, "%d %s %s", v->value,
			    v->value_name, v->value_nick);
			image_type = (GstTagImageType) v->value;
		    }
		    g_type_class_unref(e);
		    g_free(n);
		}
	    }
	    gst_caps_unref(c);
	}

	// create sample for image tag
	GstStructure * info = NULL;
	if (GST_TAG_IMAGE_TYPE_NONE != image_type) {
	    info = gst_structure_new("GstTagImageInfo",
		"image-type", GST_TYPE_TAG_IMAGE_TYPE, image_type,
		NULL);
	}
	GstSample * sample = gst_sample_new(buffer, caps, NULL, info);
	gst_caps_unref(caps);

	// append image tag with sample to our element's list
	GstAddTagMux * addtagmux = GST_ADD_TAG_MUX(parent);
	g_mutex_lock(&addtagmux->mutex);
	gst_tag_list_add(addtagmux->taglist, GST_TAG_MERGE_APPEND,
	    GST_TAG_IMAGE, sample,
	    NULL);
	g_mutex_unlock(&addtagmux->mutex);
	gst_sample_unref(sample);
    }
    gst_buffer_unref(buffer);

    GST_TRACE_OBJECT(pad, "< OK");
    return GST_FLOW_OK;
}

static GstPadLinkReturn gst_add_tag_mux_pad_link(
    GstPad *		pad,
    GstObject *		parent,
    GstPad *		peer)
{
    GST_TRACE_OBJECT(pad, ">");
    // gst_pad_set_chain*_function insists on the pad's direction property
    // being GST_PAD_SINK
    gst_pad_set_chain_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_pad_sink_chain));
    GST_TRACE_OBJECT(pad, "< OK");
    return GST_PAD_LINK_OK;
}

static gboolean gst_add_tag_mux_pad_sink_event(
    GstPad *		pad,
    GstObject *		parent,
    GstEvent *		event)
{
    GST_TRACE_OBJECT(pad, ">");
    switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_EOS: {
	    gst_pad_set_chain_function(pad,
		GST_DEBUG_FUNCPTR(gst_add_tag_mux_pad_sink_chain_eos));
	    GstAddTagMux * addtagmux = GST_ADD_TAG_MUX(parent);
	    g_mutex_lock(&addtagmux->mutex);
	    --addtagmux->count;
	    GST_DEBUG_OBJECT(pad, "EOS %d", addtagmux->count);
	    if (!addtagmux->count) {
		g_cond_signal(&addtagmux->cond);
	    }
	    g_mutex_unlock(&addtagmux->mutex);
	    break;
	}
	default:
	    break;
    }
    GST_TRACE_OBJECT(pad, "< TRUE");
    return TRUE;
}

static void
gst_add_tag_mux_pad_init(
    GstAddTagMuxPad *	addtagmuxpad)
{
    GST_TRACE_OBJECT(addtagmuxpad, ">");
    GstPad * pad = GST_PAD(addtagmuxpad);

    GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_link_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_pad_link));
    gst_pad_set_event_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_pad_sink_event));

    GST_TRACE_OBJECT(addtagmuxpad, "<");
}

// define gst_add_tag_mux class as a subclass of gst_element.
// defines static variable gst_add_tag_mux_parent_class
G_DEFINE_TYPE_WITH_CODE (
    GstAddTagMux,
    gst_add_tag_mux,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT(
	gst_add_tag_mux_debug_category,
	"addtagmux",
	0,
	"debug category for addtagmux element"
    )
);

/// GObject disposal for GstAddTagMux
/// https://developer.gnome.org/gobject/stable/howto-gobject-destruction.html
static void
gst_add_tag_mux_dispose(
    GObject *		object)
{
    GST_TRACE_OBJECT(object, ">");
    // dispose addtagmux
    G_OBJECT_CLASS(gst_add_tag_mux_parent_class)->dispose(object);
    GST_TRACE_OBJECT(object, "<");
}

/// GObject finalization for GstAddTagMux
/// https://developer.gnome.org/gobject/stable/howto-gobject-destruction.html
static void
gst_add_tag_mux_finalize(
    GObject *		object)
{
    GST_TRACE_OBJECT(object, ">");

    GstAddTagMux * addtagmux = GST_ADD_TAG_MUX(object);
    g_cond_clear(&addtagmux->cond);
    g_mutex_clear(&addtagmux->mutex);
    if (addtagmux->taglist) {
	gst_tag_list_unref(addtagmux->taglist);
    }

    G_OBJECT_CLASS(gst_add_tag_mux_parent_class)->finalize(object);
    GST_TRACE("<");
}

static GstPad *
gst_add_tag_mux_request_new_pad(
    GstElement *	element,
    GstPadTemplate *	template,
    gchar const *	name_,
    GstCaps const *	caps)
{
    GST_TRACE_OBJECT(element, "> name=%s, caps=%" GST_PTR_FORMAT, name_, caps);
    if (GST_PAD_SINK != template->direction) {
	GST_ERROR_OBJECT(element, "template not a sink");
	GST_TRACE_OBJECT(element, "< NULL");
	return NULL;
    }

    GstAddTagMux * addtagmux = GST_ADD_TAG_MUX(element);
    g_mutex_lock(&addtagmux->mutex);
    gint index = addtagmux->index++;
    ++addtagmux->count;
    g_mutex_unlock(&addtagmux->mutex);
    gchar * name
	= g_strdup_printf(GST_PAD_TEMPLATE_NAME_TEMPLATE(template), index);
    GstPad * pad = g_object_new(GST_TYPE_ADD_TAG_MUX_PAD,
	// these properties are set *after* object is initialized
	"name",		name,
	"direction",	template->direction,
	"template",	template,
	NULL);
    g_free(name);
    gst_element_add_pad(element, pad);

    GST_TRACE_OBJECT(element, "< %" GST_PTR_FORMAT, pad);
    return pad;
}

static void
gst_add_tag_mux_release_pad(
    GstElement *	element,
    GstPad *		pad)
{
    GST_TRACE_OBJECT(element, ">");
    GST_TRACE_OBJECT(element, "<");
}

/// A "sink_%u" SINK pad exists only on REQUEST
/// and only supports streams that we can turn into tags
static GstStaticPadTemplate gst_add_tag_mux_pad_sink_template =
GST_STATIC_PAD_TEMPLATE(
    "sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS(
	"image/jpeg;"
	"image/png;"
	"text/uri-list")
);

/// Our "sink" SINK pad ALWAYS exists and supports ANYthing
static GstStaticPadTemplate gst_add_tag_mux_sink_template =
GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

/// Our "src" SRC pad ALWAYS exists and supports ANYthing
static GstStaticPadTemplate gst_add_tag_mux_src_template =
GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

static void
gst_add_tag_mux_class_init(
    GstAddTagMuxClass *	klass)
{
    GST_TRACE(">");
    GObjectClass *		gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *		element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(
	element_class,
	"AddTagMux",
	"Generic",
	"Mux additional streams as tags",
	"Ross Tyler");

    // must add REQUEST SINK pad before ALWAYS SINK pad
    // or else pipeline may not link correctly and issue, for example, a
    // WARNING: erroneous pipeline: could not link filesrc0 to addtagmux
    gst_element_class_add_pad_template(element_class,
	gst_static_pad_template_get(&gst_add_tag_mux_pad_sink_template));
    gst_element_class_add_pad_template(element_class,
	gst_static_pad_template_get(&gst_add_tag_mux_sink_template));
    gst_element_class_add_pad_template(element_class,
	gst_static_pad_template_get(&gst_add_tag_mux_src_template));

    gobject_class->dispose	= gst_add_tag_mux_dispose;
    gobject_class->finalize	= gst_add_tag_mux_finalize;

    element_class->request_new_pad
	= GST_DEBUG_FUNCPTR(gst_add_tag_mux_request_new_pad);
    element_class->release_pad
	= GST_DEBUG_FUNCPTR(gst_add_tag_mux_release_pad);

    GST_TRACE("<");
}

static GstFlowReturn
gst_add_tag_mux_sink_chain_identity(
    GstPad *		pad,
    GstObject *		parent,
    GstBuffer *		buffer)
{
    GST_TRACE_OBJECT(pad, ">");
    GstFlowReturn ret = gst_pad_push(GST_ADD_TAG_MUX(parent)->src, buffer);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static gboolean gst_add_tag_mux_sink_event_identity(
    GstPad *		pad,
    GstObject *		parent,
    GstEvent *		event)
{
    GST_TRACE_OBJECT(pad, ">");
    gboolean ret = gst_pad_push_event(GST_ADD_TAG_MUX(parent)->src, event);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static GstFlowReturn
gst_add_tag_mux_src_getrange_identity(
    GstPad *		pad,
    GstObject *		parent,
    guint64		offset,
    guint		length,
    GstBuffer **	buffer)
{
    GST_TRACE_OBJECT(pad, ">");
    GstPad * peer = gst_pad_get_peer(GST_ADD_TAG_MUX(parent)->sink);
    GstFlowReturn ret = gst_pad_pull_range(peer, offset, length, buffer);
    gst_object_unref(peer);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static gboolean
gst_add_tag_mux_wait(
    GstAddTagMux *	addtagmux)
{
    GST_TRACE_OBJECT(addtagmux, ">");

    // wait while there are additional pads still streaming
    g_mutex_lock(&addtagmux->mutex);
    while (addtagmux->count) {
	GST_DEBUG_OBJECT(addtagmux, "wait %d", addtagmux->count);
	g_cond_wait(&addtagmux->cond, &addtagmux->mutex);
    }
    g_mutex_unlock(&addtagmux->mutex);

    // push our taglist as an event downstream if it has any tags
    gboolean ret;
    if (gst_tag_list_is_empty(addtagmux->taglist)) {
	ret = TRUE;
    } else {
	GstEvent * event = gst_event_new_tag(addtagmux->taglist);
	addtagmux->taglist = 0;		// transferred to event
	ret = gst_pad_push_event(addtagmux->src, event);
    }

    // use *_identity transforms/methods from now on
    gst_pad_set_chain_function(addtagmux->sink,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_sink_chain_identity));
    gst_pad_set_event_function(addtagmux->sink,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_sink_event_identity));
    gst_pad_set_getrange_function(addtagmux->src,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_src_getrange_identity));

    GST_TRACE_OBJECT(addtagmux, "<");
    return ret;
}

static GstFlowReturn
gst_add_tag_mux_sink_chain_wait(
    GstPad *		pad,
    GstObject *		parent,
    GstBuffer *		buffer)
{
    GST_TRACE_OBJECT(pad, ">");
    gst_add_tag_mux_wait(GST_ADD_TAG_MUX(parent));
    GstFlowReturn ret = gst_add_tag_mux_sink_chain_identity(
	pad, parent, buffer);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static gboolean gst_add_tag_mux_sink_event_wait(
    GstPad *		pad,
    GstObject *		parent,
    GstEvent *		event)
{
    GST_TRACE_OBJECT(pad, ">");
    gst_add_tag_mux_wait(GST_ADD_TAG_MUX(parent));
    gboolean ret = gst_add_tag_mux_sink_event_identity(pad, parent, event);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static GstFlowReturn
gst_add_tag_mux_src_getrange_wait(
    GstPad *		pad,
    GstObject *		parent,
    guint64		offset,
    guint		length,
    GstBuffer **	buffer)
{
    GST_TRACE_OBJECT(pad, ">");
    gst_add_tag_mux_wait(GST_ADD_TAG_MUX(parent));
    GstFlowReturn ret = gst_add_tag_mux_src_getrange_identity(
	pad, parent, offset, length, buffer);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static gboolean
gst_add_tag_mux_src_event_identity(
    GstPad *		pad,
    GstObject *		parent,
    GstEvent *		event)
{
    GST_TRACE_OBJECT(pad, ">");
    gboolean ret = gst_pad_push_event(GST_ADD_TAG_MUX(parent)->sink, event);
    GST_TRACE_OBJECT(pad, "< %d", ret);
    return ret;
}

static void
gst_add_tag_mux_init(
    GstAddTagMux *	addtagmux)
{
    GST_TRACE_OBJECT(addtagmux, ">");
    GstElement * element = GST_ELEMENT(addtagmux);
    GstPad * pad;

    // create "sink" pad from template and configure
    addtagmux->sink = pad = gst_pad_new_from_static_template(
	&gst_add_tag_mux_sink_template, "sink");
    GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_chain_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_sink_chain_wait));
    gst_pad_set_event_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_sink_event_wait));
    gst_element_add_pad(element, pad);

    // create "src" pad from template and configure
    addtagmux->src = pad = gst_pad_new_from_static_template(
	&gst_add_tag_mux_src_template, "src");
    GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_getrange_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_src_getrange_wait));
    gst_pad_set_event_function(pad,
	GST_DEBUG_FUNCPTR(gst_add_tag_mux_src_event_identity));
    gst_element_add_pad(element, pad);

    g_mutex_init(&addtagmux->mutex);
    addtagmux->index = 0;
    addtagmux->count = 0;
    g_cond_init(&addtagmux->cond);

    addtagmux->taglist = gst_tag_list_new_empty();

    GST_TRACE_OBJECT(addtagmux, "<");
}

static gboolean
plugin_init(
    GstPlugin *		plugin)
{
    GST_TRACE(">");
    gboolean ret = gst_element_register(
	plugin,
	"addtagmux",
	GST_RANK_NONE,
	GST_TYPE_ADD_TAG_MUX);
    GST_TRACE("< %d", ret);
    return ret;
}

#ifndef VERSION
#define VERSION "0.0"
#endif
#ifndef PACKAGE
#define PACKAGE "gstaddtagmux"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gstaddtagmux"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://github/rtyle/gstaddtagmux"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    addtagmux,
    "Mux additional streams as tags",
    plugin_init,
    VERSION,
    "LGPL",
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
