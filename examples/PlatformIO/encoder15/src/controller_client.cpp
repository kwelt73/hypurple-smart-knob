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

void HypurpleControllerClient::begin(PumpStateCallback callback)
{
    callback_ = callback;
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

    if (now - last_request_at_ < kRequestIntervalMs) {
        return;
    }
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
            success = fetchPumpState(request_session.revision);
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

bool HypurpleControllerClient::fetchPumpState(uint32_t request_revision)
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
    if (status_code != HTTP_CODE_OK || !parsePumpState(payload, state)) {
        return false;
    }
    reconcileResponse(state, request_revision);
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
    if (status_code != HTTP_CODE_OK || !parsePumpState(response, state)) {
        return false;
    }
    reconcileResponse(state, request_revision);
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
    if (status_code != HTTP_CODE_OK || !parsePumpState(response, state)) {
        return false;
    }
    reconcileResponse(state, request_revision);
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

void HypurpleControllerClient::handleCommunicationFailure()
{
    portENTER_CRITICAL(&session_mux_);
    session_ = markPumpControllerOffline(session_);
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
