#pragma once

#define CONFIG_AP_SSID		  	"UPGRADE"
#define CONFIG_AP_PASSPHRASE	"12345678"

#define AP_SSID_HIDDEN 0
#define AP_MAX_CONNECTIONS 1
#define AP_BEACON_INTERVAL 100			// in milli seconds

#define STATION_MAXIMUM_RETRY	10




void init_wifi_softap(void *arg);
void start_dhcp_server(void);
//void printStationList(void);
//void print_sta_info(void *pvParam);




