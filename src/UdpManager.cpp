#include "UdpManager.h"

UdpManager::UdpManager(Strobe& strobe, ConfigManager& config, StatusLED& led, PowerManager& powerManager)
    : _strobe(strobe), _config(config), _led(led), _powerManager(powerManager) {
    _port = 20000; // Default port
}

void UdpManager::begin() {
    _udp.begin(_port);
    Serial.printf("UDP listening on port %d\n", _port);
}

void UdpManager::receiveTask(void* parameter) {
    char packetBuffer[1024];
    while (true) {
        int packetSize = _udp.parsePacket();
        if (packetSize) {
            int len = _udp.read(packetBuffer, 1023);
            if (len > 0) {
                packetBuffer[len] = 0;
                processPacket(packetBuffer, _udp.remoteIP(), _udp.remotePort());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Reduced latency
    }
}

void UdpManager::pingTask(void* parameter) {
    while (true) {
        uint32_t server_ip = _config.param_server_ip.load();
        if (WiFi.status() == WL_CONNECTED && server_ip != 0) {
            IPAddress masterIP(server_ip);
            StaticJsonDocument<256> doc;
            doc["type"] = "ping";
            doc["ip"] = WiFi.localIP().toString();
            
            _udp.beginPacket(masterIP, _config.param_server_port.load());
            serializeJson(doc, _udp);
            _udp.endPacket();
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Ping every 2 seconds
    }
}

void UdpManager::processPacket(char* data, IPAddress remoteIP, uint16_t remotePort) {
    // Stop config portal immediately if any command received
    _config.stopConfigPortalTask();

    Serial.printf("UDP Packet from %s: %s\n", remoteIP.toString().c_str(), data);

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
        Serial.println("JSON Parse Error");
        return;
    }

    if (!doc.containsKey("command")) return;

    int cmd = doc["command"];

    switch (cmd) {
        case CMD_CHANGE_PARAMS:
            if (doc.containsKey("enabled")) {
                if (doc["enabled"]) _strobe.start();
                else _strobe.stop();
            }
            if (doc.containsKey("frequency")) _strobe.setFrequency(doc["frequency"]);
            if (doc.containsKey("fill")) _strobe.setFillCycle(doc["fill"]);
            if (doc.containsKey("brightness")) _strobe.setBrightness(doc["brightness"]);
            if (doc.containsKey("color")) {
                JsonArray color = doc["color"];
                if (color.size() >= 3) {
                    uint8_t r = color[0];
                    uint8_t g = color[1];
                    uint8_t b = color[2];
                    uint8_t w = (color.size() > 3) ? color[3] : 0;
                    _strobe.setColor(r, g, b, w);
                }
            }
            if (doc.containsKey("mask")) {
                _strobe.setBitmask(doc["mask"].as<uint64_t>());
            }
            break;
        case CMD_GET_PARAMS:
            {
                StaticJsonDocument<1024> response;
                response["type"] = "params";
                response["enabled"] = _strobe.isEnabled();
                response["frequency"] = _strobe.getFrequency();
                response["fill"] = _strobe.getFillCycle();
                response["brightness"] = _strobe.getBrightness();
                response["mask"] = _strobe.getBitmask();
                response["battery"] = _powerManager.getBatteryVoltage();
                
                uint32_t color = _strobe.getColor();
                JsonArray colorArr = response.createNestedArray("color");
                colorArr.add((color >> 16) & 0xFF);
                colorArr.add((color >> 8) & 0xFF);
                colorArr.add(color & 0xFF);
                colorArr.add((color >> 24) & 0xFF); // W channel

                sendResponse(remoteIP, remotePort, response);
            }
            break;
    }
}

void UdpManager::sendResponse(IPAddress remoteIP, uint16_t remotePort, const JsonDocument& doc) {
    _udp.beginPacket(remoteIP, remotePort);
    serializeJson(doc, _udp);
    _udp.endPacket();
}
