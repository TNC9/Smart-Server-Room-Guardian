#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h> // MQTT Library
#include <ArduinoJson.h>  // JSON Library

// ═══════════════════════════════════════════
// ⚙️ CONFIGURATION
// ═══════════════════════════════════════════

const char *ssid = "Wokwi-GUEST";
const char *password = "";

// MQTT Server
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Topics
const char *topic_publish = "cmu/iot/benz/server-room/data";
const char *topic_subscribe = "cmu/iot/benz/server-room/command";

// ═══════════════════════════════════════════
// Hardware Definitions
// ═══════════════════════════════════════════
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define GAS_PIN 34
#define BUZZER_PIN 19
#define LED_NORMAL 26
#define LED_WARNING 27
#define LED_CRITICAL 14
#define BTN_RESET 18
#define BTN_FAN 5
#define BTN_DEHUMIDIFIER 17
#define I2C_SDA 21
#define I2C_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// เกณฑ์การแจ้งเตือน (Logic ของเพื่อน)
#define TEMP_NORMAL_TARGET 22.0
#define TEMP_WARNING 26.0
#define TEMP_CRITICAL 30.0
#define HUMIDITY_LOW 35.0
#define HUMIDITY_HIGH 65.0
#define HUMIDITY_NORMAL_TARGET 50.0
#define GAS_WARNING 2500
#define GAS_CRITICAL 4000

const int buzzerChannel = 0;

// Global Objects
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient espClient;
PubSubClient client(espClient);

// Global Variables
float temperature = 22.0;
float humidity = 50.0;
float gasValue = 500.0;
String currentStatus = "normal";

// Flags
bool alertActive = false;
bool fanActive = false;
bool dehumidifierActive = false;

// Timers
unsigned long lastReadTime = 0;
unsigned long lastAlertTime = 0;

// ═══════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════

void setStatusLEDs(String status) {
  digitalWrite(LED_NORMAL, status == "normal" ? HIGH : LOW);
  digitalWrite(LED_WARNING, status == "warning" ? HIGH : LOW);
  digitalWrite(LED_CRITICAL, status == "critical" ? HIGH : LOW);
}

void beep(int duration) {
  ledcWrite(buzzerChannel, 512);
  delay(duration);
  ledcWrite(buzzerChannel, 0);
}

// 📡 MQTT Callback (รับคำสั่ง)
void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // แปลง JSON เพื่ออ่านคำสั่ง
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (!error) {
    const char* command = doc["command"];
    int value = doc["value"];

    // พิมพ์บอกใน Serial เหมือนเพื่อน แต่บอกว่ามาจาก MQTT
    if (strcmp(command, "FAN_CONTROL") == 0) {
      fanActive = (value == 1);
      Serial.printf("\n[MQTT] Fan set to: %s\n", fanActive ? "ON" : "OFF");
      beep(100);
    } 
    else if (strcmp(command, "DEHUMIDIFIER_CONTROL") == 0) {
      dehumidifierActive = (value == 1);
      Serial.printf("\n[MQTT] Dehumidifier set to: %s\n", dehumidifierActive ? "ON" : "OFF");
      beep(100);
    }
    else if (strcmp(command, "RESET_ALARM") == 0) {
      alertActive = false;
      Serial.println("\n[MQTT] Alert Reset");
      beep(100); delay(50); beep(100);
    }
  }
}

void reconnect() {
  if (!client.connected()) {
    // ไม่พิมพ์เยอะให้รกหน้าจอ พิมพ์แค่จุดพอ
    // Serial.print("Connecting MQTT...");
    String clientId = "ESP32-Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      // Serial.println("connected");
      client.subscribe(topic_subscribe);
    }
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Smart Server Guardian");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.printf("Temp: %.1f C", temperature);
  if (fanActive) display.print(" [FAN]");
  
  display.setCursor(0, 27);
  display.printf("Humi: %.1f %%", humidity);
  if (dehumidifierActive) display.print(" [DRY]");

  display.setCursor(0, 39);
  display.printf("Gas:  %d ppm", (int)gasValue);

  display.setCursor(0, 54);
  if (currentStatus == "critical") display.println("CRITICAL!");
  else if (currentStatus == "warning") display.println("Warning");
  else display.println("Normal");

  display.display();
}

