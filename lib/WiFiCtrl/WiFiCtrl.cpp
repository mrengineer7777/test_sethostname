#include "WiFiCtrl.h"

#include "mEprom.h"
#include "esp_ping.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "Esp.h"
#include "lwip/icmp.h"
#include "HardwareSerial.h"
#include "MacAddress.h"
#include "MyScan.h"

extern "C" {
  #include "esp32-hal.h"
  #include "myping.h"
}

//Must match arduino_event_id_t in WiFiGeneric.h
const char* ARDUINO_EVENT_NAMES[] = {
    "ARDUINO_EVENT_WIFI_READY",               /*!< ESP32 WiFi ready */
    "ARDUINO_EVENT_WIFI_SCAN_DONE",
    "ARDUINO_EVENT_WIFI_STA_START",
    "ARDUINO_EVENT_WIFI_STA_STOP",
    "ARDUINO_EVENT_WIFI_STA_CONNECTED",
    "ARDUINO_EVENT_WIFI_STA_DISCONNECTED",
    "ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE",
    "ARDUINO_EVENT_WIFI_STA_GOT_IP",
    "ARDUINO_EVENT_WIFI_STA_GOT_IP6",
    "ARDUINO_EVENT_WIFI_STA_LOST_IP",
    "ARDUINO_EVENT_WIFI_AP_START",
    "ARDUINO_EVENT_WIFI_AP_STOP",
    "ARDUINO_EVENT_WIFI_AP_STACONNECTED",
    "ARDUINO_EVENT_WIFI_AP_STADISCONNECTED",
    "ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED",
    "ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED",
    "ARDUINO_EVENT_WIFI_AP_GOT_IP6",
    "ARDUINO_EVENT_WIFI_FTM_REPORT",
    "ARDUINO_EVENT_ETH_START",
    "ARDUINO_EVENT_ETH_STOP",
    "ARDUINO_EVENT_ETH_CONNECTED",
    "ARDUINO_EVENT_ETH_DISCONNECTED",
    "ARDUINO_EVENT_ETH_GOT_IP",
    "ARDUINO_EVENT_ETH_GOT_IP6",
    "ARDUINO_EVENT_WPS_ER_SUCCESS",
    "ARDUINO_EVENT_WPS_ER_FAILED",
    "ARDUINO_EVENT_WPS_ER_TIMEOUT,"
    "ARDUINO_EVENT_WPS_ER_PIN",
    "ARDUINO_EVENT_WPS_ER_PBC_OVERLAP",
    "ARDUINO_EVENT_SC_SCAN_DONE",
    "ARDUINO_EVENT_SC_FOUND_CHANNEL",
    "ARDUINO_EVENT_SC_GOT_SSID_PSWD",
    "ARDUINO_EVENT_SC_SEND_ACK_DONE",
    "ARDUINO_EVENT_PROV_INIT",
    "ARDUINO_EVENT_PROV_DEINIT",
    "ARDUINO_EVENT_PROV_START",
    "ARDUINO_EVENT_PROV_END",
    "ARDUINO_EVENT_PROV_CRED_RECV",
    "ARDUINO_EVENT_PROV_CRED_FAIL",
    "ARDUINO_EVENT_PROV_CRED_SUCCESS"
};

//-------------------Local function definitions-------------------
uint8_t getSoftApChannel(void);
uint8_t FindBestSoftAPChannel(void);
void static pingResults(ping_target_id_t msgType, esp_ping_found * pf);
void static PingGateway(void);
// void static PingSoftAPClients(void);
void static PingSvc(void);
void PrintWiFiReasonCode(char *buf, wifi_err_reason_t rc);
void static ShutdownSoftAP(uint32_t disconnect_clients);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
//-------------------Local function definitions-------------------

uint8_t getSoftApChannel(void) {
  wifi_config_t conf_current;
  if(esp_wifi_get_config(WIFI_IF_AP, &conf_current) == ESP_OK) {
    return conf_current.ap.channel;
  }
  return 0;
}

