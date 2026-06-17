#include <Arduino.h>
#include <SoftwareSerial.h>
#include <WiFiEsp.h>

const int ESP_RX_PIN = 2;
const int ESP_TX_PIN = 3;

SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASSWORD";

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  Serial.println("ESP8266 Wi-Fi test started");
  Serial.println("Update ssid and pass before uploading.");

  WiFi.init(&espSerial);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ESP8266 not detected");
    while (true) {
    }
  }

  Serial.println("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    delay(3000);
  }

  Serial.println("Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  Serial.print("Wi-Fi status: ");
  Serial.println(WiFi.status());
  delay(3000);
}
