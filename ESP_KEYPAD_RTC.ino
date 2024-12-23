#include <Keypad.h>
#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

RTC_DS3231 rtc;
HTTPClient http;

const byte ROWS = 3;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
};

byte rowPins[ROWS] = {16, 17, 18}; 
byte colPins[COLS] = {13, 14, 27};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const char* ssid = "Luminoir";
const char* password = "luminate";
const char* apiEndpoint = "http://acaipad.k31.my.id/api/latency";
const char* apiLokalEndpoint = "http://192.168.179.169:8000/api/latency";
const char* secretKey = "thisispassword";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 7;
const int daylightOffset_sec = 0;

unsigned long lastSecondMillis = 0;
unsigned long rtcMillisOffset = 0;

void setup() {
  Serial.begin(115200);

  if (! rtc.begin()) {
  Serial.println("RTC module is NOT found");
  while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    DateTime ntpTime(
      timeinfo.tm_year + 1900, 
      timeinfo.tm_mon + 1, 
      timeinfo.tm_mday, 
      timeinfo.tm_hour, 
      timeinfo.tm_min, 
      timeinfo.tm_sec
    );
    rtc.adjust(ntpTime);
    Serial.println("RTC synchronized with NTP");
  } else {
    Serial.println("Failed to obtain NTP time");
  }

  DateTime now = rtc.now();
  lastSecondMillis = millis();
  rtcMillisOffset = lastSecondMillis % 1000;
}

unsigned long getAccurateMilliseconds(DateTime now) {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - lastSecondMillis;

  if (elapsedMillis >= 1000) {
    lastSecondMillis = currentMillis - (elapsedMillis % 1000);
    elapsedMillis = elapsedMillis % 1000;
    rtcMillisOffset = 0;
  }

  unsigned long accurateMillis = elapsedMillis + rtcMillisOffset;

  return accurateMillis % 1000;
}


void loop() {
  char key = keypad.getKey();
  if (key) {
    
    Serial.print("Key Pressed: ");
    Serial.println(key);
    
    Serial.println("Sending to INTERNET API");
    DateTime now = rtc.now();
    unsigned long accurateMillis = getAccurateMilliseconds(now);
    
    char dateTime[30]; 
    sprintf(dateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03lu",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second(),
            accurateMillis);

    Serial.print("Formatted Date-Time INTERNET: ");
    Serial.println(dateTime);
    
    sendToAPI(String(key).c_str(), String(dateTime));
    
    Serial.println("Sending to LOKAL API");
    DateTime nowLokal = rtc.now();
    unsigned long accurateMillisLokal = getAccurateMilliseconds(nowLokal);
    
    char dateTimeLokal[30];
    sprintf(dateTimeLokal, "%04d-%02d-%02d %02d:%02d:%02d.%03lu",
            nowLokal.year(), nowLokal.month(), nowLokal.day(),
            nowLokal.hour(), nowLokal.minute(), nowLokal.second(),
            accurateMillisLokal);
            
    Serial.print("Formatted Date-Time LOKAL: ");
    Serial.println(dateTimeLokal);
    
    sendToLokalAPI(String(key).c_str(), String(dateTimeLokal));
  }
  delay(10);
}


void sendToAPI(const char* keyPressed, const String& dateTime) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(apiEndpoint);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    String payload = "{";
    payload += "\"secret_key\":\"" + String(secretKey) + "\",";
    payload += "\"location\":\"INTERNET\",";
    payload += "\"sent_at\":\"" + dateTime + "\",";
    payload += "\"key_pressed\":\"" + String(keyPressed) + "\"";
    payload += "}";

    Serial.println("Sending to INTERNET API:");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response from INTERNET API:");
      Serial.println(response);
    } else {
      Serial.print("Error sending POST INTERNET API: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi is not connected (INTERNET API)");
  }
}

void sendToLokalAPI(const char* keyPressed, const String& dateTime) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(apiLokalEndpoint);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    String payload = "{";
    payload += "\"secret_key\":\"" + String(secretKey) + "\",";
    payload += "\"location\":\"LOKAL\",";
    payload += "\"sent_at\":\"" + dateTime + "\",";
    payload += "\"key_pressed\":\"" + String(keyPressed) + "\"";
    payload += "}";

    Serial.println("Sending to LOKAL API:");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response from LOKAL API:");
      Serial.println(response);
    } else {
      Serial.print("Error sending POST LOKAL API: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi is not connected (LOKAL API)");
  }
}
