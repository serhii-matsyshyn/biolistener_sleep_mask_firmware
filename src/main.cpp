#include <Arduino.h>
#include "Strobe.h"
#include "ConfigManager.h"
#include "StatusLED.h"
#include "UdpManager.h"
#include "PowerManager.h"

#ifndef LED_PIN
#define LED_PIN 7
#endif

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 21
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN 10
#endif

#ifndef BATTERY_PIN
#define BATTERY_PIN 34
#endif

#define NUMPIXELS 8

Strobe strobe(NUMPIXELS, LED_PIN, NEO_GRBW + NEO_KHZ800);
StatusLED statusLed(STATUS_LED_PIN);
ConfigManager configManager;
PowerManager powerManager(BUTTON_PIN, BATTERY_PIN, statusLed, strobe);
UdpManager udpManager(strobe, configManager, statusLed, powerManager);

void setup() {
  statusLed.begin();
  powerManager.checkWakeup();

  Serial.begin(1000000);
  delay(1000);
  Serial.println("Starting Mask Device");

  statusLed.setColor(StatusLED::RED); // Booting

  strobe.begin();
  strobe.setColor(255, 255, 255, 0);
  strobe.setFrequency(10.0f);
  strobe.setFillCycle(0.1f);
  strobe.setBrightness(0.5f);
  
  configManager.begin(statusLed);
  Serial.println("WiFi connection initiated (non-blocking)");

  udpManager.begin();
  powerManager.begin();

  // Launch networking tasks on Core 0
  xTaskCreatePinnedToCore(
    [](void* p){ static_cast<ConfigManager*>(p)->configPortalTask(p); },
    "configPortalTask",
    8192,
    &configManager,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    [](void* p){ static_cast<UdpManager*>(p)->receiveTask(p); },
    "udpReceiveTask",
    8192,
    &udpManager,
    2,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    [](void* p){ static_cast<UdpManager*>(p)->pingTask(p); },
    "udpPingTask",
    8192,
    &udpManager,
    1,
    NULL,
    0
  );
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