void readSensors() {
  // 1. อ่านค่า Gas (Logic เพื่อน: Smoothing)
  int rawGas = analogRead(GAS_PIN);
  static float gasSmooth = 500.0;
  float newGas = (rawGas / 4095.0) * 5000.0;
  gasSmooth = (gasSmooth * 0.95) + (newGas * 0.05); 
  gasValue = gasSmooth;

  // 2. อ่านค่า DHT
  float dhtTemp = dht.readTemperature();
  float dhtHumi = dht.readHumidity();

  if (!isnan(dhtTemp) && !fanActive && !dehumidifierActive) {
    temperature = dhtTemp;
    humidity = dhtHumi;
  }

  // 3. Logic Auto-Control (Logic เพื่อนเป๊ะๆ)
  if (fanActive) {
    if (temperature > TEMP_NORMAL_TARGET) {
      temperature -= 0.3; // ลดทีละ 0.3 ตามเพื่อน
      Serial.println("🌀 Fan cooling...");
      
      if (temperature <= TEMP_NORMAL_TARGET) {
        temperature = TEMP_NORMAL_TARGET;
        fanActive = false;
        Serial.println("✅ Fan auto-stopped (reached target)");
        beep(100); delay(50); beep(100);
      }
    } else {
      fanActive = false; // ปิดถ้าเย็นอยู่แล้ว
    }
  }

  if (dehumidifierActive) {
    // Logic เพื่อน: +- 1.0 คือ Target
    if (humidity > HUMIDITY_NORMAL_TARGET + 1.0) {
      humidity -= 1.5; // ลดทีละ 1.5 ตามเพื่อน
      Serial.println("💨 Dehumidifier drying...");
    } 
    else if (humidity < HUMIDITY_NORMAL_TARGET - 1.0) {
      humidity += 1.5; // เพิ่มทีละ 1.5 ตามเพื่อน
      Serial.println("💨 Dehumidifier humidifying...");
    }
    else {
      humidity = HUMIDITY_NORMAL_TARGET;
      dehumidifierActive = false;
      Serial.println("✅ Dehumidifier auto-stopped (reached target)");
      beep(100); delay(50); beep(100);
    }
  }

  // 4. แสดงผล Serial (Format เพื่อน)
  Serial.println("\n--- Sensor Reading ---");
  Serial.printf("Temperature: %.1f°C", temperature);
  if (fanActive) Serial.print(" [FAN ACTIVE]");
  Serial.println();

  Serial.printf("Humidity: %.1f%%", humidity);
  if (dehumidifierActive) Serial.print(" [DRY ACTIVE]");
  Serial.println();

  Serial.printf("Gas: %.0f ppm (Raw ADC: %d)\n", gasValue, rawGas);
}

void checkAlerts() {
  String newStatus = "normal";
  String alertMsg = "";

  if (temperature > TEMP_CRITICAL) {
    newStatus = "critical";
    alertMsg += "🔥 CRITICAL TEMP: " + String(temperature, 1) + "°C\n";
  }
  if (gasValue > GAS_CRITICAL) {
    newStatus = "critical";
    alertMsg += "☠️ CRITICAL GAS: " + String((int)gasValue) + " ppm\n";
  }

  if (newStatus != "critical") {
    if (temperature > TEMP_WARNING) {
      newStatus = "warning";
      alertMsg += "⚠️ High Temp: " + String(temperature, 1) + "°C\n";
    }
    if (humidity < HUMIDITY_LOW) {
      newStatus = "warning";
      alertMsg += "⚠️ Low Humidity: " + String(humidity, 1) + "%\n";
    } else if (humidity > HUMIDITY_HIGH) {
      newStatus = "warning";
      alertMsg += "⚠️ High Humidity: " + String(humidity, 1) + "%\n";
    }
    if (gasValue > GAS_WARNING) {
      newStatus = "warning";
      alertMsg += "⚠️ Gas: " + String((int)gasValue) + " ppm\n";
    }
  }

  if (newStatus != currentStatus) {
    currentStatus = newStatus;
    Serial.println("\n==================================================");
    Serial.print("STATUS CHANGED: "); Serial.println(currentStatus);
    Serial.println("==================================================");
    if (alertMsg.length() > 0) Serial.print(alertMsg);

    if (currentStatus == "critical") { setStatusLEDs("critical"); beep(500); alertActive = true; }
    else if (currentStatus == "warning") { setStatusLEDs("warning"); beep(200); alertActive = true; }
    else { setStatusLEDs("normal"); alertActive = false; }
  }

  if (currentStatus == "critical" && alertActive && millis() - lastAlertTime > 3000) {
    beep(100);
    lastAlertTime = millis();
  }
}

