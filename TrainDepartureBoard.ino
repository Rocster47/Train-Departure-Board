#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"

LiquidCrystal_I2C lcd(0x27, 20, 4);

void wifiConnect() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Lime Street Station");
  
  lcd.setCursor(0, 1);
  lcd.print("Time Pl. Destination");

  lcd.setCursor(0, 2);
  lcd.print("17:00 3 Ellesmere P.");

  lcd.setCursor(0, 3);
  lcd.print("00:00");

  Serial.begin(115200);
  wifiConnect();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Try ping me");
    delay(5000);
  } else {
    Serial.println("Connection lost\n");
    wifiConnect();
  }
}
