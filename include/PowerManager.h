#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <atomic>
#include "StatusLED.h"
#include "Strobe.h"

class PowerManager {
public:
    PowerManager(int pin, int batteryPin, StatusLED& statusLed, Strobe& strobe);
    void begin();
    void checkWakeup();
    bool isButtonPressed();
    void waitForButtonRelease();
    void enterDeepSleep();
    float getBatteryVoltage();

private:
    int _pin;
    int _batteryPin;
    StatusLED& _statusLed;
    Strobe& _strobe;
    TaskHandle_t _taskHandle;
    TaskHandle_t _batteryTaskHandle;
    std::atomic<float> _lastBatteryVoltage;

    static void powerTask(void* parameter);
    static void measureBatteryTask(void* parameter);
};

#endif // POWER_MANAGER_H
