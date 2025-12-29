#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "StatusLED.h"
#include "Strobe.h"

class PowerManager {
public:
    PowerManager(int pin, StatusLED& statusLed, Strobe& strobe);
    void begin();
    void checkWakeup();
    bool isButtonPressed();
    void waitForButtonRelease();
    void enterDeepSleep();

private:
    int _pin;
    StatusLED& _statusLed;
    Strobe& _strobe;
    TaskHandle_t _taskHandle;

    static void powerTask(void* parameter);
};

#endif // POWER_MANAGER_H
