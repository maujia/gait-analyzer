// Put this on BOTH sender + receiver, then open Serial Monitor and compare.
#include <WiFi.h>
#include <esp_wifi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(2000);


  uint8_t ch;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&ch, &second);
  Serial.printf("MAC=%s  CH=%u\n", WiFi.macAddress().c_str(), ch);
  WiFi.disconnect(true, true);
}

void loop() {}