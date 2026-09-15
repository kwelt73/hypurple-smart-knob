/*******************************************************************************
 * Size: 29 px
 * Bpp: 4
 * Source: Geist Mono Medium 500, percent sign only
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HYPURPLE_GEIST_MONO_MEDIUM_29
#define HYPURPLE_GEIST_MONO_MEDIUM_29 1
#endif

#if HYPURPLE_GEIST_MONO_MEDIUM_29

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0025 "%" */
    0x0, 0x46, 0xfe, 0xb8, 0x7, 0xab, 0xec, 0x2,
    0xb7, 0x20, 0x28, 0x90, 0xc, 0x6a, 0x12, 0x0,
    0x54, 0xa, 0xf8, 0x6, 0x30, 0xb, 0x81, 0xc8,
    0x1, 0x80, 0x6a, 0xe, 0x20, 0x80, 0x6, 0x21,
    0x80, 0x8, 0x81, 0x40, 0x25, 0xe, 0x1, 0x80,
    0xb0, 0xf, 0xfe, 0x14, 0x9, 0xa8, 0x6, 0x20,
    0x50, 0x9, 0x43, 0x89, 0x83, 0xc0, 0x3b, 0x0,
    0xd4, 0x1c, 0x41, 0x24, 0x18, 0xc0, 0x39, 0x50,
    0x2b, 0xe0, 0x19, 0x88, 0x10, 0x1, 0xf5, 0xb8,
    0x81, 0x44, 0xc0, 0x40, 0x80, 0x7e, 0x8e, 0xfd,
    0x77, 0x11, 0x1c, 0x3, 0xff, 0x82, 0x30, 0x12,
    0x77, 0xfe, 0xb2, 0x0, 0xfd, 0x60, 0x8f, 0x88,
    0x0, 0x4d, 0x20, 0xf, 0x1a, 0x84, 0xf8, 0x1f,
    0x69, 0x4, 0x80, 0x7b, 0xc1, 0xcc, 0xc1, 0x2,
    0x56, 0x8, 0x1, 0xcc, 0x63, 0x4, 0x0, 0x60,
    0x2, 0x80, 0x8, 0x3, 0x40, 0x40, 0x6, 0x30,
    0xf, 0xe8, 0x13, 0x60, 0x20, 0x1, 0x0, 0x18,
    0x0, 0x40, 0x2, 0x70, 0xf0, 0x0, 0x98, 0x48,
    0x3, 0x81, 0x0, 0x29, 0x5, 0x30, 0xb, 0xc1,
    0x77, 0x90, 0x24, 0x0, 0x88, 0xb, 0x0, 0xc7,
    0x88, 0x42, 0x9a, 0x40
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 278, .box_w = 18, .box_h = 21, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 37, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 1,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t hypurple_geist_mono_medium_29 = {
#else
lv_font_t hypurple_geist_mono_medium_29 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 21,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -3,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if HYPURPLE_GEIST_MONO_MEDIUM_29*/
