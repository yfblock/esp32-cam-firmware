#include <Arduino.h>
#include <ESP32DMASPISlave.h>
#include "control.h"
#include "esp_camera.h"
#include "sensor.h"
#include "jpeg_data.h"

ESP32DMASPI::Slave slave;

#define PIN_SCLK        12
#define PIN_MOSI        11
#define PIN_MISO        20
#define PIN_CS          10
#define DMA_BUF_SIZE    8192
#define QUEUE_SIZE      8

static uint8_t *dma_tx_buf;
static uint8_t *dma_rx_buf;

sensor_t *sensor;
camera_sensor_info_t *info;

CameraInfo camera_info;

static inline size_t dmaAlignSize(size_t size) {
    return (size + 3) & ~3;
}

uint8_t initSPISlave() {
    dma_tx_buf = ESP32DMASPI::Slave::allocDMABuffer(DMA_BUF_SIZE);
    dma_rx_buf = ESP32DMASPI::Slave::allocDMABuffer(DMA_BUF_SIZE);
    if (!dma_tx_buf || !dma_rx_buf) {
        Serial.println("Failed to allocate DMA buffers");
        return 0;
    }

    slave.setDataMode(SPI_MODE0);
    slave.setMaxTransferSize(DMA_BUF_SIZE);
    slave.setQueueSize(QUEUE_SIZE);

    if (!slave.begin(FSPI, PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS)) {
        Serial.println("Failed to initialize SPI slave");
        return 0;
    }

    Serial.println("SPI slave initialized with DMA");
    return 1;
}

uint8_t readCommand() {
    memset(dma_tx_buf, 0, 4);
    memset(dma_rx_buf, 0, 4);
    dma_tx_buf[0] = READY_CODE;

    size_t len = slave.transfer(dma_tx_buf, dma_rx_buf, 4);
    if (len > 0) {
        uint8_t cmd = dma_rx_buf[0];
        Serial.printf("Received SPI data, length: %u  cmd: %d\n", len, cmd);
        return cmd;
    } else {
        Serial.println("No data received in transaction.");
        return 0;
    }
}

void executeCommand(const uint8_t cmd)
{
    size_t offset = 0;

    switch (cmd) {
        case 0:
            break;
        case SPI_CMD_INIT:
            Serial.println("Executing command 0x01(INIT): Initialize the camera");
            break;
        case SPI_CMD_GET_CAMERA_INFO: {
            Serial.println("Executing command 0x02(GET_CAMERA_INFO): Get camera information");
            size_t aligned_size = dmaAlignSize(sizeof(camera_info));
            memset(dma_tx_buf + offset, 0, aligned_size);
            CameraInfo *ci = (CameraInfo *)(dma_tx_buf + offset);
            ci->width = 1280;
            ci->height = 720;
            ci->format = 0; // JPEG
            ci->connected = 1;
            Serial.printf("Transfer size: %d: ", sizeof(camera_info));
            for (int i = 0; i < (int)sizeof(camera_info); i++) {
                Serial.printf("%02X ", dma_tx_buf[offset + i]);
            }
            Serial.println();
            slave.transfer(dma_tx_buf + offset, NULL, aligned_size);
            Serial.printf("transfer size: %d\n", sizeof(camera_info));
            break;
        }
        case SPI_CMD_GET_CAMERA_FRAME: {
            Serial.println("Executing command 0x03(GET_CAMERA_FRAME): Get camera frame");
            int frame_len = 0x1000;
            Serial.printf("Sending camera frame, length: %u  int len: %d\n", frame_len, sizeof(int));

            memcpy(dma_tx_buf + offset, &frame_len, sizeof(int));
            slave.queue(dma_tx_buf + offset, NULL, sizeof(int));
            offset += sizeof(int);

            memcpy(dma_tx_buf + offset, capture_jpeg, frame_len);
            slave.queue(dma_tx_buf + offset, NULL, frame_len);
            break;
        }
        default:
            Serial.println("Unknown command");
    }

    std::vector<size_t> received_sizes = slave.wait();
    for (size_t i = 0; i < received_sizes.size(); i++) {
        Serial.printf("Transaction %u: %u bytes\n", i, received_sizes[i]);
    }
    Serial.println("Transaction completed");
}
