#include "lib/step_queue.h"

#include <Arduino.h>

StepQueue::StepQueue(const uint32_t delayMs) : delayMs_(delayMs) {}

void StepQueue::push(const int step)
{
    pendingStep_ += step;
    pending_ = true;
    lastPushMillis_ = millis();
}

// The unsigned subtraction keeps the elapsed time correct across a
// millis() wraparound.
int StepQueue::update()
{
    if (!pending_ || millis() - lastPushMillis_ < delayMs_) {
        return 0;
    }

    const int netStep = pendingStep_;
    clear();
    return netStep;
}

void StepQueue::clear()
{
    pendingStep_ = 0;
    pending_ = false;
}
