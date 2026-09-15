#pragma once

#include <Arduino.h>

#include "pump_control_state.h"

using PumpStateCallback = void (*)(const PumpControlState &state);

class HypurpleControllerClient {
public:
    void begin(PumpStateCallback callback);
    void loop();
    void setDesiredPumpState(const PumpControlState &state);

    bool configured() const;
    bool connected() const;

private:
    bool fetchPumpState(uint32_t request_revision);
    bool sendPumpState(PumpControlState requested_state, uint32_t request_revision);
    bool sendPumpHeartbeat(uint32_t request_revision);
    bool parsePumpState(const String &payload, PumpControlState &state) const;
    void reconcileResponse(const PumpControlState &state, uint32_t request_revision);
    void handleCommunicationFailure();
    PumpControllerSession snapshotSession() const;
    String endpoint(const char *path) const;

    PumpStateCallback callback_ = nullptr;
    PumpControllerSession session_ = beginPumpControllerSession();
    bool failure_reported_ = false;
    uint32_t last_success_at_ = 0;
    uint32_t last_request_at_ = 0;
    uint32_t last_wifi_attempt_at_ = 0;
    mutable portMUX_TYPE session_mux_ = portMUX_INITIALIZER_UNLOCKED;
};
