#include "async_http.h"

AsyncHTTP::AsyncHTTP() {}

AsyncHTTP::~AsyncHTTP() {}

void AsyncHTTP::handleRequestCleanup(AsyncClient *client)
{
    if (client) {
        client->close(true);
        delete client;
        client = nullptr;
    }
}
//--------------------------------------------------------------------------------

unsigned long AsyncHTTP::send2http(const char *auth, const char *host, int port, const char *query)
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;

    Serial.printf("[http_v] send2http:%s %i %s", host, port, query);

    HttpData data;
    data.sendID = ++getID;

    strncpy(data.auth, auth, sizeof(data.auth));
    strncpy(data.host, host, sizeof(data.host));
    data.port = port;
    strncpy(data.query, query, sizeof(data.query));

    if (send2http_queue(data)) {
        // OK
    }
    return getID;
}

unsigned long AsyncHTTP::send2http_fmt(const char *auth, const char *host, int port, const char *fmt, ...)
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;

    // Serial.printf("[http_v] send2http_fmt: %s %i", host, port);

    char query[1024]; // Adjust the buffer size as necessary

    va_list args;
    va_start(args, fmt);
    int formattedLength = vsnprintf(query, sizeof(query), fmt, args);

    va_end(args);

    if (formattedLength < 0 || static_cast<size_t>(formattedLength) >= sizeof(query)) {
        Serial.printf("[http] ERR - Formatting error or buffer overflow");
        return 0;
    }

    HttpData data;
    data.sendID = ++getID;

    strncpy(data.auth, auth, sizeof(data.auth));
    strncpy(data.host, host, sizeof(data.host));
    data.port = port;
    strncpy(data.query, query, sizeof(data.query));

    if (send2http_queue(data)) {
        // OK
    }
    return getID;
}

unsigned long AsyncHTTP::send2http_url(const char *char_url)
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;

    // logLibrary.debug("[http] url#", char_url);

    if (ESP.getFreeHeap() < 8000) {
        Serial.printf("[http] ERR Heap below threshold");
        return 0;
    }

    if (char_url == nullptr) {
        Serial.printf("[http] ERR NULL pointer");
        return 0;
    }

    std::string url = char_url;

    std::string protocol = "http";
    std::string domain;
    int         port = 80; // Default port is 80 for HTTP
    std::string path;

    if (url.find("://") != std::string::npos) {
        // Extract protocol if present
        size_t protocolEnd = url.find("://");
        protocol           = url.substr(0, protocolEnd);
        url.erase(0, protocolEnd + 3); // Remove "http://" or "https://"

        if (protocol == "https") {
            port = 443; // Default port is 443 for HTTPS
        }
    }

    // Check if there's a port specified
    auto domainEnd = std::find(url.begin(), url.end(), '/');
    auto portStart = std::find(url.begin(), domainEnd, ':');

    if (portStart != domainEnd) {
        domain       = std::string(url.begin(), portStart);
        auto portStr = std::string(portStart + 1, domainEnd);
        port         = std::stoi(portStr);
        Serial.printf("[http] ERR Invalid port number");
        return 0;

    } else {
        domain = std::string(url.begin(), domainEnd);
    }

    if (domain.empty()) {
        Serial.printf("[http] ERR Invalid URL format (missing domain)");
        return 0;
    }

    // Find the path
    if (domainEnd != url.end()) {
        path = std::string(domainEnd, url.end());
    }

    HttpData httpData;
    httpData.sendID = ++getID;

    snprintf(httpData.auth, sizeof(httpData.auth), "-");
    snprintf(httpData.host, sizeof(httpData.host), "%s", domain.c_str());
    httpData.port = port;
    snprintf(httpData.query, sizeof(httpData.query), "%s", path.c_str());

    httpDataList.push_back(httpData);

    return getID;
}

//--------------------------------------------------------------------------------

bool AsyncHTTP::send2http_queue(const HttpData &data)
{
    // Serial.printf("[http_v] send2http_queue %s %i %s", data.host, data.port, data.query);

    if (WiFi.status() == WL_CONNECTED) {
        for (auto it = httpDataList.begin(); it != httpDataList.end(); ++it) {
            if (
                strncmp(it->auth, data.auth, sizeof(it->auth)) == 0 &&
                strncmp(it->host, data.host, sizeof(it->host)) == 0 &&
                it->port == data.port &&
                strncmp(it->query, data.query, sizeof(it->query)) == 0) {
                // Serial.printf("[http] Data already exists in the queue, not adding.");
                return false; // Data already exists, so don't add it again
            }
        }

        httpDataList.push_front(data);
        return true;
    }

    return false;
}
//--------------------------------------------------------------------------------

