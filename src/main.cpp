#include <Arduino.h>
// #include <WiFi.h>
#include <SPI.h>
#include <ESP32SPISlave.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "wifi_ctl.h"

// WiFi configuration (default credentials)
const char* WIFI_SSID = "yfblock_for_guest";
const char* WIFI_PASSWORD = "secure1114";

// current credentials (can be updated at runtime)
String currentSSID = WIFI_SSID;
String currentPassword = WIFI_PASSWORD;

// SPI Slave for virtual network interface communication with Linux
ESP32SPISlave slave;

void setup(void) {
    Serial.begin(115200);
    pinMode(2, OUTPUT);

    // initialize SPI
    SPI.begin();
    pinMode(5, OUTPUT); // SS pin
    digitalWrite(5, HIGH); // deselect slave

    // initialize SPI Slave for communication with Linux
    slave.begin();

    // optionally load credentials from external source before connecting
    currentSSID = String(WIFI_SSID);
    currentPassword = String(WIFI_PASSWORD);

    // initialize WiFi with current credentials
    WiFi.mode(WIFI_STA);
    WiFi.begin(currentSSID.c_str(), currentPassword.c_str());

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected. IP address: ");
    Serial.println(WiFi.localIP());
}

// function to demonstrate SPI data transfer
void spiTransferData() {
    byte dataToSend = 0x42; // example data to send
    digitalWrite(5, LOW);   // select slave (SS low)
    byte received = SPI.transfer(dataToSend); // send and receive
    digitalWrite(5, HIGH);  // deselect slave (SS high)

    Serial.print("SPI Sent: 0x");
    Serial.print(dataToSend, HEX);
    Serial.print(", Received: 0x");
    Serial.println(received, HEX);
}

// yfblockwifi,wbnc9637
// yfblock_for_guest,secure1114
void loop(void) {
    // check for serial input to change credentials
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            // expected format: ssid,password
            int comma = line.indexOf(',');
            if (comma > 0) {
                String newSsid = line.substring(0, comma);
                String newPass = line.substring(comma + 1);
                newSsid.trim();
                newPass.trim();
                if (newSsid.length() && newPass.length()) {
                    currentSSID = newSsid;
                    currentPassword = newPass;
                    connectToWiFi(currentSSID, currentPassword);
                }
            } else {
                Serial.println("Invalid input. Use ssid,password");
            }
        }
    }

    // SPI data is handled synchronously below

    // handle SPI data for virtual network interface (synchronous)
    static uint8_t rx_buffer[1500];
    static uint8_t tx_dummy[1] = {0}; // dummy tx data
    if (slave.queue(0, tx_dummy, rx_buffer, sizeof(rx_buffer))) {
        // received data in rx_buffer
        size_t len = rx_buffer[0]; // assume first byte is length (placeholder)
        Serial.print("Received SPI packet, len: ");
        Serial.println(len);
        // TODO: implement IP forwarding to WiFi
    }

    // regular status output (every second)
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Not connected");
        }
        lastPrint = millis();
    }

    // SPI data transfer demo (every 5 seconds)
    static unsigned long lastSpi = 0;
    if (millis() - lastSpi >= 5000) {
        spiTransferData();
        lastSpi = millis();
    }
}
