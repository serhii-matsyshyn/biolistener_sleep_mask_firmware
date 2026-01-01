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
        uint32_t colorValue = 0;
        switch (color) {
            case RED:    colorValue = 0xFF0000; break;
            case GREEN:  colorValue = 0x00FF00; break;
            case BLUE:   colorValue = 0x0000FF; break;
            case YELLOW: colorValue = 0xFFFF00; break;
            case WHITE:  colorValue = 0xFFFFFF; break;
            case OFF:    colorValue = 0x000000; break;
        }
        
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
