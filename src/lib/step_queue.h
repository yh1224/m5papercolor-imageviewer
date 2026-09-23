#pragma once

#include <cstdint>

// Accumulates navigation steps and releases their net sum once no new
// step has been pushed for `delayMs`, so a burst of steps is applied as a
// single move.
class StepQueue {
public:
    explicit StepQueue(uint32_t delayMs);

    // Adds `step` to the pending sum and restarts the idle timer.
    void push(int step);

    // Call every loop cycle. Once the delay has elapsed since the last push,
    // clears the queue and returns the net pending step; returns 0 until
    // then.
    int update();

    // Discards any pending steps.
    void clear();

private:
    const uint32_t delayMs_;
    int pendingStep_ = 0;
    bool pending_ = false;
    uint32_t lastPushMillis_ = 0;
};
