// including necessary libraries
#include "MPU6050_6Axis_MotionApps20.h"
#include <WiFi.h>
#include <esp_now.h>

#define INTERRUPT_PIN 34

// instance of mpu class
MPU6050 mpu;

bool dmpReady = false;
uint8_t devStatus;
uint16_t packetSize;
uint8_t fifoBuffer[64];
uint8_t broadcastAddress[] = {ecxFF,c9xFF,ffxFF,e2xFF,8axFF,24xFF};
Quaternion q;
VectorFloat gravity;
float ypr[3]; 

// structure used to send data
typedef struct data{  
  int id;
  double mvmt_data[3];

} data; 



esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status){
  Serial.print("Last packet sent status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success": "Fail");
}

// WiFi credentials
// const char* ssid = "FBI Surveillance Van";
// const char* password = "5engineers1commie";
// const char* serverIP = "192.168.2.19";  // Python server IP
// const int serverPort = 8082;

void dmpDataReady() { mpuInterrupt = true; }

void setup() {
  Serial.begin(115200); // For one-time testing
  
  // Initialize and Set Calibrations for MPU6050
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000);
  #endif

  // Initilizations
  mpu.initialize();
  devStatus = mpu.dmpInitialize();

  // Calibration offsets
  mpu.setXGyroOffset(220);
  mpu.setYGyroOffset(76);
  mpu.setZGyroOffset(-85);
  mpu.setZAccelOffset(1788);

  if(devStatus == 0){
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.setDMPEnabled(true);
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    packetSize = mpu.dmpGetFIFOPacketSize();
    dmpReady = true;
  }
  
  
  // Initializing Device
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK){
    Serial.println("Error initilization ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent());
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo)!= ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) !=  ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  
  
  if(!dmpReady) return; // Wait until DMP ready

  if(!client.connected()){
    // Try reconnecting every second
    while(!client.connect(serverIP, serverPort)){
      delay(1000);
    }
  }
  if(mpu.dmpGetCurrentFIFOPacket(fifoBuffer)){
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    float pitch = ypr[1] * 180 / M_PI;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &pitch, sizeof(pitch));
  }
  if(result == ESP_OK){
    Serial.println("Sent with success");
  }
  else{
    Serial.println("Error sending data");
  }

  delay(50); // 50ms interval between readings
}
