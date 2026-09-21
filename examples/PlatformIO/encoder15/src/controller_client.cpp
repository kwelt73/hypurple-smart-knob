#include "controller_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cJSON.h>

#ifndef HYPURPLE_WIFI_SSID
#define HYPURPLE_WIFI_SSID ""
#endif

#ifndef HYPURPLE_WIFI_PASSWORD
#define HYPURPLE_WIFI_PASSWORD ""
#endif

#ifndef HYPURPLE_CONTROLLER_URL
#define HYPURPLE_CONTROLLER_URL ""
#endif

#ifndef HYPURPLE_SMART_KNOB_TOKEN
#define HYPURPLE_SMART_KNOB_TOKEN ""
#endif

namespace {

constexpr uint32_t kRequestIntervalMs = 1000;
constexpr uint32_t kCompressorCommandIntervalMs = 200;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kControllerStateStaleAfterMs = 3500;
constexpr uint16_t kHttpTimeoutMs = 750;

constexpr char kDeviceId[] = "DG-0001";
constexpr char kComponentId[] = "bigtreetech-skr-pico-v1";
constexpr char kWifiSsid[] = HYPURPLE_WIFI_SSID;
constexpr char kWifiPassword[] = HYPURPLE_WIFI_PASSWORD;
constexpr char kControllerUrl[] = HYPURPLE_CONTROLLER_URL;
constexpr char kControllerToken[] = HYPURPLE_SMART_KNOB_TOKEN;

const cJSON *findPumpActuator(const cJSON *root)
{
    const cJSON *actuators = cJSON_GetObjectItemCaseSensitive(root, "actuators");
    if (!cJSON_IsArray(actuators)) {
        return nullptr;
    }

    const cJSON *actuator = nullptr;
    cJSON_ArrayForEach(actuator, actuators) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(actuator, "id");
        if (cJSON_IsString(id) && strcmp(id->valuestring, "mini-pump") == 0) {
            return actuator;
        }
    }
    return nullptr;
}

}  // namespace

void HypurpleControllerClient::begin(
    PumpStateCallback callback,
    CompressorStateCallback compressor_callback,
    ScreenConfigurationCallback screen_configuration_callback)
{
    callback_ = callback;
    compressor_callback_ = compressor_callback;
    screen_configuration_callback_ = screen_configuration_callback;
    if (!configured()) {
        Serial.println("Controller client disabled: configuration incomplete");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        Serial.printf(
            "Controller WiFi disconnected (reason %u)\n",
            static_cast<unsigned>(info.wifi_sta_disconnected.reason));
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
        Serial.println("Controller WiFi connected");
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.begin(kWifiSsid, kWifiPassword);
    last_wifi_attempt_at_ = millis();
    Serial.println("Controller client connecting");
}

void HypurpleControllerClient::loop()
{
    if (!configured()) {
        return;
    }

    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
        handleCommunicationFailure();
        if (
            WiFi.status() != WL_IDLE_STATUS &&
            now - last_wifi_attempt_at_ >= kWifiRetryIntervalMs
        ) {
            WiFi.disconnect(false);
            WiFi.begin(kWifiSsid, kWifiPassword);
            last_wifi_attempt_at_ = now;
        }
        return;
    }

    if (!screen_configuration_loaded_) {
        if (now - last_request_at_ < kRequestIntervalMs) return;
        last_request_at_ = now;
        if (!fetchScreenConfiguration()) {
            handleCommunicationFailure();
        }
        return;
    }

    CompressorControlState compressor_state;
    bool compressor_write_pending;
    uint32_t compressor_revision;
    portENTER_CRITICAL(&session_mux_);
    compressor_state = compressor_state_;
    compressor_write_pending = compressor_write_pending_;
    compressor_revision = compressor_revision_;
    portEXIT_CRITICAL(&session_mux_);
    if (
        compressor_write_pending &&
        now - last_compressor_request_at_ >= kCompressorCommandIntervalMs
    ) {
        last_compressor_request_at_ = now;
        if (!sendCompressorState(compressor_state, compressor_revision)) {
            const PumpControllerSession request_session = snapshotSession();
            if (!fetchControllerState(request_session.revision)) {
                handleCommunicationFailure();
            }
        }
        return;
    }

    if (now - last_request_at_ < kRequestIntervalMs) return;
    last_request_at_ = now;

    const PumpControllerSession request_session = snapshotSession();
    bool success = false;
    switch (nextPumpControllerRequest(request_session)) {
        case PumpControllerRequest::StateWrite:
            success = sendPumpState(request_session.state, request_session.revision);
            break;
        case PumpControllerRequest::Heartbeat:
            success = sendPumpHeartbeat(request_session.revision);
            break;
        case PumpControllerRequest::StatePoll:
            success = fetchControllerState(request_session.revision);
            break;
    }
    if (!success) {
        handleCommunicationFailure();
    }
}

