#ifndef SOCKET_H
#define SOCKET_H

#include <Arduino.h>
#include <WebSocketsServer.h>

#define SOCKET_SERVER_PORT 81

class Socket {
    private: 
        static Socket* instance;
        WebSocketsServer* socket;
        Socket();
    public:        
        void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
        static Socket* getInstance();
        void init();
        void loop();
};

#endif