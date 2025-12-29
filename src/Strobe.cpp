#include "Strobe.h"
#include <ESP32TimerInterrupt.h>

// Global/Static timer instances for ISR access
static ESP32Timer ITimer(0);
static ESP32_ISR_Timer ISR_Timer;

bool IRAM_ATTR TimerHandler(void* arg) {
    ISR_Timer.run();
    return true;
}

Strobe* g_strobeInstance = nullptr;

void IRAM_ATTR StrobeCallback(void* arg) {
    Strobe::timerHandler(arg);
}

Strobe::Strobe(uint16_t numPixels, uint8_t pin, neoPixelType type)
    : _pixels(numPixels, pin, type), _numPixels(numPixels),
      _frequency(1.0f), _fillCycle(0.5f), _color(0xFFFFFF), _brightness(1.0f),
      _enabled(false), _currentState(false), _lastUpdateColor(0), 
      _lastUpdateBrightness(-1.0f), _taskHandle(NULL), _eventQueue(NULL), _timerId(-1),
      _intervalOn_ms(50), _intervalOff_ms(50), _bitmask(0xFFFFFFFFFFFFFFFFULL) {
    g_strobeInstance = this;
}

Strobe::~Strobe() {
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
    }
    if (_eventQueue) {
        vQueueDelete(_eventQueue);
    }
}

void Strobe::begin() {
    _pixels.begin();
    _pixels.show();

    _eventQueue = xQueueCreate(20, sizeof(StrobeEvent));
    
    // Start the hardware timer at 100us interval
    // This provides 100us resolution for the strobe.
    if (ITimer.attachInterruptInterval(100, (esp32_timer_callback)TimerHandler)) {
        Serial.println("ITimer started OK (100us resolution)");
    } else {
        Serial.println("Can't set ITimer");
    }

    xTaskCreatePinnedToCore(
        strobeTask,
        "StrobeTask",
        4096,
        this,
        10,
        &_taskHandle,
        1 // Core 1
    );

    // Pre-calculate intervals
    updateIntervals();

    // Start the timer immediately and keep it running
    float freq = _frequency.load();
    float fill = _fillCycle.load();
    uint32_t interval_us = (uint32_t)((1000000.0f / freq) * fill);
    if (interval_us < 100) interval_us = 100;
    _timerId = ISR_Timer.setInterval(interval_us / 100, StrobeCallback, (void*)this);
}

void Strobe::start() {
    if (!_enabled.exchange(true)) {
        // Immediately update LEDs to current strobe state
        StrobeEvent ev = {_currentState};
        xQueueSend(_eventQueue, &ev, 0);
    }
}

void Strobe::stop() {
    if (_enabled.exchange(false)) {
        // Immediately turn off LEDs
        StrobeEvent ev = {false};
        xQueueSend(_eventQueue, &ev, 0);
    }
}

void Strobe::setFrequency(float hz) {
    if (hz <= 0) hz = 0.1f;
    if (_frequency.load() != hz) {
        _frequency.store(hz);
        updateIntervals();
    }
}

void Strobe::setFillCycle(float fill) {
    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;
    if (_fillCycle.load() != fill) {
        _fillCycle.store(fill);
        updateIntervals();
    }
}

void Strobe::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    uint32_t newColor = _pixels.Color(r, g, b, w);
    _color.store(newColor);
}

void Strobe::setBrightness(float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    _brightness.store(brightness);
}

void Strobe::updateIntervals() {
    float freq = _frequency.load();
    float fill = _fillCycle.load();
    
    uint32_t on = (uint32_t)((1000000.0f / freq) * fill);
    uint32_t off = (uint32_t)((1000000.0f / freq) * (1.0f - fill));
    
    if (on < 100) on = 100;
    if (off < 100) off = 100;
    
    _intervalOn_ms.store(on);
    _intervalOff_ms.store(off);
}

void Strobe::setBitmask(uint64_t mask) {
    _bitmask.store(mask);
}

void Strobe::strobeTask(void* parameter) {
    Strobe* instance = static_cast<Strobe*>(parameter);
    StrobeEvent ev;
    
    for (;;) {
        if (xQueueReceive(instance->_eventQueue, &ev, portMAX_DELAY) == pdPASS) {
            instance->updateLeds(ev.state);
        }
    }
}

bool IRAM_ATTR Strobe::timerHandler(void* arg) {
    Strobe* instance = static_cast<Strobe*>(arg);
    // Timer always runs to keep the phase accurate

    instance->_currentState = !instance->_currentState;
    
    uint32_t interval_us;
    if (instance->_currentState) {
        interval_us = instance->_intervalOn_ms.load();
    } else {
        interval_us = instance->_intervalOff_ms.load();
    }

    if (interval_us < 100) interval_us = 100;

    // Change the interval for the next trigger (in 100us units)
    ISR_Timer.changeInterval(instance->_timerId, interval_us / 100);
    
    StrobeEvent ev = {instance->_currentState};
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(instance->_eventQueue, &ev, &xHigherPriorityTaskWoken);
    
    return xHigherPriorityTaskWoken == pdTRUE;
}

void Strobe::updateLeds(bool state) {
    uint32_t color = _color.load();
    float brightness = _brightness.load();
    uint64_t currentBitmask = _bitmask.load();

    if (state && _enabled.load()) {
        _pixels.setBrightness((uint8_t)(brightness * 255));
        for (uint16_t i = 0; i < _numPixels; i++) {
            if (i < 64 && (currentBitmask & (1ULL << i))) {
                _pixels.setPixelColor(i, color);
            } else {
                _pixels.setPixelColor(i, 0);
            }
        }
    } else {
        _pixels.clear();
    }
    
    _pixels.show();
    
    _lastUpdateColor = color;
    _lastUpdateBrightness = brightness;
}