void AsyncHTTP::sendRequest(unsigned long sendID, unsigned long timestamp, const HttpData &data)
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    // Serial.printf("[http_v] sendRequest [%lu] %s %i %s", sendID, data.host, data.port, data.query);

    if (ESP.getFreeHeap() < 10000) {
        Serial.printf("[http] sendRequest:ERR Heap below threshold");
        return;
    }

    AsyncClient *client = new AsyncClient();

    if (!client) {
        Serial.printf("[http] ## Failed to create client");
        return;
    }

    //setStatusLED(status_leds, LED_HTTP, BLUE);

    // client->onError([this, sendID](void *arg, AsyncClient *client, int8_t error)
    client->onError([this, sendID](void *arg, AsyncClient *client, err_t error) {
        Serial.printf("[http] ## [%lu] ERR %s", sendID, client->errorToString(error));

        //setStatusLED(status_leds, LED_HTTP, RED);
        // handleRequestCleanup(client);
    },
                    nullptr);

    client->onDisconnect([this, sendID](void *arg, AsyncClient *client) {
        // Serial.printf("[http] ## [%lu] onDisconnect", sendID);

        //setStatusLED(status_leds, LED_HTTP, BLACK);

        handleRequestCleanup(client);
    },
                         nullptr);

    client->onData([this, sendID](void *arg, AsyncClient *client, void *data, size_t len) {
        // Serial.printf("[http] ## [%lu] onData", sendID);
        if (len > 0) {
            handleData(client, sendID, data, len);

            //setStatusLED(status_leds, LED_HTTP, GREEN);
        }
    },
                   nullptr);

    client->onAck([sendID](void *arg, AsyncClient *client, size_t len, uint32_t time) {
        // Serial.printf("[http] ## [%lu] Acknowledgment", sendID);
    },
                  nullptr);

    /* client->onPacket([](void *arg, AsyncClient *client, pbuf *packet) {
        for (size_t i = 0; i < packet->tot_len; i++) {
            uint8_t byte;
            if (pbuf_copy_partial(packet, &byte, 1, i) == 1) {
                Serial.print(byte, HEX); // Print each byte in hexadecimal format
                Serial.print(" ");       // Separate bytes with space
            }
        }
        Serial.println(); // Print a new line after all bytes are printed
    },
                     nullptr);
    */

    client->onTimeout([sendID](void *arg, AsyncClient *client, uint32_t time) {
        Serial.printf("[http] ## [%lu] Timeout", sendID);
    },
                      nullptr);

    /*  client->onPoll([sendID](void *arg, AsyncClient *client) {
         Serial.printf("[http] ## [%lu] Poll", sendID);
     },
                    nullptr);
    */

    client->onConnect([this, data, sendID](void *arg, AsyncClient *client) {
        // Serial.printf("[http] Connected to %s", data.host);

        Serial.printf("[http] #> [%lu] %s:%i%s", data.sendID, data.host, data.port, data.query);

        char requestBuffer[1024];
        int  formattedLength = snprintf(requestBuffer, sizeof(requestBuffer),
                                        "GET %s HTTP/1.1\r\n"
                                         "Host: %s:%i\r\n"
                                         "Authorization: Basic %s\r\n"
                                         "User-Agent: iqESP\r\n"
                                         "Connection: close\r\n\r\n",
                                        data.query, data.host, data.port, data.auth);

        if (formattedLength < 0 || formattedLength >= (int)sizeof(requestBuffer)) {
            Serial.printf("[http] ERR - snprintf formatting error");
            return;
        }

        client->write(requestBuffer);
        // client->close(true);
    },
                      nullptr);

    if (!client->connect(data.host, data.port)) {
        Serial.printf("[http] ## [%lu] ERR Failed to connect %s", sendID, data.host);
        // client->setRxTimeout(5000);
        // setStatusLED(LED_HTTP, RED);
        // handleRequestCleanup(client);
    }
}

//--------------------------------------------------------------------------------

void AsyncHTTP::onData_cb(std::function<void(unsigned long, const char *)> callback)
{
    onData_cb_ = callback;
}