uint8_t FindBestSoftAPChannel(void) {
  int32_t i;
  int16_t cnt = WiFi.scanComplete();
  wifi_ap_record_t* AP;
  CHANBUCKETS_T CB[11];                                                             //Count used channels
  uint8_t ch;
  int8_t RSSI;

  if(cnt<=0) return 1;

  for(i=0;i<11;i++){
    CB[i].APcnt = 0;
    CB[i].RSSImax = INT8_MIN;
  }

  for(i=0; i < cnt; i++) {
    AP = reinterpret_cast<wifi_ap_record_t*>(WiFi.getScanInfoByIndex(i));

    ch = AP->primary;
    RSSI = AP->rssi;

    //Serial.printf("#%i ch=%u RSSI=%i\r\n", i, ch, RSSI);

    if(RSSI < -60) continue;                                                        //Only consider strong APs

    if(ch < 1) continue;                                                            //Invalid, ignore
    ch--;                                                                           //Zero offset
    CB[ch].APcnt++;
    if(CB[ch].RSSImax<RSSI)
      CB[ch].RSSImax = RSSI;

    // if(AP->rssi >= -50){
    //   j = AP->primary;
    //   if(j>=1 && j<=3)
    //     chan[0]++;
    //   else if(j>=4 && j<=8)
    //     chan[1]++;
    //   else if(j>=9 && j<=11)
    //     chan[2]++;
    // }
  }

  // Serial.println("Chan\tcnt\tRSSImax");
  // for(i=0;i<11;i++){
  //   Serial.printf("%i\t%u\t%i\r\n",
  //     i+1,
  //     CB[i].APcnt,
  //     CB[i].RSSImax);
  // }

  //Find an empty primary channel
  if(CB[0].APcnt==0) return 1;
  if(CB[5].APcnt==0) return 6;
  if(CB[10].APcnt==0) return 11;

  //Find an empty center channel - will interfere with adjacent channels
  if(CB[2].APcnt==0) return 3;
  if(CB[7].APcnt==0) return 8;
  if(CB[3].APcnt==0) return 4;
  if(CB[8].APcnt==0) return 9;

  //Find edge channel - will interfere strongly with adjacent channels
  if(CB[1].APcnt==0) return 2;
  if(CB[4].APcnt==0) return 5;
  if(CB[6].APcnt==0) return 7;
  if(CB[9].APcnt==0) return 10;

  return 3;                                                                         //Last resort
}

void static pingResults(ping_target_id_t msgType, esp_ping_found * pf){
  if(pf->ping_err != PING_RES_FINISH) return;

 	// printf("AvgTime:%.1fmS Sent:%d Rec:%d Err:%d min(mS):%d max(mS):%d ", (float)pf->total_time/pf->recv_count, pf->send_count, pf->recv_count, pf->err_count, pf->min_time, pf->max_time );
	// printf("Resp(mS):%d Timeouts:%d Total Time:%d\n",pf->resp_time, pf->timeout_count, pf->total_time);

  if(pf->err_count==0 && pf->timeout_count==0) {
    WiFiCtrl.PingFailures = 0;                                                      //Reset failure count
    //Serial.println("Ping success");
    return;
  }
  WiFiCtrl.PingFailures++;

  printf("PingFailures==%u\r\n", WiFiCtrl.PingFailures);

  if(WiFiCtrl.PingFailures >= 9)                                                    //After 90 seconds of failed pings reboot us
    ESP.restart();

  if(WiFiCtrl.PingFailures % 3 == 0)                                                //We've had 3,6,9,etc failures.  Since we ping every 10 seconds, this means we've been disconnected for 30s.
  {
    WiFi.reconnect();                                                               //If network cable isn't connected then we reassociate to WiFi but don't get an IP, so it looks like we aren't connected.
  }
}

