/*  lysdr Software Defined Radio
    (C) 2010 Gordon JC Pearce MM0YEQ
    
    scale.h
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef __SCALE_H
#define __SCALE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _SDRScale            SDRScale;
typedef struct _SDRScaleClass       SDRScaleClass;
typedef struct _SDRScalePrivate     SDRScalePrivate;

struct _SDRScale {
    GtkDrawingArea parent;
    gint width;
    gint sample_rate;
    gint centre_freq;
    gint fft_size;
};

struct _SDRScaleClass {
    GtkDrawingAreaClass parent_class;
};

struct _SDRScalePrivate {
    GMutex *mutex;
};

#define SDR_TYPE_SCALE             (sdr_scale_get_type ())
#define SDR_SCALE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SDR_TYPE_SCALE, SDRScale))
#define SDR_SCALE_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), SDR_SCALE,  SDRScaleClass))
#define SDR_IS_SCALE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SDR_TYPE_SCALE))
#define SDR_IS_SCALE_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), SDR_TYPE_SCALE))
#define SDR_SCALE_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), SDR_TYPE_SCALE, SDRScaleClass))
#define SDR_SCALE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SDR_TYPE_SCALE, SDRScalePrivate))

G_END_DECLS

#define SCALE_HEIGHT 24
#define SCALE_TICK 5000

GtkWidget *sdr_scale_new(gint sample_rate, gint centre_freq);
void sdr_scale_update(GtkWidget *widget, guchar *row);
void sdr_scale_set_scale(GtkWidget *widget, gint centre_freq);

#endif /* __SCALE_H */
/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */
