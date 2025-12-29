#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class StatusLED {
private:
    Adafruit_NeoPixel* _strip;
    uint32_t _lastColor;
    uint8_t _brightness;
    SemaphoreHandle_t _mutex;

    static constexpr uint32_t PREDEFINED_COLORS[] = {
        0xFF0000, // RED
        0x00FF00, // GREEN
        0x0000FF, // BLUE
        0xFFFF00, // YELLOW
        0xFFFFFF, // WHITE
        0x000000  // OFF
    };

public:
    enum Color {
        RED,
        GREEN,
        BLUE,
        YELLOW,
        WHITE,
        OFF
    };

    StatusLED(int pin, uint8_t brightness = 50) : _lastColor(0), _brightness(brightness) {
        _strip = new Adafruit_NeoPixel(1, pin, NEO_GRB + NEO_KHZ800);
        _mutex = xSemaphoreCreateMutex();
    }

    ~StatusLED() {
        delete _strip;
        vSemaphoreDelete(_mutex);
    }

    void begin() {
        if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
            _strip->begin();
            _strip->setBrightness(_brightness);
            _strip->show();
            xSemaphoreGive(_mutex);
        }
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        uint32_t color = _strip->Color(r, g, b);
        if (color != _lastColor) {
            if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
                _strip->setPixelColor(0, color);
                _strip->show();
                xSemaphoreGive(_mutex);
                _lastColor = color;
            }
        }
    }

    void setColor(Color color) {
        uint32_t colorValue = PREDEFINED_COLORS[color];
        if (colorValue != _lastColor) {
            if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
                _strip->setPixelColor(0, colorValue);
                _strip->show();
                xSemaphoreGive(_mutex);
                _lastColor = colorValue;
            }
        }
    }

    void off() {
        setColor(OFF);
    }
};

#endif // STATUS_LED_H
