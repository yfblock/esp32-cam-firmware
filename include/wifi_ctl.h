#pragma once
#include <WiFi.h>

void connectToWiFi(const String& ssid, const String& pass);

class WiFiCtl
{
private:
    String ssid;
    String pass;
public:
    WiFiCtl(const String& ssid, const String& pass);
    ~WiFiCtl();
    void connect();
    void connect(const String& ssid, const String& pass) {
        setSSID(ssid);
        setPass(pass);
        connect();
    }
    void init();
    void setSSID(const String& ssid) { this->ssid = ssid; }
    void setPass(const String& pass) { this->pass = pass; }
    String getSSID() const { return this->ssid; }
    String getPass() const { return this->pass; }
    void getStatus();
};
