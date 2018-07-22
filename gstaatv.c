/* GStreamer
* Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
/**
* SECTION:element-aatv
* @see_also: #GstCACASink
*
* Displays video as b/w ascii art.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch-1.0 filesrc location=test.avi ! decodebin ! videoconvert ! aatv
* ]| This pipeline renders a video to ascii art into a separate window.
* |[
* gst-launch-1.0 filesrc location=test.avi ! decodebin ! videoconvert ! aatv driver=curses
* ]| This pipeline renders a video to ascii art into the current terminal.
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaatv.h"
#include <string.h>

/* aatv signals and args */
enum
{
	LAST_SIGNAL
};

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

enum
{
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DITHER,
	PROP_FONT,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_GAMMA,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_INVERSION,
	PROP_RANDOMVAL
};

static GstStaticPadTemplate sink_template_tv = GST_STATIC_PAD_TEMPLATE ("sink",
GST_PAD_SINK,
GST_PAD_ALWAYS,
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ I420}"))
);
static GstStaticPadTemplate src_template_tv = GST_STATIC_PAD_TEMPLATE ("src",
GST_PAD_SRC,
GST_PAD_ALWAYS,
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB }"))
);

static void gst_aatv_set_property (GObject * object, guint prop_id,
const GValue * value, GParamSpec * pspec);
static void gst_aatv_get_property (GObject * object, guint prop_id,
GValue * value, GParamSpec * pspec);


#define gst_aatv_parent_class parent_class
G_DEFINE_TYPE (GstAATv, gst_aatv, GST_TYPE_VIDEO_FILTER);

static void
gst_aatv_scale (GstAATv * aatv, guchar * src, guchar * dest,
gint sw, gint sh, gint ss, gint dw, gint dh)
{
	gint ypos, yinc, y;
	gint xpos, xinc, x;

	g_return_if_fail ((dw != 0) && (dh != 0));

	ypos = 0x10000;
	yinc = (sh << 16) / dh; //how many bins wide are we?
	xinc = (sw << 16) / dw; //how many bins tall are we?

	for (y = dh; y; y--) {
		while (ypos > 0x10000) {
			ypos -= 0x10000;
			src += ss;
		}
		xpos = 0x10000;
		{
			guchar *destp = dest;
			guchar *srcp = src;

			for (x = dw; x; x--) {
				while (xpos >= 0x10000L) {
					srcp++;
					xpos -= 0x10000L;
				}
				*destp++ = *srcp;
				xpos += xinc;
			}
		}
		dest += dw;
		ypos += yinc;
	}
}


static GstFlowReturn
gst_aatv_transform_frame (GstVideoFilter * vfilter, GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
	GstAATv *aatv = GST_AATV (vfilter);
	guint8 *output_pixels = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);
	int pixel_index = 0;
	const unsigned char * font_dict_index = aa_currentfont(aatv->context)->data;
	int font_height = aa_currentfont(aatv->context)->height;		
	
	GST_OBJECT_LOCK (aatv);

	gst_aatv_scale (aatv, GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),     // src 
	aa_image (aatv->context),       //dest
	GST_VIDEO_FRAME_WIDTH(in_frame),   // sw 
	GST_VIDEO_FRAME_HEIGHT (in_frame),    // sh
	GST_VIDEO_FRAME_PLANE_STRIDE (in_frame, 0), // ss
	aa_imgwidth (aatv->context),    // dw
	aa_imgheight (aatv->context));  // dh

	aa_render (aatv->context, &aatv->ascii_parms,  0, 0, aa_imgwidth (aatv->context), aa_imgheight (aatv->context));
	

	for (int y = 0;y < aa_scrheight(aatv->context)  ;y++){
		for (int x = 0;x < aa_scrwidth(aatv->context) ;x++){
			putc(aa_text(aatv->context)[x + y *  aa_scrwidth(aatv->context)], stdout);
		}
		putc('\n', stdout);
	}
	putc('\n', stdout);


	
	//loop through the canvas height
	for (int y = 0; y < aa_scrheight(aatv->context);y++){
		//loop through the height of a character's font
		for (int font_scan_line = 0;font_scan_line < font_height;font_scan_line++){
			//loop through the canvas width
			for (int x = 0;x < aa_scrwidth(aatv->context);x++){
				//lookup what character we need to render
				char input_letter = aa_text(aatv->context)[x + y * aa_scrwidth(aatv->context)];
				//look that character up in the font glyph table
				const unsigned char font_base = font_dict_index[input_letter * font_height + font_scan_line];
				//loop through the width of a character's font
				for(int w = 0; w < 8; w++){
					if (CHECK_BIT(font_base,w)){
						output_pixels[pixel_index++] = aatv->red;
						output_pixels[pixel_index++] = aatv->green;
						output_pixels[pixel_index++] = aatv->blue;
					}else{
						output_pixels[pixel_index++] = 0;
						output_pixels[pixel_index++] = 0;
						output_pixels[pixel_index++] = 0;
					}
				}
			}
		}
	}
	
	
	GST_OBJECT_UNLOCK (aatv);
	
	return GST_FLOW_OK;
}


