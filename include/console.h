#pragma once
#include <Arduino.h>
// #if defined(ARDUINO_USB_CDC_ON_BOOT)
//     #define SERIAL_T USBCDC
// #else
//     #define SERIAL_T HardwareSerial
// #endif
#define SERIAL_T HardwareSerial

class Console
{
private:
    SERIAL_T serial;
public:
    Console();
    ~Console();

    String& readCommand();
};

