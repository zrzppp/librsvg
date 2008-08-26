/* vim: set sw=4 sts=4: -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
   rsvg-cairo-render.c: The cairo backend plugin

   Copyright (C) 2005 Dom Lachowicz <cinamod@hotmail.com>
   Caleb Moore <c.moore@student.unsw.edu.au>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dom Lachowicz <cinamod@hotmail.com>
   Caleb Moore <c.moore@student.unsw.edu.au>
*/

#include <stdio.h>
#include <stdlib.h>
#include <glib/gslist.h>
#include <math.h>
#include <string.h>

#include "rsvg.h"
#include "rsvg-cairo.h"
#include "rsvg-cairo-draw.h"
#include "rsvg-cairo-render.h"
#include "rsvg-styles.h"
#include "rsvg-structure.h"

static void
rsvg_cairo_render_free (RsvgRender * self)
{
    RsvgCairoRender *me = (RsvgCairoRender *) self;

    /* TODO */

    g_free (me);
}

RsvgCairoRender *
rsvg_cairo_render_new (cairo_t * cr, double width, double height)
{
    RsvgCairoRender *cairo_render = g_new0 (RsvgCairoRender, 1);

    cairo_render->super.free = rsvg_cairo_render_free;
    cairo_render->super.create_pango_context = rsvg_cairo_create_pango_context;
    cairo_render->super.render_pango_layout = rsvg_cairo_render_pango_layout;
    cairo_render->super.render_image = rsvg_cairo_render_image;
    cairo_render->super.render_path = rsvg_cairo_render_path;
    cairo_render->super.pop_discrete_layer = rsvg_cairo_pop_discrete_layer;
    cairo_render->super.push_discrete_layer = rsvg_cairo_push_discrete_layer;
    cairo_render->super.add_clipping_rect = rsvg_cairo_add_clipping_rect;
    cairo_render->super.get_image_of_node = rsvg_cairo_get_image_of_node;
    cairo_render->width = width;
    cairo_render->height = height;
    cairo_render->offset_x = 0;
    cairo_render->offset_y = 0;
    cairo_render->initial_cr = cr;
    cairo_render->cr = cr;
    cairo_render->cr_stack = NULL;
    cairo_render->bb_stack = NULL;
    cairo_render->pixbuf_stack = NULL;

    return cairo_render;
}

static void rsvg_cairo_transformed_image_bounding_box (
    cairo_matrix_t * transform,
    double width, double height,
    double *x0, double *y0, double *x1, double *y1)
{
    double x00 = 0, x01 = 0, x10 = width, x11 = width;
    double y00 = 0, y01 = height, y10 = 0, y11 = height;
    double t;

    /* transform the four corners of the image */
    cairo_matrix_transform_point (transform, &x00, &y00);
    cairo_matrix_transform_point (transform, &x01, &y01);
    cairo_matrix_transform_point (transform, &x10, &y10);
    cairo_matrix_transform_point (transform, &x11, &y11);

    /* find minimum and maximum coordinates */
    t = x00  < x01 ? x00  : x01;
    t = t < x10 ? t : x10;
    *x0 = floor (t < x11 ? t : x11);

    t = y00  < y01 ? y00  : y01;
    t = t < y10 ? t : y10;
    *y0 = floor (t < y11 ? t : y11);

    t = x00  > x01 ? x00  : x01;
    t = t > x10 ? t : x10;
    *x1 = ceil (t > x11 ? t : x11);

    t = y00  > y01 ? y00  : y01;
    t = t > y10 ? t : y10;
    *y1 = ceil (t > y11 ? t : y11);
}

