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

    #define SENSOR_ID                   1

    #define GYRO_LSB_PER_DPS   65.5f
    #define ACC_LSB_PER_G      16384.0f
    #define DEG2RAD            (M_PI / 180.0f)

    #include "MadgwickAHRS.h"
    static uint8_t receiver_mac[6] = {0xec, 0xc9, 0xff, 0xe2, 0x8a, 0x24};

    /* Payload (must match receiver struct fields/order/packing) */
    typedef struct __attribute__((packed)) {
        uint8_t  sensor_id;
        float    angle_deg;
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

        float ax_g = (float)(*ax) / ACC_LSB_PER_G;
        float ay_g = (float)(*ay) / ACC_LSB_PER_G;
        float az_g = (float)(*az) / ACC_LSB_PER_G;

        float gx_rads = ((float)(*gx) / GYRO_LSB_PER_DPS) * DEG2RAD;
        float gy_rads = ((float)(*gy) / GYRO_LSB_PER_DPS) * DEG2RAD;
        float gz_rads = ((float)(*gz) / GYRO_LSB_PER_DPS) * DEG2RAD;    

        MadgwickAHRSupdateIMU(gx_rads, gy_rads, gz_rads, ax_g, ay_g, az_g);

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

        static float offset = 0.0f;   
        static bool init_done = false;  
        static bool init_filt = false;
        static float pitch_filt = 0.0f;
        const float w = 0.2f;

        while (1) {
            // calculating yaw, pitch and roll using the quaternions
            int16_t ax, ay, az, gx, gy, gz;
        
            if (mpu6050_read_raw(&ax, &ay, &az, &gx, &gy, &gz) == ESP_OK) {
                
                //calculating the pitch angle, for the sensor placed on the back, above the tail
                float sin_angle = 2.0f * (q0*q2 - q3*q1);

                // clamp to [-1,1] to avoid NaN from small numerical errors
                if (sin_angle >  1.0f) sin_angle =  1.0f;
                if (sin_angle < -1.0f) sin_angle = -1.0f;

                float pitch = asinf(sin_angle) * (180.0f/(float)M_PI);                
                if(pitch < 0.0f) {pitch += 360.0f;}
                if(!init_filt){
                    pitch_filt = pitch;
                    init_filt = true;
                }
                else{
                    pitch_filt = pitch*w + (1.0f-w)*pitch_filt;
                }
                // this sets the first measured value as the offset and then never checks this again in the loop
                // if (!init_done) {
                //     offset = pitch_filt;   // standing becomes 0
                //     init_done = true;
                // }

                packet.sensor_id = SENSOR_ID;
                packet.timestamp_ms = esp_log_timestamp();            
                // float a = pitch_filt - offset;
                // if(a < 0.0f) a = 0.0f;
                packet.angle_deg = pitch_filt;
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
        mpu6050_init();   
        espnow_init();
        xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);

        ESP_LOGI(TAG, "Sensor %d ready", SENSOR_ID);
    }
