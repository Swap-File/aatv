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
#include <stdlib.h>

#define PROP_AATV_TEXT_COLOR_DEFAULT    0xffffffff  //WHITE
#define PROP_AATV_BG_COLOR_DEFAULT      0xff000000  //BLACK
#define PROP_AATV_RAIN_COLOR_DEFAULT    0xff00ff00  //GREEN
#define PROP_AATV_RAIN_MODE_DEFAULT     GST_RAIN_OFF

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
	PROP_TEXT_COLOR,
	PROP_BG_COLOR,
	PROP_RAIN_COLOR,
	PROP_RAIN_MODE,
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

#define GST_TYPE_AATV_RAIN_MODE (gst_aatv_rain_mode_get_type())

static GType
gst_aatv_rain_mode_get_type (void)
{
	static GType rain_mode = 0;

	static const GEnumValue rain_modes[] = {
		{GST_RAIN_OFF, "No Rain", "none"},
		{GST_RAIN_DOWN, "Rain Down", "down"},
		{GST_RAIN_UP, "Rain Up", "up"},
		{GST_RAIN_LEFT, "Rain Left", "left"},
		{GST_RAIN_RIGHT, "Rain Right", "right"},
		{0, NULL, NULL},
	};

	if (!rain_mode) {
		rain_mode = g_enum_register_static ("GstAATvRainModes", rain_modes);
	}
	return rain_mode;
}



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

static guint
gst_aatv_rand_range(guint lower, guint upper){
	return (rand() % (upper - lower + 1)) + lower;
}
static void
gst_aatv_rain(GstAATv *aatv){
	
	GstAATvDroplet * raindrops = aatv->raindrops;
	
	for (int i=0; i < aatv->rain_width; i++){
		if (raindrops[i].enabled == FALSE){
			if (gst_aatv_rand_range(0,1000)>950){
				
				int obstructed = FALSE;
				
				//dont let adjacent lines be enabled at the same time
				//if (i > 0)	if (raindrops[i-1].enabled == TRUE) obstructed = TRUE;
				//if (i < aatv->rain_width)  if (raindrops[i+1].enabled == TRUE) obstructed = TRUE;
				
				if (i > 0)	if (raindrops[i-1].enabled == TRUE && raindrops[i-1].location + raindrops[i-1].length < aatv->rain_height/4) obstructed = TRUE;
				if (i < aatv->rain_width)  if (raindrops[i+1].enabled == TRUE && raindrops[i+1].location + raindrops[i+1].length < aatv->rain_height/4) obstructed = TRUE;
				
				if (obstructed == FALSE){
					raindrops[i].location = 0;
					raindrops[i].length = gst_aatv_rand_range(3,10);
					raindrops[i].speed = gst_aatv_rand_range(1,8);
					raindrops[i].timer = 0;
					raindrops[i].enabled = TRUE;
				}
			}
		}else{
			raindrops[i].timer++;
			if(raindrops[i].timer > raindrops[i].speed){
				raindrops[i].timer = 0;
				raindrops[i].location++;
			}
			if(raindrops[i].location - raindrops[i].length > aatv->rain_height){
				raindrops[i].enabled = FALSE;
			}
		}
	}
}

