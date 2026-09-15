/*******************************************************************************
 * Size: 22 px
 * Bpp: 4
 * Source: Geist Sans SemiBold 600, PUMPE diagnostic glyphs
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HYPURPLE_GEIST_SEMIBOLD_22
#define HYPURPLE_GEIST_SEMIBOLD_22 1
#endif

#if HYPURPLE_GEIST_SEMIBOLD_22

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0045 "E" */
    0x3f, 0xff, 0xf2, 0x0, 0x7f, 0xf0, 0xce, 0x23,
    0xe6, 0x0, 0x99, 0xdf, 0xe2, 0x0, 0xff, 0xed,
    0xaf, 0xff, 0xc0, 0x1f, 0xfc, 0x43, 0x88, 0xf8,
    0x3, 0x33, 0xbf, 0xc0, 0x1f, 0xfd, 0x6, 0x77,
    0xf8, 0xc0, 0x23, 0x88, 0xf9, 0x0, 0x3f, 0xf8,
    0x0,

    /* U+004D "M" */
    0x3f, 0xfb, 0x0, 0x3f, 0x77, 0xfc, 0x20, 0x18,
    0xc8, 0x3, 0xc8, 0x20, 0x1f, 0xca, 0x1, 0xee,
    0x0, 0xff, 0x50, 0x7, 0x94, 0x3, 0xfc, 0x44,
    0x0, 0xca, 0x1, 0xfc, 0x60, 0xa, 0x0, 0xde,
    0x0, 0x20, 0xf, 0x31, 0x2, 0x80, 0x65, 0x5,
    0x10, 0xf, 0xa8, 0x8, 0xc0, 0xc, 0x0, 0xe0,
    0xf, 0xcc, 0x0, 0xa0, 0x6, 0x80, 0xa0, 0x7,
    0xe1, 0x30, 0x70, 0x14, 0x7, 0x0, 0xff, 0x50,
    0xa, 0x38, 0x2, 0x80, 0x3f, 0xce, 0x0, 0xda,
    0x1, 0x30, 0xf, 0xf0, 0xa8, 0x31, 0x83, 0x0,
    0x7f, 0xf0, 0x3c, 0x3, 0x50, 0x7, 0xff, 0x1,
    0x40, 0x22, 0x20, 0x7, 0xff, 0x5, 0x80, 0x14,
    0x1, 0xf0,

    /* U+0050 "P" */
    0x3f, 0xff, 0xb6, 0x4, 0x3, 0xfc, 0x4f, 0xe6,
    0x1, 0x8e, 0x22, 0x61, 0x1, 0xd0, 0xc, 0xce,
    0xf4, 0xf1, 0x0, 0xb0, 0x7, 0xf5, 0x80, 0x3c,
    0x3, 0xf8, 0xc0, 0x3f, 0xf8, 0x18, 0x0, 0xe0,
    0xf, 0x85, 0xd8, 0x0, 0xa0, 0x12, 0xff, 0xdd,
    0x0, 0x6, 0x20, 0xf, 0xf3, 0xc8, 0x6, 0x38,
    0x8d, 0x7f, 0x0, 0x1c, 0xce, 0xf9, 0x0, 0x3f,
    0xfd, 0x80,

    /* U+0055 "U" */
    0x9f, 0xf2, 0x0, 0x7b, 0xbf, 0xc0, 0x1f, 0xff,
    0xf0, 0xf, 0xfe, 0xf0, 0x80, 0x4, 0x3, 0xc2,
    0x3, 0xc0, 0xd, 0x0, 0xe2, 0x0, 0x13, 0x80,
    0x1c, 0xc0, 0x34, 0x80, 0x2c, 0x58, 0x1, 0x95,
    0x13, 0xc8, 0x2, 0xc1, 0x2a, 0x0, 0x57, 0x61,
    0x1, 0xd1, 0x0, 0x55, 0xa8, 0x80, 0x9c, 0x71,
    0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 216, .box_w = 12, .box_h = 16, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 318, .box_w = 18, .box_h = 16, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 234, .box_w = 13, .box_h = 16, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 246, .box_w = 13, .box_h = 16, .ofs_x = 1, .ofs_y = 0}
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
    -2, 0, -6, 5, -7, 0, 5, 1
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
const lv_font_t hypurple_geist_semibold_22 = {
#else
lv_font_t hypurple_geist_semibold_22 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
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



#endif /*#if HYPURPLE_GEIST_SEMIBOLD_22*/
