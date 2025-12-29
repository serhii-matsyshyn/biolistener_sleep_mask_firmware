#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <atomic>

#define FORMAT_LITTLEFS_IF_FAILED true

class IPAddressParameter : public WiFiManagerParameter {
public:
  IPAddressParameter(const char *id, const char *placeholder, uint32_t address_raw);
  uint32_t getValue();
  void setValue(uint32_t address_raw);

private:
  IPAddress ip;
};

class IntParameter : public WiFiManagerParameter {
public:
  IntParameter(const char *id, const char *placeholder, long value);
  long getValue();
  void setValue(long value);
};

class ConfigManager {
public:
  ConfigManager();
  void begin();

  void configPortalTask(void *parameter);
  void stopConfigPortalTask();

  std::atomic<uint32_t> param_server_ip;
  std::atomic<long> param_server_port;

private:
  WiFiManager wifiManager;

  void loadConfig();
  void saveConfig();
  void saveConfigCallback();

  bool shouldSaveConfig;
  bool shouldProcessConfigPortal;

  String configFilePath = "/config.json";
  String wifiName = "Mask";
  String wifiNameWithChipId;
};

extern IntParameter param_port;
extern IPAddressParameter param_ip;

#endif // CONFIG_MANAGER_H
