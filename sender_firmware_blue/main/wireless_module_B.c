// input/output, file handling
#include <stdio.h>
#include <math.h>
// perform tasks on strings
#include <string.h>

// contains specific instructions used for compiling projects in a certain manner or for a specific environment (ESP-IDF Iot in  this case)
#include "sdkconfig.h"
// Allows access to FreeRTOS core definitions needed to use RTOS features such as tasks, scheduling, and synchronization.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_now.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

static const char *TAG = "SENSOR";

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TIMEOUT_MS       1000

#define MPU6050_SENSOR_ADDR         0x68
#define MPU6050_WHO_AM_I_REG_ADDR   0x75
#define MPU6050_PWR_MGMT_1_REG_ADDR 0x6B
#define MPU6050_GYRO_CONFIG         0x1B
#define MPU6050_ACCEL_CONFIG        0x1C
#define MPU6050_ACCEL_XOUT_H        0x3B
#define MPU6050_INT_ENABLE          0x38

#define SENSOR_ID                   2

static uint8_t receiver_mac[6] = {0xec, 0xc9, 0xff, 0xe2, 0x8a, 0x24};

/* Payload (must match receiver struct fields/order/packing) */
typedef struct __attribute__((packed)) {
    uint8_t  sensor_id;
    float    pitch_deg;
    uint32_t timestamp_ms;
} sensor_data_t;

/* I2C handles */
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

static esp_err_t mpu6050_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle,
                                       &reg_addr, 1,
                                       data, len,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t mpu6050_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = { reg_addr, data };
    return i2c_master_transmit(dev_handle,
                               write_buf, sizeof(write_buf),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void i2c_master_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
}

// /* ===== I2C SCAN ADDITION ===== */
// static void i2c_scan(void)
// {
//     ESP_LOGI(TAG, "Scanning I2C bus on SDA=%d SCL=%d ...",
//              I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

//     int found = 0;
//     for (uint8_t addr = 1; addr < 0x7F; addr++) {
//         esp_err_t ret = i2c_master_probe(bus_handle, addr, 50 /*ms*/);
//         if (ret == ESP_OK) {
//             ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
//             found++;
//         }
//     }

//     if (found == 0) {
//         ESP_LOGW(TAG, "No I2C devices found.");
//     } else {
//         ESP_LOGI(TAG, "Scan done, found %d device(s).", found);
//     }
// }
// /* ============================ */

static void mpu6050_init(void)
{
    uint8_t who_am_i = 0;

    ESP_ERROR_CHECK(mpu6050_register_read(MPU6050_WHO_AM_I_REG_ADDR, &who_am_i, 1));
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", who_am_i);

    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_PWR_MGMT_1_REG_ADDR, 0x80));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_PWR_MGMT_1_REG_ADDR, 0x00));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 0x08 => FS_SEL=1 => ±500 dps => 65.5 LSB/(deg/s) (used below)
    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_GYRO_CONFIG, 0x08));
    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_ACCEL_CONFIG, 0x00));
    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_INT_ENABLE, 0x01));

    ESP_LOGI(TAG, "MPU6050 initialized");
}

static esp_err_t mpu6050_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                                 int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t raw_data[14];
    esp_err_t ret = mpu6050_register_read(MPU6050_ACCEL_XOUT_H, raw_data, 14);
    if (ret != ESP_OK) return ret;

    *ax = (raw_data[0]  << 8) | raw_data[1];
    *ay = (raw_data[2]  << 8) | raw_data[3];
    *az = (raw_data[4]  << 8) | raw_data[5];
    *gx = (raw_data[8]  << 8) | raw_data[9];
    *gy = (raw_data[10] << 8) | raw_data[11];
    *gz = (raw_data[12] << 8) | raw_data[13];

    return ESP_OK;
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Data sent");
    } else {
        ESP_LOGW(TAG, "Send failed");
    }
}

static void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Match receiver channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer = { 0 };
    memcpy(peer.peer_addr, receiver_mac, 6);
    peer.channel = 1;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW initialized (sender)");
}

static void sensor_task(void *pvParameters)
{
    (void)pvParameters;

    sensor_data_t packet;

    static int64_t last_us = 0;
    static float pitch_deg = 0.0f;
    static float pitch0_deg = 0.0f;   // standing offset
    static bool init_done = false;

    while (1) {
        int64_t now_us = esp_timer_get_time();
        float dt = (last_us == 0) ? 0.0f : (now_us - last_us) / 1000000.0f;
        last_us = now_us;

        int16_t ax, ay, az, gx, gy, gz;
        if (mpu6050_read_raw(&ax, &ay, &az, &gx, &gy, &gz) == ESP_OK) {

            float accel_pitch_deg =
                atan2f((float)ax, (float)az) * (180.0f / (float)M_PI);

            if (!init_done) {
                pitch_deg = accel_pitch_deg;
                pitch0_deg = accel_pitch_deg;   // standing becomes 0
                init_done = true;
            }

            // For GYRO_CONFIG=0x08 (±500 dps)
            float gyro_y_dps = (float)gy / 65.5f;

            pitch_deg += gyro_y_dps * dt;

            packet.sensor_id = SENSOR_ID;
            packet.timestamp_ms = esp_log_timestamp();
            packet.pitch_deg = pitch_deg - pitch0_deg;

            esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
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

    i2c_master_init();

    //i2c_scan();          // <-- scan first

    mpu6050_init();     // <-- comment out while scanning (prevents NACK from aborting)
    espnow_init();
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sensor %d ready", SENSOR_ID);
}
