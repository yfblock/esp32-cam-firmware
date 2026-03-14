#pragma once
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define SPI_CMD_INIT 0x1
#define SPI_CMD_GET_CAMERA_INFO 0x2
#define SPI_CMD_GET_CAMERA_FRAME 0x3

#define READY_CODE 0xA5

#define SPI_READY_POLL_INTERVAL_US  200
#define SPI_READY_TIMEOUT_MS        2000
#define SPI_FRAME_CHUNK_SIZE        4096
#define SPI_FRAME_MAX_SIZE          (512 * 1024)

struct CameraInfo {
    uint16_t width;
    uint16_t height;
    uint8_t format;     // e.g., 0 for JPEG, 1 for RAW
    uint8_t connected;  // 0 for not connected, 1 for connected
};

#ifndef __KERNEL__
uint8_t initSPISlave(void);
uint8_t readCommand(void);
void executeCommand(const uint8_t cmd);
#endif
