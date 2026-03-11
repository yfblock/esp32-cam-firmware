#include <Arduino.h>
#include "wifi_ctl.h"
#include "esp_wifi.h"

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

WiFiCtl::WiFiCtl(const String& ssid, const String& pass) : ssid(ssid), pass(pass)
{
}

WiFiCtl::~WiFiCtl()
{
}

void WiFiCtl::init()
{
    WiFi.mode(WIFI_STA);
}

void WiFiCtl::connect()
{
    connectToWiFi(this->ssid, pass);
    // 1. 初始化 NVS (WiFi 堆栈需要)
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);

    // // 2. 初始化 WiFi 堆栈
    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    // // 3. 必须设置为 STA 或 AP 模式才能发送
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_start());

    // 4. 固定物理信道（例如 Channel 6）
    // 发送原始包必须在确定的频率上
    // esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    // esp_wifi_80211_tx(WIFI_IF_STA, (const uint8_t*)"Hello", 5, true); // send test packet

    esp_wifi_set_promiscuous(true);
    // esp_wifi_set_promiscuous_rx_cb()
}

void WiFiCtl::getStatus()
{
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
}

// 回调函数定义
void your_callback_function(void* buf, wifi_promiscuous_pkt_type_t type) {
    // 1. 将 void 指针转换为混杂模式包结构体
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    
    // 2. 获取包的元数据
    int8_t rssi = pkt->rx_ctrl.rssi;    // 信号强度
    uint16_t len = pkt->rx_ctrl.sig_len; // 包长度
    
    // 3. 获取原始 802.11 字节数据
    // payload 包含了 MAC Header 和 Data
    uint8_t *payload = pkt->payload;

    // 4. 打印基本信息
    Serial.printf("[%ld] Type: %d | RSSI: %d | Len: %d | MAC: ", millis(), type, rssi, len);

    // 5. 打印报文的前 6 个字节 (通常是 Frame Control 和 部分地址)
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X ", payload[i]);
    }
    Serial.println("...");
}