/*******************************************************************************
 * Size: 18 px
 * Bpp: 4
 * Source: Geist Sans SemiBold 600, PUMPE diagnostic glyphs
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HYPURPLE_GEIST_SEMIBOLD_18
#define HYPURPLE_GEIST_SEMIBOLD_18 1
#endif

#if HYPURPLE_GEIST_SEMIBOLD_18

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0045 "E" */
    0x9f, 0xff, 0xc0, 0x1f, 0xfc, 0x13, 0xee, 0xf0,
    0x6, 0x11, 0xf0, 0x7, 0xff, 0x4, 0xff, 0xfb,
    0x40, 0x3f, 0xf8, 0x27, 0xff, 0xda, 0x1, 0xff,
    0xcc, 0x11, 0xf0, 0x4, 0x7d, 0xde, 0x30, 0xf,
    0xf0,

    /* U+004D "M" */
    0x9f, 0xf6, 0x0, 0x7a, 0x3f, 0xd8, 0x1, 0x18,
    0x80, 0x73, 0x0, 0x7c, 0xc0, 0x18, 0xc4, 0x3,
    0xea, 0x0, 0xd4, 0x1, 0xe2, 0x2, 0x20, 0x4,
    0xe0, 0x40, 0x18, 0x50, 0x28, 0x0, 0x82, 0x25,
    0x0, 0xee, 0x5, 0x0, 0x70, 0x38, 0x7, 0x94,
    0x8, 0xc1, 0x42, 0x80, 0x3e, 0x60, 0xa5, 0x2,
    0x30, 0xf, 0xb4, 0x1f, 0x82, 0x80, 0x3f, 0x20,
    0x89, 0x1, 0x40, 0x3f, 0x98, 0x0, 0x64, 0x1,
    0xfd, 0x40, 0xa, 0x0, 0xe0,

    /* U+0050 "P" */
    0x9f, 0xfe, 0xea, 0x30, 0xf, 0xc2, 0xb8, 0x80,
    0x11, 0xf7, 0x56, 0x41, 0x20, 0x18, 0x46, 0x48,
    0x2, 0x10, 0xf, 0xfe, 0x29, 0xc0, 0x8, 0x80,
    0x7, 0xff, 0x61, 0x5, 0x80, 0x7e, 0x4b, 0x50,
    0x8, 0xfb, 0xaf, 0xb4, 0x0, 0xe1, 0x18, 0x3,
    0xff, 0xb4,

    /* U+0055 "U" */
    0xdf, 0x80, 0xe, 0xef, 0x60, 0xf, 0xff, 0xf8,
    0x0, 0x40, 0x40, 0x38, 0x40, 0xd8, 0x10, 0x3,
    0x18, 0x1, 0xb8, 0xa, 0x8c, 0x57, 0x40, 0x8d,
    0x18, 0x17, 0x3a, 0x84, 0x38, 0x1, 0x34, 0x82,
    0x4, 0xdc, 0x60
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 177, .box_w = 10, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 260, .box_w = 14, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 191, .box_w = 11, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 201, .box_w = 11, .box_h = 13, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x8, 0xb, 0x10
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 69, .range_length = 17, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 1, 2, 3, 4
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 1, 1, 1, 2
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    -1, 0, -5, 4, -6, 0, 4, 1
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 4,
    .right_class_cnt     = 2,
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
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 1,
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
const lv_font_t hypurple_geist_semibold_18 = {
#else
lv_font_t hypurple_geist_semibold_18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if HYPURPLE_GEIST_SEMIBOLD_18*/
