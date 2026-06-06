#pragma once

#include "mbed.h"

// ServoOutput est un petit adaptateur: angle (degres) -> signal PWM.
// Il encapsule les details de pulsewidth pour garder main.cpp lisible.
class ServoOutput {
public:
    explicit ServoOutput(PinName pin,
                         int minPulseUs = 1000,
                         int maxPulseUs = 2000,
                         int startAngleDeg = 0);

    void writeAngle(int angleDeg);
    int lastAngle() const;

private:
    PwmOut pwm_;
    int minPulseUs_;
    int maxPulseUs_;
    int lastAngleDeg_;
};
