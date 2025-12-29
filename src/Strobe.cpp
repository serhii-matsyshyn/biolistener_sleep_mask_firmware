#include "Strobe.h"
#include <ESP32TimerInterrupt.h>

// Global/Static timer instances for ISR access
static ESP32Timer ITimer(0);

Strobe* g_strobeInstance = nullptr;

bool IRAM_ATTR TimerHandler(void* arg) {
    if (g_strobeInstance) {
        g_strobeInstance->onTimerTick();
    }
    return true;
}

Strobe::Strobe(uint16_t numPixels, uint8_t pin, neoPixelType type)
    : _pixels(numPixels, pin, type), _numPixels(numPixels),
      _frequency(1.0f), _fillCycle(0.5f), _color(0xFFFFFF), _brightness(1.0f),
      _enabled(false), _currentState(false), _lastUpdateColor(0), 
      _lastUpdateBrightness(-1.0f), _taskHandle(NULL), _eventQueue(NULL), _timerId(-1),
      _intervalOn_us(500000), _intervalOff_us(500000), _lastToggleTime_us(0),
      _bitmask(0xFFFFFFFFFFFFFFFFULL) {
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
    
    // Pre-calculate intervals
    updateIntervals();
    _lastToggleTime_us.store(esp_timer_get_time());

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
}

void Strobe::start() {
    if (!_enabled.exchange(true)) {
        // Immediately update LEDs to current strobe state
        StrobeEvent ev = {_currentState.load()};
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
    
    uint32_t on_us = (uint32_t)((1000000.0f / freq) * fill);
    uint32_t off_us = (uint32_t)((1000000.0f / freq) * (1.0f - fill));
    
    if (on_us < 100) on_us = 100;
    if (off_us < 100) off_us = 100;
    
    _intervalOn_us.store(on_us);
    _intervalOff_us.store(off_us);
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

void IRAM_ATTR Strobe::onTimerTick() {
    int64_t now = esp_timer_get_time();
    int64_t last = _lastToggleTime_us.load();
    int64_t elapsed = now - last;
    uint32_t currentInterval = _currentState.load() ? _intervalOn_us.load() : _intervalOff_us.load();

    if (elapsed >= currentInterval) {
        bool nextState = !_currentState.load();
        _currentState.store(nextState);
        _lastToggleTime_us.store(now);
        
        if (_enabled.load()) {
            StrobeEvent ev = {nextState};
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            if (xQueueSendFromISR(_eventQueue, &ev, &xHigherPriorityTaskWoken) == pdPASS) {
                if (xHigherPriorityTaskWoken == pdTRUE) {
                    portYIELD_FROM_ISR();
                }
            }
        }
    }
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
