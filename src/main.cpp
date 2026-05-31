#include "mbed.h"

using namespace std::chrono_literals;

int main()
{
    printf("Hello world from Fusee_Jeremy_CNES!\r\n");

    while (true) {
        printf("Loop is running: everything compiles and uploads correctly.\r\n");
        ThisThread::sleep_for(1s);
    }
}