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

// WiFi
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

// Status LEDs
#define LED_NORMAL 26
#define LED_WARNING 27
#define LED_CRITICAL 14

// Buttons
#define BTN_RESET 18
#define BTN_FAN 5
#define BTN_DEHUMIDIFIER 17

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// Realistic Thresholds for Server Room
#define TEMP_NORMAL_TARGET 22.0     
#define TEMP_WARNING 28.0           
#define TEMP_CRITICAL 32.0          
#define HUMIDITY_LOW 40.0           
#define HUMIDITY_HIGH 60.0          
#define HUMIDITY_NORMAL_TARGET 50.0 
#define GAS_WARNING 400             
#define GAS_CRITICAL 800            

// PWM Settings
const int buzzerChannel = 0;
const int pwmFreq = 1000;
const int pwmResolution = 10;

// ═══════════════════════════════════════════
// Global Objects
// ═══════════════════════════════════════════
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient espClient;
PubSubClient client(espClient);

// Global Variables
float baseTemperature = 22.0;    // ค่าจริงจาก Sensor
float displayTemperature = 22.0; // ค่าที่แสดง (หลังจำลอง Cooling)
float baseHumidity = 50.0;
float displayHumidity = 50.0;
int gasValue = 0;

String currentStatus = "normal";
bool alertActive = false;
bool fanManual = false;
bool dehumidifierManual = false;

unsigned long lastReadTime = 0;
unsigned long lastAlertTime = 0;
unsigned long lastCoolingTime = 0;

// Cooling/Dehumidifying rates
const float coolingRate = 0.3;              
const float dehumidifyingRate = 1.0;        
const unsigned long controlInterval = 2000;

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

