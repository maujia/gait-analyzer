#include <Wifi.h>
// PRESS RESET BUTTON TO SEE MAC ADDRESS
void getMacAddress() {
}

void setup() {
  Serial.begin(115200);
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  for(int i = 0; i <6; i++){
    Serial.printf("%02X:", baseMac[i])
  }
}

void loop() {
}
