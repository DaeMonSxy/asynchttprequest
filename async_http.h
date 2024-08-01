// 20230902

#ifndef ASYNC_HTTP_H
#define ASYNC_HTTP_H

#include <string>
#include <list>
#include "ArduinoJson.h"
#include <vector> // Include the <vector> header

#if defined(ESP8266)

    #include <ESPAsyncTCP.h> // _async_queue = xQueueCreate(64, sizeof(lwip_event_packet_t *));
    #include <ESP8266WiFi.h>

#endif

#if defined(ESP32)

    // #define CONFIG_ASYNC_TCP_RUNNING_CORE 0
    //  #define CONFIG_LWIP_TCP_WND_DEFAULT 8192 // 5744
    //  #define ASYNC_MAX_ACK_TIME       2000
    //  #define TCP_WND                  TCP_MSS * 10
    //   TCP_WND = 5744
    //   TCP_MSS = 1436
    //  #define ARDUHAL_LOG_LEVEL ARDUHAL_LOG_LEVEL_ERROR

    #include <AsyncTCP.h> // _async_queue = xQueueCreate(64, sizeof(lwip_event_packet_t *));
    #include <WiFi.h>

#endif

class AsyncHTTP
{
   public:
    AsyncHTTP();
    ~AsyncHTTP();

    void loop();

    unsigned long send2http(const char *auth, const char *host, int port, const char *query);
    unsigned long send2http_url(const char *char_url);
    unsigned long send2http_fmt(const char *auth, const char *host, int port, const char *fmt, ...);

    void handleData(AsyncClient *c, unsigned long sendID, void *data, size_t len);
    void onData_cb(std::function<void(unsigned long, const char *)> callback);
    void onData_cb_json(std::function<void(unsigned long, JsonDocument &doc)> callback);

   private:
    AsyncClient *client;
    
    struct HttpData
    {
        char          auth[64];
        char          host[64];
        int           port;
        char          query[400];
        unsigned long sendID    = 0;
        unsigned long timestamp = 0; // millis timestamp,to check, if no response for 5sec-delete the item from list
    };
    std::list<HttpData> httpDataList;
    unsigned long       getID = 0; // approximately 1,193,046 hours or roughly 49,710 days.

    bool send2http_queue(const HttpData &data);
    void sendRequest(unsigned long sendID, unsigned long timestamp, const HttpData &data);
    void handleRequestCleanup(AsyncClient *client);

    std::function<void(unsigned long, const char *)>        onData_cb_;
    std::function<void(unsigned long, JsonDocument &doc)> onData_cb_json_;
};

#endif // ASYNC_HTTP_H
