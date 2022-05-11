#include <Arduino.h>
#include "mEprom.h"
#include "MyScan.h"
#include "WiFiCtrl.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Booting...");     // USB won't be connected yet, so we will only see this on a reboot
    delay(2000);                      // Delay for debugging. Allows time for USB to connect to computer.

    EP.Start();                       // Load our settings
    WiFiCtrl.StartEventHandler();
    WiFiCtrl.StartPingSvc();          // Make sure SoftAP or WiFi are up
    StartScanAsync();                 // Start scan before starting WiFi as scan will fail if WiFi isn't connected. Completion of WiFi scan starts WiFi.
}

void loop() {
  delay(1000);                        // Allow other tasks to run
}