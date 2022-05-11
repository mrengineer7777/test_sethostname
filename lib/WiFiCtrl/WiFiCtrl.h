#pragma once

#include "WiFi.h"
#include "Ticker.h"

typedef struct {
	uint8_t APcnt;
	int8_t RSSImax;
} CHANBUCKETS_T;

extern esp_netif_t* get_esp_interface_netif(esp_interface_t interface);             //Import from WiFiGeneric.cpp

class WiFiCtrlClass
{
public:
    WiFiCtrlClass();
    ~WiFiCtrlClass();
    void ConfigSoftAP(uint8_t chan, uint8_t ShutdownTimeMins);
    void ConfigWiFi(void);
    void ConnectAsync(void);
    int  GetDHCPStatus(void); //
    void ScheduleSoftAPShutdownTimeSecs(uint32_t, bool);
    void StartEventHandler(void);
    void StartPingSvc(void);
    void StopEventHandler(void);
    void StopPingSvc(void);
    uint8_t SoftAP_Chan;
    wifi_mode_t WiFiTargetState = WIFI_MODE_NULL;
    wifi_mode_t WiFiTargetSubState = WIFI_MODE_NULL;
    uint32_t PingFailures = 0;
    uint16_t WiFiDisconnectCnt = 0;
    uint8_t SoftAPUpCnt = 0;                                                        //How long it's been since SoftAP started
    uint8_t SoftAPDisconnectCnt = 0;                                                //How long it's been without clients connected
    uint8_t SoftAPState = 0;
    uint8_t WiFiDisconnectedReason = 0;
protected:
    Ticker SoftAP_Shutdown_Timer;
    Ticker Ping_Timer;
private:
    wifi_event_id_t eventhandler_id = 0;
};

extern WiFiCtrlClass WiFiCtrl;

//WPA2 Enterprise
//  https://gist.github.com/Matheus-Garbelini/332dc35dbdf640d4e5f2672636e19bde
//  https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEnterprise/WiFiClientEnterprise.ino

//How to set SSID from webpage: https://www.teachmemicro.com/esp32-wifi-manager-dynamic-ssid-password/

//ESP32 WiFi guide (low level code)
//https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html

//WiFi manager library source. Lots of good ideas.
//https://github.com/khoih-prog/ESPAsync_WiFiManager/blob/master/src_cpp/ESPAsync_WiFiManager.cpp