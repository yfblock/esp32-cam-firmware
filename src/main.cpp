// #include <Arduino.h>
// // #include <WiFi.h>
// #include <SPI.h>
// #include <ESP32SPISlave.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/queue.h>
// #include "wifi_ctl.h"

// // WiFi configuration (default credentials)
// const char* WIFI_SSID = "yfblock_for_guest";
// const char* WIFI_PASSWORD = "secure1114";

// // SPI Slave for virtual network interface communication with Linux
// ESP32SPISlave slave;

// WiFiCtl wifiCtl(WIFI_SSID, WIFI_PASSWORD);

// void setup(void) {
//     Serial.begin(115200);

//     slave.setSpiMode(SPI_MODE0); // Set SPI mode (CPOL=0, CPHA=0)
//     slave.setQueueSize(1); // Set queue size for transactions
//     // initialize SPI Slave for communication with Linux
//     slave.begin();

//     // initialize WiFi with current credentials
//     // wifiCtl.init();

//     // Serial.print("Connecting to WiFi");
//     // int retryCount = 0;
//     // while (WiFi.status() != WL_CONNECTED) {
//     //     delay(500);
//     //     Serial.print(".");
//     //     retryCount++;
//     //     if (retryCount > 20) { // timeout after 10 seconds
//     //         Serial.println("Failed to connect to WiFi");
//     //         break;
//     //     }
//     // }
//     // if (WiFi.status() == WL_CONNECTED) {
//     //     Serial.println();
//     //     Serial.print("Connected. IP address: ");
//     //     Serial.println(WiFi.localIP());
//     // }
// }
// void loop(void) {
    
//     static constexpr size_t BUFFER_SIZE = 8;
//     static constexpr size_t QUEUE_SIZE = 1;
//     uint8_t tx_buf[BUFFER_SIZE] {1, 2, 3, 4, 5, 6, 7, 8};
//     uint8_t rx_buf[BUFFER_SIZE] {0, 1, 2, 3, 4, 5, 6, 7};

//     const size_t received_bytes = slave.transfer(tx_buf, rx_buf, BUFFER_SIZE);
//     Serial.print("Received SPI data, length: ");
//     // // check for serial input to change credentials
//     // if (Serial.available()) {
//     //     String line = Serial.readStringUntil('\n');
//     //     line.trim();
//     //     if (line.length() > 0) {
//     //         // expected format: ssid,password
//     //         int comma = line.indexOf(',');
//     //         if (comma > 0) {
//     //             String newSsid = line.substring(0, comma);
//     //             String newPass = line.substring(comma + 1);
//     //             newSsid.trim();
//     //             newPass.trim();
//     //             if (newSsid.length() && newPass.length()) {
//     //                 wifiCtl.connect(newSsid, newPass);
//     //             }
//     //         } else {
//     //             Serial.println("Invalid input. Use ssid,password");
//     //         }
//     //     }
//     // }

//     // // SPI data handling with connection check (synchronous mode with timeout)
//     // static uint8_t rx_buffer[64];  // Reduced buffer size to avoid issues
//     // static uint8_t tx_dummy[1] = {0}; // dummy tx data
//     // static unsigned long last_check_time = 0;
//     // const unsigned long check_interval = 5000; // Check every 5 seconds

//     // // Periodic connection check
//     // if (millis() - last_check_time > check_interval) {
//     //     Serial.println("SPI Slave: Checking connection... (Send data from Master to test)");
//     //     last_check_time = millis();
//     // }

//     // // Synchronous SPI transaction with timeout simulation
//     // unsigned long start_time = millis();
//     // bool transaction_success = false;
//     // while (millis() - start_time < 1000) {  // Timeout after 1 second
//     //     if (slave.queue(tx_dummy, rx_buffer, sizeof(rx_buffer))) {
//     //         transaction_success = true;
//     //         break;
//     //     }
//     //     delay(1);  // Small delay to avoid busy loop
//     // }

//     // if (transaction_success) {
//     //     // Transaction completed, process received data
//     //     Serial.println("SPI Slave: Transaction detected - connection appears correct!");
//     //     size_t len = rx_buffer[0];  // Assume first byte is length
//     //     if (len > 0 && len < sizeof(rx_buffer)) {
//     //         Serial.print("Received SPI data, length: ");
//     //         Serial.println(len);
//     //         Serial.print("Data: ");
//     //         for (size_t i = 1; i <= len && i < 10; i++) {
//     //             Serial.print(rx_buffer[i], HEX);
//     //             Serial.print(" ");
//     //         }
//     //         Serial.println();
//     //         // TODO: implement IP forwarding to WiFi
//     //     } else {
//     //         Serial.println("Received invalid or empty SPI data.");
//     //     }
//     // }
// }

#include <ESP32SPISlave.h>

ESP32SPISlave slave;

static constexpr size_t BUFFER_SIZE = 8;
static constexpr size_t QUEUE_SIZE = 8;
uint8_t tx_buf[BUFFER_SIZE] {1, 2, 3, 4, 5, 6, 7, 8};
uint8_t rx_buf[BUFFER_SIZE] {0, 0, 0, 0, 0, 0, 0, 0};

void setup()
{
    Serial.begin(115200);
    slave.setDataMode(SPI_MODE3);   // default: SPI_MODE0
    // slave.setQueueSize(QUEUE_SIZE); // default: 1

    // begin() after setting
    slave.begin(HSPI, 12, 13, 11, 10);
    // slave.begin();  // default: HSPI (please refer README for pin assignments)
}

void loop()
{
    // do some initialization for tx_buf and rx_buf

    // Queue the transaction
    if (slave.queue(tx_buf, rx_buf, BUFFER_SIZE)) {
        // Wait for the transaction to complete
        std::vector<size_t> received_sizes = slave.wait();
        if (!received_sizes.empty()) {
            const size_t received_bytes = received_sizes[0];
            Serial.printf("Received SPI data, length: %u\n", received_bytes);
            Serial.print("Received data: ");
            for (size_t i = 0; i < received_bytes; i++) {
                Serial.printf("%02X ", rx_buf[i]);
            }
            Serial.println();
        } else {
            Serial.println("No data received in transaction.");
        }
    } else {
        Serial.println("Failed to queue SPI transaction.");
    }
    // do something with received_bytes and rx_buf if needed
}