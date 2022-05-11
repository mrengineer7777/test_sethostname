#include "MyScan.h"
#include <HardwareSerial.h>
#include <WiFiScan.h>
#include <WiFi.h>

//-------------------Local function definitions-------------------
void PrintAP(int32_t i, const wifi_ap_record_t *AP);
void PrintAPs(void);
void PrintAuthMode(wifi_auth_mode_t mode, char *buf);
//void PrintCypher(wifi_cyper_type_t cyp, char *buf);
void StartScanAsync(void);
int  StartScanSync(void);
//-------------------Local function definitions-------------------

void PrintAP(int32_t i, const wifi_ap_record_t *AP) {
  char buf[192];
  char buf2[32];
  char *ptr = buf;

  if(!AP) return;

  buf[0] = 0;
  ptr += sprintf(ptr, "%2i ", i);                                                   //#
  ptr += sprintf(ptr, "%32s  ", AP->ssid[0] != 0 ? (const char *)AP->ssid : "<hidden>");          //SSID
  ptr += sprintf(ptr, "%02X:%02X:%02X:%02X:%02X:%02X  ", 
      AP->bssid[0], AP->bssid[1], AP->bssid[2],
      AP->bssid[3], AP->bssid[4], AP->bssid[5]);                                    //BSSID
  ptr += sprintf(ptr, "%4i ", AP->rssi);                                            //RSSI
  ptr += sprintf(ptr, "%3u  ", AP->primary);                                        //Channel
  PrintAuthMode(AP->authmode, buf2);
  ptr += sprintf(ptr, "%-8s  ", buf2);                                              //Authmode
  // PrintCypher(AP->pairwise_cipher, buf2);
  // ptr += sprintf(ptr, "%10s", buf2);                                             //pwciper
  // PrintCypher(AP->group_cipher, buf2);
  // ptr += sprintf(ptr, "%10s", buf2);                                             //gpciper
  if(AP->phy_11b) ptr += sprintf(ptr, "B");                                         //Options
  if(AP->phy_11g) ptr += sprintf(ptr, "G");
  if(AP->phy_11n) ptr += sprintf(ptr, "N");
  if(AP->phy_lr) ptr += sprintf(ptr, "L");

  //*Unused*
  //AP->ant             //This is always 0
  //AP->wps             //Not used
  //wifi_country_t      //Not used
  //Secondary channel.  This always returns 'NONE'. Doesn't appear to be implemented.
  // switch(AP->second) {
  //   case WIFI_SECOND_CHAN_NONE:
  //     Serial.print("NONE\t");
  //     break;
  //   case WIFI_SECOND_CHAN_ABOVE:
  //     Serial.print("ABOVE\t");
  //     break;
  //   case WIFI_SECOND_CHAN_BELOW:
  //     Serial.print("BELOW\t");
  //     break;
  // }
  Serial.println(buf);
}

void PrintAPs(void) {
  int32_t i;
  int16_t cnt = WiFi.scanComplete();
  wifi_ap_record_t* AP;

  Serial.println(" #                             SSID  BSSID              RSSI  Ch  Authmode  Rate" );
  Serial.println("-----------------------------------------------------------------------------------" );

  if(cnt < 0) {
    if(cnt == WIFI_SCAN_FAILED)
        Serial.println("<Scan error>");
    if(cnt == WIFI_SCAN_RUNNING)
        Serial.println("<Scan in progress>");
    return;
  };

  for(i=0; i < cnt; i++) {
    AP = reinterpret_cast<wifi_ap_record_t*>(WiFi.getScanInfoByIndex(i));
    PrintAP(i, AP);
  }
}

void PrintAuthMode(wifi_auth_mode_t mode, char *buf) {
  switch(mode) {
    case WIFI_AUTH_OPEN:
      sprintf(buf, "OPEN");
      break;
    case WIFI_AUTH_WEP:
      sprintf(buf, "WEP");
      break;
    case WIFI_AUTH_WPA_PSK:
      sprintf(buf, "WPA");
      break;
    case WIFI_AUTH_WPA2_PSK:
      sprintf(buf, "WPA2");
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      sprintf(buf, "MIXED");
      break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      sprintf(buf, "ENTERPRI");
      break;
    default:
      sprintf(buf, "UNKNOWN");
      break;
  }
}

// void PrintCypher(wifi_cyper_type_t cyp, char *buf) {
// switch (cyp)
//   {
//     case WIFI_CIPHER_TYPE_NONE:
//       Serial.print("NONE     ");
//       break;
//     case WIFI_CIPHER_TYPE_WEP40:
//       Serial.print("WEP40    ");
//       break;
//     case WIFI_CIPHER_TYPE_WEP104:
//       Serial.print("WEP104   ");
//       break;
//     case WIFI_CIPHER_TYPE_TKIP:
//       Serial.print("TKIP     ");
//       break;
//     case WIFI_CIPHER_TYPE_CCMP:
//       Serial.print("CCMP     ");
//       break;
//     case WIFI_CIPHER_TYPE_TKIP_CCMP:
//       Serial.print("TKIP-CCMP");
//       break;
//     case WIFI_CIPHER_TYPE_AES_CMAC128:
//       Serial.print("AES-CM128");
//       break;
//     case WIFI_CIPHER_TYPE_SMS4:
//       Serial.print("SMS4     ");
//       break;
//     case WIFI_CIPHER_TYPE_GCMP:
//       Serial.print("GCMP     ");
//       break;
//     case WIFI_CIPHER_TYPE_GCMP256:
//       Serial.print("GCMP-256 ");
//       break;
//     case WIFI_CIPHER_TYPE_AES_GMAC128:
//       Serial.print("AES-GM128");
//       break;
//     case WIFI_CIPHER_TYPE_AES_GMAC256:
//       Serial.print("AES-GM256");
//       break;
//     case WIFI_CIPHER_TYPE_UNKNOWN:
//       Serial.print("UNKNOWN  ");
//       break;
//   }
// }

//Scan takes 5s
//This should be run before connecting to WiFi:
//1) WiFi needs a scan before it can connect to network.
//2) Scans conflict with WiFi connecting stage.
//It is ok to scan after WiFi is connected. Scanning temporarily disrupts connection to AP.
void StartScanAsync(void) {
  //Must show hidden networks or WiFi connect won't connect to hidden network.
  WiFi.scanNetworks(true, true, false, 600, 0);
}

//Scan takes about 6 seconds
//Call this for user console commands
//This function blocks until results are ready
int StartScanSync(void) {
  return WiFi.scanNetworks(false, true, false, 800, 0);  //Could save # of scan results here, or we can call WiFi.scanComplete();
}