// 20240802

#ifndef ASYNC_HTTP_H
#define ASYNC_HTTP_H

#include <string>
#include <list>
#include <vector>
#include "ArduinoJson.h"

#if defined(ESP8266)
    #include <ESPAsyncTCP.h>
    #include <ESP8266WiFi.h>
#elif defined(ESP32)

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
        unsigned long sendID     = 0; // approximately 1,193,046 hours or roughly 49,710 days.
        unsigned long timestamp = 0;
    };

    std::list<HttpData> httpDataList;
    unsigned long       getID = 0;

    bool send2http_queue(const HttpData &data);
    void sendRequest(unsigned long sendID, unsigned long timestamp, const HttpData &data);
    void handleRequestCleanup(AsyncClient *client);
    void clearRequestFromQueue(unsigned long sendID);

    std::function<void(unsigned long, const char *)>      onData_cb_;
    std::function<void(unsigned long, JsonDocument &doc)> onData_cb_json_;

    static const size_t MAX_HTTP_DATA_LIST_SIZE = 10;
};

#endif // ASYNC_HTTP_H
