#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "dhcpserver/dhcpserver.h"
#include "freertos/event_groups.h"

#include "OTAServer.h"
#include "MyWiFi.h"


static const char *TAG = "TEC";
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        if (*server == NULL) {
            ESP_LOGI(TAG, "Starting webserver");
           *server = start_OTA_webserver();
        }
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        if (*server) {
                ESP_LOGI(TAG, "Stopping webserver");
                stop_OTA_webserver(*server);
                *server = NULL;
        }
        else if (event_id == WIFI_EVENT_AP_START){
            ESP_LOGI(TAG, "AP STARTED...");
        }
        else if (event_id == WIFI_EVENT_AP_STOP){
            ESP_LOGI(TAG, "AP STOPPED...");
        }
    }
}

void init_wifi_softap(void *arg)
{
    char lcdx1[24];
    uint8_t mac[6];
    static esp_netif_t *netif_ap = NULL;
    static httpd_handle_t server = NULL;
    esp_log_level_set("wifi", ESP_LOG_NONE);
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_ap = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192,168,4,1);
    IP4_ADDR(&ipInfo.gw, 192,168,4,1);
    IP4_ADDR(&ipInfo.netmask, 255,255,255,0);

    dhcps_lease_t dhcps_poll = { .enable = true };
    uint32_t len = sizeof(dhcps_poll);
    IP4_ADDR(&dhcps_poll.start_ip, 192, 168, 4, 100);
    IP4_ADDR(&dhcps_poll.end_ip, 192, 168, 4, 110);
    esp_netif_dhcps_option(netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &dhcps_poll, len);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        &server,
                                                        NULL));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "UPGRADE",
            .ssid_len = 0,
            .channel = 0,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    esp_efuse_mac_get_default(mac);
    sprintf((char *)lcdx1,"UPGRADE_%02X%02X%02X",mac[3],mac[4],mac[5]);
    strcpy((char*)wifi_config.ap.ssid, lcdx1);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_netif_dhcps_stop(netif_ap);
    esp_netif_set_ip_info(netif_ap, &ipInfo);
    esp_netif_dhcps_start(netif_ap);
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
            wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
}
