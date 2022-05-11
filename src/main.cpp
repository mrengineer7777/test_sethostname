#include <Arduino.h>
#include "WiFi.h"

void setup() {
    Serial.begin(115200);
    delay(2000);                      // Delay for debugging. Allows time for USB to connect to computer.

    int APcnt = WiFi.scanNetworks(false, true, false, 800, 0);;  //Blocks until scan completes
    //PrintAPs();
    //Scan complete, connect to WiFi

    //Hostname is for DHCP and possibly DNS resolution.
    //For Windows NETBIOS name lookups use MDNS
    //WiFi.mode(WIFI_MODE_NULL);                                                        //Bugfix: hostname won't be set if STA mode is already active, so shut down WiFi.
    WiFi.setHostname("BORG8472");                                                     //Hostname isn't set until WiFi.mode() is called, and only if STA mode wasn't active.
    WiFi.begin("SSID", "password");
}

void loop() {
  delay(1000);                        // Allow other tasks to run
}
