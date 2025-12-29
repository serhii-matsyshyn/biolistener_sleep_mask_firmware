#include "PowerManager.h"

PowerManager::PowerManager(int pin, StatusLED& statusLed, Strobe& strobe)
    : _pin(pin), _statusLed(statusLed), _strobe(strobe), _taskHandle(NULL) {}

void PowerManager::begin() {
    xTaskCreatePinnedToCore(
        powerTask,
        "powerTask",
        4096,
        this,
        1,
        &_taskHandle,
        0 // Core 0
    );
}

void PowerManager::checkWakeup() {
    pinMode(_pin, INPUT_PULLDOWN);
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Wakeup caused by external signal using RTC_IO");
        
        unsigned long startHold = millis();
        bool held = true;
        while (millis() - startHold < 3000) {
            if (digitalRead(_pin) == LOW) {
                held = false;
                break;
            }
            delay(10);
        }

        if (!held) {
            Serial.println("Button released too early, going back to sleep");
            _statusLed.off();
            esp_sleep_enable_ext0_wakeup((gpio_num_t)_pin, 1);
            esp_deep_sleep_start();
        } else {
            Serial.println("wake up deep sleep");
            _statusLed.setColor(StatusLED::YELLOW);
            waitForButtonRelease();
        }
    }
}

bool PowerManager::isButtonPressed() {
    if (digitalRead(_pin) == HIGH) {
        delay(50);
        return digitalRead(_pin) == HIGH;
    }
    return false;
}

void PowerManager::waitForButtonRelease() {
    while (digitalRead(_pin) == HIGH) {
        delay(10);
    }
    delay(50); // Debounce release
}

void PowerManager::enterDeepSleep() {
    waitForButtonRelease();

    Serial.println("entering deep sleep");
    _statusLed.off();
    _strobe.stop();
    delay(100); // Give some time for LEDs to turn off

    esp_sleep_enable_ext0_wakeup((gpio_num_t)_pin, 1);
    esp_deep_sleep_start();
}

void PowerManager::powerTask(void* parameter) {
    PowerManager* instance = static_cast<PowerManager*>(parameter);
    for (;;) {
        if (instance->isButtonPressed()) {
            unsigned long startHold = millis();
            while (instance->isButtonPressed()) {
                if (millis() - startHold > 3000) {
                    instance->_statusLed.setColor(StatusLED::YELLOW);
                    instance->enterDeepSleep();
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
