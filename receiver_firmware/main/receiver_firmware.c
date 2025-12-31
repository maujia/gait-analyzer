#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <conio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    float    pitch_deg;
    uint32_t timestamp_ms;
} angles_t;

File *pfile;
pfile = fopen("Angle Data", "a");


// Called by ESP-IDF when data is received through ESP-NOW.
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    (void)recv_info;

    if (len == (int)sizeof(angles_t)) {
        const angles_t *packet = (const angles_t *)data;

        printf("Sensor_%u: Pitch Angle: %.2f deg, t=%" PRIu32 " ms\n",
               (unsigned)packet->sensor_id,
               (double)packet->pitch_deg,
               packet->timestamp_ms);
    } else {
        ESP_LOGW(TAG, "Invalid packet size: %d (expected %u)",
                 len, (unsigned)sizeof(angles_t));
    }
    
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

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    while(espnow_init()){
        fprintf(pfile, )

    }   

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Receiver ready - waiting for data...");
    // if a keystroke is made, send a signal back to the sensors telling them to stop sending data forward
}
