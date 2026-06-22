#include <stdio.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "OTAServer.h"
#include "MyWiFi.h"

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

	// Start as an AP
 	init_wifi_softap(&OTA_server);
	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
//----------------------------------------------------------------------