void AsyncHTTP::onData_cb_json(std::function<void(unsigned long, JsonDocument &doc)> callback)
{
    onData_cb_json_ = callback;
}
/* void AsyncHTTP::handleData(AsyncClient *c, unsigned long sendID, void *data, size_t len)
{
    // debug
    Serial.print("[http] Received data: ");
    for (size_t i = 0; i < len; i++)
        Serial.write(reinterpret_cast<char *>(data)[i]);
    Serial.println();

    try {
        size_t jsonStart = 0;
        size_t jsonEnd   = 0;

        for (size_t i = 0; i < len; i++) {
            if (reinterpret_cast<char *>(data)[i] == '{') {
                jsonStart = i;
                break;
            }
        }

        for (size_t i = jsonStart; i < len; i++) {
            if (reinterpret_cast<char *>(data)[i] == '}') {
                jsonEnd = i;
                break;
            }
        }

        size_t jsonLen = jsonEnd - jsonStart + 1; // Calculate the length of the JSON data

        if (jsonStart < jsonEnd && jsonLen > 0) {
            // Extract the JSON portion from the data and put it in a std::string
            std::string jsonString(reinterpret_cast<char *>(data) + jsonStart, jsonLen);

            // Remove whitespace characters
            jsonString.erase(std::remove_if(jsonString.begin(), jsonString.end(), [](char c) {
                                 return std::isspace(static_cast<unsigned char>(c));
                             }),
                             jsonString.end());

            Serial.printf("[http] #< [%lu] %s", sendID, jsonString.c_str());

            if (onDataCallback_)
                onDataCallback_(sendID, jsonString.c_str());

        }

        else {
            Serial.printf("[http] [%lu] ERR response not JSON");
        }

    } catch (const std::exception &e) {
        // Handle any other exceptions that might occur
        Serial.print("Exception: ");
        Serial.println(e.what());
    }
} */

void AsyncHTTP::handleData(AsyncClient *c, unsigned long sendID, void *data, size_t len)
{
    // debug
    /* Serial.print("[http] Received data: ");
    for (size_t i = 0; i < len; i++)
        Serial.write(reinterpret_cast<char *>(data)[i]);
    Serial.println(); */

    // size_t jsonStart = 0;

    // Find the start of the JSON content
    /*  for (size_t i = 0; i < len; i++) {
         if (reinterpret_cast<char *>(data)[i] == '{') {
             jsonStart = i;
             break;
         }
     } */
    // Find the start of the JSON content while ignoring spaces
    /*  std::string jsonData;
     bool        startCopying = false;
     for (size_t i = 0; i < len; i++) {
         char currentChar = reinterpret_cast<char *>(data)[i];

         if (currentChar == '{') {
             startCopying = true;
         }

         if (startCopying && !std::isspace(static_cast<unsigned char>(currentChar))) {
             jsonData += currentChar;
         }
     }  */

    if (data == nullptr) {
        // Handle invalid payload
        return;
    }

    const char *payloadStart = static_cast<const char *>(data);
    const char *jsonStart    = strstr(payloadStart, "{");
    if (!jsonStart)
        return; // JSON start not found

    const char *jsonEnd = strrchr(payloadStart, '}');
    if (!jsonEnd || jsonEnd < jsonStart)
        return; // JSON end not found or occurs before JSON start

    size_t jsonLen = (jsonEnd - jsonStart) + 1;
    if (jsonLen > (size_t)(payloadStart + sizeof(data) - jsonStart)) // JSON length exceeds available payload length
        return;                                                      // JSON length exceeds available payload length

    // Extract JSON data
    std::string jsonData(jsonStart, jsonLen);

    if (!jsonData.empty()) {
        // Remove spaces and tabs from the JSON string
        jsonData.erase(std::remove_if(jsonData.begin(), jsonData.end(), [](char c) {
                           return std::isspace(static_cast<unsigned char>(c));
                       }),
                       jsonData.end());

        // Use ArduinoJson to parse the modified JSON
        JsonDocument         doc; // Adjust capacity based on JSON size
        DeserializationError error = deserializeJson(doc, jsonData);
        if (error)
            Serial.printf("[http] ## [%lu] ERR json: %s", sendID, error.c_str());
        /* else
            Serial.printf("[http] [%lu] OK JSON ", sendID); */

        Serial.printf("[http] #< [%lu] %s", sendID, jsonData.c_str());
        if (onData_cb_json_)
            // onDataCallback_(sendID, jsonData.c_str());
            onData_cb_json_(sendID, doc);
    } else {
        Serial.printf("[http] [%lu] ERR response not JSON", sendID);
    }
}

//--------------------------------------------------------------------------------

void AsyncHTTP::loop()
{
    if (WiFi.status() != WL_CONNECTED || httpDataList.empty())
        return;

    static unsigned long lastExecutionTime = 0;
    unsigned long        currentTime       = millis();

    if (currentTime - lastExecutionTime >= 250) {
        lastExecutionTime = currentTime; // Set the last execution time to the current time

        auto oldestUnsent = httpDataList.begin(); // Find the first (oldest) unsent request

        if (oldestUnsent != httpDataList.end()) {
            oldestUnsent->timestamp = millis();

            if (oldestUnsent->sendID) {
                if (oldestUnsent->host[0] != '\0' && oldestUnsent->query[0] != '\0') {
                    sendRequest(oldestUnsent->sendID, oldestUnsent->timestamp, *oldestUnsent);
                } else {
                    Serial.printf("[http] ## ERR Incomplete data, skipping request");
                }
            }

            httpDataList.erase(oldestUnsent); // Remove the oldest request from the list
        }
    }
}

//--------------------------------------------------------------------------------
