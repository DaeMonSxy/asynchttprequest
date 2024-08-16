#ifndef ASYNC_HTTP_H
#define ASYNC_HTTP_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <queue>
#include <string>

#if defined(ESP8266)
    #include <ESPAsyncTCP.h>
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <AsyncTCP.h>
    #include <WiFi.h>
#endif

#define LOG_V(fmt, ...)
// #define LOG_V(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// #define LOG_I(fmt, ...)
#define LOG_I(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

class AsyncHTTP
{
   public:
    AsyncHTTP();
    ~AsyncHTTP();

    unsigned long send2http(const std::string auth, const std::string host, int port, const std::string query);
    unsigned long send2http_url(const std::string url);

    void loop(); // Call this method in the main loop to process the request queue
    void onDataCallback(std::function<void(unsigned long, const std::string &)> callback);
    void onDataCallbackJson(std::function<void(unsigned long, JsonDocument &doc)> callback);

   private:
    struct Request
    {
        std::string   auth;
        std::string   host;
        int           port;
        std::string   query;
        //
        unsigned long id;
    };
    std::queue<Request> requestQueue;

    unsigned long sendID          = 0;
    bool          requestOngoing  = false;
    unsigned long lastRequestTime = 0;

    void          processQueue();
    unsigned long sendRequest(const Request &req);
    void          clearGlobalVariables();

    static void clientError(void *arg, AsyncClient *client, err_t error);
    static void clientDisconnect(void *arg, AsyncClient *client);
    static void clientData(void *arg, AsyncClient *client, void *data, size_t len);
    static void clientConnect(void *arg, AsyncClient *client);

    void handleData(AsyncClient *c, unsigned long sendID, void *data, size_t len);

    std::function<void(unsigned long, const std::string &)> onDataCallback_;
    std::function<void(unsigned long, JsonDocument &doc)>   onDataCallbackJson_;
};

#endif // ASYNC_HTTP_H
