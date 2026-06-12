#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"

// ================== Structs ==================

struct Departure {
  String time;
  String platform;
  String destination;
};

struct DestinationAbbreviation {
  const char* fullName;
  const char* shortName;
};

// ========== Variables and Constants ==========

LiquidCrystal_I2C lcd(0x27, 20, 4);

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

const int ABBREV_DESTINATION_COUNT = sizeof(abbrevDestinations) / sizeof(abbrevDestinations[0]);

Departure departures[50];

const unsigned long API_INTERVAL_IN_MILLIS = 3600000; // Change (in milliseconds) the interval to fetch new data from TransportAPI
const unsigned long PAGE_INTERVAL_IN_MILLIS = 10000; // Change (in milliseconds) the interval to switch page on the departure board

unsigned long apiPrevMillis;
unsigned long pagePrevMillis;
unsigned long currentMillis;

unsigned int page = 0; // Page 0 is the home page (station name and time)
unsigned int maxPage = 0;

String currentTime;
String prevTime;

int departureCount = 0; // Number of departures fetched
int firstValid = 0; // Index of the first valid departure (timetabled later than or equal to the current time)
int validDepartureCount = 0; // Number of departures fetched from the first valid departure

bool fetchOrPagePerformed = false;

// ============ Function Prototypes ============

void wifiConnect();
void fetchDepartures();
void changePage();
String getAbbrevStationName(const String& stationName);
String getCurrentTime();
int timeToMinutes(String time);

// =============== Main Functions ==============

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(3, 1);
  lcd.println("Connecting....");

  Serial.begin(115200);
  Serial.println("");

  wifiConnect();

  configTzTime(
    "GMT0BST,M3.5.0/1,M10.5.0/2",
    "pool.ntp.org"
  );

  currentMillis = millis();
  apiPrevMillis = currentMillis;
  pagePrevMillis = currentMillis;

  fetchDepartures();

  prevTime = getCurrentTime();

  lcd.clear();

  lcd.setCursor(1, 1);
  lcd.print("Lime Street Statn.");
  Serial.println("Lime Street Statn.");

  lcd.setCursor(6, 2);
  lcd.print(prevTime);
  Serial.println(prevTime);
  Serial.println("");
}

void loop() {
  currentMillis = millis();
  currentTime = getCurrentTime();

  // Fetch updated set of departures if it is on the home page (outdated set of departures finished showing) and if it is the time to do so

  if (currentMillis - apiPrevMillis >= API_INTERVAL_IN_MILLIS && page == 0) {
    if (!fetchOrPagePerformed) {
      Serial.println();
    }

    fetchDepartures();

    fetchOrPagePerformed = true;
    apiPrevMillis = currentMillis;
  }

  // Change the page if it is the time to do so

  if (currentMillis - pagePrevMillis >= PAGE_INTERVAL_IN_MILLIS) {
    if (!fetchOrPagePerformed) {
      Serial.println();
    }

    changePage();

    fetchOrPagePerformed = true;
    pagePrevMillis = currentMillis;
  }

  // Update the time displayed

  if (currentTime != prevTime) {
    if (page == 0) {
      lcd.setCursor(6, 2);
      lcd.print(currentTime);
    } else {
      lcd.setCursor(6, 3);
      lcd.print(currentTime);
    }

    Serial.println(currentTime);

    fetchOrPagePerformed = false;
    prevTime = currentTime;
  }
}

// ============== Helper Functions =============

void wifiConnect() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\n\nConnected to the WiFi network.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void fetchDepartures() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection lost.");
    wifiConnect();
  }

  Serial.println("Fetching new departure data...");

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

    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      Serial.println("");
      return;
    }

    departureCount = 0;

    JsonArray trains = doc["departures"]["all"];

    for (JsonObject train : trains) {
      String operatorCode = train["operator"].as<String>();

      if (operatorCode != "ME") {
        continue;
      }

      departures[departureCount].time = train["aimed_departure_time"].as<String>();
      departures[departureCount].platform = train["platform"].as<String>();
      departures[departureCount].destination = train["destination_name"].as<String>();

      departureCount++;

      if (departureCount >= 50) {
        break;
      }
    }

    firstValid = 0;

    Serial.print("Loaded ");
    Serial.print(departureCount);
    Serial.println(" Merseyrail departures.");

    for (int i = 0; i < departureCount; i++) {
      Serial.print(departures[i].time);
      Serial.print(" ");

      Serial.print(departures[i].platform);
      Serial.print(" ");

      Serial.println(departures[i].destination);
    }

    Serial.println("");

  } else {
    Serial.println("Error on HTTP request.\n");
  }

  client.end();
}

void changePage() {
  Serial.println("Changing page...");

  int currentMinutes = timeToMinutes(currentTime.substring(0, 5));

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
    Serial.println("Lime Street Statn.");

    lcd.setCursor(6, 2);
    lcd.print(currentTime);
    Serial.println(currentTime);
  } else {
    page++;
    
    int startIndex = firstValid + ((page - 1) * 2);

    String displayText = "Pl  Time Destination";

    lcd.setCursor(0, 0);
    lcd.print(displayText);
    Serial.println(displayText);

    if (startIndex < departureCount) {
      String stationName = getAbbrevStationName(departures[startIndex].destination);
      displayText = " " + departures[startIndex].platform + " " + departures[startIndex].time + " " + stationName;
      
      lcd.setCursor(0, 1);
      lcd.print(displayText);
      Serial.println(displayText);
    }

    if (startIndex + 1 < departureCount) {
      String stationName = getAbbrevStationName(departures[startIndex + 1].destination);
      displayText = " " + departures[startIndex + 1].platform + " " + departures[startIndex + 1].time + " " + stationName;

      lcd.setCursor(0, 2);
      lcd.print(displayText);
      Serial.println(displayText);
    }

    lcd.setCursor(6, 3);
    lcd.print(currentTime);
    Serial.println(currentTime);
  }

  Serial.println("");
}

String getAbbrevStationName(const String& stationName) {
  for (int i = 0; i < ABBREV_DESTINATION_COUNT; i++) {
    if (stationName == abbrevDestinations[i].fullName) {
      return abbrevDestinations[i].shortName;
    }
  }

  if (stationName.length() > 11) {
    return stationName.substring(0, 11);
  }

  return stationName;
}

String getCurrentTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "00:00:00";
  }

  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  return String(timeStr);
}

int timeToMinutes(String time) {
  int hours = time.substring(0, 2).toInt();
  int minutes = time.substring(3, 5).toInt();

  return hours * 60 + minutes;
}
