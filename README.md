# Train Departure Board

A small project involving an ESP32 microcontroller to display train departures from the Merseyrail Station in Liverpool Lime Street Station.

## Prerequisites

Before getting started, ensure you have the following:

- An ESP32 microcontroller and compatible LCD display.
- Access to a Wi-Fi network with an internet connection.
- A TransportAPI account with an application configured. You will need the application's App ID and App Key.

## Environment Variables

Create `secrets.h` in the root directory and ensure it is listed in your `.gitignore` file.

Copy and paste the following, and change the contents of each constant:

```c++
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* TRANSPORT_API_APP_ID = "YOUR_APP_ID";
const char* TRANSPORT_API_APP_KEY = "YOUR_APP_KEY";
```