//Ping gateway every 10 seconds
void static PingGateway(void) {
  IPAddress gwip;

  gwip = WiFi.gatewayIP();                                                          //This should work for static or DHCP configured address when connected to network.
  //Serial.printf("Pinging gateway %s\r\n", gwip.toString().c_str());

  //Serial.println("PingGateway() started ping");
  if(PingIP_Start(gwip, 1, 450, 500, &pingResults)) {
    WiFiCtrl.WiFiDisconnectCnt=0;                                                   //Reset count
    return;
  }
  else{
    ;//Serial.println("PingGateway() failed to start ping");                        //Error not handled. We don't expect this to happen.
  }
}

// void static PingSoftAPClients(void) {
//     See esp_err_t esp_wifi_deauth_sta(uint16_t aid) and esp_wifi_set_inactive_time() in esp_wifi.h
//     Default inactive time is 300s, meaning if STA connected to SoftAP doesn't send data for 300s it will be disconnected.
//     Code below doesn't work because there is no IP address in esp_wifi_ap_get_sta_list()
//     wifi_sta_list_t clients;
//     int32_t i;
//     if(esp_wifi_ap_get_sta_list(&clients) != ESP_OK) {
//         return;
//     }
//     for(i=0;i<clients.num;i++){
//       if(clients.sta[i].??)
//       {
//       }
//     }
// }

void static PingSvc(void) {
  //wifi_mode_t wmode = WiFi.getMode();
  uint8_t clientcnt = WiFi.softAPgetStationNum();

  if(WiFiCtrl.WiFiTargetState == WIFI_MODE_AP) {                                    //SoftAP
    //if(wmode && WIFI_MODE_AP) {                                                     //SoftAP active
    //  PingSoftAPClients();
    //}
    return; 
  }

  //WiFiTargetState == WIFI_MODE_STA
  switch(WiFiCtrl.WiFiTargetSubState) {
    case WIFI_MODE_STA:
      if(WiFi.isConnected()) WiFiCtrl.WiFiDisconnectCnt = 0;
      else WiFiCtrl.WiFiDisconnectCnt++;
      break;
    case WIFI_MODE_AP:
      WiFiCtrl.WiFiDisconnectCnt++;
      WiFiCtrl.SoftAPUpCnt++;
      if(clientcnt) WiFiCtrl.SoftAPDisconnectCnt = 0;
      else WiFiCtrl.SoftAPDisconnectCnt++;
      break;
    default:
      WiFiCtrl.WiFiDisconnectCnt++;
      break;
  }

  //DEBUG ONLY
  // Serial.printf("Target=%u, current=%u, clients=%u, DisCnt=%u, APupCnt=%u, APdisCnt=%u", 
  //   WiFiCtrl.WiFiTargetState, wmode, clientcnt,
  //   WiFiCtrl.WiFiDisconnectCnt, WiFiCtrl.SoftAPUpCnt, WiFiCtrl.SoftAPDisconnectCnt);
  // Serial.println();
  //DEBUG ONLY

  //Switch modes?
  //If we can't connect to WiFi, switch to SoftAP after 30s.  After 1 minute of AP, switch back to WiFi.
  switch(WiFiCtrl.WiFiTargetSubState) {
    case WIFI_MODE_AP:
      //Switch to WiFi
      if(WiFiCtrl.SoftAPUpCnt >= 6 && WiFiCtrl.SoftAPDisconnectCnt >= 3) {          //SoftAP has been up for at least 60s and clients have been disconnected for at least 30s.
        WiFiCtrl.WiFiTargetSubState = WIFI_MODE_STA;
        log_d("Timeout. Switching SoftAP->WiFi");
        WiFi.enableAP(false);
        delay(100);
        WiFiCtrl.ConfigWiFi();                                                      //Use this instead of WiFi.mode(WIFI_MODE_STA) because that doesn't re-scan for missing APs
        return;
      }
      break;
    case WIFI_MODE_STA:
    default:
      //Switch to SoftAP
      if ( (WiFiCtrl.WiFiDisconnectCnt > 0) &&
           (WiFiCtrl.WiFiDisconnectCnt % 3 == 0) ) {                                //WiFi has been disconnected for 30s. Try SoftAP.
        WiFiCtrl.WiFiTargetSubState = WIFI_MODE_AP;
        WiFiCtrl.SoftAPUpCnt = 0;
        WiFiCtrl.SoftAPDisconnectCnt = 0;
        log_d("Timeout. Switching WiFi->SoftAP");
        WiFi.disconnect(true, false);
        delay(100);
        WiFiCtrl.ConfigSoftAP(WiFiCtrl.SoftAP_Chan, 0);                             //Use this so we init softAP instead of //WiFi.mode(WIFI_MODE_AP);
        return;
      }
      break;
  }

  //If WiFi hasn't connected for 30 minutes, restart us
  if(WiFiCtrl.WiFiDisconnectCnt >= 180) {
    log_d("WiFi has been down for 30 minutes. Restarting!");
    ESP.restart();
    return;
  }

  //WiFi enabled and working, ping gateway
  if(WiFiCtrl.WiFiDisconnectCnt==0) PingGateway();
}