static void
gst_aatv_render(GstAATv *aatv,guchar * dest){
	
	//int autobrightness = 0;
	GstAATvDroplet * raindrops =aatv->raindrops;
	int pixel_index = 0;
	const unsigned char * font_dict_index = aa_currentfont(aatv->context)->data;
	int font_height = aa_currentfont(aatv->context)->height;
	
	/*
	for (int y = 0;y < aa_scrheight(aatv->context)  ;y++){
		for (int x = 0;x < aa_scrwidth(aatv->context) ;x++){
			putc(aa_text(aatv->context)[x + y *  aa_scrwidth(aatv->context)], stdout);
		}
		putc('\n', stdout);
	}
	putc('\n', stdout);
	*/
	
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
					int colored_pixel = FALSE;
					//autobrightness+= 10;
					
					if (aatv->rain_mode == GST_RAIN_DOWN){
						if (raindrops[x].enabled){
							if (y <= raindrops[x].location && y >= (raindrops[x].location - raindrops[x].length))
							colored_pixel = TRUE;
						}
					}else if (aatv->rain_mode == GST_RAIN_UP){
						if (raindrops[x].enabled){
							if ((aatv->rain_height-y) <= raindrops[x].location &&(aatv->rain_height-y)>= (raindrops[x].location - raindrops[x].length))
							colored_pixel = TRUE;
						}
					}else if (aatv->rain_mode == GST_RAIN_LEFT){
						if (raindrops[y].enabled){
							if (x <= raindrops[y].location && x >= (raindrops[y].location - raindrops[y].length))
							colored_pixel = TRUE;
						}
					}else if (aatv->rain_mode == GST_RAIN_RIGHT) {
						if (raindrops[y].enabled){
							if (aatv->rain_height-x <= raindrops[y].location && aatv->rain_height-x >= (raindrops[y].location - raindrops[y].length))
							colored_pixel = TRUE;	
						}
					}
					
					
					if (CHECK_BIT(font_base,w)){					
						if (colored_pixel==FALSE){
							dest[pixel_index++] = aatv->text_color_r;
							dest[pixel_index++] = aatv->text_color_g;
							dest[pixel_index++] = aatv->text_color_b;
						}else{
							dest[pixel_index++] = aatv->rain_color_r;
							dest[pixel_index++] = aatv->rain_color_g;
							dest[pixel_index++] = aatv->rain_color_b;
						}
					}else{
						//autobrightness--;
						dest[pixel_index++] = aatv->bg_color_r;
						dest[pixel_index++] = aatv->bg_color_g;
						dest[pixel_index++] = aatv->bg_color_b;
					}
				}
			}
		}
	}
	
	//if (autobrightness > 0) if (aatv->ascii_parms.bright > -200) aatv->ascii_parms.bright--;
	//if (autobrightness < 0) if (aatv->ascii_parms.bright < 200) aatv->ascii_parms.bright++;
	//printf("%d\n",aatv->ascii_parms.bright);
}

