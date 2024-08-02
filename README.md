### Sample usage

```cpp
#include <TimeLib.h> // time_long (date, time lib)

#include "modules/async_http.h"
AsyncHTTP asynchttp;

struct online_settings
{
    bool          wifi         = 0;
    bool          wan          = 0;
    bool          time         = 0;
    bool          mqtt         = 0;
    bool          domo         = 0;
    bool          ws           = 0;
    //
    unsigned long id_http_wan  = 0;
    unsigned long id_http_domo = 0;
    unsigned long id_http_time = 0;
    unsigned long id_http_sun  = 0;

} online;

void asynchttp_onDataCallback(unsigned long sentID, JsonDocument &doc)
{
    // Serial.println(sentID);
    // Serial.println(data);

    // time
    // date.jsontest.com:80/
    // {"date":"07-31-2024","milliseconds_since_epoch":1722398208355,"time":"03:56:48AM"}
    if (sentID == online.id_http_time) //
    {
        const char *dateString = doc["date"];
        const char *timeString = doc["time"];

        doc.remove("milliseconds_since_epoch");

        if ((dateString) && (timeString)) // date-time OK
        {
            // Parse date
            int year, month, day;
            if (sscanf(dateString, "%d-%d-%d", &month, &day, &year) == 3) // date-OK
            {
                int hour, minute, second;
                if (sscanf(timeString, "%d:%d:%d", &hour, &minute, &second) == 3) // time-ok
                {
                    // Convert PM time to 24-hour format
                    if (hour == 12 && strstr(timeString, "PM")) {
                        hour = 12;
                    } else if (strstr(timeString, "PM")) {
                        hour += 12;
                    } else if (hour == 12 && strstr(timeString, "AM")) {
                        hour = 0;
                    }

                    // todo, summer/winter
                    hour += wifi.time_offset; // Add offset for time hour

                    setTime(hour, minute, second, day, month, year);

                    // setStatusLED(LED_TIME, sLED_GREEN);
                    online.time = true;
                    
                    Serial.printf("[time] Synced", time_long());
                }
            }
        }
    }

    // wan
    else if (sentID == online.id_http_wan) //
    {
        const char *_ip = doc["ip"].as<const char *>(); // wan   {"ip": "89.135.140.144"}
        if (_ip) {
            //setStatusLED(status_leds, LED_WAN, GREEN);
            online.wan = true;
        }
    }

    // [domo]
    else if (sentID == online.id_http_domo) //
    {
        const char *status = doc["status"];
        const char *title  = doc["title"];

        // {"status":"OK","title":"UpdateSensor"}
        if (title && status) {
            //setStatusLED(status_leds, LED_DOMO, GREEN);
            online.domo = true;
        }

        // "/json.htm?type=command&param=getServerTime"
        // {"ServerTime":"2023-03-28 22:11:04","status":"OK","title":"getServerTime"}
        const char *serverTime = doc["ServerTime"];
        if (serverTime) {
            online.domo = true;
            Serial.printf("[DOMO] #< serverTime:", serverTime);
        }
    }

    // sunrise-sundown
    //{"results":{"sunrise":"5:34:37AM","sunset":"3:20:22PM","solar_noon":"10:27:29AM","day_length":"09:45:45","civil_twilight_begin":"5:03:40AM","civil_twilight_end":"3:51:19PM","nautical_twilight_begin":"4:26:59AM","nautical_twilight_end":"4:28:00PM","astronomical_twilight_begin":"3:51:07AM","astronomical_twilight_end":"5:03:52PM"}
    //
    // [http] #< [3] {"results":{"sunrise":"3:16:54AM","sunset":"6:23:44PM","solar_noon":"10:50:19AM","day_length":"15:06:50","civil_twilight_begin":"2:42:22AM","civil_twilight_end":"6:58:16PM","nautical_twilight_begin":"1:55:26AM","nautical_twilight_end":"7:45:13PM","astronomical_twilight_begin":"12:57:54AM","astronomical_twilight_end":"8:42:45PM"},"status":"OK","tzid":"UTC"}
    // [http] sunrise: 03:16:54
    // [http] sundown: 18:23:44
    else if (sentID == online.id_http_sun) //
    {
        const char *sunriseTime = doc["results"]["sunrise"];
        if (sunriseTime) {
            int  hours, minutes, seconds;
            char meridian[3];
            sscanf(sunriseTime, "%d:%d:%d%2s", &hours, &minutes, &seconds, meridian);
            if (strcmp(meridian, "PM") == 0 && hours < 12) {
                hours += 12;
            } else if (strcmp(meridian, "AM") == 0 && hours == 12) {
                hours = 0;
            }
            //snprintf(wifi.sunrise, sizeof(wifi.sunrise), "%02d:%02d:%02d", hours, minutes, seconds);

            Serial.printf("[http] sunrise:", wifi.sunrise);
        }

        const char *sundownTime = doc["results"]["sunset"];
        if (sundownTime) {
            int  hours, minutes, seconds;
            char meridian[3];
            sscanf(sundownTime, "%d:%d:%d%2s", &hours, &minutes, &seconds, meridian);
            if (strcmp(meridian, "PM") == 0 && hours < 12) {
                hours += 12;
            } else if (strcmp(meridian, "AM") == 0 && hours == 12) {
                hours = 0;
            }
            //snprintf(wifi.sundown, sizeof(wifi.sundown), "%02d:%02d:%02d", hours, minutes, seconds);

            Serial.printf("[http] sundown:", wifi.sundown);
        }
    }
}


void Setup_Wifi_http_arg()
{
    asynchttp.onData_cb_json(asynchttp_onDataCallback);

    online.id_http_wan  = asynchttp.send2http_url("ip.jsontest.com");
    online.id_http_time = asynchttp.send2http_url("date.jsontest.com/");
    online.id_http_sun  = asynchttp.send2http_url("api.sunrise-sunset.org/json?lat=47.4978918&lng=19.0401609");
    online.id_http_domo = asynchttp.send2http_fmt(domo.auth, domo.srv, domo.port, "/json.htm?type=command&param=addlogmessage&message=ping_%s_%s_%s&level=1", wifi.ip_local, dev_model, dev_id); // ping_192.168.11.24_iqRelay4CH_34ab951a6a0b
}

example:
arg = "http//192.168.1.10:80/json.htm?type=command&param=udevice&idx=25&nvalue=0&svalue=230.17"
asynchttp.send2http_url(arg);

output:
// [http] #> [7] 192.168.1.10:80/json.htm?type=command&param=udevice&idx=25&nvalue=0&svalue=230.17
// [http] #< [7] {"status":"OK","title":"UpdateDevice"}
```