void HypurpleControllerClient::setDesiredPumpState(const PumpControlState &state)
{
    portENTER_CRITICAL(&session_mux_);
    session_ = applyPumpUserInteraction(session_, state);
    portEXIT_CRITICAL(&session_mux_);
}

void HypurpleControllerClient::setDesiredCompressorState(
    const CompressorControlState &state)
{
    portENTER_CRITICAL(&session_mux_);
    compressor_state_ = state.paused && state.resume_percent >= kCompressorMinimumActivePercent
        ? CompressorControlState{0, true, clampCompressorPercent(state.resume_percent)}
        : CompressorControlState{clampCompressorPercent(state.actual_percent), false, 0};
    compressor_write_pending_ = true;
    ++compressor_revision_;
    portEXIT_CRITICAL(&session_mux_);
}

bool HypurpleControllerClient::configured() const
{
    return kWifiSsid[0] != '\0' && kControllerUrl[0] != '\0' && kControllerToken[0] != '\0';
}

bool HypurpleControllerClient::connected() const
{
    if (!configured() || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    portENTER_CRITICAL(&session_mux_);
    const bool fresh = session_.connected &&
        millis() - last_success_at_ <= kControllerStateStaleAfterMs;
    portEXIT_CRITICAL(&session_mux_);
    return fresh;
}

bool HypurpleControllerClient::fetchScreenConfiguration()
{
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(endpoint("/smart-knob/screens"))) {
        return false;
    }
    http.addHeader("X-Hypurple-HMI-Token", kControllerToken);
    const int status_code = http.GET();
    const String payload = status_code == HTTP_CODE_OK ? http.getString() : String();
    http.end();
    if (status_code != HTTP_CODE_OK) {
        return false;
    }

    cJSON *root = cJSON_Parse(payload.c_str());
    const cJSON *schema_version = root == nullptr
        ? nullptr
        : cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    const cJSON *screens = root == nullptr
        ? nullptr
        : cJSON_GetObjectItemCaseSensitive(root, "screens");
    SmartKnobScreenConfiguration configuration = {};
    if (!cJSON_IsNumber(schema_version) || schema_version->valueint != 1 || !cJSON_IsArray(screens)) {
        cJSON_Delete(root);
        return false;
    }

    const cJSON *screen = nullptr;
    cJSON_ArrayForEach(screen, screens) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(screen, "id");
        if (!cJSON_IsString(id) || configuration.count >= kSmartKnobMaxScreens) {
            cJSON_Delete(root);
            return false;
        }
        SmartKnobScreenKind kind;
        if (strcmp(id->valuestring, "mini-pump") == 0) {
            kind = SmartKnobScreenKind::MiniPump;
        } else if (strcmp(id->valuestring, "hanbuild-stepper") == 0) {
            continue;
        } else if (strcmp(id->valuestring, "compressor-flow") == 0) {
            kind = SmartKnobScreenKind::CompressorFlow;
        } else {
            cJSON_Delete(root);
            return false;
        }
        for (size_t index = 0; index < configuration.count; ++index) {
            if (configuration.screens[index] == kind) {
                cJSON_Delete(root);
                return false;
            }
        }
        configuration.screens[configuration.count++] = kind;
    }
    cJSON_Delete(root);
    if (configuration.count == 0) {
        return false;
    }

    screen_configuration_loaded_ = true;
    if (screen_configuration_callback_ != nullptr) {
        screen_configuration_callback_(configuration);
    }
    Serial.println("Smart Knob screen configuration loaded");
    return true;
}

bool HypurpleControllerClient::fetchControllerState(uint32_t request_revision)
{
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(endpoint("/skr-pico/status"))) {
        return false;
    }
    const int status_code = http.GET();
    const String payload = status_code == HTTP_CODE_OK ? http.getString() : String();
    http.end();

    PumpControlState state = resetPumpState();
    CompressorControlState compressor_state = stoppedCompressorState();
    if (
        status_code != HTTP_CODE_OK ||
        !parsePumpState(payload, state) ||
        !parseCompressorState(payload, compressor_state)
    ) {
        return false;
    }
    reconcileResponse(state, request_revision);
    if (compressor_callback_ != nullptr) compressor_callback_(compressor_state);
    return true;
}