void PrintWiFiReasonCode(char *buf, wifi_err_reason_t rc) {
  switch(rc)                                                                        //See wifi_err_reason_t in esp_wifi_types.h
  {
    case WIFI_REASON_UNSPECIFIED:
      strcpy(buf, "WIFI_REASON_UNSPECIFIED");
      break;
    case WIFI_REASON_AUTH_EXPIRE:
      strcpy(buf, "WIFI_REASON_AUTH_EXPIRE");
      break;
    case WIFI_REASON_AUTH_LEAVE:
      strcpy(buf, "WIFI_REASON_AUTH_LEAVE");
      break;
    case WIFI_REASON_ASSOC_EXPIRE:
      strcpy(buf, "WIFI_REASON_ASSOC_EXPIRE");
      break;
    case WIFI_REASON_ASSOC_TOOMANY:
      strcpy(buf, "WIFI_REASON_ASSOC_TOOMANY");
      break;
    case WIFI_REASON_NOT_AUTHED:
      strcpy(buf, "WIFI_REASON_NOT_AUTHED");
      break;
    case WIFI_REASON_NOT_ASSOCED:
      strcpy(buf, "WIFI_REASON_NOT_ASSOCED");
      break;
    case WIFI_REASON_ASSOC_LEAVE:
      strcpy(buf, "WIFI_REASON_ASSOC_LEAVE");
      break;
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      strcpy(buf, "WIFI_REASON_ASSOC_NOT_AUTHED");
      break;
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
      strcpy(buf, "WIFI_REASON_DISASSOC_PWRCAP_BAD");
      break;
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
      strcpy(buf, "WIFI_REASON_DISASSOC_SUPCHAN_BAD");
      break;
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
      strcpy(buf, "WIFI_REASON_DISASSOC_SUPCHAN_BAD");
      break;
    case WIFI_REASON_IE_INVALID:
      strcpy(buf, "WIFI_REASON_IE_INVALID");
      break;
    case WIFI_REASON_MIC_FAILURE:
      strcpy(buf, "WIFI_REASON_MIC_FAILURE");
      break;
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      strcpy(buf, "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT");
      break;
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      strcpy(buf, "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT");
      break;
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      strcpy(buf, "WIFI_REASON_IE_IN_4WAY_DIFFERS");
      break;
    case WIFI_REASON_GROUP_CIPHER_INVALID:
      strcpy(buf, "WIFI_REASON_GROUP_CIPHER_INVALID");
      break;
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
      strcpy(buf, "WIFI_REASON_PAIRWISE_CIPHER_INVALID");
      break;
    case WIFI_REASON_AKMP_INVALID:
      strcpy(buf, "WIFI_REASON_AKMP_INVALID");
      break;
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
      strcpy(buf, "WIFI_REASON_UNSUPP_RSN_IE_VERSION");
      break;
    case WIFI_REASON_INVALID_RSN_IE_CAP:
      strcpy(buf, "WIFI_REASON_INVALID_RSN_IE_CAP");
      break;
    case WIFI_REASON_802_1X_AUTH_FAILED:
      strcpy(buf, "WIFI_REASON_802_1X_AUTH_FAILED");
      break;
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
      strcpy(buf, "WIFI_REASON_CIPHER_SUITE_REJECTED");
      break;
    case WIFI_REASON_INVALID_PMKID:
      strcpy(buf, "WIFI_REASON_INVALID_PMKID");
      break;
    case WIFI_REASON_BEACON_TIMEOUT:
      strcpy(buf, "WIFI_REASON_BEACON_TIMEOUT");
      break;
    case WIFI_REASON_NO_AP_FOUND:
      strcpy(buf, "WIFI_REASON_NO_AP_FOUND");
      break;
    case WIFI_REASON_AUTH_FAIL:
      strcpy(buf, "WIFI_REASON_AUTH_FAIL");
      break;
    case WIFI_REASON_ASSOC_FAIL:
      strcpy(buf, "WIFI_REASON_ASSOC_FAIL");
      break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      strcpy(buf, "WIFI_REASON_HANDSHAKE_TIMEOUT");
      break;
    case WIFI_REASON_CONNECTION_FAIL:
      strcpy(buf, "WIFI_REASON_CONNECTION_FAIL");
      break;
    case WIFI_REASON_AP_TSF_RESET:
      strcpy(buf, "WIFI_REASON_AP_TSF_RESET");
      break;
    case WIFI_REASON_ROAMING:
      strcpy(buf, "WIFI_REASON_ROAMING");
      break;
    default:
      sprintf(buf, "Reason %d", rc);
      break;
  }
}