// 📡 MQTT Callback (รับคำสั่งจาก Dashboard)
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("📩 Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (!error) {
    const char* command = doc["command"];
    int value = doc["value"];

    if (strcmp(command, "FAN_CONTROL") == 0) {
      fanManual = (value == 1);
      Serial.printf("✅ MQTT Command: Fan set to %s\n", fanManual ? "ON" : "OFF");
      beep(100);
    } 
    else if (strcmp(command, "RESET_ALARM") == 0) {
      alertActive = false;
      Serial.println("✅ MQTT Command: Alarm Reset");
      beep(100); delay(100); beep(100);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32-Guardian-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
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
  display.print("Temp: "); display.print(displayTemperature, 1); display.print("C");
  if (fanManual) display.print(" [COOL]");
  display.println();

  display.setCursor(0, 16); // ขยับบรรทัดให้ห่างกันหน่อย
  display.print("Humi: "); display.print(displayHumidity, 1); display.print("%");
  if (dehumidifierManual) display.print(" [DRY]");
  display.println();

  display.setCursor(0, 32);
  display.print("Gas:  "); display.print(gasValue);
  display.println();

  display.setCursor(0, 52);
  if (currentStatus == "critical") display.println("CRITICAL!");
  else if (currentStatus == "warning") display.println("WARNING");
  else display.println("NORMAL");

  display.display();
}

void readSensors() {
  // 1. อ่านค่าจริง
  float rawH = dht.readHumidity();
  float rawT = dht.readTemperature();

  if (!isnan(rawH) && !isnan(rawT)) {
    baseTemperature = rawT;
    baseHumidity = rawH;
  }

  // อ่าน Gas และแปลงค่าให้สมจริง (0-1000)
  int rawGas = analogRead(GAS_PIN);
  gasValue = map(rawGas, 0, 4095, 0, 1000);

  // 2. จำลองการทำงานของแอร์ (Cooling Effect)
  unsigned long currentTime = millis();
  if (currentTime - lastCoolingTime >= controlInterval) {
    
    // Logic แอร์
    if (fanManual) {
      if (displayTemperature > TEMP_NORMAL_TARGET) {
        displayTemperature -= coolingRate; // เย็นลง
        if (displayTemperature < TEMP_NORMAL_TARGET) displayTemperature = TEMP_NORMAL_TARGET;
      }
    } else {
      // ถ้าปิดแอร์ อุณหภูมิจะค่อยๆ กลับไปหาค่าจริง
      if (displayTemperature < baseTemperature) {
        displayTemperature += coolingRate; 
        if (displayTemperature > baseTemperature) displayTemperature = baseTemperature;
      }
    }

    // Logic เครื่องลดความชื้น
    if (dehumidifierManual) {
      if (displayHumidity > HUMIDITY_NORMAL_TARGET) {
        displayHumidity -= dehumidifyingRate;
        if (displayHumidity < HUMIDITY_NORMAL_TARGET) displayHumidity = HUMIDITY_NORMAL_TARGET;
      }
    } else {
      if (displayHumidity < baseHumidity) {
        displayHumidity += dehumidifyingRate;
        if (displayHumidity > baseHumidity) displayHumidity = baseHumidity;
      }
    }

    lastCoolingTime = currentTime;
  }
}

void checkAlerts() {
  String newStatus = "normal";

  // เช็คเงื่อนไข
  if (displayTemperature > TEMP_CRITICAL || gasValue > GAS_CRITICAL) {
    newStatus = "critical";
  } else if (displayTemperature > TEMP_WARNING || 
             displayHumidity < HUMIDITY_LOW || 
             displayHumidity > HUMIDITY_HIGH || 
             gasValue > GAS_WARNING) {
    newStatus = "warning";
  }

  // เปลี่ยนสถานะ
  if (newStatus != currentStatus) {
    currentStatus = newStatus;
    Serial.println("⚠️ STATUS CHANGE: " + currentStatus);
    
    if (currentStatus == "critical") {
      setStatusLEDs("critical");
      alertActive = true;
      beep(500);
    } else if (currentStatus == "warning") {
      setStatusLEDs("warning");
      alertActive = true;
      beep(200);
    } else {
      setStatusLEDs("normal");
      alertActive = false;
    }
  }

  // เสียงเตือนต่อเนื่องถ้ายัง Critical
  if (currentStatus == "critical" && alertActive) {
    if (millis() - lastAlertTime > 3000) {
      beep(100);
      lastAlertTime = millis();
    }
  }
}

void handleButtons() {
  if (digitalRead(BTN_RESET) == LOW) {
    alertActive = false;
    beep(100); delay(300);
  }
  if (digitalRead(BTN_FAN) == LOW) {
    fanManual = !fanManual;
    beep(100); delay(300);
  }
  if (digitalRead(BTN_DEHUMIDIFIER) == LOW) {
    dehumidifierManual = !dehumidifierManual;
    beep(100); delay(300);
  }
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
  pinMode(BTN_DEHUMIDIFIER, INPUT_PULLUP);
  pinMode(GAS_PIN, INPUT);

  ledcSetup(buzzerChannel, pwmFreq, pwmResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  // Init Sensors
  Wire.begin(21, 22);
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();

  // WiFi & MQTT
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" Connected!");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  setStatusLEDs("normal");
}

void loop() {
  // 1. รักษาการเชื่อมต่อ MQTT
  if (!client.connected()) reconnect();
  client.loop();

  // 2. Loop หลักทำงานทุก 2 วินาที
  unsigned long now = millis();
  if (now - lastReadTime >= 2000) {
    lastReadTime = now;
    
    // อ่านค่า -> เช็คแจ้งเตือน -> อัปเดตจอ
    readSensors();
    checkAlerts();
    updateOLED();

    // 📤 ส่งข้อมูลขึ้น Dashboard (ใช้ค่าที่ผ่าน Logic แล้ว)
    StaticJsonDocument<200> doc;
    doc["temp"] = displayTemperature;
    doc["humi"] = displayHumidity;
    doc["gas"] = gasValue;
    doc["status"] = currentStatus;
    
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(topic_publish, buffer);
    Serial.print("📤 MQTT Published: "); Serial.println(buffer);
  }

  // 3. รับปุ่มกด (ทำงานตลอดเวลา)
  handleButtons();
  delay(50); // Small delay for stability
}