#include <cassert>

#include "../src/pump_control_state.h"
#include "../src/smart_knob_navigation.h"

int main()
{
    const auto boot = resetPumpState();
    assert(boot.desired_percent == 0);
    assert(!boot.paused);
    assert(effectivePumpPercent(boot) == 0);

    const auto one_percent = adjustPumpState(boot, 1);
    assert(one_percent.desired_percent == 1);
    assert(effectivePumpPercent(one_percent) == 1);

    const auto five_percent = adjustPumpState(one_percent, 4);
    assert(five_percent.desired_percent == 5);
    assert(effectivePumpPercent(five_percent) == 5);

    const auto paused = togglePumpPause({20, false});
    assert(paused.paused);
    assert(effectivePumpPercent(paused) == 0);

    const auto adjusted_while_paused = adjustPumpState(paused, 15);
    assert(adjusted_while_paused.desired_percent == 35);
    assert(adjusted_while_paused.paused);
    assert(effectivePumpPercent(adjusted_while_paused) == 0);

    const auto resumed = togglePumpPause(adjusted_while_paused);
    assert(!resumed.paused);
    assert(effectivePumpPercent(resumed) == 35);

    const auto zero = adjustPumpState({1, true}, -1);
    assert(zero.desired_percent == 0);
    assert(!zero.paused);
    assert(effectivePumpPercent(zero) == 0);

    const auto reset = resetPumpState();
    assert(reset.desired_percent == 0);
    assert(!reset.paused);
    assert(effectivePumpPercent(reset) == 0);

    auto session = beginPumpControllerSession();
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StatePoll);

    session = reconcilePumpControllerResponse(session, {0, false}, session.revision);
    assert(session.connected);
    assert(!session.read_before_write);

    session = applyPumpUserInteraction(session, {10, false});
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StateWrite);
    const uint32_t command_revision = session.revision;
    session = reconcilePumpControllerResponse(session, {10, false}, command_revision);
    assert(session.state.desired_percent == 10);
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::Heartbeat);
    session = reconcilePumpControllerResponse(session, {10, false}, session.revision);
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::Heartbeat);

    session = reconcilePumpControllerResponse(session, {0, false}, session.revision);
    assert(session.state.desired_percent == 0);
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StatePoll);
    session = reconcilePumpControllerResponse(session, {25, false}, session.revision);
    assert(session.state.desired_percent == 25);
    session = applyPumpUserInteraction(session, {26, false});
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StateWrite);

    session = reconcilePumpControllerResponse(session, {35, false}, session.revision);
    session = markPumpControllerOffline(session);
    assert(!session.connected);
    assert(session.state.desired_percent == 35);
    assert(effectivePumpPercent(session.state) == 0);
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StatePoll);
    const auto ignored_offline_change = applyPumpUserInteraction(session, {36, false});
    assert(ignored_offline_change.revision == session.revision);
    session = reconcilePumpControllerResponse(session, {0, false}, session.revision);
    assert(session.state.desired_percent == 0);
    assert(!session.dirty);
    session = applyPumpUserInteraction(session, {1, false});
    assert(nextPumpControllerRequest(session) == PumpControllerRequest::StateWrite);

    auto racing_poll = reconcilePumpControllerResponse(
        beginPumpControllerSession(),
        {0, false},
        0);
    const uint32_t poll_revision = racing_poll.revision;
    racing_poll = applyPumpUserInteraction(racing_poll, {10, false});
    racing_poll = reconcilePumpControllerResponse(racing_poll, {0, false}, poll_revision);
    assert(racing_poll.state.desired_percent == 10);
    assert(racing_poll.dirty);

    auto hanbuild = beginHanBuildControllerSession();
    assert(hanbuild.read_before_write);
    assert(hanbuild.state.target.rpm == 0);
    const auto ignored_before_read = applyHanBuildUserInteraction(hanbuild, {60});
    assert(ignored_before_read.state.target.rpm == 0);

    hanbuild = reconcileHanBuildControllerResponse(
        hanbuild,
        {{50}, 48.5F, true},
        hanbuild.revision);
    assert(hanbuild.connected);
    assert(!hanbuild.read_before_write);
    assert(hanbuild.state.target.rpm == 50);
    assert(hanbuild.state.commanded_rpm == 48.5F);
    assert(hanbuild.state.moving);

    const uint32_t stale_poll_revision = hanbuild.revision;
    hanbuild = applyHanBuildUserInteraction(hanbuild, {51});
    assert(hanbuild.dirty);
    hanbuild = reconcileHanBuildControllerResponse(
        hanbuild,
        {{50}, 50.0F, true},
        stale_poll_revision);
    assert(hanbuild.state.target.rpm == 51);
    assert(hanbuild.dirty);

    const uint32_t hanbuild_command_revision = hanbuild.revision;
    hanbuild = reconcileHanBuildControllerResponse(
        hanbuild,
        {{51}, 51.0F, true},
        hanbuild_command_revision);
    assert(hanbuild.state.target.rpm == 51);
    assert(!hanbuild.dirty);

    hanbuild = reconcileHanBuildControllerResponse(
        hanbuild,
        {{0}, 0.0F, false},
        hanbuild.revision,
        false);
    assert(hanbuild.state.target.rpm == 51);
    hanbuild = acknowledgeHanBuildCommand(hanbuild, hanbuild.revision);
    assert(!hanbuild.dirty);
    hanbuild = reconcileHanBuildControllerResponse(
        hanbuild,
        {{0}, 0.0F, false},
        hanbuild.revision,
        true);
    assert(hanbuild.state.target.rpm == 0);
    assert(!hanbuild.state.moving);

    hanbuild = applyHanBuildUserInteraction(hanbuild, {-60});
    assert(hanbuild.state.target.rpm == -60);
    hanbuild = markHanBuildControllerOffline(hanbuild);
    assert(!hanbuild.connected);
    assert(hanbuild.read_before_write);
    assert(!hanbuild.dirty);
    assert(hanbuild.state.target.rpm == 0);
    const auto ignored_offline_hanbuild = applyHanBuildUserInteraction(hanbuild, {-61});
    assert(ignored_offline_hanbuild.state.target.rpm == 0);
}
