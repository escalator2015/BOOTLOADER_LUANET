#include "ota_switch.h"
#include "leds.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "ota_sw";

#define OTA_LONG_PRESS_MS   5000
#define OTA_POLL_MS         20

// ---------------------------------------------------------------------------
// Switch boot partition to OTA_1 and restart
// ---------------------------------------------------------------------------
void ota_switch_trigger(void)
{
    ESP_LOGW(TAG, "Long press detected! Entering APP mode...");

    // Blinking orange LED to indicate OTA mode entry
    for (int i = 0; i < 6; i++) {
        led_rgb_set(16, 8, 0);     // Orange
        vTaskDelay(pdMS_TO_TICKS(100));
        led_rgb_set(0, 0, 0);      // Off
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Find the OTA_1 partition (0x110000)
    const esp_partition_t *ota_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_1,
        NULL);

    if (ota_part == NULL) {
        ESP_LOGE(TAG, "OTA_1 partition not found!");
        led_rgb_set(16, 0, 0);     // Solid red = error
        return;
    }

    ESP_LOGI(TAG, "OTA_1 partition found: address 0x%lx, size %lu bytes",
             (unsigned long)ota_part->address,
             (unsigned long)ota_part->size);

    // Set the boot partition
    esp_err_t err = esp_ota_set_boot_partition(ota_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        led_rgb_set(16, 0, 0);     // Solid red = error
        return;
    }

    ESP_LOGW(TAG, "Boot partition changed to OTA_1 (0x110000). Rebooting!");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

// ---------------------------------------------------------------------------
// Switch monitor task
// ---------------------------------------------------------------------------
static void ota_switch_task(void *arg)
{
    int gpio      = (int)(intptr_t)arg;
    int64_t press_start_us = 0;
    bool was_pressed = false;
    bool handled     = false;

    while (1) {
        int level  = gpio_get_level(gpio);
        bool pressed = (level == 0);     // Active low

        if (pressed && !was_pressed) {
            // Button press started
            press_start_us = esp_timer_get_time();
            handled = false;
        } else if (!pressed && was_pressed) {
            // Button released before threshold
            was_pressed = false;
        } else if (pressed && was_pressed && !handled) {
            int64_t ms = (esp_timer_get_time() - press_start_us) / 1000;
            if (ms >= OTA_LONG_PRESS_MS) {
                handled = true;
                ota_switch_trigger();
            }
        }

        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(OTA_POLL_MS));
    }

    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ota_switch_start(int sw_gpio)
{
    // Configure GPIO as input with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sw_gpio),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Create monitor task
    xTaskCreatePinnedToCore(ota_switch_task, "ota_sw", 3072,
                            (void *)(intptr_t)sw_gpio, 2, NULL, 0);

    ESP_LOGI(TAG, "OTA monitor started — GPIO%d, press >%d ms → OTA mode",
             sw_gpio, OTA_LONG_PRESS_MS);
}
