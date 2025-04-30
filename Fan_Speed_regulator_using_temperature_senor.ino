// Include necessary libraries
#define BLYNK_TEMPLATE_ID "TMPL3nAuAXiku"
#define BLYNK_TEMPLATE_NAME "NEW FAN"
#define BLYNK_AUTH_TOKEN "iiXRVPtJrcmDbirqeVvNZst_hB6ixdEW"

#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <OneWire.h>
#include <WiFi.h>
#include <DallasTemperature.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi & Blynk credentials
#define WIFI_SSID "Redmi"
#define WIFI_PASSWORD "prem9802"

// Pin definitions
#define ONE_WIRE_BUS 19  // DS18B20 sensor pin
#define FAN_RELAY_1 2    // Fan Low Speed
#define FAN_RELAY_2 4    // Fan Medium Speed
#define FAN_RELAY_3 5    // Fan High Speed

#define SDA_PIN 21  // I2C SDA
#define SCL_PIN 22  // I2C SCL

// OneWire & Temperature Sensor Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// LCD setup
LiquidCrystal_PCF8574 lcd(0x27);

float outsideTemp = 25.0;
float adjustedThreshold = 28.0;
unsigned long lastAPICall = 0;
unsigned long lastCheckTime = 0;
bool autoMode = true;
bool fanPower = false; // Fan power state
String detectedCity = "Unknown";

// Connect to WiFi
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

// Get City Name from IP
String getCityFromIP() {
    HTTPClient http;
    http.begin("http://ip-api.com/json/");
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        return doc["city"].as<String>();
    }
    return "Unknown";
}

// Fetch Outdoor Temperature
float getOutdoorTemperature(String city) {
    String apiKey = "9d3bdacd5eed91f17e3de6f8b1592d00";
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + apiKey + "&units=metric";
    
    HTTPClient http;
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        return doc["main"]["temp"];
    }
    return 25.0;
}

// Update Threshold and Print to Serial Monitor
void updateThreshold() {
    detectedCity = getCityFromIP();
    Serial.println("Detected City: " + detectedCity);

    outsideTemp = getOutdoorTemperature(detectedCity);
    Serial.print("Outdoor Temperature: ");
    Serial.print(outsideTemp);
    Serial.println("°C");

    adjustedThreshold = 28.0 + (outsideTemp - 25) * 0.5;
    Serial.print("Updated Threshold: ");
    Serial.println(adjustedThreshold);
}

// Display Data on LCD (without City)
void updateLCD(float temp, String mode, String fanSpeed, float outTemp) {
    lcd.clear(); // Clears the screen before writing new data

    lcd.setCursor(0, 0);
    lcd.print(mode + " " + fanSpeed + "      ");  // Mode & Fan Speed on Same Line

    lcd.setCursor(0, 1);
    lcd.print("Temp: " + String(temp, 1) + " C ");
}


// Control Fan Speed
void controlFanSpeed() {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    
    Serial.print("Room Temperature: ");
    Serial.print(currentTemp);
    Serial.println("°C");

    Blynk.virtualWrite(V3, currentTemp);

    String modeText = autoMode ? "AUTO" : "MANUAL";
    String fanSpeedText = "";

    if (!fanPower) {
        digitalWrite(FAN_RELAY_1, LOW);
        digitalWrite(FAN_RELAY_2, LOW);
        digitalWrite(FAN_RELAY_3, LOW);
        updateLCD(currentTemp, "OFF", "OFF", outsideTemp);
        return;
    }

    if (autoMode) {
        if (currentTemp >= adjustedThreshold + 3) {
            digitalWrite(FAN_RELAY_1, LOW);
            digitalWrite(FAN_RELAY_2, LOW);
            digitalWrite(FAN_RELAY_3, HIGH);
            fanSpeedText = "HIGH";
        } else if (currentTemp > adjustedThreshold) {
            digitalWrite(FAN_RELAY_1, LOW);
            digitalWrite(FAN_RELAY_2, HIGH);
            digitalWrite(FAN_RELAY_3, LOW);
            fanSpeedText = "MEDIUM";
        } else if (currentTemp > adjustedThreshold - 3) {
            digitalWrite(FAN_RELAY_1, HIGH);
            digitalWrite(FAN_RELAY_2, LOW);
            digitalWrite(FAN_RELAY_3, LOW);
            fanSpeedText = "LOW";
        } else {
            digitalWrite(FAN_RELAY_1, LOW);
            digitalWrite(FAN_RELAY_2, LOW);
            digitalWrite(FAN_RELAY_3, LOW);
        }
    }

    updateLCD(currentTemp, modeText, fanSpeedText, outsideTemp);
}

// Blynk Virtual Pins
BLYNK_WRITE(V0) { // Fan ON/OFF
    fanPower = param.asInt();
    Serial.println(fanPower ? "Fan Turned ON" : "Fan Turned OFF");

    if (!fanPower) {
        digitalWrite(FAN_RELAY_1, LOW);
        digitalWrite(FAN_RELAY_2, LOW);
        digitalWrite(FAN_RELAY_3, LOW);
    }
}

BLYNK_WRITE(V1) { // Manual Fan Speed Control
    if (!fanPower || autoMode) return;

    int fanSpeed = param.asInt();
    if (fanSpeed == 0) {
        digitalWrite(FAN_RELAY_1, LOW);
        digitalWrite(FAN_RELAY_2, LOW);
        digitalWrite(FAN_RELAY_3, LOW);
    } else if (fanSpeed == 1) {
        digitalWrite(FAN_RELAY_1, HIGH);
        digitalWrite(FAN_RELAY_2, LOW);
        digitalWrite(FAN_RELAY_3, LOW);
    } else if (fanSpeed == 2) {
        digitalWrite(FAN_RELAY_1, LOW);
        digitalWrite(FAN_RELAY_2, HIGH);
        digitalWrite(FAN_RELAY_3, LOW);
    } else if (fanSpeed == 3) {
        digitalWrite(FAN_RELAY_1, LOW);
        digitalWrite(FAN_RELAY_2, LOW);
        digitalWrite(FAN_RELAY_3, HIGH);
    }
}

BLYNK_WRITE(V2) { // Mode Toggle (Auto/Manual)
    autoMode = param.asInt();
    Serial.println(autoMode ? "Mode: AUTO" : "Mode: MANUAL");
}

void setup() {
    Serial.begin(115200);
    connectWiFi();
    sensors.begin();
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

    pinMode(FAN_RELAY_1, OUTPUT);
    pinMode(FAN_RELAY_2, OUTPUT);
    pinMode(FAN_RELAY_3, OUTPUT);

    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.begin(20, 4);
    lcd.setBacklight(255);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" IOT PROJECT");
    lcd.setCursor(0, 1);
    lcd.print(" SMART FAN");
    delay(1000);

    updateThreshold();
}

void loop() {
    Blynk.run();

    if (millis() - lastCheckTime > 2000) {
        controlFanSpeed();
        lastCheckTime = millis();
    }

    if (millis() - lastAPICall > 900000) {
        updateThreshold();
        lastAPICall = millis();
    }
}