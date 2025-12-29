#include <Arduino.h>
#include "Strobe.h"
#include "ConfigManager.h"
#include "StatusLED.h"
#include "UdpManager.h"

#ifndef LED_PIN
#define LED_PIN 7
#endif

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 21
#endif

#define NUMPIXELS 16

Strobe strobe(NUMPIXELS, LED_PIN, NEO_GRBW + NEO_KHZ800);
StatusLED statusLed(STATUS_LED_PIN);
ConfigManager configManager;
UdpManager udpManager(strobe, configManager, statusLed);

void setup() {
  Serial.begin(1000000);
  delay(1000);
  Serial.println("Starting Mask Device");

  statusLed.begin();
  statusLed.setColor(StatusLED::RED); // Booting

  strobe.begin();
  strobe.setColor(255, 255, 255, 0);
  strobe.setFrequency(10.0f);
  strobe.setFillCycle(0.1f);
  strobe.setBrightness(0.5f);
  
  configManager.begin();
  
  if (WiFi.status() != WL_CONNECTED) {
    statusLed.setColor(StatusLED::BLUE); // Connecting
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  statusLed.setColor(StatusLED::GREEN); // Ready

  udpManager.begin();

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
  // Main loop does nothing; everything is handled by tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
