#include "DebouncedInput.h"

using namespace std::chrono;

DebouncedInput::DebouncedInput(PinName pin, bool activeLow, uint32_t debounceMs, PinMode mode)
    : input_(pin),
      activeLow_(activeLow),
      debounceMs_(debounceMs),
      stableRaw_(readRaw()),
      lastRaw_(stableRaw_),
      stableState_(normalize(stableRaw_)),
      rose_(false),
      fell_(false),
      lastEdgeTs_(Kernel::Clock::now()) {
    input_.mode(mode);
}

void DebouncedInput::update(Kernel::Clock::time_point now) {
    // Reset des evenements impulsionnels pour ce cycle.
    rose_ = false;
    fell_ = false;

    const bool raw = readRaw();
    if (raw != lastRaw_) {
        // On memorise l'instant du dernier changement brut.
        lastRaw_ = raw;
        lastEdgeTs_ = now;
    }

    const auto elapsed = duration_cast<milliseconds>(now - lastEdgeTs_).count();
    if (raw != stableRaw_ && elapsed >= static_cast<int64_t>(debounceMs_)) {
        // Le signal est reste stable assez longtemps: on valide la nouvelle valeur.
        stableRaw_ = raw;
        const bool newState = normalize(stableRaw_);
        if (newState != stableState_) {
            // Generation d'evenements de front (utiles pour les boutons).
            rose_ = (!stableState_ && newState);
            fell_ = (stableState_ && !newState);
            stableState_ = newState;
        }
    }
}

bool DebouncedInput::isActive() const {
    return stableState_;
}

bool DebouncedInput::rose() const {
    return rose_;
}

bool DebouncedInput::fell() const {
    return fell_;
}

bool DebouncedInput::readRaw() {
    return input_.read() != 0;
}

bool DebouncedInput::normalize(bool raw) const {
    return activeLow_ ? !raw : raw;
}
