#include "socket.h"
#include "../logger/logger.h"
#include <WiFi.h>
#include <ArduinoJson.h>

Socket *Socket::instance = nullptr;

Socket::Socket() {}

Socket *Socket::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Socket();
    }

    return instance;
}

void Socket::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type) {
        case WStype_CONNECTED: {
            String clientMsg = String("Client #") + num + " connected";
            Logger::getInstance()->log(clientMsg.c_str(), INFO);
            break;
        }
        case WStype_DISCONNECTED: {
            String clientMsg = String("Client #") + num + " disconnected";
            Logger::getInstance()->log(clientMsg.c_str(), INFO);
            break;
        }
        case WStype_TEXT: {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, payload, length);
            if (error)
            {
                String errorMsg = String("deserializeJson() failed: ") + error.f_str();
                Logger::getInstance()->log(errorMsg.c_str(), ERROR);
                break;
            }

            const char *request = doc["request"];
            String requestMsg = String("received request: ") + request;
            Logger::getInstance()->log(requestMsg.c_str(), INFO);

            const char *ssid = doc["payload"]["ssid"];
            const char *pwd = doc["payload"]["password"];

            String ssidMsg = String("SSID: ") + ssid;
            Logger::getInstance()->log(ssidMsg.c_str(), INFO);
            String pwdMsg = String("PWD: ") + pwd;
            Logger::getInstance()->log(pwdMsg.c_str(), INFO);

            WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
                String ip = WiFi.localIP().toString();
                Logger::getInstance()->log(("WiFi IP: " + ip).c_str(), INFO);
                
            }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

            WiFi.begin(ssid, pwd);
            
            break;
        }
        default:
            break;
    }
}

static void onWebSocketEventStatic(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    Socket::getInstance()->onWebSocketEvent(num, type, payload, length);
}

void Socket::init()
{
    // Start an AP for testing if WiFi is not connected
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() == WIFI_OFF) {
        Logger::getInstance()->log("WiFi not connected! Starting AP", WARNING);
        WiFi.mode(WIFI_AP); 
        WiFi.softAP("MeinESP32S3", "passwort123"); // TODO: move SSID and password to config
        Logger::getInstance()->log("AP started with SSID: MeinESP32S3", INFO);
        String ipMsg = String("AP IP address: ") + WiFi.softAPIP().toString();
        Logger::getInstance()->log(ipMsg.c_str(), INFO);
    }

    this->socket = new WebSocketsServer(SOCKET_SERVER_PORT);
    this->socket->begin();
    this->socket->onEvent(onWebSocketEventStatic);
    Logger::getInstance()->log("Socket listening on 81", INFO);
}

void Socket::loop()
{
    this->socket->loop();
}