static GstFlowReturn
gst_aatv_transform_frame (GstVideoFilter * vfilter, GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
	GstAATv *aatv = GST_AATV (vfilter);

	if (aatv->rain_mode != GST_RAIN_OFF) gst_aatv_rain(aatv);
	
	GST_OBJECT_LOCK (aatv);

	gst_aatv_scale (aatv, GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0), // src 
	aa_image (aatv->context),       //dest
	GST_VIDEO_FRAME_WIDTH(in_frame),   // sw 
	GST_VIDEO_FRAME_HEIGHT (in_frame),    // sh
	GST_VIDEO_FRAME_PLANE_STRIDE (in_frame, 0), // ss
	aa_imgwidth (aatv->context),    // dw
	aa_imgheight (aatv->context));  // dh

	aa_render (aatv->context, &aatv->ascii_parms,  0, 0, aa_imgwidth (aatv->context), aa_imgheight (aatv->context));
	gst_aatv_render(aatv,GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0));	
	
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
	g_param_spec_int ("width", "width", "Width of the ASCII canvas", 0, G_MAXINT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HEIGHT,
	g_param_spec_int ("height", "height", "Height of the ASCII canvas", 0, G_MAXINT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DITHER,
	g_param_spec_enum ("dither", "dither", "Dithering Mode", GST_TYPE_AADITHER, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT,
	g_param_spec_enum ("font", "font", "ASCII font", GST_TYPE_AAFONT, 0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT_COLOR,
	g_param_spec_uint ("color", "color",
	"Color to use for ASCII text (big-endian ARGB).", 0, G_MAXUINT32,
	PROP_AATV_TEXT_COLOR_DEFAULT,
	G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BG_COLOR,
	g_param_spec_uint ("background-color", "background-color",
	"Color to use behind ASCII text (big-endian ARGB).", 0, G_MAXUINT32,
	PROP_AATV_BG_COLOR_DEFAULT,
	G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RAIN_MODE,
	g_param_spec_enum ("rain-mode", "rain-mode", "Set type of Rain",
	GST_TYPE_AATV_RAIN_MODE, PROP_AATV_RAIN_MODE_DEFAULT,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RAIN_COLOR,
	g_param_spec_uint ("rain-color", "rain-color",
	"Color to use for ASCII text rain overlay (big-endian ARGB).", 0, G_MAXUINT32,
	PROP_AATV_RAIN_COLOR_DEFAULT,
	G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BRIGHTNESS,
	g_param_spec_int ("brightness", "brightness", "Brightness", -255,
	255, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_CONTRAST,
	g_param_spec_int ("contrast", "contrast", "Contrast", 0, G_MAXUINT8,
	0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GAMMA,
	g_param_spec_float ("gamma", "gamma", "set gamma correction value", 0.0, 5.0, 1.0,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_INVERSION,
	g_param_spec_boolean ("inversion", "inversion", "Inverse rendering", TRUE,
	G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RANDOMVAL,
	g_param_spec_int ("randomval", "randomval", "Random dithering value", 0,
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
static void gst_aatv_rain_init(GstAATv * aatv){
	
	switch(aatv->rain_mode) {
		
	case GST_RAIN_DOWN:
	case GST_RAIN_UP:
		aatv-> rain_width =  aa_defparams.width;
		aatv-> rain_height =  aa_defparams.height;
		break;
	case GST_RAIN_LEFT:
	case GST_RAIN_RIGHT:
		aatv-> rain_width = aa_defparams.height ;
		aatv-> rain_height = aa_defparams.width;
		break; 
		
	case GST_RAIN_OFF:
		return;
	}
	
	aatv->raindrops = realloc( aatv->raindrops , aatv-> rain_width * sizeof(struct _GstAATvDroplet));
	for (int i=0; i < aatv->rain_width; i++) aatv->raindrops[i].enabled = FALSE;
	
}

static void
gst_aatv_init (GstAATv * aatv)
{  

	

	aa_defparams.width = 80;
	aa_defparams.height = 24;
	
	aatv->ascii_parms.bright = 0;
	aatv->ascii_parms.contrast = 10;
	aatv->ascii_parms.gamma = 1.0;
	aatv->ascii_parms.dither = 0;
	aatv->ascii_parms.inversion = 0;
	aatv->ascii_parms.randomval = 0;
	aatv -> context =  aa_init(&mem_d,&aa_defparams,NULL);
	aa_setfont(aatv->context, aa_fonts[0]);
	
	aatv->text_color = PROP_AATV_TEXT_COLOR_DEFAULT;
	aatv->text_color_r = (aatv->text_color  >> 16) & 0xff;
	aatv->text_color_g = (aatv->text_color  >> 8) & 0xff;
	aatv->text_color_b  = (aatv->text_color  >> 0) & 0xff;
	
	aatv->bg_color = PROP_AATV_BG_COLOR_DEFAULT;
	aatv->bg_color_r = (aatv->bg_color >> 16) & 0xff;
	aatv->bg_color_g = (aatv->bg_color >> 8) & 0xff;
	aatv->bg_color_b  = (aatv->bg_color >> 0) & 0xff;

	aatv->rain_color = PROP_AATV_RAIN_COLOR_DEFAULT;
	aatv->rain_color_r = (aatv->rain_color  >> 16) & 0xff;
	aatv->rain_color_g = (aatv->rain_color  >> 8) & 0xff;
	aatv->rain_color_b  = (aatv->rain_color  >> 0) & 0xff;
	

	aatv->rain_mode = GST_RAIN_RIGHT;
	
	gst_aatv_rain_init(aatv);
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
	case PROP_TEXT_COLOR:{
			aatv->text_color = g_value_get_uint (value);
			aatv->text_color_r = (aatv->text_color >> 16) & 0xff;
			aatv->text_color_g = (aatv->text_color >> 8) & 0xff;
			aatv->text_color_b  = (aatv->text_color >> 0) & 0xff;
			break;
		}
	case PROP_BG_COLOR:{
			aatv->bg_color = g_value_get_uint (value);
			aatv->bg_color_r = (aatv->bg_color >> 16) & 0xff;
			aatv->bg_color_g = (aatv->bg_color >> 8) & 0xff;
			aatv->bg_color_b  = (aatv->bg_color >> 0) & 0xff;
			break;
		}
	case PROP_RAIN_COLOR:{
			aatv->rain_color = g_value_get_uint (value);
			aatv->rain_color_r = (aatv->rain_color  >> 16) & 0xff;
			aatv->rain_color_g = (aatv->rain_color  >> 8) & 0xff;
			aatv->rain_color_b  = (aatv->rain_color  >> 0) & 0xff;
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
	case PROP_RAIN_MODE:{  
			aatv->rain_mode = g_value_get_enum (value);
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
	case PROP_TEXT_COLOR:{
			g_value_set_uint  (value, aatv->text_color);
			break;
		}
	case PROP_BG_COLOR:{
			g_value_set_uint  (value, aatv->bg_color);
			break;
		}
	case PROP_RAIN_COLOR:{
			g_value_set_uint  (value, aatv->rain_color);
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
	case PROP_RAIN_MODE:{  
			g_value_set_enum (value, aatv->rain_mode);
			break;
		}
	default:{
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
		}
	}
}
