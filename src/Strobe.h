#ifndef STROBE_H
#define STROBE_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <atomic>
#include <vector>

class Strobe {
public:
    Strobe(uint16_t numPixels, uint8_t pin, neoPixelType type = NEO_GRBW + NEO_KHZ800);
    ~Strobe();

    void begin();
    
    void start();
    void stop();
    
    void setFrequency(float hz);
    void setFillCycle(float fill); // 0.0 to 1.0
    void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void setBrightness(float brightness); // 0.0 to 1.0
    void setBitmask(uint64_t mask);
    
    float getFrequency() const { return _frequency.load(); }
    float getFillCycle() const { return _fillCycle.load(); }
    uint32_t getColor() const { return _color.load(); }
    float getBrightness() const { return _brightness.load(); }
    bool isEnabled() const { return _enabled.load(); }
    uint64_t getBitmask() const { return _bitmask.load(); }

    static void strobeTask(void* parameter);
    void onTimerTick();

private:
    Adafruit_NeoPixel _pixels;
    uint16_t _numPixels;
    
    std::atomic<float> _frequency;
    std::atomic<float> _fillCycle;
    std::atomic<uint32_t> _color;
    std::atomic<float> _brightness;
    std::atomic<bool> _enabled;
    
    std::atomic<uint32_t> _intervalOn_us;
    std::atomic<uint32_t> _intervalOff_us;

    std::atomic<int64_t> _lastToggleTime_us;
    std::atomic<bool> _currentState;

    std::atomic<uint64_t> _bitmask;
    
    QueueHandle_t _eventQueue;
    TaskHandle_t _taskHandle;
    int _timerId;
    
    struct StrobeEvent {
        bool state; // true = ON, false = OFF
    };

    // Internal state for the task
    uint32_t _lastUpdateColor;
    float _lastUpdateBrightness;

    void updateLeds(bool state);
    void updateIntervals();
};

#endif // STROBE_H
