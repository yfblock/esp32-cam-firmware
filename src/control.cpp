#include <Arduino.h>
#include "control.h"
#include "esp_camera.h"
#include "sensor.h"

// Configurable Serial port and pins via macros
#ifndef CTRL_SERIAL
#define CTRL_SERIAL Serial
#endif

#ifndef CTRL_SERIAL_BAUD
#define CTRL_SERIAL_BAUD 1500000
#endif

#ifndef CTRL_SERIAL_RX
#define CTRL_SERIAL_RX 13
#endif

#ifndef CTRL_SERIAL_TX
#define CTRL_SERIAL_TX 15
#endif

#ifdef ENABLE_LOG
#ifndef LOG_SERIAL
#define LOG_SERIAL Serial1
#endif
#define LOG_PRINTLN(...)  LOG_SERIAL.println(__VA_ARGS__)
#define LOG_PRINTF(...)   LOG_SERIAL.printf(__VA_ARGS__)
#else
#define LOG_PRINTLN(...)  ((void)0)
#define LOG_PRINTF(...)   ((void)0)
#endif

#define RX_BUF_SIZE 4096

static uint8_t rx_buf[RX_BUF_SIZE];
static size_t rx_len = 0;
camera_fb_t *fb;

// ---- CRC-16/CCITT-FALSE (incremental) ----

static uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static inline uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    return crc16_update(0xFFFF, data, len);
}

// ---- SLIP ----

// SLIP TX buffer: worst case each byte doubles + 2 END markers
#define TX_BUF_SIZE ((FRAME_CHUNK_SIZE + 6) * 2 + 2)
static uint8_t tx_buf[TX_BUF_SIZE];

static size_t slipEncode(uint8_t *buf, size_t pos,
                         const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] == SLIP_END) {
            buf[pos++] = SLIP_ESC;
            buf[pos++] = SLIP_ESC_END;
        } else if (data[i] == SLIP_ESC) {
            buf[pos++] = SLIP_ESC;
            buf[pos++] = SLIP_ESC_ESC;
        } else {
            buf[pos++] = data[i];
        }
    }
    return pos;
}

static int slipDecode(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len) {
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        if (in[i] == SLIP_ESC) {
            if (++i >= in_len) return -1;
            if (in[i] == SLIP_ESC_END)
                out[j++] = SLIP_END;
            else if (in[i] == SLIP_ESC_ESC)
                out[j++] = SLIP_ESC;
            else
                return -1;
        } else {
            out[j++] = in[i];
        }
    }
    *out_len = j;
    return 0;
}

// ---- Packet build / parse ----
// Wire format: type(1) | seq(1) | payload_len(2 LE) | payload | crc16(2 LE)

static void sendPacket(uint8_t type, uint8_t seq,
                       const uint8_t *payload, uint16_t payload_len) {
    uint8_t header[4] = {
        type, seq,
        (uint8_t)(payload_len & 0xFF),
        (uint8_t)((payload_len >> 8) & 0xFF)
    };

    uint16_t crc = crc16_update(0xFFFF, header, 4);
    if (payload_len > 0 && payload)
        crc = crc16_update(crc, payload, payload_len);
    uint8_t crc_bytes[2] = {(uint8_t)(crc & 0xFF), (uint8_t)((crc >> 8) & 0xFF)};

    size_t pos = 0;
    tx_buf[pos++] = SLIP_END;
    pos = slipEncode(tx_buf, pos, header, 4);
    if (payload_len > 0 && payload)
        pos = slipEncode(tx_buf, pos, payload, payload_len);
    pos = slipEncode(tx_buf, pos, crc_bytes, 2);
    tx_buf[pos++] = SLIP_END;

    CTRL_SERIAL.write(tx_buf, pos);
}

static int parsePacket(const uint8_t *data, size_t len,
                       uint8_t *type, uint8_t *seq,
                       const uint8_t **payload, uint16_t *payload_len) {
    if (len < 6) return -1;

    *type = data[0];
    *seq  = data[1];
    *payload_len = data[2] | ((uint16_t)data[3] << 8);

    if (len != (size_t)(4 + *payload_len + 2))
        return -1;

    *payload = data + 4;

    uint16_t recv_crc = data[4 + *payload_len] |
                        ((uint16_t)data[4 + *payload_len + 1] << 8);
    uint16_t calc_crc = crc16_ccitt(data, 4 + *payload_len);
    if (recv_crc != calc_crc)
        return -1;

    return 0;
}

