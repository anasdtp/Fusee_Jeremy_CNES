#pragma once

#include "mbed.h"

// UartLogger centralise les traces texte pour le debug terrain.
// Il expose une API minimaliste: ecrire une ligne et lire une commande.
class UartLogger {
public:
    UartLogger(PinName tx, PinName rx, int baud);

    void info(const char *fmt, ...);
    bool readChar(char &c);

private:
    BufferedSerial serial_;
};
