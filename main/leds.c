#include "leds.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "esp_err.h"

// ---------------------------------------------------------------------------
// RMT — resolución 10 MHz (100 ns/tick)
// WS2812B:  T0H=400 ns, T0L=800 ns  |  T1H=800 ns, T1L=400 ns
// ---------------------------------------------------------------------------
#define RMT_RESOLUTION_HZ   10000000U   // 10 MHz

static const char *TAG = "leds";

static rmt_channel_handle_t s_rmt_chan  = NULL;
static rmt_encoder_handle_t s_bytes_enc = NULL;
extern gpio_num_t rgbpin;

// --- Blink control ---
static TaskHandle_t s_blink_task = NULL;
static volatile bool s_blink_enabled = false;
static SemaphoreHandle_t s_rmt_mutex = NULL;

// ---------------------------------------------------------------------------

void leds_init(void)
{
    // --- Crear mutex para acceso seguro al RMT ---
    s_rmt_mutex = xSemaphoreCreateMutex();
    assert(s_rmt_mutex != NULL);

    // --- WS2812 RGB on GPIO via RMT ---
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num          = rgbpin,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_rmt_chan));

    rmt_bytes_encoder_config_t enc_cfg = {
        // Bit 0: 1→400 ns high, 0→800 ns low
        .bit0 = { .duration0 = 4, .level0 = 1, .duration1 = 8, .level1 = 0 },
        // Bit 1: 1→800 ns high, 0→400 ns low
        .bit1 = { .duration0 = 8, .level0 = 1, .duration1 = 4, .level1 = 0 },
        .flags.msb_first = 1,   // WS2812 sends MSB first
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_bytes_enc));
    ESP_ERROR_CHECK(rmt_enable(s_rmt_chan));

    ESP_LOGI(TAG, "LED initialized — GPIO%d (WS2812)",
             rgbpin);
}
//---------------------------------------------------------------------------
void led_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    // WS2812 espera orden GRB
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_conf = { .loop_count = 0 };

    if (s_rmt_mutex != NULL)
        xSemaphoreTake(s_rmt_mutex, portMAX_DELAY);

    esp_err_t err = rmt_transmit(s_rmt_chan, s_bytes_enc, grb, sizeof(grb), &tx_conf);
    if (err == ESP_OK)
        err = rmt_tx_wait_all_done(s_rmt_chan, portMAX_DELAY);

    if (s_rmt_mutex != NULL)
        xSemaphoreGive(s_rmt_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_rgb_set failed: %s", esp_err_to_name(err));
    }
}
//---------------------------------------------------------------------------
static void blink_task_fn(void *pvParameter)
{
    (void)pvParameter;
    /* Orange = R=32, G=8, B=0 */
    const uint8_t orange_r = 32;
    const uint8_t orange_g = 8;
    const uint8_t orange_b = 0;

    while (s_blink_enabled) {
        led_rgb_set(orange_r, orange_g, orange_b);
        if (!s_blink_enabled) break;
        vTaskDelay(250 / portTICK_PERIOD_MS);
        led_rgb_set(0, 0, 0);
        if (!s_blink_enabled) break;
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    led_rgb_set(0, 0, 0);
    /* Clear handle before deleting so led_startup_blink can create again */
    s_blink_task = NULL;
    vTaskDelete(NULL);
}
//---------------------------------------------------------------------------
void led_startup_blink(void)
{
    if (s_blink_task != NULL) return;   /* already running */

    s_blink_enabled = true;
    xTaskCreate(blink_task_fn, "led_blink", 3072, NULL, 5, &s_blink_task);
}
//---------------------------------------------------------------------------
void led_blink_stop(void)
{
    s_blink_enabled = false;
    /* Wait for the task to actually exit and clear s_blink_task */
    while (s_blink_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}