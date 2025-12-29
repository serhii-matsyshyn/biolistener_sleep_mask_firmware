#ifndef UDP_MANAGER_H
#define UDP_MANAGER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "Strobe.h"
#include "ConfigManager.h"
#include "StatusLED.h"

class UdpManager {
public:
    enum Command {
        CMD_CHANGE_PARAMS = 0,
        CMD_GET_PARAMS = 1
    };

    UdpManager(Strobe& strobe, ConfigManager& config, StatusLED& led);
    void begin();
    void receiveTask(void* parameter);
    void pingTask(void* parameter);

private:
    Strobe& _strobe;
    ConfigManager& _config;
    StatusLED& _led;
    WiFiUDP _udp;
    uint16_t _port;

    void processPacket(char* data, IPAddress remoteIP, uint16_t remotePort);
    void sendResponse(IPAddress remoteIP, uint16_t remotePort, const JsonDocument& doc);
};

#endif // UDP_MANAGER_H
