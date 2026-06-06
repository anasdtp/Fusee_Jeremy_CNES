#include "ServoOutput.h"

ServoOutput::ServoOutput(PinName pin, int minPulseUs, int maxPulseUs, int startAngleDeg)
    : pwm_(pin),
      minPulseUs_(minPulseUs),
      maxPulseUs_(maxPulseUs),
      lastAngleDeg_(startAngleDeg) {
    // Servo standard: periode 20 ms (50 Hz).
    pwm_.period_ms(20);
    writeAngle(startAngleDeg);
}

void ServoOutput::writeAngle(int angleDeg) {
    // Protection simple: on borne l'angle dans une plage mecanique classique.
    if (angleDeg < 0) {
        angleDeg = 0;
    }
    if (angleDeg > 180) {
        angleDeg = 180;
    }

    lastAngleDeg_ = angleDeg;

    // Conversion lineaire angle -> largeur d'impulsion.
    const int pulseUs = minPulseUs_ + ((maxPulseUs_ - minPulseUs_) * angleDeg) / 180;
    pwm_.pulsewidth_us(pulseUs);
}

int ServoOutput::lastAngle() const {
    return lastAngleDeg_;
}