bool HypurpleControllerClient::sendPumpState(
    PumpControlState requested_state,
    uint32_t request_revision)
{
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(endpoint("/skr-pico/command"))) {
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Hypurple-HMI-Token", kControllerToken);

    const String payload = String("{\"action\":\"skr.actuator.set_pump_state\",") +
        "\"componentId\":\"" + kComponentId + "\"," +
        "\"deviceId\":\"" + kDeviceId + "\"," +
        "\"parameters\":{\"desiredPercent\":" + String(requested_state.desired_percent) +
        ",\"paused\":" + (requested_state.paused ? "true" : "false") + "}}";
    const int status_code = http.POST(payload);
    const String response = status_code == HTTP_CODE_OK ? http.getString() : String();
    http.end();

    PumpControlState state = resetPumpState();
    CompressorControlState compressor_state = stoppedCompressorState();
    if (
        status_code != HTTP_CODE_OK ||
        !parsePumpState(response, state) ||
        !parseCompressorState(response, compressor_state)
    ) {
        return false;
    }
    reconcileResponse(state, request_revision);
    if (compressor_callback_ != nullptr) compressor_callback_(compressor_state);
    return true;
}

bool HypurpleControllerClient::sendPumpHeartbeat(uint32_t request_revision)
{
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(endpoint("/skr-pico/command"))) {
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Hypurple-HMI-Token", kControllerToken);

    const String payload = String("{\"action\":\"skr.actuator.pump_heartbeat\",") +
        "\"componentId\":\"" + kComponentId + "\"," +
        "\"deviceId\":\"" + kDeviceId + "\"," +
        "\"parameters\":{}}";
    const int status_code = http.POST(payload);
    const String response = status_code == HTTP_CODE_OK ? http.getString() : String();
    http.end();

    PumpControlState state = resetPumpState();
    CompressorControlState compressor_state = stoppedCompressorState();
    if (
        status_code != HTTP_CODE_OK ||
        !parsePumpState(response, state) ||
        !parseCompressorState(response, compressor_state)
    ) {
        return false;
    }
    reconcileResponse(state, request_revision);
    if (compressor_callback_ != nullptr) compressor_callback_(compressor_state);
    return true;
}

bool HypurpleControllerClient::sendCompressorState(
    CompressorControlState requested_state,
    uint32_t request_revision)
{
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(endpoint("/skr-pico/command"))) return false;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Hypurple-HMI-Token", kControllerToken);
    const String payload = String("{\"action\":\"skr.compressor.set_output\",") +
        "\"componentId\":\"" + kComponentId + "\"," +
        "\"deviceId\":\"" + kDeviceId + "\"," +
        "\"parameters\":{\"outputPercent\":" +
        String(requested_state.actual_percent) +
        ",\"paused\":" + (requested_state.paused ? "true" : "false") +
        ",\"resumeOutputPercent\":" + String(requested_state.resume_percent) + "}}";
    const int status_code = http.POST(payload);
    const String response = status_code == HTTP_CODE_OK ? http.getString() : String();
    http.end();

    CompressorControlState confirmed = stoppedCompressorState();
    if (status_code != HTTP_CODE_OK || !parseCompressorState(response, confirmed)) return false;
    portENTER_CRITICAL(&session_mux_);
    if (compressor_revision_ == request_revision) {
        compressor_state_ = confirmed;
        compressor_write_pending_ = false;
    }
    failure_reported_ = false;
    last_success_at_ = millis();
    portEXIT_CRITICAL(&session_mux_);
    if (compressor_callback_ != nullptr) compressor_callback_(confirmed);
    return true;
}

void HypurpleControllerClient::reconcileResponse(
    const PumpControlState &state,
    uint32_t request_revision)
{
    portENTER_CRITICAL(&session_mux_);
    const uint32_t revision_before = session_.revision;
    session_ = reconcilePumpControllerResponse(session_, state, request_revision);
    failure_reported_ = false;
    last_success_at_ = millis();
    const PumpControlState reconciled_state = session_.state;
    const bool response_is_current = revision_before == request_revision;
    portEXIT_CRITICAL(&session_mux_);
    if (callback_ != nullptr && response_is_current) {
        callback_(reconciled_state);
    }
}

