/*  lysdr Software Defined Radio
    (C) 2010 Gordon JC Pearce MM0YEQ
    
    scale.c
    draw the tuning scale
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include <gtk/gtk.h>
#include <math.h>

#include "scale.h"

static GtkWidgetClass *parent_class = NULL;
G_DEFINE_TYPE (SDRScale, sdr_scale, GTK_TYPE_DRAWING_AREA);

static gboolean sdr_scale_expose(GtkWidget *widget, GdkEventExpose *event);

static void sdr_scale_class_init (SDRScaleClass *class) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    parent_class = gtk_type_class(GTK_TYPE_DRAWING_AREA);

/*
    widget_class->realize = sdr_waterfall_realize;
    widget_class->unrealize = sdr_waterfall_unrealize; // hate american spelling
    widget_class->expose_event = sdr_waterfall_expose;
    widget_class->button_press_event = sdr_waterfall_button_press;
    widget_class->button_release_event = sdr_waterfall_button_release;
    widget_class->motion_notify_event = sdr_waterfall_motion_notify;
    widget_class->scroll_event = sdr_waterfall_scroll;
*/
    widget_class->expose_event = sdr_scale_expose;

    g_type_class_add_private (class, sizeof (SDRScalePrivate));

}

static void sdr_scale_init (SDRScale *scale) {
    
    SDRScalePrivate *priv = SDR_SCALE_GET_PRIVATE(scale);
    //gtk_widget_set_events(GTK_WIDGET(wf), GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
    scale->centre_freq = 0;
}

GtkWidget *sdr_scale_new(gint sample_rate, gint centre_freq) {
    SDRScale *scale;

    scale = g_object_new(
        SDR_TYPE_SCALE,
        NULL
    );
    scale->sample_rate = sample_rate;
    scale->centre_freq = centre_freq;
    return GTK_WIDGET(scale);
}

static gboolean sdr_scale_expose(GtkWidget *widget, GdkEventExpose *event) {
    // draw the scale to a handy pixmap
    SDRScale *scale = SDR_SCALE(widget);
    gint width = 1024; // FIXME scale->width;
    cairo_text_extents_t extent;
    cairo_t *cr;
    gint i, j, val;
    gchar s[10];
    
    if (!scale->scale) scale->scale = gdk_pixmap_new(gtk_widget_get_window(widget), width, SCALE_HEIGHT, -1);
    
    cr = gdk_cairo_create(gtk_widget_get_window(widget));
    cairo_rectangle(cr, 0, 0, width, SCALE_HEIGHT);
    cairo_clip(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, width, 0);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 1);
    
    val = (trunc(scale->sample_rate/SCALE_TICK)+1)*SCALE_TICK;
    
    // work out how wide the frequency label is going to be
    sprintf(s, "%4.3f", scale->centre_freq/1000000.0f);
    cairo_text_extents(cr, s, &extent);
    
    for (i=-val; i<val; i+=SCALE_TICK) {  // FIXME hardcoded scale tick
        j = width * (0.5+((double)i/scale->sample_rate));
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr, 0.5+j, 0);
        cairo_line_to(cr, 0.5+j, 8);
        cairo_stroke(cr);
        cairo_move_to(cr, j-(extent.width/2), 18);
        cairo_set_source_rgb(cr, .75, .75, .75);
        sprintf(s, "%4.3f", (scale->centre_freq/1000000.0f)+(i/1000000.0f));
        cairo_show_text(cr,s);
    }
    cairo_destroy(cr);
 
}

void sdr_scale_set_scale(GtkWidget *widget, gint centre_freq) {
	SDRScale *scale = SDR_SCALE(widget);
	scale->centre_freq = centre_freq;
	gtk_widget_queue_draw(widget);
}

/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */

