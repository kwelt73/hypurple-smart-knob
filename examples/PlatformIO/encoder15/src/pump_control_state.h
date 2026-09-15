#pragma once

#include <cstdint>

struct PumpControlState {
    int desired_percent;
    bool paused;
};

enum class PumpControllerRequest {
    StateWrite,
    Heartbeat,
    StatePoll,
};

struct PumpControllerSession {
    PumpControlState state;
    bool dirty;
    bool read_before_write;
    bool connected;
    uint32_t revision;
};

constexpr int clampPumpPercent(int value)
{
    return value < 0 ? 0 : (value > 100 ? 100 : value);
}

constexpr PumpControlState normalizePumpState(PumpControlState state)
{
    const int desired_percent = clampPumpPercent(state.desired_percent);
    return {desired_percent, desired_percent > 0 && state.paused};
}

constexpr int effectivePumpPercent(PumpControlState state)
{
    state = normalizePumpState(state);
    return state.paused ? 0 : state.desired_percent;
}

constexpr PumpControlState adjustPumpState(PumpControlState state, int delta)
{
    state.desired_percent = clampPumpPercent(state.desired_percent + delta);
    return normalizePumpState(state);
}

constexpr PumpControlState togglePumpPause(PumpControlState state)
{
    state = normalizePumpState(state);
    if (state.desired_percent > 0) {
        state.paused = !state.paused;
    }
    return state;
}

constexpr PumpControlState resetPumpState()
{
    return {0, false};
}

constexpr PumpControllerSession beginPumpControllerSession()
{
    return {resetPumpState(), false, true, false, 0};
}

constexpr PumpControllerSession applyPumpUserInteraction(
    PumpControllerSession session,
    PumpControlState state)
{
    if (!session.connected || session.read_before_write) {
        return session;
    }
    session.state = normalizePumpState(state);
    session.dirty = true;
    ++session.revision;
    return session;
}

constexpr PumpControllerSession markPumpControllerOffline(PumpControllerSession session)
{
    session.state.paused = session.state.desired_percent > 0;
    session.dirty = false;
    session.read_before_write = true;
    session.connected = false;
    ++session.revision;
    return session;
}

constexpr PumpControllerSession reconcilePumpControllerResponse(
    PumpControllerSession session,
    PumpControlState authoritative_state,
    uint32_t request_revision)
{
    session.connected = true;
    session.read_before_write = false;
    if (session.revision == request_revision) {
        session.state = normalizePumpState(authoritative_state);
        session.dirty = false;
    }
    return session;
}

constexpr PumpControllerRequest nextPumpControllerRequest(
    const PumpControllerSession &session
)
{
    if (session.read_before_write) {
        return PumpControllerRequest::StatePoll;
    }
    if (session.dirty) {
        return PumpControllerRequest::StateWrite;
    }
    return effectivePumpPercent(session.state) > 0
        ? PumpControllerRequest::Heartbeat
        : PumpControllerRequest::StatePoll;
}
