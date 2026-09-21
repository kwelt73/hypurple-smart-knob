#include <Arduino.h>
#include <Button.h>
#include <ESP_Knob.h>
#include <ESP_Panel_Library.h>
#include <lvgl.h>

#include "controller_client.h"
#include "fonts/hypurple_fonts.h"
#include "lvgl_port_v8.h"
#include "smart_knob_navigation.h"

namespace {

constexpr uint32_t kWelcomeDurationMs = 2000;
constexpr int kPumpValueMin = 0;
constexpr int kPumpValueMax = 100;
constexpr bool kShowColorDiagnostics = false;

struct HardwareScreenStyle {
    uint32_t accent_color;
    uint32_t background_color;
    uint32_t primary_text_color;
    uint32_t secondary_text_color;
    uint32_t track_color;
    lv_coord_t ring_diameter;
    lv_coord_t ring_width;
    const lv_font_t *label_font;
    const lv_font_t *value_font;
    const lv_font_t *unit_font;
};

// Resolved from the web Smart Knob's 466 x 466 HMI tokens.
constexpr HardwareScreenStyle kPumpStyle = {
    .accent_color = 0xC78B28,
    .background_color = 0x030100,
    .primary_text_color = 0xFEFBF9,
    .secondary_text_color = 0x8D8B89,
    .track_color = 0x211F1E,
    .ring_diameter = 390,
    .ring_width = 14,
    .label_font = &hypurple_geist_semibold_22,
    .value_font = &hypurple_geist_mono_semibold_84,
    .unit_font = &hypurple_geist_mono_medium_29,
};

constexpr uint32_t kCompressorAccentColor = 0x6BB9F8;
constexpr uint32_t kCompressorTrackColor = 0xB3B2AF;

enum class HmiScreen {
    Pump,
    Compressor,
    ColorDiagnostics,
};

enum class HmiPhase {
    Welcome,
    Interactive,
};

ESP_Knob *knob = nullptr;
Button *button = nullptr;
HypurpleControllerClient controller_client;

HmiPhase hmi_phase = HmiPhase::Welcome;
HmiScreen active_screen = HmiScreen::Pump;
SmartKnobScreenConfiguration screen_configuration = defaultSmartKnobScreenConfiguration();
size_t active_screen_index = 0;
int pending_screen_index = -1;
PumpControlState pump_state = resetPumpState();
CompressorControlState compressor_state = stoppedCompressorState();

lv_obj_t *welcome_screen = nullptr;
lv_obj_t *pump_screen = nullptr;
lv_obj_t *color_test_screen = nullptr;
lv_obj_t *pump_arc = nullptr;
lv_obj_t *pump_value_label = nullptr;
lv_obj_t *pump_percent_label = nullptr;
lv_obj_t *pump_pause_label = nullptr;
lv_obj_t *pump_zero_marker = nullptr;
lv_obj_t *pump_connection_dot = nullptr;
lv_obj_t *pump_offline_label = nullptr;
lv_obj_t *compressor_screen = nullptr;
lv_obj_t *compressor_arc = nullptr;
lv_obj_t *compressor_value_label = nullptr;
lv_obj_t *compressor_percent_label = nullptr;
lv_obj_t *compressor_off_label = nullptr;
lv_obj_t *compressor_pause_label = nullptr;
lv_obj_t *compressor_zero_marker = nullptr;
lv_obj_t *compressor_connection_dot = nullptr;
lv_obj_t *compressor_offline_label = nullptr;
bool pump_controller_connected = false;

void handleScreenGesture(lv_event_t *event);

void configureScreen(lv_obj_t *screen, uint32_t background_color)
{
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(background_color), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

void createWelcomeScreen()
{
    welcome_screen = lv_obj_create(nullptr);
    configureScreen(welcome_screen, 0x000000);

    lv_obj_t *brand = lv_label_create(welcome_screen);
    lv_label_set_text(brand, "H Y P U R P L E");
    lv_obj_set_style_text_color(brand, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(brand, &hypurple_geist_semibold_20, 0);
    lv_obj_set_style_text_letter_space(brand, 2, 0);
    lv_obj_center(brand);
}

void updatePumpScreen()
{
    lv_label_set_text_fmt(pump_value_label, "%d", pump_state.desired_percent);
    lv_arc_set_value(pump_arc, pump_state.desired_percent);
    lv_obj_align(pump_value_label, LV_ALIGN_CENTER, -17, 3);
    lv_obj_align_to(pump_percent_label, pump_value_label, LV_ALIGN_OUT_RIGHT_MID, 5, 18);
    lv_obj_set_style_arc_color(pump_arc, lv_color_hex(kPumpStyle.accent_color), LV_PART_INDICATOR);

    if (pump_state.paused && pump_state.desired_percent > 0) {
        lv_obj_clear_flag(pump_pause_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(pump_pause_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (pump_state.desired_percent == 0) {
        lv_obj_clear_flag(pump_zero_marker, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(pump_zero_marker, LV_OBJ_FLAG_HIDDEN);
    }
}

void updatePumpConnectionIndicator(bool connected)
{
    pump_controller_connected = connected;
    lv_obj_set_style_bg_color(
        pump_connection_dot,
        lv_color_hex(connected ? 0x4F9B69 : 0x65615F),
        0);
    if (connected) {
        lv_obj_add_flag(pump_offline_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(pump_offline_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (compressor_connection_dot != nullptr) {
        lv_obj_set_style_bg_color(
            compressor_connection_dot,
            lv_color_hex(connected ? 0x4F9B69 : 0x65615F),
            0);
        lv_obj_align(
            compressor_connection_dot,
            LV_ALIGN_CENTER,
            connected ? 0 : -37,
            kCompressorDisplayGeometry.connection_y - kCompressorDisplayGeometry.center);
    }
    if (compressor_offline_label != nullptr) {
        if (connected) {
            lv_obj_add_flag(compressor_offline_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(compressor_offline_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void alignCompressorValue(lv_coord_t y)
{
    constexpr lv_coord_t kUnitGap = 5;
    constexpr lv_coord_t kUnitBaselineOffset = 18;
    lv_obj_update_layout(compressor_percent_label);
    const lv_coord_t unit_offset = -(lv_obj_get_width(compressor_percent_label) + kUnitGap) / 2;
    lv_obj_align(compressor_value_label, LV_ALIGN_CENTER, unit_offset, y);
    lv_obj_align_to(
        compressor_percent_label,
        compressor_value_label,
        LV_ALIGN_OUT_RIGHT_MID,
        kUnitGap,
        kUnitBaselineOffset);
}

void createPumpScreen()
{
    pump_screen = lv_obj_create(nullptr);
    configureScreen(pump_screen, kPumpStyle.background_color);

    pump_arc = lv_arc_create(pump_screen);
    lv_obj_set_size(pump_arc, kPumpStyle.ring_diameter, kPumpStyle.ring_diameter);
    lv_obj_center(pump_arc);
    lv_arc_set_rotation(pump_arc, 270);
    lv_arc_set_bg_angles(pump_arc, 0, 360);
    lv_arc_set_range(pump_arc, kPumpValueMin, kPumpValueMax);
    lv_obj_remove_style(pump_arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(pump_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(pump_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_outline_width(pump_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(pump_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(pump_arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(pump_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(pump_arc, kPumpStyle.ring_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(pump_arc, lv_color_hex(kPumpStyle.track_color), LV_PART_MAIN);
    lv_obj_set_style_arc_width(pump_arc, kPumpStyle.ring_width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(pump_arc, lv_color_hex(kPumpStyle.accent_color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(pump_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(pump_arc, true, LV_PART_INDICATOR);

    pump_zero_marker = lv_obj_create(pump_screen);
    lv_obj_set_size(pump_zero_marker, 8, 8);
    lv_obj_set_style_radius(pump_zero_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pump_zero_marker, lv_color_hex(kPumpStyle.accent_color), 0);
    lv_obj_set_style_bg_opa(pump_zero_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pump_zero_marker, 0, 0);
    lv_obj_set_style_pad_all(pump_zero_marker, 0, 0);
    lv_obj_clear_flag(pump_zero_marker, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(pump_zero_marker, LV_ALIGN_CENTER, 0, -(kPumpStyle.ring_diameter / 2 - kPumpStyle.ring_width / 2));

    lv_obj_t *title = lv_label_create(pump_screen);
    lv_label_set_text(title, "PUMPE");
    lv_obj_set_style_text_color(title, lv_color_hex(kPumpStyle.secondary_text_color), 0);
    lv_obj_set_style_text_font(title, kPumpStyle.label_font, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -69);

    pump_value_label = lv_label_create(pump_screen);
    lv_obj_set_style_text_color(pump_value_label, lv_color_hex(kPumpStyle.primary_text_color), 0);
    lv_obj_set_style_text_font(pump_value_label, kPumpStyle.value_font, 0);

    pump_percent_label = lv_label_create(pump_screen);
    lv_label_set_text(pump_percent_label, "%");
    lv_obj_set_style_text_color(pump_percent_label, lv_color_hex(kPumpStyle.secondary_text_color), 0);
    lv_obj_set_style_text_font(pump_percent_label, kPumpStyle.unit_font, 0);

    pump_pause_label = lv_label_create(pump_screen);
    lv_label_set_text(pump_pause_label, "PAUSE");
    lv_obj_set_style_text_color(pump_pause_label, lv_color_hex(kPumpStyle.accent_color), 0);
    lv_obj_set_style_text_font(pump_pause_label, &hypurple_geist_semibold_20, 0);
    lv_obj_set_style_text_letter_space(pump_pause_label, 2, 0);
    lv_obj_align(pump_pause_label, LV_ALIGN_CENTER, 0, 75);

    pump_connection_dot = lv_obj_create(pump_screen);
    lv_obj_set_size(pump_connection_dot, 7, 7);
    lv_obj_set_style_radius(pump_connection_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pump_connection_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pump_connection_dot, 0, 0);
    lv_obj_set_style_pad_all(pump_connection_dot, 0, 0);
    lv_obj_clear_flag(pump_connection_dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(pump_connection_dot, LV_ALIGN_CENTER, -37, 119);

    pump_offline_label = lv_label_create(pump_screen);
    lv_label_set_text(pump_offline_label, "OFFLINE");
    lv_obj_set_style_text_color(pump_offline_label, lv_color_hex(0x65615F), 0);
    lv_obj_set_style_text_font(pump_offline_label, &hypurple_geist_semibold_14, 0);
    lv_obj_set_style_text_letter_space(pump_offline_label, 1, 0);
    lv_obj_align(pump_offline_label, LV_ALIGN_CENTER, 3, 119);

    updatePumpScreen();
    updatePumpConnectionIndicator(false);
    lv_obj_add_event_cb(pump_screen, handleScreenGesture, LV_EVENT_GESTURE, nullptr);
    for (uint32_t index = 0; index < lv_obj_get_child_cnt(pump_screen); ++index) {
        lv_obj_add_flag(lv_obj_get_child(pump_screen, index), LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

void updateCompressorScreen()
{
    const int value = clampCompressorPercent(
        compressor_state.paused ? compressor_state.resume_percent : compressor_state.actual_percent);
    lv_arc_set_value(compressor_arc, value);
    if (compressor_state.paused) {
        lv_label_set_text_fmt(compressor_value_label, "%d", value);
        lv_obj_clear_flag(compressor_value_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(compressor_percent_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(compressor_pause_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_off_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_zero_marker, LV_OBJ_FLAG_HIDDEN);
        alignCompressorValue(
            kCompressorDisplayGeometry.paused_value_y - kCompressorDisplayGeometry.center);
    } else if (value == 0) {
        lv_obj_add_flag(compressor_value_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_percent_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_pause_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(compressor_off_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(compressor_zero_marker, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text_fmt(compressor_value_label, "%d", value);
        lv_obj_clear_flag(compressor_value_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(compressor_percent_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_pause_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_off_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(compressor_zero_marker, LV_OBJ_FLAG_HIDDEN);
        alignCompressorValue(
            kCompressorDisplayGeometry.running_value_y - kCompressorDisplayGeometry.center);
    }
}

void createCompressorScreen()
{
    compressor_screen = lv_obj_create(nullptr);
    configureScreen(compressor_screen, 0x000000);

    compressor_arc = lv_arc_create(compressor_screen);
    const lv_coord_t compressor_ring_diameter =
        2 * kCompressorDisplayGeometry.ring_radius + kCompressorDisplayGeometry.progress_width;
    lv_obj_set_size(compressor_arc, compressor_ring_diameter, compressor_ring_diameter);
    lv_obj_center(compressor_arc);
    lv_arc_set_rotation(compressor_arc, 270);
    lv_arc_set_bg_angles(compressor_arc, 0, 360);
    lv_arc_set_range(compressor_arc, 0, 100);
    lv_obj_remove_style(compressor_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(compressor_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(compressor_arc, kCompressorDisplayGeometry.track_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(compressor_arc, lv_color_hex(kCompressorTrackColor), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(compressor_arc, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_arc_width(compressor_arc, kCompressorDisplayGeometry.progress_width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(compressor_arc, lv_color_hex(kCompressorAccentColor), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(compressor_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(compressor_arc, true, LV_PART_INDICATOR);

    compressor_zero_marker = lv_obj_create(compressor_screen);
    lv_obj_set_size(
        compressor_zero_marker,
        2 * kCompressorDisplayGeometry.zero_marker_radius,
        2 * kCompressorDisplayGeometry.zero_marker_radius);
    lv_obj_set_style_radius(compressor_zero_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compressor_zero_marker, lv_color_hex(kCompressorAccentColor), 0);
    lv_obj_set_style_bg_opa(compressor_zero_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(compressor_zero_marker, 0, 0);
    lv_obj_set_style_pad_all(compressor_zero_marker, 0, 0);
    lv_obj_clear_flag(compressor_zero_marker, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(compressor_zero_marker, LV_ALIGN_CENTER, 0, -kCompressorDisplayGeometry.ring_radius);

    lv_obj_t *title = lv_label_create(compressor_screen);
    lv_label_set_text(title, "COMPRESSOR");
    lv_obj_set_style_text_color(title, lv_color_hex(kPumpStyle.secondary_text_color), 0);
    lv_obj_set_style_text_font(title, &hypurple_geist_semibold_20, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(
        title,
        LV_ALIGN_CENTER,
        0,
        kCompressorDisplayGeometry.label_y - kCompressorDisplayGeometry.center);

    compressor_value_label = lv_label_create(compressor_screen);
    lv_obj_set_style_text_color(compressor_value_label, lv_color_hex(kPumpStyle.primary_text_color), 0);
    lv_obj_set_style_text_font(compressor_value_label, kPumpStyle.value_font, 0);

    compressor_percent_label = lv_label_create(compressor_screen);
    lv_label_set_text(compressor_percent_label, "%");
    lv_obj_set_style_text_color(compressor_percent_label, lv_color_hex(kPumpStyle.secondary_text_color), 0);
    lv_obj_set_style_text_font(compressor_percent_label, kPumpStyle.unit_font, 0);

    compressor_off_label = lv_label_create(compressor_screen);
    lv_label_set_text(compressor_off_label, "OFF");
    lv_obj_set_style_text_color(compressor_off_label, lv_color_hex(kPumpStyle.primary_text_color), 0);
    lv_obj_set_style_text_font(compressor_off_label, &hypurple_geist_semibold_20, 0);
    lv_obj_set_style_text_letter_space(compressor_off_label, 2, 0);
    lv_obj_align(compressor_off_label, LV_ALIGN_CENTER, 0, 3);

    compressor_pause_label = lv_label_create(compressor_screen);
    lv_label_set_text(compressor_pause_label, "PAUSED");
    lv_obj_set_style_text_color(compressor_pause_label, lv_color_hex(kCompressorAccentColor), 0);
    lv_obj_set_style_text_font(compressor_pause_label, &hypurple_geist_semibold_20, 0);
    lv_obj_set_style_text_letter_space(compressor_pause_label, 2, 0);
    lv_obj_align(
        compressor_pause_label,
        LV_ALIGN_CENTER,
        0,
        kCompressorDisplayGeometry.paused_y - kCompressorDisplayGeometry.center);

    compressor_connection_dot = lv_obj_create(compressor_screen);
    lv_obj_set_size(compressor_connection_dot, 7, 7);
    lv_obj_set_style_radius(compressor_connection_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(compressor_connection_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(compressor_connection_dot, 0, 0);
    lv_obj_set_style_pad_all(compressor_connection_dot, 0, 0);
    lv_obj_clear_flag(compressor_connection_dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(
        compressor_connection_dot,
        LV_ALIGN_CENTER,
        -37,
        kCompressorDisplayGeometry.connection_y - kCompressorDisplayGeometry.center);

    compressor_offline_label = lv_label_create(compressor_screen);
    lv_label_set_text(compressor_offline_label, "OFFLINE");
    lv_obj_set_style_text_color(compressor_offline_label, lv_color_hex(0x65615F), 0);
    lv_obj_set_style_text_font(compressor_offline_label, &hypurple_geist_semibold_14, 0);
    lv_obj_set_style_text_letter_space(compressor_offline_label, 1, 0);
    lv_obj_align(
        compressor_offline_label,
        LV_ALIGN_CENTER,
        3,
        kCompressorDisplayGeometry.connection_y - kCompressorDisplayGeometry.center);

    updateCompressorScreen();
    updatePumpConnectionIndicator(false);
    lv_obj_add_event_cb(compressor_screen, handleScreenGesture, LV_EVENT_GESTURE, nullptr);
    for (uint32_t index = 0; index < lv_obj_get_child_cnt(compressor_screen); ++index) {
        lv_obj_add_flag(lv_obj_get_child(compressor_screen, index), LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

struct ColorTestCell {
    const char *label;
    uint32_t background;
    uint32_t foreground;
};

constexpr ColorTestCell kColorTestCells[] = {
    {"RED", 0xFF0000, 0xFFFFFF},
    {"GREEN", 0x00FF00, 0x000000},
    {"BLUE", 0x0000FF, 0xFFFFFF},
    {"WHITE", 0xFFFFFF, 0x000000},
    {"GRAY", 0x808080, 0xFFFFFF},
    {"AMBER", 0xC78B28, 0x000000},
};

void createColorTestCell(
    lv_obj_t *parent,
    const ColorTestCell &cell,
    lv_coord_t x,
    lv_coord_t y,
    lv_coord_t width,
    lv_coord_t height)
{
    lv_obj_t *field = lv_obj_create(parent);
    lv_obj_set_pos(field, x, y);
    lv_obj_set_size(field, width, height);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(field, lv_color_hex(cell.background), 0);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(field, 0, 0);
    lv_obj_set_style_outline_width(field, 0, 0);
    lv_obj_set_style_shadow_width(field, 0, 0);
    lv_obj_set_style_pad_all(field, 0, 0);
    lv_obj_set_style_radius(field, 0, 0);

    lv_obj_t *label = lv_label_create(field);
    lv_label_set_text(label, cell.label);
    lv_obj_set_style_text_color(label, lv_color_hex(cell.foreground), 0);
    lv_obj_set_style_text_font(label, &hypurple_geist_semibold_14, 0);
    lv_obj_center(label);
}

void createFontTestRow(lv_obj_t *parent, const char *size_label, const lv_font_t *font, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, size_label);
    lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(label, &hypurple_geist_semibold_14, 0);
    lv_obj_set_pos(label, 52, y + 4);

    lv_obj_t *sample = lv_label_create(parent);
    lv_label_set_text(sample, "PUMPE");
    lv_obj_set_style_text_color(sample, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(sample, font, 0);
    lv_obj_set_pos(sample, 154, y);
}

void createColorTestScreen()
{
    color_test_screen = lv_obj_create(nullptr);
    configureScreen(color_test_screen, 0x000000);

    constexpr lv_coord_t kGridHeight = 246;
    constexpr lv_coord_t kCellHeight = kGridHeight / 2;
    constexpr lv_coord_t kCellWidth = ESP_PANEL_LCD_WIDTH / 3;

    for (size_t index = 0; index < 6; ++index) {
        const lv_coord_t column = index % 3;
        const lv_coord_t row = index / 3;
        const lv_coord_t x = column * kCellWidth;
        const lv_coord_t width = column == 2 ? ESP_PANEL_LCD_WIDTH - x : kCellWidth;
        createColorTestCell(
            color_test_screen,
            kColorTestCells[index],
            x,
            row * kCellHeight,
            width,
            kCellHeight);
    }

    createFontTestRow(color_test_screen, "14 PX", &hypurple_geist_semibold_14, 258);
    createFontTestRow(color_test_screen, "18 PX", &hypurple_geist_semibold_18, 301);
    createFontTestRow(color_test_screen, "22 PX", &hypurple_geist_semibold_22, 346);
    createFontTestRow(color_test_screen, "26 PX", &hypurple_geist_semibold_26, 395);
}

HmiScreen toHmiScreen(SmartKnobScreenKind kind)
{
    switch (kind) {
        case SmartKnobScreenKind::MiniPump:
            return HmiScreen::Pump;
        case SmartKnobScreenKind::CompressorFlow:
            return HmiScreen::Compressor;
    }
    return HmiScreen::Pump;
}

lv_obj_t *screenObject(HmiScreen screen)
{
    switch (screen) {
        case HmiScreen::Pump:
            return pump_screen;
        case HmiScreen::Compressor:
            return compressor_screen;
        case HmiScreen::ColorDiagnostics:
            return color_test_screen;
    }
    return pump_screen;
}

void showConfiguredScreen(size_t index)
{
    if (screen_configuration.count == 0 || index >= screen_configuration.count) return;
    active_screen_index = index;
    active_screen = toHmiScreen(screen_configuration.screens[index]);
    lv_scr_load(screenObject(active_screen));
    hmi_phase = HmiPhase::Interactive;
    Serial.printf("Smart Knob screen: %s\n", smartKnobScreenId(screen_configuration.screens[index]));
}

void requestConfiguredScreen(size_t index)
{
    if (index >= screen_configuration.count || index == active_screen_index) return;
    if (active_screen == HmiScreen::Pump && pump_state.desired_percent > 0) {
        pending_screen_index = static_cast<int>(index);
        pump_state = resetPumpState();
        updatePumpScreen();
        controller_client.setDesiredPumpState(pump_state);
        Serial.println("Screen change waiting for Pump STOP confirmation");
        return;
    }
    if (
        active_screen == HmiScreen::Compressor &&
        (compressor_state.actual_percent > 0 || compressor_state.paused)
    ) {
        pending_screen_index = static_cast<int>(index);
        compressor_state = stoppedCompressorState();
        updateCompressorScreen();
        controller_client.setDesiredCompressorState(compressor_state);
        Serial.println("Screen change waiting for Compressor OFF confirmation");
        return;
    }
    showConfiguredScreen(index);
}

void handleScreenGesture(lv_event_t *event)
{
    (void)event;
    if (hmi_phase != HmiPhase::Interactive || screen_configuration.count < 2) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_get_act());
    const int step = direction == LV_DIR_LEFT ? 1 : direction == LV_DIR_RIGHT ? -1 : 0;
    if (step == 0) return;
    requestConfiguredScreen(resolveBoundedScreenIndex(
        active_screen_index,
        step,
        screen_configuration.count));
}

void showPostWelcomeScreen(lv_timer_t *timer)
{
    (void)timer;

    if (kShowColorDiagnostics) {
        lv_scr_load(color_test_screen);
        active_screen = HmiScreen::ColorDiagnostics;
        hmi_phase = HmiPhase::Interactive;
        Serial.println("Color test screen");
    } else {
        showConfiguredScreen(0);
    }

    if (welcome_screen != nullptr) {
        lv_obj_del_async(welcome_screen);
        welcome_screen = nullptr;
    }
}

void adjustActiveValue(int delta)
{
    lvgl_port_lock(-1);

    if (hmi_phase != HmiPhase::Interactive) {
        lvgl_port_unlock();
        return;
    }

    if (active_screen == HmiScreen::Compressor) {
        const CompressorControlState next_state = applyCompressorDelta(compressor_state, delta);
        if (
            next_state.actual_percent == compressor_state.actual_percent &&
            next_state.paused == compressor_state.paused &&
            next_state.resume_percent == compressor_state.resume_percent
        ) {
            lvgl_port_unlock();
            return;
        }
        compressor_state = next_state;
        updateCompressorScreen();
        lvgl_port_unlock();
        controller_client.setDesiredCompressorState(compressor_state);
        Serial.printf(
            "Compressor command actual=%u%% paused=%s resume=%u%%\n",
            compressor_state.actual_percent,
            compressor_state.paused ? "true" : "false",
            compressor_state.resume_percent);
        return;
    }

    if (active_screen != HmiScreen::Pump) {
        lvgl_port_unlock();
        return;
    }

    const PumpControlState next_state = adjustPumpState(pump_state, delta);

    if (next_state.desired_percent == pump_state.desired_percent) {
        lvgl_port_unlock();
        return;
    }

    pump_state = next_state;
    updatePumpScreen();
    lvgl_port_unlock();

    controller_client.setDesiredPumpState(pump_state);

    Serial.printf("Pump value %d%%\n", pump_state.desired_percent);
}

void applyControllerPumpState(const PumpControlState &state)
{
    lvgl_port_lock(-1);
    pump_state = normalizePumpState(state);
    updatePumpScreen();
    if (pending_screen_index >= 0 && pump_state.desired_percent > 0) {
        pump_state = resetPumpState();
        updatePumpScreen();
        controller_client.setDesiredPumpState(pump_state);
        lvgl_port_unlock();
        return;
    }
    if (
        pending_screen_index >= 0 &&
        pump_state.desired_percent == 0 &&
        effectivePumpPercent(pump_state) == 0
    ) {
        const size_t target = static_cast<size_t>(pending_screen_index);
        pending_screen_index = -1;
        showConfiguredScreen(target);
    }
    lvgl_port_unlock();
}

void applyControllerCompressorState(const CompressorControlState &state)
{
    lvgl_port_lock(-1);
    compressor_state = state;
    updateCompressorScreen();
    if (
        pending_screen_index >= 0 &&
        (compressor_state.actual_percent > 0 || compressor_state.paused)
    ) {
        compressor_state = stoppedCompressorState();
        updateCompressorScreen();
        controller_client.setDesiredCompressorState(compressor_state);
        lvgl_port_unlock();
        return;
    }
    if (
        pending_screen_index >= 0 &&
        compressor_state.actual_percent == 0 &&
        !compressor_state.paused
    ) {
        const size_t target = static_cast<size_t>(pending_screen_index);
        pending_screen_index = -1;
        showConfiguredScreen(target);
    }
    lvgl_port_unlock();
}

void applyScreenConfiguration(const SmartKnobScreenConfiguration &configuration)
{
    lvgl_port_lock(-1);
    const HmiScreen previous_screen = active_screen;
    screen_configuration = configuration;
    size_t matching_index = configuration.count;
    for (size_t index = 0; index < configuration.count; ++index) {
        if (toHmiScreen(configuration.screens[index]) == previous_screen) {
            matching_index = index;
            break;
        }
    }
    if (matching_index < configuration.count) {
        active_screen_index = matching_index;
    } else if (
        previous_screen == HmiScreen::Pump &&
        pump_state.desired_percent > 0
    ) {
        pending_screen_index = 0;
        pump_state = resetPumpState();
        updatePumpScreen();
        controller_client.setDesiredPumpState(pump_state);
    } else if (
        previous_screen == HmiScreen::Compressor &&
        (compressor_state.actual_percent > 0 || compressor_state.paused)
    ) {
        pending_screen_index = 0;
        compressor_state = stoppedCompressorState();
        updateCompressorScreen();
        controller_client.setDesiredCompressorState(compressor_state);
    } else {
        showConfiguredScreen(0);
    }
    lvgl_port_unlock();
}

void onKnobLeftEventCallback(int count, void *user_data)
{
    (void)count;
    (void)user_data;
    adjustActiveValue(-1);
}

void onKnobRightEventCallback(int count, void *user_data)
{
    (void)count;
    (void)user_data;
    adjustActiveValue(1);
}

void onSingleClick(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    lvgl_port_lock(-1);

    if (hmi_phase != HmiPhase::Interactive) {
        lvgl_port_unlock();
        return;
    }

    if (active_screen == HmiScreen::Compressor) {
        const CompressorControlState requested_state = toggleCompressorPause(compressor_state);
        lvgl_port_unlock();
        controller_client.setDesiredCompressorState(requested_state);
        Serial.printf(
            "Compressor button actual=%u%% paused=%s resume=%u%%\n",
            requested_state.actual_percent,
            requested_state.paused ? "true" : "false",
            requested_state.resume_percent);
        return;
    }

    if (active_screen != HmiScreen::Pump) {
        lvgl_port_unlock();
        return;
    }

    if (pump_state.desired_percent == 0) {
        lvgl_port_unlock();
        return;
    }

    pump_state = togglePumpPause(pump_state);
    updatePumpScreen();
    lvgl_port_unlock();

    controller_client.setDesiredPumpState(pump_state);

    Serial.println(pump_state.paused ? "Pump paused" : "Pump resumed");
}

void onLongPressStart(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    lvgl_port_lock(-1);
    if (hmi_phase != HmiPhase::Interactive) {
        lvgl_port_unlock();
        return;
    }
    if (active_screen == HmiScreen::Compressor) {
        lvgl_port_unlock();
        return;
    }
    if (active_screen != HmiScreen::Pump) {
        lvgl_port_unlock();
        return;
    }
    pump_state = resetPumpState();
    updatePumpScreen();
    lvgl_port_unlock();

    controller_client.setDesiredPumpState(pump_state);
    Serial.println("Pump reset");
}

}  // namespace

void setup()
{
#ifdef BOARD_UEDX46460015_MD50E
    pinMode(17, OUTPUT);
    digitalWrite(17, HIGH);
#endif

    Serial.begin(115200);
    Serial.println("HMI boot");

    ESP_Panel *panel = new ESP_Panel();
    panel->init();

#if LVGL_PORT_AVOID_TEAR
    ESP_PanelBus *lcd_bus = panel->getLcd()->getBus();
#if ESP_PANEL_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_RGB
    static_cast<ESP_PanelBus_RGB *>(lcd_bus)->configRgbBounceBufferSize(LVGL_PORT_RGB_BOUNCE_BUFFER_SIZE);
    static_cast<ESP_PanelBus_RGB *>(lcd_bus)->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#elif ESP_PANEL_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_MIPI_DSI
    static_cast<ESP_PanelBus_DSI *>(lcd_bus)->configDpiFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#endif
#endif

    panel->begin();

    knob = new ESP_Knob(GPIO_NUM_KNOB_PIN_A, GPIO_NUM_KNOB_PIN_B);
    knob->begin();
    knob->attachLeftEventCallback(onKnobRightEventCallback);
    knob->attachRightEventCallback(onKnobLeftEventCallback);

    button = new Button(GPIO_BUTTON_PIN, false);
    button->attachSingleClickEventCb(&onSingleClick, nullptr);
    button->attachLongPressStartEventCb(&onLongPressStart, nullptr);

    lvgl_port_init(panel->getLcd(), panel->getTouch());

    lvgl_port_lock(-1);
    createWelcomeScreen();
    createPumpScreen();
    createCompressorScreen();
    createColorTestScreen();
    lv_scr_load(welcome_screen);
    lv_timer_t *welcome_timer = lv_timer_create(showPostWelcomeScreen, kWelcomeDurationMs, nullptr);
    lv_timer_set_repeat_count(welcome_timer, 1);
    lvgl_port_unlock();

    controller_client.begin(
        applyControllerPumpState,
        applyControllerCompressorState,
        applyScreenConfiguration);

    Serial.println("Welcome screen");
}

void loop()
{
    controller_client.loop();
    const bool connected = controller_client.connected();
    if (connected != pump_controller_connected) {
        lvgl_port_lock(-1);
        updatePumpConnectionIndicator(connected);
        lvgl_port_unlock();
    }
    delay(50);
}
