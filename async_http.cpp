#include "async_http.h"

AsyncHTTP::AsyncHTTP() : sendID(0), requestOngoing(false), lastRequestTime(0) {}

AsyncHTTP::~AsyncHTTP() {}

unsigned long AsyncHTTP::send2http(const std::string auth, const std::string host, int port, const std::string query)
{
    sendID++;

    Request req = {auth, host, port, query, sendID};
    requestQueue.push(req);

    //LOG_V("[http] [%lu] %s:%i%s\n", sendID, host.c_str(), port, query.c_str());
    return sendID;
}

unsigned long AsyncHTTP::send2http_url(const std::string url)
{
    std::string protocol;
    std::string host;
    int         port  = 80;
    std::string query = "/";

    size_t protocolPos = url.find("://");
    if (protocolPos != std::string::npos) {
        protocol = url.substr(0, protocolPos);
        if (protocol == "https") {
            port = 443;
        }
    }

    size_t hostStart = (protocolPos == std::string::npos) ? 0 : protocolPos + 3;

    size_t hostEnd = url.find('/', hostStart);
    size_t portPos = url.find(':', hostStart);

    if (portPos != std::string::npos && portPos < hostEnd) {
        host = url.substr(hostStart, portPos - hostStart);
        port = std::stoi(url.substr(portPos + 1, hostEnd - portPos - 1));
    } else {
        host = url.substr(hostStart, hostEnd - hostStart);
    }

    if (hostEnd != std::string::npos) {
        query = url.substr(hostEnd);
    }

    sendID++;

    Request req = {"", host, port, query, sendID};
    requestQueue.push(req);

    //LOG_V("[http] [%lu] %s:%i%s\n", sendID, host.c_str(), port, query.c_str());
    return sendID;
}

void AsyncHTTP::handleData(AsyncClient *c, unsigned long sendID, void *data, size_t len)
{
    const char *payloadStart = static_cast<const char *>(data);
    const char *jsonStart    = strstr(payloadStart, "{");
    if (!jsonStart) return;

    const char *jsonEnd = strrchr(payloadStart, '}');
    if (!jsonEnd || jsonEnd < jsonStart) return;

    size_t jsonLen = (jsonEnd - jsonStart) + 1;
    if (jsonLen > len) return;

    std::string jsonData(jsonStart, jsonLen);

    if (!jsonData.empty()) {
        jsonData.erase(std::remove_if(jsonData.begin(), jsonData.end(), ::isspace), jsonData.end());

        JsonDocument         doc;
        DeserializationError error = deserializeJson(doc, jsonData);
        if (error) {
           LOG_V("[http] #< [%lu] nonjson: %s\n", sendID, error.c_str());
        } else {
           LOG_I("[http] #< [%lu] json: %s\n", sendID, jsonData.c_str());
            if (onDataCallbackJson_) {
                onDataCallbackJson_(sendID, doc);
            }
        }
    } else {
       LOG_V("[http] #< [%lu] ERR response not JSON\n", sendID);
    }
}

void AsyncHTTP::onDataCallback(std::function<void(unsigned long, const std::string &)> callback)
{
    onDataCallback_ = callback;
}

void AsyncHTTP::onDataCallbackJson(std::function<void(unsigned long, JsonDocument &doc)> callback)
{
    onDataCallbackJson_ = callback;
}

unsigned long AsyncHTTP::sendRequest(const Request &req)
{
    AsyncClient *client = new AsyncClient();
    if (!client)
        return 0;

    std::string request =
        "GET " + req.query + " HTTP/1.1\r\n" +
        "Host: " + req.host + ":" + std::to_string(req.port) + "\r\n" +
        "Authorization: Basic " + req.auth + "\r\n" +
        "User-Agent: iqESP\r\n" +
        "Connection: close\r\n\r\n";

    unsigned long requestID = req.id;

    client->onError([this, requestID](void *arg, AsyncClient *client, int error) {
       LOG_V("[http] ## [%lu] onError\n", requestID);
        delete client;
        requestOngoing = false;
    },
                    nullptr);

    client->onConnect([this, request, requestID](void *arg, AsyncClient *client) {
       LOG_V("[http] ## [%lu] onConnect\n", requestID);
        client->onError(nullptr, nullptr);

        client->onDisconnect([this, requestID](void *arg, AsyncClient *client) {
           LOG_V("[http] ## [%lu] onDisconnect\n", requestID);
            delete client;
            requestOngoing = false;
        },
                             nullptr);

        client->onData([this, requestID](void *arg, AsyncClient *client, void *data, size_t len) {
           LOG_V("[http] #< [%lu] onData\n", requestID);
            handleData(client, requestID, data, len);
            client->close();
        },
                       nullptr);

        client->write(request.c_str());
        lastRequestTime = millis();
        delay(1);
    },
                      nullptr);

   LOG_I("[http] #> [%lu] %s:%i%s\n", requestID, req.host.c_str(), req.port, req.query.c_str());
    if (!client->connect(req.host.c_str(), req.port)) {
       LOG_V("[http] ## [%lu] Connect ERR\n", requestID);
        delete client;
        return 0;
    }

    return requestID;
}

void AsyncHTTP::loop()
{
    if (!requestOngoing && !requestQueue.empty()) {
        processQueue();
    }

    if (requestOngoing && millis() - lastRequestTime > 10000) {
        LOG_V("[http] ## Timeout - closing connection");
        requestOngoing = false;
        processQueue();
    }
}

void AsyncHTTP::processQueue()
{
    if (!requestQueue.empty()) {
        Request req = requestQueue.front();
        requestQueue.pop();
        sendRequest(req);
        lastRequestTime = millis();
        requestOngoing  = true;
    }
}