void static ShutdownSoftAP(uint32_t disconnect_clients) {
  if(disconnect_clients==0 && WiFi.softAPgetStationNum()>0) {                       //Client is still connected. Try again in 1 minute.
      WiFiCtrl.ScheduleSoftAPShutdownTimeSecs(60, false);
      return;
    }
    WiFi.enableAP(false);
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  char cs[64];
  IPAddress ip;
  String ssid;

  //SoftAP
  if(event == ARDUINO_EVENT_WIFI_AP_START) {
    if(WiFiCtrl.SoftAPState==1)                                                     //Ignore duplicate start message
      return;
    WiFiCtrl.SoftAPState = 1;
    Serial.printf("'%s' chan=%u AP=%s\r\n",
      netconf.softap.ssid,
      getSoftApChannel(),
      WiFi.softAPIP().toString().c_str());
  }
  else if(event==ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    MacAddress(info.wifi_ap_staconnected.mac).toCStr(cs);
    Serial.printf("Client %u [%s] connected\r\n", info.wifi_ap_staconnected.aid, cs);
  }
  else if(event==ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    MacAddress(info.wifi_ap_stadisconnected.mac).toCStr(cs);
    Serial.printf("Client %u [%s] disconnected\r\n", info.wifi_ap_stadisconnected.aid, cs);
  }
  else if(event==ARDUINO_EVENT_WIFI_AP_STOP) {
    if(WiFiCtrl.SoftAPState==0)
      return;
    WiFiCtrl.SoftAPState = 0;
    Serial.printf("'%s' Stopped\r\n", netconf.softap.ssid);
    WiFiCtrl.SoftAPUpCnt = 0;
    WiFiCtrl.SoftAPDisconnectCnt = 0;
  }
  else if(event==ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
    ip = info.wifi_ap_staipassigned.ip.addr;
    Serial.printf("Client IP=%s\r\n", ip.toString().c_str());
  }

  //WiFi
  else if(event==ARDUINO_EVENT_WIFI_SCAN_DONE) {
    Serial.printf("%d APs found\r\n", info.wifi_scan_done.number);
    PrintAPs();
    //Serial.println("--Start WiFi--");                                             //Start WiFi after scan completes
    WiFiCtrl.ConnectAsync();
  }
  else if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    WiFiCtrl.WiFiDisconnectedReason = 0;
    ssid = WiFi.SSID();
    Serial.printf("Connected to '%s'\r\n", ssid.c_str());
  }
  else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    ssid = WiFi.SSID();
    ip = info.got_ip.ip_info.ip.addr;                                               //Or we can retrieve IP with WiFi.localIP();
    Serial.printf("'%s' IP=%s\r\n", 
      ssid.c_str(),
      ip.toString().c_str());
    WiFiCtrl.ScheduleSoftAPShutdownTimeSecs(10, true);                              //WLAN is up. Shutdown SoftAP and disconnect clients.
  }
  else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    if(WiFiCtrl.WiFiDisconnectedReason==info.wifi_sta_disconnected.reason)          //Ignore duplicate messages (wifi retries)
      return;
    WiFiCtrl.WiFiDisconnectedReason = info.wifi_sta_disconnected.reason;
    PrintWiFiReasonCode(cs, (wifi_err_reason_t)info.wifi_sta_disconnected.reason);
    Serial.printf("WiFi disconnected '%s'\r\n", cs);

    // if(WiFi.getMode() != WIFI_MODE_APSTA)                                        //If WiFi is down for any reason then bring up SoftAP
    //   WiFi.mode(WIFI_MODE_APSTA);

    // if(info.disconnected.reason==WIFI_REASON_ASSOC_EXPIRE)
    // {//ESP won't retry on this error, so force a reconnect.  This can happen when router reboots.
    //   WiFi.reconnect(); 
    // }
  }
  else {
    //Serial.print("[WiFi-event] ");
    if((int)event<ARDUINO_EVENT_MAX)
      Serial.println(ARDUINO_EVENT_NAMES[(int)event]);
    else
      Serial.println("Unknown WiFi event");
  }
}

