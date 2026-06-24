#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "OTAServer.h"
#include "MyWiFi.h"
#include "leds.h"
#include "ota_switch.h"

static const char *TAG = "BOOT";

gpio_num_t rgbpin, sw1;
typedef enum {
    ROLE_SINK_LINE  = 0,
    ROLE_SINK_AMP  = 1,
    ROLE_SOURCE_LINE = 2,
    ROLE_SOURCE_MIC = 3,
} node_role_t;

//--------------------------------------------------------
void app_main()
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
    printf("\r\n***************************\r\n");
    printf("***   HYBRID BOOTLOADER  ***\r\n");
    printf("***************************\r\n");

	printf("Compiled at:");
	printf(__TIME__);
	printf(" ");
	printf(__DATE__);
	printf("\r\n");
    // Configurar GPIO4 y GPIO17 como entrada con pull-up interno desactivado
    // (usa solo el pull-up externo de 2k2)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_17),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
 	int g1 = gpio_get_level(GPIO_NUM_4);
    int g2 = gpio_get_level(GPIO_NUM_17);
	node_role_t role = ROLE_SINK_LINE;
    if (g1 == 0 && g2 == 0) {
        role = ROLE_SINK_LINE;
    } else if (g1 == 1 && g2 == 0) {
        role = ROLE_SOURCE_MIC;
    } else if (g1 == 0 && g2 == 1) {
        role = ROLE_SOURCE_LINE;
    } else { // g1 == 1 && g2 == 1
        role = ROLE_SINK_AMP;
    }

    ESP_LOGI(TAG, "GPIO4=%d  GPIO17=%d  →  role = %s", g1, g2,
             (role == ROLE_SINK_LINE) ? "SINK_LINE" :
             (role == ROLE_SINK_AMP)  ? "SINK_AMP" :
             (role == ROLE_SOURCE_LINE) ? "SOURCE_LINE" : "SOURCE_MIC");

    sw1 = GPIO_NUM_0; 

	if (role == ROLE_SINK_LINE) {
        rgbpin = GPIO_NUM_45;
        ESP_LOGI(TAG, "Starting as SINK_LINE");
       /// led_rgb_set(16, 0, 16);   // magenta
    } else if (role == ROLE_SINK_AMP) {
        // TAS5828 amplifier       
        rgbpin = GPIO_NUM_4;
        ESP_LOGI(TAG, "Starting as SINK_AMP (TAS5828)");
        ///led_rgb_set(16, 0, 0);     // rojo
    } else if (role == ROLE_SOURCE_LINE) {
        rgbpin = GPIO_NUM_16;
        ESP_LOGI(TAG, "Starting as SOURCE_LINE (Stream ID = 0)");	
	}else { // ROLE_SOURCE_MIC
        rgbpin = GPIO_NUM_16;
        ESP_LOGI(TAG, "Starting as SOURCE_MIC (Stream ID = 0)");
        ///led_rgb_set(0, 0, 16);    // azul
    }
    leds_init();
    led_startup_blink();
    ota_switch_start(sw1);
	// Start as an AP
 	init_wifi_softap(&OTA_server);
	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
//----------------------------------------------------------------------