void handleButtons() {
  if (digitalRead(BTN_RESET) == LOW) {
    Serial.println("\n[BUTTON] Alert Reset");
    alertActive = false;
    beep(100); delay(300);
  }
  if (digitalRead(BTN_FAN) == LOW) {
    fanActive = !fanActive;
    Serial.printf("\n[BUTTON] Fan: %s\n", fanActive ? "ON" : "OFF");
    beep(100); delay(300);
  }
  if (digitalRead(BTN_DEHUMIDIFIER) == LOW) {
    dehumidifierActive = !dehumidifierActive;
    Serial.printf("\n[BUTTON] Dehumidifier: %s\n", dehumidifierActive ? "ON" : "OFF");
    beep(100); delay(300);
  }
}

// ═══════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  
  // Format เพื่อน
  Serial.println("\n\n==============================================");
  Serial.println("   Smart Server Room Guardian Pro");
  Serial.println("   Fixed Version - Stable & Auto-Control + MQTT");
  Serial.println("==============================================\n");

  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  pinMode(LED_CRITICAL, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_FAN, INPUT_PULLUP);
  pinMode(BTN_DEHUMIDIFIER, INPUT_PULLUP);
  pinMode(GAS_PIN, INPUT);

  ledcSetup(buzzerChannel, 1000, 10);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 15); display.println("Smart Server");
  display.setCursor(5, 30); display.println("Room Guardian");
  display.display();
  delay(1000); // ลด delay หน่อยให้ boot เร็ว

  // WiFi & MQTT Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");
  Serial.printf("IP Address: %s\n\n", WiFi.localIP().toString().c_str());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  setStatusLEDs("normal");

  // Print Thresholds (Format เพื่อน)
  Serial.println("==== System Thresholds ====");
  Serial.printf("Temperature:\n  • Normal: < %.1f°C\n  • Warning: %.1f - %.1f°C\n  • Critical: > %.1f°C\n", TEMP_WARNING, TEMP_WARNING, TEMP_CRITICAL, TEMP_CRITICAL);
  Serial.printf("Gas:\n  • Normal: < %d ppm\n  • Warning: %d - %d ppm\n  • Critical: > %d ppm\n\n", GAS_WARNING, GAS_WARNING, GAS_CRITICAL, GAS_CRITICAL);
  
  Serial.println("System Ready! 🚀\n");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (millis() - lastReadTime >= 2000) {
    readSensors();
    checkAlerts();
    updateOLED();

    // 📤 MQTT Publish Logic (แอบส่งอยู่เบื้องหลัง)
    StaticJsonDocument<200> doc;
    doc["temp"] = temperature;
    doc["humi"] = humidity;
    doc["gas"] = (int)gasValue;
    doc["status"] = currentStatus;
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(topic_publish, buffer);
    
    // Serial.println(buffer); // ปิดอันนี้ไว้ เพื่อไม่ให้รกหน้าจอเหมือนของเพื่อน
    
    lastReadTime = millis();
  }

  handleButtons();
  delay(50);
}