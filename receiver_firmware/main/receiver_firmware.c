#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"

static const char *TAG = "RECEIVER";

/* Must match sender payload exactly */
typedef struct __attribute__((packed)) {
    uint8_t  sensor_id;
    float    angle_deg;   // generalized name
    uint32_t timestamp_ms;
} angles_t;

static QueueHandle_t rx_q;

/* --- optional: calibration state if you later want a neutral pose ---
static bool calib_done = false;
static float calib_rel = 0.0f;
------------------------------------------------------------------- */

// Called by ESP-IDF when data is received through ESP-NOW.
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    (void)recv_info;

    if (len != (int)sizeof(angles_t)) {
        ESP_LOGW(TAG, "Invalid packet size: %d (expected %u)",
                 len, (unsigned)sizeof(angles_t));
        return;
    }

    angles_t packet;
    memcpy(&packet, data, sizeof(packet));

    xQueueSendFromISR(rx_q, &packet, NULL);
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Ensure receiver stays on same channel as sender
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_LOGI(TAG, "WiFi initialized (STA, channel 1)");
}

static void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW initialized");
}

static void process_task(void *pvParameters)
{
    (void)pvParameters;

    angles_t packet;
    angles_t thigh = {0};
    angles_t back  = {0};
    bool have_thigh = false;
    bool have_back  = false;

    while (1) {
        if (xQueueReceive(rx_q, &packet, portMAX_DELAY)) {

            // DEBUG: raw packet print (can keep or remove)
            printf("RAW %u %.2f %" PRIu32 "\n",
                   (unsigned)packet.sensor_id,
                   (double)packet.angle_deg,
                   packet.timestamp_ms);

            // FIXED MAPPING:
            // sensor_id 1 = BACK module
            // sensor_id 2 = THIGH module
            if (packet.sensor_id == 1) {        // back module
                back = packet;
                have_back = true;
            } else if (packet.sensor_id == 2) { // thigh module
                thigh = packet;
                have_thigh = true;
            }

            if (have_thigh && have_back) {
                int32_t dt = (int32_t)thigh.timestamp_ms - (int32_t)back.timestamp_ms;
                if (dt < 0) dt = -dt;

                // For now, DO NOT filter by dt so we always see something.
                // Later you can re-enable with a looser threshold (e.g. dt <= 100).
                // if (dt <= 30) {

                // 1) Mounting offsets and wrap to 0..360
                //    thigh standing ≈ 86-90°, use 88° to center
                float thigh_cal = thigh.angle_deg - 88.0f;
                if (thigh_cal < 0.0f)    thigh_cal += 360.0f;
                if (thigh_cal >= 360.0f) thigh_cal -= 360.0f;

                // back sensor: standing near 360°/0°, treat as already around 0
                float back_cal = back.angle_deg;  // 0..360 from sender

                // 2) Relative hip angle = thigh - back, shortest path on circle
                float hip = thigh_cal - back_cal;
                if (hip > 180.0f)  hip -= 360.0f;
                if (hip < -180.0f) hip += 360.0f;  // signed [-180,180)

                // CLAMP: never allow negative (no extension)
                if (hip < 0.0f) hip = 0.0f;

                // log: t thigh_cal back_cal hip
                printf("%" PRIu32 " %.2f %.2f %.2f\n",
                    thigh.timestamp_ms,
                    (double)thigh_cal,
                    (double)back_cal,
                    (double)hip);
            }
        }
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    
    // Create queue and processing task *first*
    rx_q = xQueueCreate(64, sizeof(angles_t));
    ESP_ERROR_CHECK(rx_q ? ESP_OK : ESP_FAIL);

    xTaskCreate(process_task, "process", 4096, NULL, 5, NULL);

    wifi_init();
    espnow_init();

    ESP_LOGI(TAG, "Receiver ready - waiting for data...");
}