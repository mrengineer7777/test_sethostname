#include "mEprom.h"
#include "HardwareSerial.h"
#include "IPAddress.h"
#include "esp32-hal-log.h"
#include "esp32-hal.h"


//-------------------Variables-------------------
Preferences DeviceSettings = {};
mEprom EP = {};                                                                     //Global reference to class
network_config_t netconf = {};                                                      //WiFi/Ethernet settings
//-------------------Variables-------------------

mEprom::mEprom() {
}

mEprom::~mEprom() {
    Stop();
}

void static EP_Save(void) {
    size_t bytes_saved = DeviceSettings.putBytes("netconf", &netconf, sizeof(netconf));

    if(bytes_saved>0)
        log_w("Settings Saved");
    else
        log_e("Failed to save settings");
}


void mEprom::Start(void) {
    size_t len = 0;

    DeviceSettings.begin("TEST",false);

    len = 0;
    if(DeviceSettings.isKey("netconf"))
    {
        len = DeviceSettings.getBytes("netconf", &netconf, sizeof(netconf));
    }
    if(len>0 && netconf.magic_number == NET_SETTINGS_MAGIC_NUMBER) {
        log_v("Loaded netconf Setting");
    }
    else
    {
        SetNetworkDefaults();
        Schedule_Save_Event(15000);													//Save variables.  This should be long enough that WiFi/SoftAP has time to come up before the save.
    }
}

void mEprom::Stop(void) {
    DeviceSettings.end();
}

void mEprom::SetNetworkDefaults(void) {
    uint8_t mac[6];

    log_i("Setting network defaults");
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    netconf.magic_number = NET_SETTINGS_MAGIC_NUMBER;

    netconf.softap.chan = 0;                    //Auto channel
    netconf.softap.flags.dhcp = 1;              //Default DHCP on
    netconf.softap.password[0] = 0;             //Blank password
    netconf.softap.setup_timeout_mins = 0;
    sprintf(netconf.softap.ssid, "SoftAP%02X%02X%02X", mac[3], mac[4], mac[5]);
    netconf.softap.sttc.dns1 = 0;               //Unused
    netconf.softap.sttc.gateway = 0;
    netconf.softap.sttc.ip = 0;
    netconf.softap.sttc.netmask = 0;

    netconf.wlan.bssidval = 0;
    netconf.wlan.flags.dhcp = 1;                //Default DHCP on
    netconf.wlan.password[0] = 0;               //Blank password
    netconf.wlan.ssid[0] = 0;                   //Blank ssid
    netconf.wlan.sttc.dns1 = 0;
    netconf.wlan.sttc.gateway = 0;
    netconf.wlan.sttc.ip = 0;
    netconf.wlan.sttc.netmask = 0;
}

void mEprom::Schedule_Save_Event(uint32_t ms) {
    EP_Save_Timer.once_ms(ms, EP_Save);
}