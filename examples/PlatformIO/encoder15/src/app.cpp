#include <Arduino.h>
#include <Button.h>
#include <ESP_Knob.h>
#include <ESP_Panel_Library.h>
#include <lvgl.h>

#include "controller_client.h"
#include "fonts/hypurple_fonts.h"
#include "lvgl_port_v8.h"

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

enum class HmiScreen {
    Pump,
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
PumpControlState pump_state = resetPumpState();

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
bool pump_controller_connected = false;

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

void showPumpScreen()
{
    lv_scr_load(pump_screen);
    active_screen = HmiScreen::Pump;
    hmi_phase = HmiPhase::Interactive;
    Serial.println("Pump screen");
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
        showPumpScreen();
    }

    if (welcome_screen != nullptr) {
        lv_obj_del_async(welcome_screen);
        welcome_screen = nullptr;
    }
}

void adjustPumpValue(int delta)
{
    lvgl_port_lock(-1);

    if (hmi_phase != HmiPhase::Interactive || active_screen != HmiScreen::Pump) {
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
    lvgl_port_unlock();
}

void onKnobLeftEventCallback(int count, void *user_data)
{
    (void)count;
    (void)user_data;
    adjustPumpValue(-1);
}

void onKnobRightEventCallback(int count, void *user_data)
{
    (void)count;
    (void)user_data;
    adjustPumpValue(1);
}

void onSingleClick(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    lvgl_port_lock(-1);

    if (hmi_phase != HmiPhase::Interactive || active_screen != HmiScreen::Pump) {
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
    if (hmi_phase != HmiPhase::Interactive || active_screen != HmiScreen::Pump) {
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

    // Touch remains intentionally disabled; input is encoder + primary button.
    // Future screens use swipes for navigation, the encoder for the current
    // parameter, and the button for the primary action.
    lvgl_port_init(panel->getLcd(), nullptr);

    lvgl_port_lock(-1);
    createWelcomeScreen();
    createPumpScreen();
    createColorTestScreen();
    lv_scr_load(welcome_screen);
    lv_timer_t *welcome_timer = lv_timer_create(showPostWelcomeScreen, kWelcomeDurationMs, nullptr);
    lv_timer_set_repeat_count(welcome_timer, 1);
    lvgl_port_unlock();

    controller_client.begin(applyControllerPumpState);

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