WiFiCtrlClass::WiFiCtrlClass() {
  //Can't call StartEventHandler() here as event loop hasn't been started yet
}

WiFiCtrlClass::~WiFiCtrlClass() {
  StopEventHandler();
  StopPingSvc();
}

void WiFiCtrlClass::ConnectAsync(void) {
  //Not using AP+STA mode because SoftAP doesn't work properly while STA is trying to connect.
  //WiFi.mode(WIFI_MODE_APSTA);    //Note: when in SoftAP + WiFi, the radio can likely only be on one channel, and will take the channel of the SSID it is connecting to.
  if(netconf.wlan.ssid[0] != 0x00) {
    WiFiTargetState = WIFI_MODE_STA;
    WiFiTargetSubState = WIFI_MODE_STA;
    ConfigWiFi();
  }
  else {                                                                            //Setup mode
    WiFiCtrl.SoftAP_Chan = netconf.softap.chan;
    if(WiFiCtrl.SoftAP_Chan==0){
      WiFiCtrl.SoftAP_Chan = FindBestSoftAPChannel();                               //Don't modify netconf.softap.chan or we won't automatically find a channel on next boot.
      log_v("Selected SoftAP channel %u\r\n", WiFiCtrl.SoftAP_Chan);
    }
    WiFiTargetState = WIFI_MODE_AP;
    ConfigSoftAP(SoftAP_Chan, netconf.softap.setup_timeout_mins);
  }
}

//Get DHCP status
//Return values:
//-1=not STA or err, 0=off, 1=pending, 2=success
int WiFiCtrlClass::GetDHCPStatus(void) {
  esp_err_t err = ESP_OK;
 	esp_netif_t *esp_netif = get_esp_interface_netif(ESP_IF_WIFI_STA);
	esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;

  if(WiFi.getMode() != WIFI_MODE_STA) return -1;

  err = esp_netif_dhcpc_get_status(esp_netif, &status);
  if(err){
    log_e("DHCPC Get Status Failed! 0x%04x", err);
    return -1;
  }

  if(!netconf.wlan.flags.dhcp) return 0;                                            //If static IP specified then DHCP should be off
  if(status == ESP_NETIF_DHCP_STARTED) return 2;
  //Not sure what state DHCP returns while pending
  // if(status == ESP_NETIF_DHCP_INIT) return 1;
  // if(status == ESP_NETIF_DHCP_STOPPED) return 1;
  return 1;
}

