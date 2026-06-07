#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

const unsigned long API_INTERVAL_IN_MILLIS = 3600000;
const unsigned long PAGE_INTERVAL_IN_MILLIS = 10000;

unsigned long apiPrevMillis;
unsigned long pagePrevMillis;
unsigned long currentMillis;

unsigned int page = 0;
unsigned int maxPage = 0;

String currentTime;
String prevTime;

struct Departure {
  String time;
  String platform;
  String destination;
};

Departure departures[50];
int departureCount = 0;
int firstValid = 0;
int validDepartureCount = 0;

struct DestinationAbbreviation {
    const char* fullName;
    const char* shortName;
};

DestinationAbbreviation abbrevDestinations[] = {
  {"Chester", "Chester"},
  {"Bache", "Bache"},
  {"Capenhurst", "Capenhurst"},
  {"Hooton", "Hooton"},
  {"Eastham Rake", "Easthm Rake"},
  {"Bromborough", "Bromborough"},
  {"Bromborough Rake", "Brom. Rake"},
  {"Spital", "Spital"},
  {"Port Sunlight", "P. Sunlight"},
  {"Bebington", "Bebington"},
  {"Rock Ferry", "Rock Ferry"},
  {"Green Lane", "Green Lane"},
  {"Birkenhead Central", "B'head Cent"},
  {"Birkenhead Hamilton Square", "Ham. Square"},
  {"Liverpool James Street", "James Strt"},
  {"Moorfields", "Moorfields"},
  {"Liverpool Lime Street (High Level)", "Lime Street"},
  {"Liverpool Central", "Liv Central"},

  {"West Kirby", "West Kirby"},
  {"Hoylake", "Hoylake"},
  {"Manor Road", "Manor Road"},
  {"Meols", "Meols"},
  {"Moreton (Merseyside)", "Moreton"},
  {"Leasowe", "Leasowe"},
  {"Bidston", "Bidston"},
  {"Birkenhead North", "B'head Nrth"},
  {"Birkenhead Park", "B'head Park"},
  {"Birkenhead Conway Park", "Conway Park"},

  {"New Brighton", "N. Brighton"},
  {"Wallasey Grove Road", "W. Grove Rd"},
  {"Wallasey Village", "W. Village"},

  {"Ellesmere Port", "Elles. Port"},
  {"Overpool", "Overpool"},
  {"Little Sutton", "L. Sutton"}
};

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

  configTzTime(
    "GMT0BST,M3.5.0/1,M10.5.0/2",
    "pool.ntp.org"
  );
}

String getCurrentTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
      return "00:00:00";
  }

  char buffer[9];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

  return String(buffer);
}

int timeToMinutes(String time) {
  int hours = time.substring(0, 2).toInt();
  int minutes = time.substring(3, 5).toInt();

  return hours * 60 + minutes;
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

String getAbbrevStation(String stationName) {
  int stationCount = sizeof(abbrevDestinations) / sizeof(abbrevDestinations[0]);

  for (int i = 0; i < stationCount; i++) {
    if (stationName == abbrevDestinations[i].fullName) {
      return abbrevDestinations[i].shortName;
    }
  }

  if (stationName.length() > 11) {
    return stationName.substring(0, 11);
  }

  return stationName;
}

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(3, 1);
  lcd.print("Connecting....");

  Serial.begin(115200);
  wifiConnect();

  currentMillis = millis();
  apiPrevMillis = currentMillis;
  pagePrevMillis = apiPrevMillis;

  fetchDepartures();

  prevTime = getCurrentTime();

  lcd.clear();

  lcd.setCursor(1, 1);
  lcd.print("Lime Street Statn.");

  lcd.setCursor(6, 2);
  lcd.print(getCurrentTime());
}

void loop() {
  currentMillis = millis();

  if (currentMillis - apiPrevMillis >= API_INTERVAL_IN_MILLIS && page == 0) {
    fetchDepartures();
    firstValid = 0;
    apiPrevMillis = currentMillis;
  }

  if (currentMillis - pagePrevMillis >= PAGE_INTERVAL_IN_MILLIS) {
    int currentMinutes = timeToMinutes(getCurrentTime().substring(0, 5));

    while (firstValid < departureCount && timeToMinutes(departures[firstValid].time) < currentMinutes) {
      firstValid++;
    }
    
    validDepartureCount = departureCount - firstValid;

    maxPage = (validDepartureCount + 1) / 2;

    if (maxPage > 3) {
      maxPage = 3;
    }
    
    lcd.clear();
    
    if (page >= maxPage) {
      page = 0;

      lcd.setCursor(1, 1);
      lcd.print("Lime Street Statn.");

      lcd.setCursor(6, 2);
      lcd.print(getCurrentTime());
    } else {
      page++;
      
      int startIndex = firstValid + ((page - 1) * 2);

      lcd.setCursor(0, 0);
      lcd.print("Pl  Time Destination");

      if (startIndex < departureCount) {
        String stationName = getAbbrevStation(departures[startIndex].destination);
        lcd.setCursor(0, 1);
        lcd.print(" " + departures[startIndex].platform + " " + departures[startIndex].time + " " + stationName);
      }

      if (startIndex + 1 < departureCount) {
        String stationName = getAbbrevStation(departures[startIndex + 1].destination);
        lcd.setCursor(0, 2);
        lcd.print(" " + departures[startIndex + 1].platform + " " + departures[startIndex + 1].time + " " + stationName);
      }

      lcd.setCursor(6, 3);
      lcd.print(getCurrentTime());
    }

    pagePrevMillis = currentMillis;
  }

  currentTime = getCurrentTime();

  if (currentTime != prevTime) {
    if (page == 0) {
      lcd.setCursor(6, 2);
      lcd.print(currentTime);
    } else {
      lcd.setCursor(6, 3);
      lcd.print(getCurrentTime());
    }

    prevTime = currentTime;
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
