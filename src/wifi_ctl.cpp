#include <Arduino.h>
#include "wifi_ctl.h"

// connect to WiFi and print status
void connectToWiFi(const String& ssid, const String& pass) {
    Serial.print("Switching to WiFi network: ");
    Serial.println(ssid);
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 20000) { // timeout after 20 seconds
            Serial.println(" failed (timeout)");
            return;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected. IP address: ");
    Serial.println(WiFi.localIP());
}
