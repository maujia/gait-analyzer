#include <WiFi.h>
#include <esp_wifi.h>
// PRESS RESET BUTTON TO SEE MAC ADDRESS
void getMacAddress() {
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK){
    Serial.printf("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  }
  else {
    Serial.println("Failed to read MAC address");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();
  Serial.print("Mac Address: ");
  getMacAddress();

  
  // uint8_t baseMac[6];
  // esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // for(int i = 0; i <6; i++){
  //   Serial.printf("%02X:", baseMac[i])
  // }
}

void loop() {
}
