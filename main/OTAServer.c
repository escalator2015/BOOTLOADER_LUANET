#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "sys/param.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "freertos/event_groups.h"
#include "OTAServer.h"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");


httpd_handle_t OTA_server = NULL;
int8_t flash_status = 0;

EventGroupHandle_t reboot_event_group;
const int REBOOT_BIT = BIT0;

static const char *TAG = "OTA_STREAM";

//----------------------------------------------------------------------------------
void systemRebootTask(void * parameter)
{
	reboot_event_group = xEventGroupCreate();
	xEventGroupClearBits(reboot_event_group, REBOOT_BIT);

	for (;;)
	{
		EventBits_t staBits = xEventGroupWaitBits(reboot_event_group, REBOOT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
		if ((staBits & REBOOT_BIT) != 0)
		{
			ESP_LOGI(TAG, "Reboot Command, Restarting");
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			esp_restart();
		}
	}
}
//----------------------------------------------------------------------------------
/* Send index.html Page */
esp_err_t OTA_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI("OTA", "index.html Requested");

	// Clear this every time page is requested
	flash_status = 0;
	
	httpd_resp_set_type(req, "text/html");

	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;
}
//----------------------------------------------------------------------------------
/* Send .ICO (icon) file  */
esp_err_t OTA_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI("OTA", "favicon_ico Requested");
    
	httpd_resp_set_type(req, "image/x-icon");

	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}
//----------------------------------------------------------------------------------
/* Status */
esp_err_t OTA_update_status_handler(httpd_req_t *req)
{
	char ledJSON[100];
	
	ESP_LOGI("OTA", "Status Requested");

	sprintf(ledJSON, "{\"status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", flash_status, __TIME__, __DATE__);
	
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ledJSON, strlen(ledJSON));
	
	if (flash_status == 1)
	{
		// We cannot directly call reboot here because we need the 
		// browser to get the ack back. 
		xEventGroupSetBits(reboot_event_group, REBOOT_BIT);		
	}

	return ESP_OK;
}
//----------------------------------------------------------------------------------
esp_err_t OTA_update_goapp_handler(httpd_req_t *req)
{
	ESP_LOGI("OTA", "goapp Requested");

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
	if(esp_ota_set_boot_partition(update_partition) == ESP_OK)
			{
		      xEventGroupSetBits(reboot_event_group, REBOOT_BIT);
			}
	return ESP_OK;
}
//----------------------------------------------------------------------------------
/* Receive .Bin file — streaming OTA, no RAM buffer */
esp_err_t OTA_update_post_handler(httpd_req_t *req)
{
    char ota_buff[8192];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    int total_written = 0;

    flash_status = -1;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota_handle;

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "OTA started, partition at 0x%lx", (unsigned long)update_partition->address);

    do {
        recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)));
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "recv error %d", recv_len);
            esp_ota_abort(ota_handle);
            return ESP_FAIL;
        }

        if (!is_req_body_started) {
            is_req_body_started = true;
            char *body_start = strstr(ota_buff, "\r\n\r\n");
            if (!body_start) {
                ESP_LOGE(TAG, "Malformed multipart header");
                esp_ota_abort(ota_handle);
                return ESP_FAIL;
            }
            body_start += 4;
            int body_len = recv_len - (body_start - ota_buff);
            err = esp_ota_write(ota_handle, body_start, body_len);
            if (err != ESP_OK) { esp_ota_abort(ota_handle); return ESP_FAIL; }
            total_written += body_len;
            content_received = body_len;
        } else {
            err = esp_ota_write(ota_handle, ota_buff, recv_len);
            if (err != ESP_OK) { esp_ota_abort(ota_handle); return ESP_FAIL; }
            total_written += recv_len;
            content_received += recv_len;
        }

        int pct = (content_received * 100) / content_length;
        printf("OTA: %d/%d (%d%%)\r", content_received, content_length, pct);

    } while (content_received < content_length);

    printf("\n");

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }

    flash_status = 1;
    ESP_LOGI(TAG, "OTA success! %d bytes written. Rebooting...", total_written);
    xEventGroupSetBits(reboot_event_group, REBOOT_BIT);

    return ESP_OK;
}
//----------------------------------------------------------------------------------
httpd_uri_t OTA_index_html = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = OTA_index_html_handler,
	/* Let's pass response string in user
	 * context to demonstrate it's usage */
	.user_ctx = NULL
};
//----------------------------------------------------------------------------------
httpd_uri_t OTA_favicon_ico = {
	.uri = "/favicon.ico",
	.method = HTTP_GET,
	.handler = OTA_favicon_ico_handler,
	.user_ctx = NULL
};
//----------------------------------------------------------------------------------
httpd_uri_t OTA_update = {
	.uri = "/update",
	.method = HTTP_POST,
	.handler = OTA_update_post_handler,
	.user_ctx = NULL
};
//----------------------------------------------------------------------------------
httpd_uri_t OTA_status = {
	.uri = "/status",
	.method = HTTP_POST,
	.handler = OTA_update_status_handler,
	.user_ctx = NULL
};
//----------------------------------------------------------------------------------
httpd_uri_t OTA_goapp = {
	.uri = "/goapp",
	.method = HTTP_POST,
	.handler = OTA_update_goapp_handler,
	.user_ctx = NULL
};
//----------------------------------------------------------------------------------
httpd_handle_t start_OTA_webserver(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	// Need this task to spin up, see why in task
	xTaskCreate(&systemRebootTask, "rebootTask", 2048, NULL, 5, NULL);
	
	// Lets change this from port 80 (default) to 8080
	//config.server_port = 8080;
	
	
	// Lets bump up the stack size (default was 4096)
	config.stack_size = 24768;
	
	
	// Start the httpd server
	ESP_LOGI("OTA", "Starting server on port: '%d'", config.server_port);
	
	if (httpd_start(&OTA_server, &config) == ESP_OK) 
	{
		// Set URI handlers
		ESP_LOGI("OTA", "Registering URI handlers");
		httpd_register_uri_handler(OTA_server, &OTA_index_html);
		httpd_register_uri_handler(OTA_server, &OTA_favicon_ico);
		httpd_register_uri_handler(OTA_server, &OTA_update);
		httpd_register_uri_handler(OTA_server, &OTA_status);
		httpd_register_uri_handler(OTA_server, &OTA_goapp);
		return OTA_server;
	}

	ESP_LOGI("OTA", "Error starting server!");
	return NULL;
}
//----------------------------------------------------------------------------------
void stop_OTA_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	httpd_stop(server);
}
//----------------------------------------------------------------------------------

