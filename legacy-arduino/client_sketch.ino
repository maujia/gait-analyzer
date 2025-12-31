// including necessary libraries
#include "MPU6050_6Axis_MotionApps20.h"
#include <WiFi.h>
#include <esp_now.h>

uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};


#define INTERRUPT_PIN 34
MPU6050 mpu;
bool dmpReady = false;
uint8_t devStatus;
uint16_t packetSize;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3]; 

// WiFi credentials
const char* ssid = "FBI Surveillance Van";
const char* password = "5engineers1commie";
const char* serverIP = "192.168.2.19";  // Python server IP
const int serverPort = 8082;

WiFiClient client;
volatile bool mpuInterrupt = false;
void dmpDataReady() { mpuInterrupt = true; }

void setup() {
  Serial.begin(115200); // For one-time testing

  // Initialize I2C for MPU6050
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000);
  #endif

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500); // Wait until connected
  }
  
  // Initialize MPU6050
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

  // Try connecting to Python server once
  if(client.connect(serverIP, serverPort)){
    Serial.println("Connected to Python server"); // One-time check
  } else {
    Serial.println("Failed to connect to server");
    //Serial.println("ESP32 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Connecting to: ");
    Serial.println(serverIP);
    // Optional: blink an LED here to indicate failure
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

    client.println(pitch); // Send pitch to Python server
    // One-time Serial print for testing
    Serial.print("Pitch sent: ");
    Serial.println(pitch);
  }

  delay(50); // 50ms interval between readings
}
