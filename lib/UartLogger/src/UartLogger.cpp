#include "UartLogger.h"

#include <cstdarg>
#include <cstdio>

UartLogger::UartLogger(PinName tx, PinName rx, int baud)
    : serial_(tx, rx, baud) {
    // Non bloquant pour ne jamais figer la boucle de controle.
    serial_.set_blocking(false);
}

void UartLogger::info(const char *fmt, ...) {
    // Formattage printf dans un buffer borne pour eviter les debordements.
    char buffer[160];
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (n <= 0) {
        return;
    }

    // Une ligne complete = message + fin de ligne CRLF.
    serial_.write(buffer, static_cast<size_t>(n));
    serial_.write("\r\n", 2);
}

bool UartLogger::readChar(char &c) {
    return serial_.read(&c, 1) == 1;
}