#define GST_TYPE_AADITHER (gst_aatv_dither_get_type())
static GType
gst_aatv_dither_get_type (void)
{
	static GType dither_type = 0;

	if (!dither_type) {
		GEnumValue *ditherers;
		gint n_ditherers;
		gint i;

		for (n_ditherers = 0; aa_dithernames[n_ditherers]; n_ditherers++) {
			//count number of ditherers 
		}

		ditherers = g_new0 (GEnumValue, n_ditherers + 1);

		for (i = 0; i < n_ditherers; i++) {
			ditherers[i].value = i;
			ditherers[i].value_name = g_strdup (aa_dithernames[i]);
			ditherers[i].value_nick =
			g_strdelimit (g_strdup (aa_dithernames[i]), " _", '-');
		}
		ditherers[i].value = 0;
		ditherers[i].value_name = NULL;
		ditherers[i].value_nick = NULL;

		dither_type = g_enum_register_static ("GstAATvDitherers", ditherers);
	}
	return dither_type;
}

#define GST_TYPE_AAFONT (gst_aatv_font_get_type())
static GType
gst_aatv_font_get_type (void)
{
	static GType font_type = 0;

	if (!font_type) {
		GEnumValue *fonts;
		gint n_fonts;
		gint i;

		for (n_fonts = 0; aa_fonts[n_fonts]; n_fonts++) {
			//count number of fonts 
		}

		fonts = g_new0 (GEnumValue, n_fonts + 1);

		for (i = 0; i < n_fonts; i++) {
			fonts[i].value = i;
			fonts[i].value_name = g_strdup (aa_fonts[i]->shortname);
			fonts[i].value_nick = g_strdelimit (g_strdup (aa_fonts[i]->name), " _", '-');
		}
		fonts[i].value = 0;
		fonts[i].value_name = NULL;
		fonts[i].value_nick = NULL;

		font_type = g_enum_register_static ("GstAATvFonts", fonts);
	}
	return font_type;
}

static gboolean
gst_aatv_setcaps (GstVideoFilter * filter, GstCaps * incaps,
GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{

	return TRUE;
}

//use a custom transform_caps 
static GstCaps *
gst_aatv_transform_caps (GstBaseTransform * trans, GstPadDirection direction,    GstCaps * caps, GstCaps * filter)
{
	GstCaps *ret;
	GstAATv *aatv = GST_AATV (trans);
	GValue formats = G_VALUE_INIT;
	GValue value = G_VALUE_INIT;
	GValue src_width = G_VALUE_INIT;
	GValue src_height = G_VALUE_INIT;

	if (direction == GST_PAD_SINK) {


		ret = gst_caps_copy (caps);

		g_value_init (&src_width, G_TYPE_INT);
		g_value_init (&src_height, G_TYPE_INT);
		//calculate output resolution from canvas size and font size 

		g_value_set_int (&src_width,  aa_defparams.width * 8);
		g_value_set_int (&src_height,  aa_defparams.height * aa_currentfont(aatv->context)->height);

		gst_caps_set_value (ret, "width", &src_width);
		gst_caps_set_value (ret, "height", &src_height);
		//force ARGB output format 
		g_value_init (&formats, GST_TYPE_LIST);
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, "RGB");
		gst_value_list_append_value (&formats, &value);

		gst_caps_set_value (ret, "format", &formats);
		
	} else {
		ret = gst_static_pad_template_get_caps (&sink_template_tv);
	}

	return ret;
}


