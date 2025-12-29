#include "ConfigManager.h"

IPAddressParameter::IPAddressParameter(const char *id, const char *placeholder,
                                       uint32_t address_raw)
    : WiFiManagerParameter("") {
  IPAddress address(address_raw);
  init(id, placeholder, address.toString().c_str(), 16, "", WFM_LABEL_BEFORE);
}

uint32_t IPAddressParameter::getValue() {
  ip.fromString(WiFiManagerParameter::getValue());
  return (uint32_t)ip;
}

void IPAddressParameter::setValue(uint32_t address_raw) {
  ip = IPAddress(address_raw);
  WiFiManagerParameter::setValue(ip.toString().c_str(), 16);
}

IntParameter::IntParameter(const char *id, const char *placeholder, long value)
    : WiFiManagerParameter("") {
  init(id, placeholder, String(value).c_str(), 10, "", WFM_LABEL_BEFORE);
}

long IntParameter::getValue() {
  return String(WiFiManagerParameter::getValue()).toInt();
}

void IntParameter::setValue(long value) {
  WiFiManagerParameter::setValue(String(value).c_str(), 10);
}

IntParameter param_port("port", "Master Port", 20000);
IPAddressParameter param_ip("server", "Master IP", 0);

ConfigManager::ConfigManager()
    : shouldSaveConfig(false), param_server_ip(0), param_server_port(20000),
      shouldProcessConfigPortal(true) {
    wifiNameWithChipId = wifiName + "_" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
}

void ConfigManager::begin() {
  Serial.println("Mounting FS...");
  if (LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("Mounted file system");
    loadConfig();
  } else {
    Serial.println("Failed to mount FS");
  }

  Serial.println("Starting config portal");
  Serial.println("Param server ip: " + IPAddress(param_server_ip).toString());
  Serial.println("Param server port: " + String(param_server_port));

  wifiManager.setSaveConfigCallback([this]() { saveConfigCallback(); });
  wifiManager.setSaveParamsCallback([this]() { saveConfigCallback(); });
  wifiManager.addParameter(&param_port);
  wifiManager.addParameter(&param_ip);
  wifiManager.setHostname("mask-device");

  std::vector<const char *> menu = {"wifi","param","sep","info","update","sep","restart","exit"};
  wifiManager.setMenu(menu);
  wifiManager.setClass("invert"); // dark theme
  wifiManager.setAPClientCheck(true);

  if (wifiManager.autoConnect(wifiNameWithChipId.c_str())) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi!");
  }

  Serial.println("Starting config portal for params configuration");

  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.startConfigPortal(wifiNameWithChipId.c_str());
}

void ConfigManager::loadConfig() {
  if (LittleFS.exists(configFilePath)) {
    Serial.println("Reading config file");
    File configFile = LittleFS.open(configFilePath, "r");
    if (configFile) {
      Serial.println("Opened config file");
      StaticJsonDocument<1024> json;
      auto deserializeError = deserializeJson(json, configFile);
      if (!deserializeError) {
        Serial.println("Parsed json");
        param_server_ip = json["param_server_ip"].as<uint32_t>();
        param_server_port = json["param_server_port"].as<long>();

        param_port.setValue(param_server_port.load());
        param_ip.setValue(param_server_ip.load());
        
        Serial.printf("Loaded Master IP: %s\n", IPAddress(param_server_ip.load()).toString().c_str());
        Serial.printf("Loaded Master Port: %ld\n", param_server_port.load());
      } else {
        Serial.println("Failed to load json config: " + String(deserializeError.c_str()));
      }
      configFile.close();
    }
  } else {
    Serial.println("Config file does not exist");
  }
}

void ConfigManager::saveConfig() {
  Serial.println("Saving config to file...");
  StaticJsonDocument<1024> json;
  json["param_server_ip"] = param_server_ip.load();
  json["param_server_port"] = param_server_port.load();

  File configFile = LittleFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }
  if (serializeJson(json, configFile) == 0) {
    Serial.println("Failed to write to file");
  }
  configFile.close();
  Serial.println("Config saved successfully");
}

void ConfigManager::saveConfigCallback() {
  Serial.println("Saving config (custom params)");
  param_server_ip = param_ip.getValue();
  param_server_port = param_port.getValue();
  saveConfig();
}

void ConfigManager::configPortalTask(void *parameter) {
  while (shouldProcessConfigPortal) {
    wifiManager.process();
    delay(1);
  }

  Serial.println("Config portal task ended. Stopping config portal.");
  wifiManager.stopConfigPortal();
  vTaskDelete(NULL);
}

void ConfigManager::stopConfigPortalTask() {
  shouldProcessConfigPortal = false;
}
