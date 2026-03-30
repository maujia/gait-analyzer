#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

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

// Data Packet being sent to the receiver from the sender firmware
typedef struct __attribute__((packed)) {
    uint8_t sensor_id; // Sensor sending data
    float angle_deg; //angle value sent 
    uint32_t timestamp_ms; // time sent at
} angles_t;

// The Queue used to move received packets (data) out of the callback
static QueueHandle_t rx_q;

// This runs whenever an ESP-NOW packet comes in.
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{

    // Skip data collection if the packet size received doesn't match with expected amount

    if (len != (int)sizeof(angles_t)) {
        ESP_LOGW(TAG, "Wrong packet size: %d (expected %u)",
                 len, (unsigned)sizeof(angles_t));
        return;
    }

    angles_t packet;
    memcpy(&packet, data, sizeof(packet));

    // Add data packet to the queue to be stored using another task
    xQueueSendFromISR(rx_q, &packet, NULL);
}
// Set up the microncontroller to use Wi-Fi in station mode and lock it to channel 1 (All controllers should be on this channel)
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_LOGI(TAG, "WiFi ready on channel 1");
}

// Turns on ESP-NOW to register the receive callback.
static void espnow_init(void)
{
    // initialize esp_now communication protocol
    ESP_ERROR_CHECK(esp_now_init());
    // run the callback function when a successfull packet arrives
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW ready");
}

// This task pulls packets out of the queue and does the real angle math.
static void process_task(void *pvParameters)
{
    // generic parameter variable for FreeRTOS Task, ignored for now
    (void)pvParameters;

    // create a struct of type angles_t to hold the received data
    angles_t packet;
    // a struct for holding the data from the sensor on the thigh
    angles_t thigh = {0};
    // a struct for holding the data from the sensor on the back
    angles_t back = {0};

    // variable to check whether thigh or back data has been received yet
    bool have_thigh = false;
    bool have_back = false;

    // infinite loop for processing data
    while (1) {
        // checking for data received in the Queue and stores it in the "packet" variable
        if (xQueueReceive(rx_q, &packet, portMAX_DELAY)) {

            // incoming data is printed for debuggin
            printf("RAW %u %.2f %" PRIu32 "\n",
                   (unsigned)packet.sensor_id,
                   (double)packet.angle_deg,
                   packet.timestamp_ms);

            // sensor_id 1 = back sensor
            // sensor_id 2 = thigh sensor
            if (packet.sensor_id == 1) {
                back = packet;
                have_back = true;
            } else if (packet.sensor_id == 2) {
                thigh = packet;
                have_thigh = true;
            }

            // only calculate hip angle once data's been received from both sensors at least once
            if (have_thigh && have_back) {
                int32_t dt = (int32_t)thigh.timestamp_ms - (int32_t)back.timestamp_ms;
                if (dt < 0) dt = -dt;

                
                // // Thigh sensor has a mounting offset, so subtract it out first.
                // float thigh_cal = thigh.angle_deg - 88.0f;
                // if (thigh_cal < 0.0f) thigh_cal += 360.0f;
                // if (thigh_cal >= 360.0f) thigh_cal -= 360.0f;

                // back angle is already close to the reference (basically straight).
                float back_cal = back.angle_deg;

                // Hip angle = thigh relative to back.
                // Wrap it so we get the shortest circular difference.
                float hip = thigh_cal - back_cal;
                if (hip > 180.0f) hip -= 360.0f;
                if (hip < -180.0f) hip += 360.0f;

                // Ignore negative values so we don't show extension.
                if (hip < 0.0f) hip = 0.0f;

                // Final log: timestamp, thigh angle, back angle, hip angle
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
    // NVS has to be ready before Wi-Fi starts.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Make the queue before packets start arriving.
    rx_q = xQueueCreate(64, sizeof(angles_t));
    ESP_ERROR_CHECK(rx_q ? ESP_OK : ESP_FAIL);

    xTaskCreate(process_task, "process", 4096, NULL, 5, NULL);

    wifi_init();
    espnow_init();

    ESP_LOGI(TAG, "Receiver ready");
}