static void
gst_aatv_class_init (GstAATvClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstVideoFilterClass *videofilter_class;
	GstBaseTransformClass *transform_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	videofilter_class = (GstVideoFilterClass *) klass;
	transform_class = (GstBaseTransformClass *) klass;

	gobject_class->set_property = gst_aatv_set_property;
	gobject_class->get_property = gst_aatv_get_property;

	/* FIXME: add long property descriptions */
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WIDTH,
	g_param_spec_int ("width", "width", "width", 0, G_MAXINT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HEIGHT,
	g_param_spec_int ("height", "height", "height", 0, G_MAXINT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DITHER,
	g_param_spec_enum ("dither", "dither", "dither", GST_TYPE_AADITHER, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT,
	g_param_spec_enum ("font", "font", "font", GST_TYPE_AAFONT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RED,
	g_param_spec_int ("red", "red", "red", 0,
	G_MAXUINT8, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GREEN,
	g_param_spec_int ("green", "green", "green", 0,
	G_MAXUINT8, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BLUE,
	g_param_spec_int ("blue", "blue", "blue", 0,
	G_MAXUINT8, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BRIGHTNESS,
	g_param_spec_int ("brightness", "brightness", "brightness", 0,
	G_MAXUINT8, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_CONTRAST,
	g_param_spec_int ("contrast", "contrast", "contrast", 0, G_MAXUINT8,
	0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GAMMA,
	g_param_spec_float ("gamma", "gamma", "gamma", 0.0, 5.0, 1.0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_INVERSION,
	g_param_spec_boolean ("inversion", "inversion", "inversion", TRUE,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RANDOMVAL,
	g_param_spec_int ("randomval", "randomval", "randomval", 0,
	G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gst_element_class_add_static_pad_template (gstelement_class,
	&sink_template_tv);
	gst_element_class_add_static_pad_template (gstelement_class,
	&src_template_tv);

	gst_element_class_set_static_metadata (gstelement_class,
	"aaTV effect", "Filter/Effect/Video",
	"ASCII art effect", "Eric Marks <bigmarkslp@gmail.com>");
	
	transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_aatv_transform_caps);
	videofilter_class->transform_frame = GST_DEBUG_FUNCPTR (gst_aatv_transform_frame);
	videofilter_class->set_info = GST_DEBUG_FUNCPTR (gst_aatv_setcaps);
}

static void
gst_aatv_init (GstAATv * aatv)
{  

	aa_defparams.width = 80;
	aa_defparams.height = 24;
	aatv->red = 255;
	aatv->green = 255;
	aatv->blue = 255;
	aatv->ascii_parms.bright = 0;
	aatv->ascii_parms.contrast = 10;
	aatv->ascii_parms.gamma = 1.0;
	aatv->ascii_parms.dither = 0;
	aatv->ascii_parms.inversion = 0;
	aatv->ascii_parms.randomval = 0;
	aatv -> context =  aa_init(&mem_d,&aa_defparams,NULL);
	aa_setfont(aatv->context, aa_fonts[0]);
}



static void
gst_aatv_set_property (GObject * object, guint prop_id, const GValue * value,
GParamSpec * pspec)
{
	GstAATv *aatv;

	aatv = GST_AATV (object);

	switch (prop_id) {
	case PROP_WIDTH:
		aa_defparams.width = g_value_get_int (value);
		/* recalculate output resolution based on new width */
		gst_pad_mark_reconfigure (GST_BASE_TRANSFORM_SRC_PAD (object));
		break;
	case PROP_HEIGHT:{
			aa_defparams.height = g_value_get_int (value);
			/* recalculate output resolution based on new height */
			gst_pad_mark_reconfigure (GST_BASE_TRANSFORM_SRC_PAD (object));
			break;
		}
	case PROP_DITHER:{
			aatv->ascii_parms.dither = g_value_get_enum (value);
			break;
		}
	case PROP_FONT:{
			aa_setfont(aatv -> context, aa_fonts[ g_value_get_enum (value)]);
			/* recalculate output resolution based on new font */
			gst_pad_mark_reconfigure (GST_BASE_TRANSFORM_SRC_PAD (object));
			break;
		}
	case PROP_BRIGHTNESS:{
			aatv->ascii_parms.bright = g_value_get_int (value);
			break;
		}
	case PROP_CONTRAST:{
			aatv->ascii_parms.contrast = g_value_get_int (value);
			break;
		}
	case PROP_GAMMA:{
			aatv->ascii_parms.gamma = g_value_get_float (value);
			break;
		}
	case PROP_RED:{
			aatv->red = g_value_get_int  (value);
			break;
		}
	case PROP_GREEN:{
			aatv->green = g_value_get_int  (value);
			break;
		}
	case PROP_BLUE:{
			aatv->blue = g_value_get_int  (value);
			break;
		}		
	case PROP_INVERSION:{
			aatv->ascii_parms.inversion = g_value_get_boolean (value);
			break;
		}
	case PROP_RANDOMVAL:{
			aatv->ascii_parms.randomval = g_value_get_int (value);
			break;
		}
	default:
		break;
	}
}

static void
gst_aatv_get_property (GObject * object, guint prop_id, GValue * value,
GParamSpec * pspec)
{
	GstAATv *aatv;

	aatv = GST_AATV (object);

	switch (prop_id) {
	case PROP_WIDTH:{
			g_value_set_int (value, aa_defparams.width);
			break;
		}
	case PROP_HEIGHT:{
			g_value_set_int (value, aa_defparams.height);

			break;
		}
	case PROP_DITHER:{
			g_value_set_enum (value, aatv->ascii_parms.dither);
			break;
		}
	case PROP_FONT:{
			g_value_set_enum (value, aatv->ascii_parms.dither);
			break;
		}
	case PROP_BRIGHTNESS:{
			g_value_set_int (value, aatv->ascii_parms.bright);
			break;
		}
	case PROP_CONTRAST:{
			g_value_set_int (value, aatv->ascii_parms.contrast);
			break;
		}
	case PROP_GAMMA:{
			g_value_set_float (value, aatv->ascii_parms.gamma);
			break;
		}
	case PROP_RED:{
			g_value_set_int (value, aatv->red);
			break;
		}
	case PROP_GREEN:{
			g_value_set_int (value, aatv->green);
			break;
		}
	case PROP_BLUE:{
			g_value_set_int (value, aatv->blue);
			break;
		}
	case PROP_INVERSION:{
			g_value_set_boolean (value, aatv->ascii_parms.inversion);
			break;
		}
	case PROP_RANDOMVAL:{
			g_value_set_int (value, aatv->ascii_parms.randomval);
			break;
		}
	default:{
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
		}
	}
}
