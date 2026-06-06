#pragma once

#include "mbed.h"

#include <cstdint>

// DebouncedInput transforme un signal electrique bruitte en evenement fiable.
// Exemple: un switch mecanique peut "rebondir" pendant quelques ms.
// Cette classe attend que le niveau reste stable avant de le valider.
class DebouncedInput {
public:
    DebouncedInput(PinName pin, bool activeLow, uint32_t debounceMs = 30, PinMode mode = PullUp);

    // A appeler a chaque tour de boucle pour mettre a jour l'etat filtre.
    void update(Kernel::Clock::time_point now);

    // Etat logique stable apres anti-rebond.
    bool isActive() const;

    // Front montant: vrai une seule fois lors du passage 0 -> 1.
    bool rose() const;

    // Front descendant: vrai une seule fois lors du passage 1 -> 0.
    bool fell() const;

private:
    bool readRaw();
    bool normalize(bool raw) const;

    DigitalIn input_;
    bool activeLow_;
    uint32_t debounceMs_;

    bool stableRaw_;
    bool lastRaw_;
    bool stableState_;
    bool rose_;
    bool fell_;
    Kernel::Clock::time_point lastEdgeTs_;
};
