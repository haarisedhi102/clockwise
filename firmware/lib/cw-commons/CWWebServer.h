#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include "StatusController.h"
#include "SettingsWebPage.h"

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

WiFiServer server(80);

struct ClockwiseWebServer
{
  String httpBuffer;
  bool force_restart;
  const char* HEADER_TEMPLATE_D = "X-%s: %d\r\n";
  const char* HEADER_TEMPLATE_F = "X-%s: %.6f\r\n";
  const char* HEADER_TEMPLATE_S = "X-%s: %s\r\n";

  String urlDecode(const String& encoded)
  {
    String decoded;
    decoded.reserve(encoded.length());

    for (size_t i = 0; i < encoded.length(); i++) {
      if (encoded[i] == '%' && i + 2 < encoded.length()) {
        char high = encoded[i + 1];
        char low = encoded[i + 2];
        auto hexValue = [](char c) -> int {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          return -1;
        };
        int highValue = hexValue(high);
        int lowValue = hexValue(low);
        if (highValue >= 0 && lowValue >= 0) {
          decoded += static_cast<char>((highValue << 4) | lowValue);
          i += 2;
          continue;
        }
      }

      decoded += encoded[i] == '+' ? ' ' : encoded[i];
    }

    return decoded;
  }
 
  static ClockwiseWebServer *getInstance()
  {
    static ClockwiseWebServer base;
    return &base;
  }

  void startWebServer()
  {
    server.begin();
    StatusController::getInstance()->blink_led(100, 3);
  }

  void stopWebServer()
  {
    server.stop();
  }

  void handleHttpRequest()
  {
    if (force_restart)
      StatusController::getInstance()->forceRestart();


    WiFiClient client = server.available();
    if (client)
    {
      StatusController::getInstance()->blink_led(100, 1);

      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          httpBuffer.concat(c);

          if (c == '\n')
          {
            uint8_t method_pos = httpBuffer.indexOf(' ');
            uint8_t path_pos = httpBuffer.indexOf(' ', method_pos + 1);

            String method = httpBuffer.substring(0, method_pos);
            String path = httpBuffer.substring(method_pos + 1, path_pos);
            String key = "";
            String value = "";

            if (path.indexOf('?') > 0)
            {
              key = path.substring(path.indexOf('?') + 1, path.indexOf('='));
              value = path.substring(path.indexOf('=') + 1);
              key = urlDecode(key);
              value = urlDecode(value);
              path = path.substring(0, path.indexOf('?'));
            }

            processRequest(client, method, path, key, value);
            httpBuffer = "";
            break;
          }
        }
      }
      delay(1);
      client.stop();
    }
  }

  void processRequest(WiFiClient client, String method, String path, String key, String value)
  {
    if (method == "GET" && path == "/") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println(SETTINGS_PAGE);
    } else if (method == "GET" && path == "/get") {
      getCurrentSettings(client);
    } else if (method == "GET" && path == "/read") {
      if (key == "pin") {
        readPin(client, key, value.toInt());
      }
    } else if (method == "POST" && path == "/restart") {
      client.println("HTTP/1.0 204 No Content");
      force_restart = true;
    } else if (method == "POST" && path == "/set") {
      ClockwiseParams::getInstance()->load();
      //a baby seal has died due this ifs
      if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT) {
        ClockwiseParams::getInstance()->displayBright = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_SSID) {
        ClockwiseParams::getInstance()->wifiSsid = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_PASSWORD) {
        ClockwiseParams::getInstance()->wifiPwd = value;
      } else if (key == "autoBright") {   //autoBright=0010,0800
        ClockwiseParams::getInstance()->autoBrightMin = value.substring(0,4).toInt();
        ClockwiseParams::getInstance()->autoBrightMax = value.substring(5,9).toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN) {
        ClockwiseParams::getInstance()->swapBlueGreen = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED) {
        ClockwiseParams::getInstance()->swapBlueRed = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT) {
        ClockwiseParams::getInstance()->use24hFormat = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_LDR_PIN) {
        ClockwiseParams::getInstance()->ldrPin = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_TIME_ZONE) {
        ClockwiseParams::getInstance()->timeZone = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_LATITUDE) {
        ClockwiseParams::getInstance()->latitude = value.toDouble();
      } else if (key == ClockwiseParams::getInstance()->PREF_LONGITUDE) {
        ClockwiseParams::getInstance()->longitude = value.toDouble();
      } else if (key == ClockwiseParams::getInstance()->PREF_NTP_SERVER) {
        ClockwiseParams::getInstance()->ntpServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_FILE) {
        ClockwiseParams::getInstance()->canvasFile = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_SERVER) {
        ClockwiseParams::getInstance()->canvasServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MANUAL_POSIX) {
        ClockwiseParams::getInstance()->manualPosix = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION) {
        ClockwiseParams::getInstance()->displayRotation = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_DRIVER) {
        ClockwiseParams::getInstance()->driver = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_I2CSPEED) {
        ClockwiseParams::getInstance()->i2cSpeed = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_E_PIN) {
        ClockwiseParams::getInstance()->E_pin = value.toInt();
      }
      ClockwiseParams::getInstance()->save();
      client.println("HTTP/1.0 204 No Content");
    }
  }



  void readPin(WiFiClient client, String key, uint16_t pin) {
    ClockwiseParams::getInstance()->load();

    client.println("HTTP/1.0 204 No Content");
    client.printf(HEADER_TEMPLATE_D, key, analogRead(pin));
    
    client.println();
  }


  void getCurrentSettings(WiFiClient client) {
    ClockwiseParams::getInstance()->load();

    client.println("HTTP/1.0 204 No Content");

    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT, ClockwiseParams::getInstance()->displayBright);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MIN, ClockwiseParams::getInstance()->autoBrightMin);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MAX, ClockwiseParams::getInstance()->autoBrightMax);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN, ClockwiseParams::getInstance()->swapBlueGreen);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED, ClockwiseParams::getInstance()->swapBlueRed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT, ClockwiseParams::getInstance()->use24hFormat);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_LDR_PIN, ClockwiseParams::getInstance()->ldrPin);    
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_TIME_ZONE, ClockwiseParams::getInstance()->timeZone.c_str());
    client.printf(HEADER_TEMPLATE_F, ClockwiseParams::getInstance()->PREF_LATITUDE, ClockwiseParams::getInstance()->latitude);
    client.printf(HEADER_TEMPLATE_F, ClockwiseParams::getInstance()->PREF_LONGITUDE, ClockwiseParams::getInstance()->longitude);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_WIFI_SSID, ClockwiseParams::getInstance()->wifiSsid.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_NTP_SERVER, ClockwiseParams::getInstance()->ntpServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_FILE, ClockwiseParams::getInstance()->canvasFile.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_SERVER, ClockwiseParams::getInstance()->canvasServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MANUAL_POSIX, ClockwiseParams::getInstance()->manualPosix.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION, ClockwiseParams::getInstance()->displayRotation);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DRIVER, ClockwiseParams::getInstance()->driver);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_I2CSPEED, ClockwiseParams::getInstance()->i2cSpeed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_E_PIN, ClockwiseParams::getInstance()->E_pin);

    client.printf(HEADER_TEMPLATE_S, "CW_FW_VERSION", CW_FW_VERSION);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_NAME", CW_FW_NAME);
    client.printf(HEADER_TEMPLATE_S, "CLOCKFACE_NAME", CLOCKFACE_NAME);
    client.println();
  }
  
};
