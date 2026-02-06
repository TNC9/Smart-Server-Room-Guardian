#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h> // MQTT Library
#include <ArduinoJson.h>  // JSON Library

// ═══════════════════════════════════════════
// ⚙️ CONFIGURATION (แก้ไขตรงนี้ให้ตรงกับ React)
// ═══════════════════════════════════════════

// WiFi (Wokwi Guest)
const char *ssid = "Wokwi-GUEST";
const char *password = "";

// MQTT Server
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883; // ESP32 ใช้ Port 1883 (TCP) ส่วน React ใช้ 8000 (WebSocket)

// 🔥 Topics (ต้องตรงกับใน React เป๊ะๆ!)
const char *topic_publish = "cmu/iot/benz/server-room/data";    // ส่งข้อมูลออก
const char *topic_subscribe = "cmu/iot/benz/server-room/command"; // รอรับคำสั่ง

// ═══════════════════════════════════════════
// Hardware Definitions
// ═══════════════════════════════════════════
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define GAS_PIN 34
#define BUZZER_PIN 19
#define LED_R 25
#define LED_G 33
#define LED_B 32
#define LED_NORMAL 26
#define LED_WARNING 27
#define LED_CRITICAL 14
#define BTN_RESET 18
#define BTN_FAN 5

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// Global Objects
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient espClient;
PubSubClient client(espClient);

// Global Variables
float temperature = 0;
float humidity = 0;
int gasValue = 0;
String currentStatus = "normal";
bool alertActive = false;
bool fanManual = false;
unsigned long lastMsg = 0;

// PWM Channels
const int buzzerChannel = 0;
const int ledRChannel = 1;
const int ledGChannel = 2;
const int ledBChannel = 3;

// ═══════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════

void setRGB(int r, int g, int b) {
  ledcWrite(ledRChannel, r);
  ledcWrite(ledGChannel, g);
  ledcWrite(ledBChannel, b);
}

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

// ฟังก์ชันเมื่อได้รับข้อความจาก MQTT (เช่น กดปุ่มจากหน้าเว็บ)
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("📩 Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // แปลง JSON ที่ได้รับมา
  // คาดหวัง: {"command": "FAN_CONTROL", "value": 1}
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (!error) {
    const char* command = doc["command"];
    int value = doc["value"];

    if (strcmp(command, "FAN_CONTROL") == 0) {
      fanManual = (value == 1);
      Serial.printf("✅ Fan set to: %s\n", fanManual ? "ON" : "OFF");
      beep(100);
    } 
    else if (strcmp(command, "RESET_ALARM") == 0) {
      alertActive = false;
      Serial.println("✅ Alarm Reset via MQTT");
      beep(100);
      delay(100);
      beep(100);
    }
  }
}

void reconnect() {
  // วนลูปจนกว่าจะต่อติด
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // พยายามเชื่อมต่อ
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // เมื่อต่อติด ให้ Subscribe รอรับคำสั่งทันที
      client.subscribe(topic_subscribe);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Guardian: ONLINE");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  display.setCursor(0, 15);
  display.printf("Temp: %.1f C\n", temperature);
  display.printf("Humi: %.1f %%\n", humidity);
  display.printf("Gas:  %d\n", gasValue);

  display.setCursor(0, 50);
  if (fanManual) display.print("[FAN ON] ");
  display.print(currentStatus);
  
  display.display();
}

// ═══════════════════════════════════════════
// Setup & Loop
// ═══════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  
  // Init Pins
  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  pinMode(LED_CRITICAL, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_FAN, INPUT_PULLUP);
  
  // Init PWM
  ledcSetup(buzzerChannel, 1000, 10);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);
  ledcSetup(ledRChannel, 1000, 10);
  ledcSetup(ledGChannel, 1000, 10);
  ledcSetup(ledBChannel, 1000, 10);
  ledcAttachPin(LED_R, ledRChannel);
  ledcAttachPin(LED_G, ledGChannel);
  ledcAttachPin(LED_B, ledBChannel);

  // Init Sensors
  Wire.begin(21, 22);
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // WiFi Connect
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  // MQTT Config
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // ตั้งฟังก์ชันรอรับข้อความ
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // ให้ MQTT ทำงานตลอดเวลา

  unsigned long now = millis();
  
  // อ่านค่าและส่งข้อมูลทุก 2 วินาที
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // 1. อ่านค่าเซนเซอร์
    float newT = dht.readTemperature();
    float newH = dht.readHumidity();
    int newG = analogRead(GAS_PIN);

    if (!isnan(newT)) temperature = newT;
    if (!isnan(newH)) humidity = newH;
    gasValue = newG;

    // 2. คำนวณสถานะ
    String lastStatus = currentStatus;
    if (temperature > 40 || gasValue > 3000) currentStatus = "critical";
    else if (temperature > 35 || humidity < 30 || humidity > 70 || gasValue > 2000) currentStatus = "warning";
    else currentStatus = "normal";

    // 3. ควบคุม LED
    if (currentStatus == "critical") { setRGB(1023, 0, 0); setStatusLEDs("critical"); }
    else if (currentStatus == "warning") { setRGB(512, 512, 0); setStatusLEDs("warning"); }
    else { setRGB(0, 512, 0); setStatusLEDs("normal"); }

    // 4. สร้าง JSON เพื่อส่ง MQTT
    // รูปแบบ: {"temp": 32.5, "humi": 60.0, "gas": 1500, "status": "normal"}
    StaticJsonDocument<200> doc;
    doc["temp"] = temperature;
    doc["humi"] = humidity;
    doc["gas"] = gasValue;
    doc["status"] = currentStatus;
    
    char buffer[256];
    serializeJson(doc, buffer);

    // 5. ส่งข้อมูลออกไป!
    client.publish(topic_publish, buffer);
    Serial.print("📤 Published: ");
    Serial.println(buffer);
    
    updateOLED();
  }

  // Manual Controls (Physical Buttons)
  if (digitalRead(BTN_FAN) == LOW) {
    fanManual = !fanManual;
    beep(100);
    delay(300);
  }
}