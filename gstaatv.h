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


#ifndef __GST_AATV_H__
#define __GST_AATV_H__

#include <gst/gst.h>
#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>
#include <aalib.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GST_TYPE_AATV \
		(gst_aatv_get_type())
#define GST_AATV(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AATV,GstAATv))
#define GST_AATV_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AATV,GstAATvClass))
#define GST_IS_AATV(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AATV))
#define GST_IS_AATV_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AATV))

	typedef struct _GstAATv GstAATv;
	typedef struct _GstAATvClass GstAATvClass;
	typedef struct _GstAATvDroplet GstAATvDroplet;
typedef struct _GstAATvARGB GstAATvARGB;

	typedef enum {
		GST_RAIN_OFF,
		GST_RAIN_DOWN,
		GST_RAIN_UP,
		GST_RAIN_LEFT,
		GST_RAIN_RIGHT
	} GstRainMode;

	struct _GstAATvDroplet {
		gboolean enabled;
		gint location;		
		gint length;			
		gint delay;
		gint delay_counter;
	};
	
	struct _GstAATvARGB {
		guint32 argb;
     	guint8 a;
		guint8 r;
		guint8 g;
		guint8 b;
	};
	
	struct _GstAATv {
		GstVideoFilter videofilter;

		aa_context *context;

		guint32 text_color;
		GstAATvARGB text_color_bright,text_color_normal,text_color_dim;
		guint32 rain_color;
		GstAATvARGB rain_color_bright,rain_color_normal,rain_color_dim;
		GstAATvARGB bg_color;
		
		GstRainMode rain_mode;

		gint rain_width;
		gint rain_height;
		
	    gint rain_length_min;
		gint rain_length_max;

	    gint rain_delay_min;
		gint rain_delay_max;
	
		gfloat rain_spawn_rate;
		gboolean auto_brightness;
		gfloat  brightness_target_min;
		gfloat  brightness_target_max;
		gfloat lit_percentage;
		
		GstAATvDroplet * raindrops;
		struct aa_renderparams ascii_parms;
	};

	struct _GstAATvClass {
		GstVideoFilterClass parent_class;
	};

	GType gst_aatv_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_AASINKE_H__ */
