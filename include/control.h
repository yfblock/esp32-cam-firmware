#pragma once
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define SPI_CMD_INIT 0x1
#define SPI_CMD_GET_CAMERA_INFO 0x2
#define SPI_CMD_GET_CAMERA_FRAME 0x3

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
