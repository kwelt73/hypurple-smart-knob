#pragma once

#include <stddef.h>
#include <stdint.h>

enum class SmartKnobScreenKind : uint8_t {
    MiniPump,
    CompressorFlow,
};

constexpr size_t kSmartKnobMaxScreens = 4;

struct SmartKnobScreenConfiguration {
    SmartKnobScreenKind screens[kSmartKnobMaxScreens];
    size_t count;
};

constexpr SmartKnobScreenConfiguration defaultSmartKnobScreenConfiguration()
{
    return {{
        SmartKnobScreenKind::MiniPump,
        SmartKnobScreenKind::CompressorFlow,
    }, 2};
}

constexpr uint8_t kCompressorMinimumActivePercent = 5;
constexpr uint8_t kCompressorMaximumPercent = 100;

// Canonical 466 x 466 compressor HMI geometry, mirrored by the web simulator.
struct CompressorDisplayGeometry {
    int16_t center;
    int16_t connection_y;
    int16_t label_y;
    int16_t paused_value_y;
    int16_t paused_y;
    int16_t progress_width;
    int16_t ring_radius;
    int16_t running_value_y;
    int16_t track_width;
    int16_t zero_marker_radius;
};

constexpr int16_t kCompressorDisplayResolution = 466;
constexpr CompressorDisplayGeometry kCompressorDisplayGeometry = {
    .center = 233,
    .connection_y = 352,
    .label_y = 164,
    .paused_value_y = 276,
    .paused_y = 208,
    .progress_width = 10,
    .ring_radius = 188,
    .running_value_y = 236,
    .track_width = 3,
    .zero_marker_radius = 4,
};

struct CompressorControlState {
    uint8_t actual_percent;
    bool paused;
    uint8_t resume_percent;
};

constexpr CompressorControlState stoppedCompressorState()
{
    return {0, false, 0};
}

constexpr uint8_t clampCompressorPercent(int value)
{
    return value < 0
        ? 0
        : value > kCompressorMaximumPercent
            ? kCompressorMaximumPercent
            : static_cast<uint8_t>(value);
}

constexpr CompressorControlState applyCompressorDelta(
    CompressorControlState previous,
    int delta)
{
    const uint8_t current = clampCompressorPercent(
        previous.paused ? previous.resume_percent : previous.actual_percent);
    if (delta == 0) return previous;
    if (delta > 0) {
        const uint8_t next = current == 0
            ? kCompressorMinimumActivePercent
            : clampCompressorPercent(static_cast<int>(current) + delta);
        return previous.paused ? CompressorControlState{0, true, next} : CompressorControlState{next, false, 0};
    }
    if (current <= kCompressorMinimumActivePercent) return stoppedCompressorState();
    const uint8_t next = clampCompressorPercent(static_cast<int>(current) + delta);
    if (next < kCompressorMinimumActivePercent) return stoppedCompressorState();
    return previous.paused ? CompressorControlState{0, true, next} : CompressorControlState{next, false, 0};
}

constexpr CompressorControlState toggleCompressorPause(CompressorControlState previous)
{
    if (previous.paused && previous.resume_percent >= kCompressorMinimumActivePercent) {
        return {clampCompressorPercent(previous.resume_percent), false, 0};
    }
    if (previous.actual_percent >= kCompressorMinimumActivePercent) {
        return {0, true, clampCompressorPercent(previous.actual_percent)};
    }
    return {kCompressorMinimumActivePercent, false, 0};
}

constexpr size_t resolveBoundedScreenIndex(size_t current, int direction, size_t count)
{
    if (count == 0 || direction == 0) return current;
    if (direction < 0) return current == 0 ? 0 : current - 1;
    return current + 1 >= count ? count - 1 : current + 1;
}

constexpr const char *smartKnobScreenId(SmartKnobScreenKind kind)
{
    switch (kind) {
        case SmartKnobScreenKind::MiniPump:
            return "mini-pump";
        case SmartKnobScreenKind::CompressorFlow:
            return "compressor-flow";
    }
    return "mini-pump";
}

static_assert(resolveBoundedScreenIndex(0, -1, 2) == 0);
static_assert(resolveBoundedScreenIndex(0, 1, 2) == 1);
static_assert(resolveBoundedScreenIndex(1, 1, 2) == 1);
static_assert(applyCompressorDelta({0, false, 0}, 1).actual_percent == 5);
static_assert(applyCompressorDelta({5, false, 0}, 1).actual_percent == 6);
static_assert(applyCompressorDelta({6, false, 0}, -1).actual_percent == 5);
static_assert(applyCompressorDelta({5, false, 0}, -1).actual_percent == 0);
static_assert(applyCompressorDelta({100, false, 0}, 1).actual_percent == 100);
static_assert(applyCompressorDelta({0, false, 0}, -1).actual_percent == 0);
static_assert(applyCompressorDelta({0, true, 25}, 1).resume_percent == 26);
static_assert(applyCompressorDelta({0, true, 6}, -1).resume_percent == 5);
static_assert(!applyCompressorDelta({0, true, 5}, -1).paused);
static_assert(toggleCompressorPause({0, false, 0}).actual_percent == 5);
static_assert(toggleCompressorPause({25, false, 0}).paused);
static_assert(toggleCompressorPause({25, false, 0}).resume_percent == 25);
static_assert(toggleCompressorPause({0, true, 100}).actual_percent == 100);
static_assert(kCompressorDisplayGeometry.label_y < kCompressorDisplayGeometry.paused_y);
static_assert(kCompressorDisplayGeometry.paused_y + 10 < kCompressorDisplayGeometry.paused_value_y - 42);
static_assert(kCompressorDisplayGeometry.paused_value_y + 42 < kCompressorDisplayGeometry.connection_y);
