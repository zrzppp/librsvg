/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 expandtab: */
/* 
   rsvg-filter.c: Provides filters
 
   Copyright (C) 2004 Caleb Moore
  
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
  
   Author: Caleb Moore <calebmm@tpg.com.au>
*/

#include "rsvg-private.h"
#include "rsvg-mask.h"
#include "rsvg-styles.h"
#include "rsvg-css.h"
#include <string.h>

struct _RsvgMask {
    RsvgLength x, y, width, height;
    RsvgMaskUnits maskunits;
    RsvgMaskUnits contentunits;
};

static void
rsvg_mask_set_atts (RsvgNode *node, gpointer impl, RsvgHandle *handle, RsvgPropertyBag *atts)
{
    RsvgMask *mask = impl;
    const char *value;

    if ((value = rsvg_property_bag_lookup (atts, "maskUnits"))) {
        if (!strcmp (value, "userSpaceOnUse"))
            mask->maskunits = userSpaceOnUse;
        else
            mask->maskunits = objectBoundingBox;
    }
    if ((value = rsvg_property_bag_lookup (atts, "maskContentUnits"))) {
        if (!strcmp (value, "objectBoundingBox"))
            mask->contentunits = objectBoundingBox;
        else
            mask->contentunits = userSpaceOnUse;
    }
    if ((value = rsvg_property_bag_lookup (atts, "x")))
        mask->x = rsvg_length_parse (value, LENGTH_DIR_HORIZONTAL);
    if ((value = rsvg_property_bag_lookup (atts, "y")))
        mask->y = rsvg_length_parse (value, LENGTH_DIR_VERTICAL);
    if ((value = rsvg_property_bag_lookup (atts, "width")))
        mask->width = rsvg_length_parse (value, LENGTH_DIR_HORIZONTAL);
    if ((value = rsvg_property_bag_lookup (atts, "height")))
        mask->height = rsvg_length_parse (value, LENGTH_DIR_VERTICAL);
}

static void
rsvg_mask_draw (RsvgNode *node, gpointer impl, RsvgDrawingCtx *ctx, int dominate)
{
    /* nothing; this gets drawn specially in rsvg-cairo-draw.c */
}

RsvgNode *
rsvg_new_mask (const char *element_name, RsvgNode *parent)
{
    RsvgMask *mask;

    mask = g_new0 (RsvgMask, 1);
    mask->maskunits = objectBoundingBox;
    mask->contentunits = userSpaceOnUse;
    mask->x = rsvg_length_parse ("0", LENGTH_DIR_HORIZONTAL);
    mask->y = rsvg_length_parse ("0", LENGTH_DIR_VERTICAL);
    mask->width = rsvg_length_parse ("1", LENGTH_DIR_HORIZONTAL);
    mask->height = rsvg_length_parse ("1", LENGTH_DIR_VERTICAL);

    return rsvg_rust_cnode_new (RSVG_NODE_TYPE_MASK,
                                parent,
                                rsvg_state_new (),
                                mask,
                                rsvg_mask_set_atts,
                                rsvg_mask_draw,
                                g_free);
}

RsvgLength
rsvg_node_mask_get_x (RsvgMask *mask)
{
    return mask->x;
}

RsvgLength
rsvg_node_mask_get_y (RsvgMask *mask)
{
    return mask->y;
}

RsvgLength
rsvg_node_mask_get_width (RsvgMask *mask)
{
    return mask->width;
}

RsvgLength
rsvg_node_mask_get_height (RsvgMask *mask)
{
    return mask->height;
}

RsvgCoordUnits
rsvg_node_mask_get_units (RsvgMask *mask)
{
    return mask->maskunits;
}

RsvgCoordUnits
rsvg_node_mask_get_content_units (RsvgMask *mask)
{
    return mask->contentunits;
}
