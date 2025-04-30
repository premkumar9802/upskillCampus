#define BLYNK_TEMPLATE_ID "TMPL3nAuAXiku"
#define BLYNK_TEMPLATE_NAME "NEW FAN"
#define BLYNK_AUTH_TOKEN "iiXRVPtJrcmDbirqeVvNZst_hB6ixdEW"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define WIFI_SSID "abc"
#define WIFI_PASSWORD "123456789"
#define ONE_WIRE_BUS 19
#define FAN_RELAY_1 2
#define FAN_RELAY_2 4
#define FAN_RELAY_3 5
#define UDP_PORT 4210
#define SDA_PIN 21
#define SCL_PIN 22

WiFiUDP udp;
WebServer server(80);
WebSocketsServer webSocket(81);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_PCF8574 lcd(0x27);

float outsideTemp = 0.0;
float indoorTemp = 0.0;
float adjustedThreshold = 28.0;
String fanSpeed = "OFF";
bool autoMode = true;
bool fanPower = false;

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  sensors.begin();
  udp.begin(UDP_PORT);
  server.begin();
  webSocket.begin();

  pinMode(FAN_RELAY_1, OUTPUT);
  pinMode(FAN_RELAY_2, OUTPUT);
  pinMode(FAN_RELAY_3, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.begin(20, 4);
  lcd.setBacklight(255);
  updateLCD();
}

void receiveTemperature() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[10];
    int len = udp.read(incomingPacket, 10);
    if (len > 0) {
      incomingPacket[len] = '\0';
      outsideTemp = atof(incomingPacket);

      if (outsideTemp <= 10)
        adjustedThreshold = 24.0;
      else if (outsideTemp <= 20)
        adjustedThreshold = 26.0;
      else if (outsideTemp <= 30)
        adjustedThreshold = 27.5 + (outsideTemp - 25) * 0.3;
      else if (outsideTemp <= 40)
        adjustedThreshold = 28.0 + (outsideTemp - 30) * 0.4;
      else
        adjustedThreshold = 32.0;

      adjustedThreshold = min(max(adjustedThreshold, 24.0f), 32.0f);
      Blynk.virtualWrite(V4, outsideTemp);
      updateLCD();
    }
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("In:" + String(indoorTemp, 1));
  lcd.setCursor(8, 0);
  lcd.print("Out:" + String(outsideTemp, 1));
  lcd.setCursor(0, 1);
  lcd.print("Th:" + String(adjustedThreshold, 1));
  lcd.setCursor(8, 1);
  lcd.print(autoMode ? "A" : "M");
  lcd.setCursor(9, 1);
  lcd.print(" FS:" + fanSpeed);
}

void controlFanSpeed() {
  sensors.requestTemperatures();
  indoorTemp = sensors.getTempCByIndex(0);
  Blynk.virtualWrite(V3, indoorTemp);

  if (!fanPower) {
    digitalWrite(FAN_RELAY_1, LOW);
    digitalWrite(FAN_RELAY_2, LOW);
    digitalWrite(FAN_RELAY_3, LOW);
    fanSpeed = "OFF";
    return;
  }

  if (autoMode) {
    if (indoorTemp >= adjustedThreshold + 3) {
      digitalWrite(FAN_RELAY_1, LOW);
      digitalWrite(FAN_RELAY_2, LOW);
      digitalWrite(FAN_RELAY_3, HIGH);
      fanSpeed = "HIGH";
    } else if (indoorTemp > adjustedThreshold) {
      digitalWrite(FAN_RELAY_1, LOW);
      digitalWrite(FAN_RELAY_2, HIGH);
      digitalWrite(FAN_RELAY_3, LOW);
      fanSpeed = "MEDIUM";
    } else if (indoorTemp > adjustedThreshold - 3) {
      digitalWrite(FAN_RELAY_1, HIGH);
      digitalWrite(FAN_RELAY_2, LOW);
      digitalWrite(FAN_RELAY_3, LOW);
      fanSpeed = "LOW";
    } else {
      digitalWrite(FAN_RELAY_1, LOW);
      digitalWrite(FAN_RELAY_2, LOW);
      digitalWrite(FAN_RELAY_3, LOW);
      fanSpeed = "OFF";
    }
  }

  webSocket.broadcastTXT(fanSpeed);
  updateLCD();
}

BLYNK_WRITE(V0) {
  fanPower = param.asInt();
  updateLCD();
}

BLYNK_WRITE(V1) {
  if (!fanPower || autoMode) return;
  int level = param.asInt();
  digitalWrite(FAN_RELAY_1, level == 1);
  digitalWrite(FAN_RELAY_2, level == 2);
  digitalWrite(FAN_RELAY_3, level == 3);
  adjustedThreshold = (level == 1) ? 26 : (level == 2) ? 27 : 28;
  fanSpeed = (level == 1) ? "LOW" : (level == 2) ? "MEDIUM" : "HIGH";
  updateLCD();
}

BLYNK_WRITE(V2) {
  autoMode = param.asInt();
  updateLCD();
}

void loop() {
  Blynk.run();
  receiveTemperature();
  controlFanSpeed();
  server.handleClient();
  webSocket.loop();
  delay(1000);
}

