#include <stdio.h>
#include <unistd.h>

#include <wiringx.h>

int main() {
    int DUO_GPIO = 0;

    if(wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        return -1;
    }

    if(wiringXValidGPIO(DUO_GPIO) != 0) {
        printf("Invalid GPIO %d\n", DUO_GPIO);
    }

    pinMode(DUO_GPIO, PINMODE_OUTPUT);

    while(1) {
        printf("Duo GPIO (wiringX) %d: High\n", DUO_GPIO);
        digitalWrite(DUO_GPIO, HIGH);
        sleep(1);
        printf("Duo GPIO (wiringX) %d: Low\n", DUO_GPIO);
        digitalWrite(DUO_GPIO, LOW);
        sleep(1);
    }

    return 0;
}