void WiFiCtrlClass::ScheduleSoftAPShutdownTimeSecs(uint32_t timeout_secs, bool DisconnectClients) {
  //log_d("Scheduling SoftAP shutdown for %u secs\r\n", timeout_secs);
  uint32_t u;
  DisconnectClients == true ? u=1 : u=0;

  if(timeout_secs == 0)
    WiFiCtrl.SoftAP_Shutdown_Timer.detach();
  else
    WiFiCtrl.SoftAP_Shutdown_Timer.once_ms(1000*timeout_secs, ShutdownSoftAP, u);
}

void WiFiCtrlClass::StartEventHandler(void) {
  if(eventhandler_id) return;
  eventhandler_id = WiFi.onEvent(WiFiEvent);
}

void WiFiCtrlClass::StartPingSvc(void) {
  Ping_Timer.attach_ms(10000, PingSvc);                                             //Every 10 seconds ping gateway or softap clients
}

void WiFiCtrlClass::StopEventHandler(void) {
  if(eventhandler_id)
    WiFi.removeEvent(eventhandler_id);
  eventhandler_id = 0;
}

void WiFiCtrlClass::StopPingSvc(void) {
  Ping_Timer.detach();
}

void WiFiCtrlClass::ConfigSoftAP(uint8_t chan, uint8_t ShutdownTimeMins)
{
  WiFiCtrl.SoftAPState = 0;
  if(!netconf.softap.flags.dhcp) {
    WiFi.softAPConfig(
      IPAddress(netconf.softap.sttc.ip),
      IPAddress(netconf.softap.sttc.gateway),
      IPAddress(netconf.softap.sttc.netmask));
  }
  
  WiFi.softAP(netconf.softap.ssid, netconf.softap.password, chan, 0, 4);
  ScheduleSoftAPShutdownTimeSecs(ShutdownTimeMins*60, false);                       //Schedule shutdown time if set
}

void WiFiCtrlClass::ConfigWiFi(void)
{
  WiFi.persistent(false);                                                           //Base library should NOT save SSID/pwd to NVS. This also prevents wifi autostart.
  //https://stackoverflow.com/questions/65873683/how-do-you-erase-esp32-wifi-smartconfig-credentials-from-flash
  //Namespace "nvs.net80211": "sta.authmode", "sta.ssid", "sta.pswd".
  WiFi.setAutoReconnect(true);
  PingFailures = 0;                                                                 //Reset ping failures before connecting to WiFi
  WiFiDisconnectCnt = 0;                                                            //Reset count since we are likely connecting to a new network

  //Hostname is for DHCP and possibly DNS resolution.
  //For Windows NETBIOS name lookups use MDNS. Windows used to limit Netbios max length to 15 chars.  Is this still true?
  //WiFi.mode(WIFI_MODE_NULL);                                                        //Bugfix: hostname won't be set if STA mode is already active, so shut down WiFi.
  WiFi.setHostname("BORG8472");                                                     //Hostname isn't set until WiFi.mode() is called, and only if STA mode wasn't active.

  if(!netconf.wlan.flags.dhcp) {
    log_v("Config WiFi StaticIP");//TODO test this
    WiFi.config(
      netconf.wlan.sttc.ip,
      netconf.wlan.sttc.gateway,
      netconf.wlan.sttc.netmask,
      netconf.wlan.sttc.dns1);
  }

  MacAddress oBssid = MacAddress(netconf.wlan.bssidval);

  log_v("Start STA BSSID='%s' SSID='%s', PASS='%s'",
     oBssid.Value() ? oBssid.toString().c_str() : "", 
     netconf.wlan.ssid,
     netconf.wlan.password);//TODO test this

  if(!netconf.wlan.bssidval) {
    WiFi.begin(netconf.wlan.ssid, netconf.wlan.password);
  }
  else {
    uint8_t bssid[6];
    oBssid.toBytes(bssid);
    WiFi.begin(netconf.wlan.ssid, netconf.wlan.password, 0, bssid, true);//TODO test this
  }
}

WiFiCtrlClass WiFiCtrl;