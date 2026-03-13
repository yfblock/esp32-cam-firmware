#include <Arduino.h>
#include <ESP32SPISlave.h>
#include "control.h"
#include "esp_camera.h"
#include "sensor.h"
#include "jpeg_data.h"

ESP32SPISlave slave;

static constexpr size_t QUEUE_SIZE = 8;
sensor_t *sensor;
camera_sensor_info_t *info;

CameraInfo camera_info;

uint8_t initSPISlave() {
    slave.setDataMode(SPI_MODE3);   // default: SPI_MODE0
    slave.setQueueSize(QUEUE_SIZE); // default: 1

    // begin() after setting
    if (!slave.begin(HSPI, 12, 13, 11, 10)) {
        Serial.println("Failed to initialize SPI slave");
        return 0;
    }
    return 1;
}

uint8_t readCommand() {
    uint8_t cmd = 0;
    uint8_t len = slave.transfer(NULL, &cmd, sizeof(cmd)); // Queue a transaction to read a command byte
    if (len > 0) {
        Serial.printf("Received SPI data, length: %u  cmd: %d\n", len, cmd);
        return cmd;
    } else {
        Serial.println("No data received in transaction.");
        return 0;
    }
    // #define BUFFER_SIZE 8
    // uint8_t tx_buf[BUFFER_SIZE] {1, 2, 3, 4, 5, 6, 7, 8};
    // uint8_t rx_buf[BUFFER_SIZE] {0, 0, 0, 0, 0, 0, 0, 0};

    // while(true) {
    //     if (slave.queue(tx_buf, rx_buf, BUFFER_SIZE)) {
    //         // Wait for the transaction to complete
    //         std::vector<size_t> received_sizes = slave.wait();
    //         if (!received_sizes.empty()) {
    //             // uint8_t cmd = rx_buf[0];  // Assume first byte is command
    //             // executeCommand(cmd);  // Execucte command based on received byte
    //             const size_t received_bytes = received_sizes[0];
    //             Serial.printf("Received SPI data, length: %u\n", received_bytes);
    //             Serial.print("Received data: ");
    //             for (size_t i = 0; i < received_bytes; i++) {
    //                 Serial.printf("%02X ", rx_buf[i]);
    //             }
    //             Serial.println();
    //         } else {
    //             Serial.println("No data received in transaction.");
    //         }
    //     } else {
    //         Serial.println("Failed to queue SPI transaction.");
    //     }
    // }
    // return 0;
}

void executeCommand(const uint8_t cmd)
{
    int ret = 0;
    int len = 0;
    switch (cmd) {
        case SPI_CMD_INIT:
            // sensor = esp_camera_sensor_get();
            // if(sensor) {
            //     info = esp_camera_sensor_get_info(&sensor->id);
            // }
            Serial.println("Executing command 0x01(INIT): Initialize the camera");
            break;
        case SPI_CMD_GET_CAMERA_INFO:
            Serial.println("Executing command 0x02(GET_CAMERA_INFO): Get camera information");
            camera_info.width = 1280;
            camera_info.height = 720;
            camera_info.format = 0; // JPEG
            camera_info.connected = 1;
            
            ret = slave.transfer((uint8_t*)&camera_info, NULL, sizeof(camera_info));
            Serial.printf("tranfer size: %d\n", sizeof(camera_info));
            if (ret != ESP_OK) {
                Serial.println("Failed to queue camera info");
            }
            break;
        case SPI_CMD_GET_CAMERA_FRAME:
            Serial.println("Executing command 0x03(GET_CAMERA_FRAME): Get camera frame");
            len = sizeof(capture_jpeg);
            Serial.printf("Sending camera frame, length: %u  int len: %d\n", len, sizeof(int));
            ret = slave.transfer((uint8_t*)&len, NULL, sizeof(int));
            if (ret != ESP_OK) {
                Serial.println("Failed to send camera frame length");
            }
            // ret = slave.transfer(capture_jpeg, NULL, len);
            // if (ret != ESP_OK) {
            //     Serial.println("Failed to send camera frame data");
            // }
            break;
        default:
            Serial.println("Unknown command");
    }
}