bool HypurpleControllerClient::parsePumpState(const String &payload, PumpControlState &state) const
{
    cJSON *root = cJSON_Parse(payload.c_str());
    if (root == nullptr) {
        return false;
    }
    const cJSON *pump = findPumpActuator(root);
    if (pump == nullptr) {
        cJSON_Delete(root);
        return false;
    }

    const cJSON *desired = cJSON_GetObjectItemCaseSensitive(pump, "desiredPercent");
    const cJSON *paused = cJSON_GetObjectItemCaseSensitive(pump, "paused");
    const cJSON *effective = cJSON_GetObjectItemCaseSensitive(pump, "effectivePercent");
    const cJSON *active = cJSON_GetObjectItemCaseSensitive(pump, "active");
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(pump, "outputPercent");

    const bool has_new_state =
        cJSON_IsNumber(desired) && cJSON_IsBool(paused) && cJSON_IsNumber(effective);
    const bool has_legacy_state = cJSON_IsBool(active) && cJSON_IsNumber(output);
    if (!has_new_state && !has_legacy_state) {
        cJSON_Delete(root);
        return false;
    }

    state = has_new_state
        ? normalizePumpState({desired->valueint, cJSON_IsTrue(paused) != 0})
        : normalizePumpState({output->valueint, !cJSON_IsTrue(active) && output->valueint > 0});
    const int reported_effective = has_new_state ? effective->valueint : output->valueint;
    if (clampPumpPercent(reported_effective) != effectivePumpPercent(state)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(root);
    return true;
}

bool HypurpleControllerClient::parseCompressorState(
    const String &payload,
    CompressorControlState &state) const
{
    cJSON *root = cJSON_Parse(payload.c_str());
    if (root == nullptr) return false;
    const cJSON *control = cJSON_GetObjectItemCaseSensitive(root, "compressorControl");
    const cJSON *actual = cJSON_IsObject(control)
        ? cJSON_GetObjectItemCaseSensitive(control, "actualOutputPercent")
        : nullptr;
    const cJSON *paused = cJSON_IsObject(control)
        ? cJSON_GetObjectItemCaseSensitive(control, "paused")
        : nullptr;
    const cJSON *resume = cJSON_IsObject(control)
        ? cJSON_GetObjectItemCaseSensitive(control, "resumeOutputPercent")
        : nullptr;
    if (
        !cJSON_IsNumber(actual) || actual->valueint < 0 || actual->valueint > 100 ||
        !cJSON_IsBool(paused) ||
        !cJSON_IsNumber(resume) || resume->valueint < 0 || resume->valueint > 100
    ) {
        cJSON_Delete(root);
        return false;
    }
    const bool is_paused = cJSON_IsTrue(paused) != 0;
    if (
        (is_paused && (actual->valueint != 0 || resume->valueint < kCompressorMinimumActivePercent)) ||
        (!is_paused && resume->valueint != 0)
    ) {
        cJSON_Delete(root);
        return false;
    }
    state = {
        clampCompressorPercent(actual->valueint),
        is_paused,
        clampCompressorPercent(resume->valueint),
    };
    cJSON_Delete(root);
    return true;
}

void HypurpleControllerClient::handleCommunicationFailure()
{
    portENTER_CRITICAL(&session_mux_);
    session_ = markPumpControllerOffline(session_);
    compressor_state_ = stoppedCompressorState();
    compressor_write_pending_ = false;
    ++compressor_revision_;
    screen_configuration_loaded_ = false;
    if (failure_reported_) {
        portEXIT_CRITICAL(&session_mux_);
        return;
    }
    failure_reported_ = true;
    const PumpControlState offline_state = session_.state;
    portEXIT_CRITICAL(&session_mux_);
    if (callback_ != nullptr) {
        callback_(offline_state);
    }
    if (compressor_callback_ != nullptr) {
        compressor_callback_(stoppedCompressorState());
    }
    Serial.println("Controller communication unavailable; pump state forced OFF");
}

PumpControllerSession HypurpleControllerClient::snapshotSession() const
{
    portENTER_CRITICAL(&session_mux_);
    const PumpControllerSession snapshot = session_;
    portEXIT_CRITICAL(&session_mux_);
    return snapshot;
}

String HypurpleControllerClient::endpoint(const char *path) const
{
    String base(kControllerUrl);
    while (base.endsWith("/")) {
        base.remove(base.length() - 1);
    }
    return base + path;
}
