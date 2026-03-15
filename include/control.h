#pragma once
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

// Command IDs
#define CMD_INIT            0x01
#define CMD_GET_CAMERA_INFO 0x02
#define CMD_GET_CAMERA_FRAME 0x03
#define CMD_PING            0x7F

// Response: request_id | RESP_MASK
#define RESP_MASK           0x80
#define RESP_FRAME_CHUNK    0x90

// SLIP framing
#define SLIP_END            0xC0
#define SLIP_ESC            0xDB
#define SLIP_ESC_END        0xDC
#define SLIP_ESC_ESC        0xDD

// Frame transfer parameters
#define FRAME_CHUNK_SIZE    4096
#define FRAME_MAX_SIZE      (2 * 1024 * 1024)

struct CameraInfo {
    uint16_t width;
    uint16_t height;
    uint8_t format;     // 0 = JPEG, 1 = RAW
    uint8_t connected;  // 0 = disconnected, 1 = connected
};

#ifndef __KERNEL__
void initUart(void);
void handleUart(void);
#endif
