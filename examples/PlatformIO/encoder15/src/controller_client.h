#pragma once

#include <Arduino.h>

#include "pump_control_state.h"
#include "smart_knob_navigation.h"

using PumpStateCallback = void (*)(const PumpControlState &state);
using CompressorStateCallback = void (*)(const CompressorControlState &state);
using ScreenConfigurationCallback = void (*)(const SmartKnobScreenConfiguration &configuration);
using HanBuildSafetyStopCallback = void (*)();

class HypurpleControllerClient {
public:
    void begin(
        PumpStateCallback callback,
        CompressorStateCallback compressor_callback,
        ScreenConfigurationCallback screen_configuration_callback,
        HanBuildSafetyStopCallback hanbuild_safety_stop_callback);
    void loop();
    void setDesiredPumpState(const PumpControlState &state);
    void setDesiredCompressorState(const CompressorControlState &state);
    void setHanBuildSpeedTarget(HanBuildSpeedTarget target);
    void stopHanBuild();

    bool configured() const;
    bool connected() const;

private:
    bool fetchControllerState(uint32_t request_revision);
    bool fetchScreenConfiguration();
    bool sendPumpState(PumpControlState requested_state, uint32_t request_revision);
    bool sendPumpHeartbeat(uint32_t request_revision);
    bool sendCompressorState(CompressorControlState requested_state, uint32_t request_revision);
    bool sendHanBuildSegment(HanBuildSpeedTarget target);
    bool sendHanBuildHeartbeat();
    bool parsePumpState(const String &payload, PumpControlState &state) const;
    bool parseCompressorState(const String &payload, CompressorControlState &state) const;
    void reconcileResponse(const PumpControlState &state, uint32_t request_revision);
    void handleCommunicationFailure();
    PumpControllerSession snapshotSession() const;
    String endpoint(const char *path) const;

    PumpStateCallback callback_ = nullptr;
    CompressorStateCallback compressor_callback_ = nullptr;
    ScreenConfigurationCallback screen_configuration_callback_ = nullptr;
    HanBuildSafetyStopCallback hanbuild_safety_stop_callback_ = nullptr;
    PumpControllerSession session_ = beginPumpControllerSession();
    bool failure_reported_ = false;
    bool screen_configuration_loaded_ = false;
    uint32_t last_success_at_ = 0;
    uint32_t last_request_at_ = 0;
    uint32_t last_hanbuild_segment_at_ = 0;
    uint32_t last_compressor_request_at_ = 0;
    uint32_t last_wifi_attempt_at_ = 0;
    HanBuildSpeedTarget hanbuild_target_ = stoppedHanBuildSpeedTarget();
    bool hanbuild_write_pending_ = false;
    uint32_t hanbuild_revision_ = 0;
    CompressorControlState compressor_state_ = stoppedCompressorState();
    bool compressor_write_pending_ = false;
    uint32_t compressor_revision_ = 0;
    mutable portMUX_TYPE session_mux_ = portMUX_INITIALIZER_UNLOCKED;
};
