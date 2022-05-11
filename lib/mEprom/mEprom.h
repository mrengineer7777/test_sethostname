#pragma once

#include "Preferences.h"
#include "Ticker.h"

//--------------------Constants--------------------
#define NET_SETTINGS_MAGIC_NUMBER 0x4UL

//--------------------Structures--------------------
typedef struct {
  uint32_t magic_number;
  struct __attribute__((__packed__)) {
    char ssid[32];
    char password[64];
	struct {
		uint32_t ip;
		uint32_t gateway;
		uint32_t netmask;
		uint32_t dns1;																//Unused
	} sttc;
    struct {
		uint8_t dhcp:1;	//1=DHCP on
		uint8_t ___0:7;	//Align
		uint8_t ___1;   //Align
	} flags;			//16 bits of flags
	uint8_t chan;
	uint8_t	setup_timeout_mins;
  } softap;																			//116 bytes
  struct __attribute__((__packed__)) {
    char ssid[32];
    char password[64];
	uint64_t bssidval;
	struct {
		uint32_t ip;
		uint32_t gateway;
		uint32_t netmask;
		uint32_t dns1;
	} sttc;
	struct {
		uint8_t dhcp:1;	//1=DHCP on
		uint8_t ___0:7;	//Align
		uint8_t ___1;	//Align
	} flags;			//16 bits of flags
	uint8_t	___2;		//Align
	uint8_t	___3;		//Align
  } wlan;																			//124
} network_config_t;																	//116+124=240

//--------------------Variables--------------------

//Class
class mEprom {
    public:
		mEprom();
		~mEprom();
		void Start(void);
		void Stop(void);
		void SetNetworkDefaults(void);
		void Schedule_Save_Event(uint32_t);
	protected:
		Ticker EP_Save_Timer;
};

extern mEprom EP;
extern network_config_t netconf;