static RsvgDrawingCtx *
rsvg_cairo_new_drawing_ctx (cairo_t * cr, RsvgHandle * handle)
{
    RsvgDimensionData data;
    RsvgDrawingCtx *draw;
    RsvgCairoRender *render;
    RsvgState *state;
    cairo_matrix_t cairo_transform;
    double affine[6], bbx0, bby0, bbx1, bby1;

    rsvg_handle_get_dimensions (handle, &data);
    if (data.width == 0 || data.height == 0)
        return NULL;

    draw = g_new (RsvgDrawingCtx, 1);

    cairo_get_matrix (cr, &cairo_transform);

    /* find bounding box of image as transformed by the current cairo context
     * The size of this bounding box determines the size of the intermediate
     * surfaces allocated during drawing. */
    rsvg_cairo_transformed_image_bounding_box (&cairo_transform,
                                               data.width, data.height,
                                               &bbx0, &bby0, &bbx1, &bby1);

    render = rsvg_cairo_render_new (cr, bbx1 - bbx0, bby1 - bby0);

    if (!render)
        return NULL;

    draw->render = (RsvgRender *) render;
    render->offset_x = bbx0;
    render->offset_y = bby0;

    draw->state = NULL;

    draw->defs = handle->priv->defs;
    draw->base_uri = g_strdup (handle->priv->base_uri);
    draw->dpi_x = handle->priv->dpi_x;
    draw->dpi_y = handle->priv->dpi_y;
    draw->vb.w = data.em;
    draw->vb.h = data.ex;
    draw->pango_context = NULL;
    draw->drawsub_stack = NULL;

    rsvg_state_push (draw);
    state = rsvg_state_current (draw);

    /* apply cairo transformation to our affine transform */
    affine[0] = cairo_transform.xx;
    affine[1] = cairo_transform.yx;
    affine[2] = cairo_transform.xy;
    affine[3] = cairo_transform.yy;
    affine[4] = cairo_transform.x0;
    affine[5] = cairo_transform.y0;
    _rsvg_affine_multiply (state->affine, affine, state->affine);

    /* scale according to size set by size_func callback */
    affine[0] = data.width / data.em;
    affine[1] = 0;
    affine[2] = 0;
    affine[3] = data.height / data.ex;
    affine[4] = 0;
    affine[5] = 0;
    _rsvg_affine_multiply (state->affine, affine, state->affine);

    /* adjust transform so that the corner of the bounding box above is
     * at (0,0) - we compensate for this in _set_rsvg_affine() in
     * rsvg-cairo-render.c and a few other places */
    state->affine[4] -= render->offset_x;
    state->affine[5] -= render->offset_y;

    rsvg_bbox_init (&((RsvgCairoRender *) draw->render)->bbox, state->affine);

    return draw;
}

/**
 * rsvg_handle_render_cairo_sub
 * @handle: A RsvgHandle
 * @cr: A Cairo renderer
 * @id: An element's id within the SVG, or %NULL to render the whole SVG. For
 * example, if you have a layer called "layer1" that you wish to render, pass 
 * "##layer1" as the id.
 *
 * Draws a subset of a SVG to a Cairo surface
 *
 * Since: 2.14
 */
void
rsvg_handle_render_cairo_sub (RsvgHandle * handle, cairo_t * cr, const char *id)
{
    RsvgDrawingCtx *draw;
    RsvgNode *drawsub = NULL;

    g_return_if_fail (handle != NULL);

    if (!handle->priv->finished)
        return;

    draw = rsvg_cairo_new_drawing_ctx (cr, handle);
    if (!draw)
        return;

    if (id && *id)
        drawsub = rsvg_defs_lookup (handle->priv->defs, id);

    while (drawsub != NULL) {
        draw->drawsub_stack = g_slist_prepend (draw->drawsub_stack, drawsub);
        drawsub = drawsub->parent;
    }

    rsvg_state_push (draw);
    cairo_save (cr);

    rsvg_node_draw ((RsvgNode *) handle->priv->treebase, draw, 0);

    cairo_restore (cr);
    rsvg_state_pop (draw);
    rsvg_drawing_ctx_free (draw);
}

/**
 * rsvg_handle_render_cairo
 * @handle: A RsvgHandle
 * @cr: A Cairo renderer
 *
 * Draws a SVG to a Cairo surface
 *
 * Since: 2.14
 */
void
rsvg_handle_render_cairo (RsvgHandle * handle, cairo_t * cr)
{
    rsvg_handle_render_cairo_sub (handle, cr, NULL);
}
