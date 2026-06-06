#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

const unsigned long API_INTERVAL_IN_MILLIS = 3600000;
const unsigned long PAGE_INTERVAL_IN_MILLIS = 10000;

unsigned long apiPrevMillis;
unsigned long pagePrevMillis;
unsigned long currentMillis;

struct Departure {
  String time;
  String platform;
  String destination;
};

Departure departures[50];
int departureCount = 0;

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

void fetchDepartures() {
  HTTPClient client;
    
  String url =
    "https://transportapi.com/v3/uk/train/station_timetables/LIV.json"
    "?app_id=" + String(TRANSPORT_API_APP_ID) +
    "&app_key=" + String(TRANSPORT_API_APP_KEY) +
    "&train_status=passenger";

  client.begin(url);
  int httpCode = client.GET();

  if (httpCode > 0) {
    String payload = client.getString();
    
    DynamicJsonDocument doc(32768);

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    departureCount = 0;

    JsonArray trains =
        doc["departures"]["all"];

    for (JsonObject train : trains) {

        String operatorCode =
            train["operator"].as<String>();

        if (operatorCode != "ME") {
            continue;
        }

        departures[departureCount].time =
            train["aimed_departure_time"].as<String>();

        departures[departureCount].platform =
            train["platform"].as<String>();

        departures[departureCount].destination =
            train["destination_name"].as<String>();

        departureCount++;

        if (departureCount >= 50) {
            break;
        }
    }

    Serial.print("Loaded ");
    Serial.print(departureCount);
    Serial.println(" Merseyrail departures");

    for (int i = 0; i < departureCount; i++) {
      Serial.print(departures[i].time);
      Serial.print(" ");

      Serial.print(departures[i].platform);
      Serial.print(" ");

      Serial.println(departures[i].destination);
    }

  } else {
    Serial.println("Error on HTTP request");
  }

  client.end();
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

  currentMillis = millis();
  apiPrevMillis = currentMillis;
  pagePrevMillis = apiPrevMillis;

  fetchDepartures();
}

void loop() {
  currentMillis = millis();

  if (currentMillis - apiPrevMillis >= API_INTERVAL_IN_MILLIS) {
    fetchDepartures();
    apiPrevMillis = currentMillis;
    pagePrevMillis = currentMillis;
  }
  
  if (currentMillis - pagePrevMillis >= PAGE_INTERVAL_IN_MILLIS) {
    // Next page.
    pagePrevMillis = currentMillis;
  }
}

/*
  if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Try ping me");
        delay(5000);
      } else {
        Serial.println("Connection lost\n");
        wifiConnect();
      }
*/
