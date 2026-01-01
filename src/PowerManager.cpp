#include "PowerManager.h"

PowerManager::PowerManager(int pin, int batteryPin, StatusLED& statusLed, Strobe& strobe)
    : _pin(pin), _batteryPin(batteryPin), _statusLed(statusLed), _strobe(strobe), _taskHandle(NULL), _batteryTaskHandle(NULL), _lastBatteryVoltage(0.0f) {}

void PowerManager::begin() {
    if (_pin != -1) {
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

    xTaskCreatePinnedToCore(
        measureBatteryTask,
        "measureBatteryTask",
        4096,
        this,
        1,
        &_batteryTaskHandle,
        0 // Core 0
    );
}

void PowerManager::checkWakeup() {
    if (_pin == -1) return;

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
    if (_pin == -1) return false;

    if (digitalRead(_pin) == HIGH) {
        delay(50);
        return digitalRead(_pin) == HIGH;
    }
    return false;
}

void PowerManager::waitForButtonRelease() {
    if (_pin == -1) return;

    while (digitalRead(_pin) == HIGH) {
        delay(10);
    }
    delay(50); // Debounce release
}

void PowerManager::enterDeepSleep() {
    if (_pin == -1) return;

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

void PowerManager::measureBatteryTask(void* parameter) {
    PowerManager* instance = static_cast<PowerManager*>(parameter);
    
    analogSetPinAttenuation(instance->_batteryPin, ADC_0db);
    analogSetWidth(12);

    for (;;) {
        // Serial.println("TaskId_ReadBatteryVoltage");
        uint16_t analogReadVal = analogRead(instance->_batteryPin);
        float batteryVoltage = (analogReadVal * 4.73) / 4096.0; // 4.73 = 1.1 * (330.0 + 100.0) / 100.0;
        // Serial.printf("Battery voltage: %f\n", batteryVoltage);
        
        instance->_lastBatteryVoltage = batteryVoltage;

        if (batteryVoltage < 3.3) {
            instance->_statusLed.setColor(StatusLED::YELLOW);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

float PowerManager::getBatteryVoltage() {
    return _lastBatteryVoltage.load();
}
