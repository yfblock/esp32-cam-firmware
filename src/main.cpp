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
#include "esp_camera.h"
#include "sensor.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_jpg_decode.h"

ESP32SPISlave slave;

static constexpr size_t BUFFER_SIZE = 8;
static constexpr size_t QUEUE_SIZE = 8;
uint8_t tx_buf[BUFFER_SIZE] {1, 2, 3, 4, 5, 6, 7, 8};
uint8_t rx_buf[BUFFER_SIZE] {0, 0, 0, 0, 0, 0, 0, 0};

// WiFi configuration (replace with your network)
const char* WIFI_SSID = "yfblock_for_guest";
const char* WIFI_PASSWORD = "secure1114";
// (SERVER_URL no longer used)

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    // .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    // .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.
    .frame_size = FRAMESIZE_XGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x", err);
        return err;
    }

    return ESP_OK;
}

// connect to WiFi and block until associated
static void connectWiFi()
{
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// web server instance
WebServer server(80);

// forward declaration of handlers
void handleCapture();
void handleStream();
void setup()
{
    Serial.begin(115200);
    slave.setDataMode(SPI_MODE3);   // default: SPI_MODE0
    // slave.setQueueSize(QUEUE_SIZE); // default: 1

    // bring up WiFi before camera (optional order)
    connectWiFi();

    if(ESP_OK != init_camera()) {
        return;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    Serial.printf("Camera sensor detected: %d\n", sensor->id);
    Serial.printf("Camera clk: %d\n", sensor->xclk_freq_hz);
    Serial.printf("Camera scale: %d\n", sensor->status.scale);
    camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
    Serial.printf("Camera sensor name: %s\n", info->name);
    Serial.printf("Camera support jpeg: %d\n", info->support_jpeg);

    // register HTTP endpoints
    server.on("/capture", HTTP_GET, handleCapture);
    server.on("/stream", HTTP_GET, handleStream);
    server.begin();
    Serial.println("HTTP server started");
}

void loop()
{
    // handle any incoming client requests
    server.handleClient();
}

// handler implementations
void handleCapture() {
    int start = millis();
    camera_fb_t * fb = esp_camera_fb_get();
    int end = millis();
    Serial.printf("Frame capture time: %d ms\n", end - start);
    if (!fb) {
        server.send(500, "text/plain", "Camera error");
        return;
    }
    Serial.printf("Captured image, size: %u bytes\n", fb->len);
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handleStream() {
    WiFiClient client = server.client();
    String header = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(header);

    while (client.connected()) {
        int start = millis();
        camera_fb_t * fb = esp_camera_fb_get();
        int end = millis();
        Serial.printf("Frame capture time: %d ms\n", end - start);
        if (!fb) break;
        String part = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
        server.sendContent(part);

        // guard against broken connection when writing image data
        size_t sent = client.write(fb->buf, fb->len);
        if (sent != fb->len) {
            Serial.printf("Stream write failed (%u/%u), closing\n", sent, fb->len);
            esp_camera_fb_return(fb);
            break;
        }

        server.sendContent("\r\n");
        client.flush();
        esp_camera_fb_return(fb);
        delay(100);

        // check again in case client dropped between frames
        if (!client.connected()) {
            Serial.println("client disconnected during stream");
            break;
        }
    }
}