// ---- SLIP frame extraction from rx_buf ----

static int extractFrame(uint8_t *frame_buf, size_t *frame_len) {
    // Skip leading END markers
    size_t start = 0;
    while (start < rx_len && rx_buf[start] == SLIP_END)
        start++;

    if (start >= rx_len) {
        rx_len = 0;
        return 0;
    }

    // Find terminating END
    size_t end = start;
    while (end < rx_len && rx_buf[end] != SLIP_END)
        end++;

    if (end >= rx_len)
        return 0; // incomplete frame

    if (slipDecode(rx_buf + start, end - start, frame_buf, frame_len) != 0) {
        // bad frame, discard
        memmove(rx_buf, rx_buf + end + 1, rx_len - end - 1);
        rx_len -= end + 1;
        return -1;
    }

    memmove(rx_buf, rx_buf + end + 1, rx_len - end - 1);
    rx_len -= end + 1;
    return 1;
}

static int times = 0;

// ---- Command handlers ----

static void handleCommand(uint8_t cmd, uint8_t seq,
                          const uint8_t *payload, uint16_t payload_len) {
    uint8_t resp_type = cmd | RESP_MASK;

    switch (cmd) {
        case CMD_PING:
            LOG_PRINTLN("CMD: PING");
            sendPacket(resp_type, seq, payload, payload_len);
            break;

        case CMD_INIT:
            LOG_PRINTLN("CMD: INIT");
            sendPacket(resp_type, seq, NULL, 0);
            break;

        case CMD_GET_CAMERA_INFO: {
            LOG_PRINTLN("CMD: GET_CAMERA_INFO");
            CameraInfo ci;
            ci.width = 1280;
            ci.height = 720;
            ci.format = 0;
            ci.connected = 1;
            sendPacket(resp_type, seq, (const uint8_t *)&ci, sizeof(ci));
            break;
        }

        case CMD_GET_CAMERA_FRAME: {
            LOG_PRINTLN("CMD: GET_CAMERA_FRAME");
            unsigned long t_start = micros();
            fb = esp_camera_fb_get();
            CTRL_SERIAL.flush();
            uint32_t frame_len = fb->len; // TODO: replace with actual captured frame size
            sendPacket(resp_type, seq, (const uint8_t *)&frame_len, sizeof(frame_len));

            uint32_t offset = 0;
            uint8_t chunk_seq = 0;
            while (offset < frame_len) {
                uint16_t chunk_size = FRAME_CHUNK_SIZE;
                if (offset + chunk_size > frame_len)
                    chunk_size = frame_len - offset;
                sendPacket(RESP_FRAME_CHUNK, chunk_seq++,
                           fb->buf + offset, chunk_size);
                offset += chunk_size;
            }
            CTRL_SERIAL.flush();
            esp_camera_fb_return(fb);
            unsigned long elapsed_us = micros() - t_start;
            times = elapsed_us;
            LOG_PRINTF("Frame sent: %u bytes in %lu us (%.1f KB/s)\n",
                       frame_len, elapsed_us,
                       frame_len * 1000.0f / elapsed_us);
            break;
        }

        default:
            LOG_PRINTF("Unknown command: 0x%02X\n", cmd);
            break;
    }
}

// ---- Public API ----

void initUart() {
    CTRL_SERIAL.begin(CTRL_SERIAL_BAUD);
    // CTRL_SERIAL.begin(CTRL_SERIAL_BAUD, SERIAL_8N1, CTRL_SERIAL_RX, CTRL_SERIAL_TX);
    rx_len = 0;
    LOG_PRINTLN("UART control channel initialized");
}

void handleUart() {
    while (CTRL_SERIAL.available() && rx_len < RX_BUF_SIZE)
        rx_buf[rx_len++] = CTRL_SERIAL.read();

    uint8_t frame_buf[RX_BUF_SIZE];
    size_t frame_len = 0;

    int result = extractFrame(frame_buf, &frame_len);
    if (result <= 0)
        return;

    uint8_t type, seq;
    const uint8_t *payload;
    uint16_t payload_len;

    if (parsePacket(frame_buf, frame_len, &type, &seq,
                    &payload, &payload_len) != 0) {
        return;
    }

    handleCommand(type, seq, payload, payload_len);
}
