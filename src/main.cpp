#include <Arduino.h>
#include <WiFi.h>
<<<<<<< HEAD
#include "DHT.h" // ต้องลง Library "DHT sensor library" เพิ่มใน Wokwi หรือ IDE

// --- 1. ตั้งค่า Wi-Fi ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- 2. ตั้งค่า Pin อุปกรณ์ ---
#define LED_PIN 2      // ต่อ R คร่อมลง GND (Active High)
#define GAS_PIN 34     // Potentiometer จำลองแก๊ส
#define BUZZER_PIN 19
#define DHT_PIN 15     // ขา Data ของ DHT22
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// ตั้งค่า PWM สำหรับ Buzzer
const int buzzerChannel = 0; // ใช้ Channel 0 ก็พอ
const int buzzerFreq = 2000;
const int buzzerResolution = 8;
=======
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

// ═══════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════

// WiFi (Wokwi)
const char *ssid = "Wokwi-GUEST";
const char *password = "";

// Pin Definitions
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define GAS_PIN 34
#define BUZZER_PIN 19

// RGB LED
#define LED_R 25
#define LED_G 33
#define LED_B 32

// Status LEDs
#define LED_NORMAL 26
#define LED_WARNING 27
#define LED_CRITICAL 14

// Buttons
#define BTN_RESET 18
#define BTN_FAN 5

// I2C for OLED
#define I2C_SDA 21
#define I2C_SCL 22

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
>>>>>>> b1cee04255997d8098520eff9262de55c6928bea

// Thresholds
#define TEMP_WARNING 35.0
#define TEMP_CRITICAL 40.0
#define HUMIDITY_LOW 30.0
#define HUMIDITY_HIGH 70.0
#define GAS_WARNING 2000
#define GAS_CRITICAL 3000

// PWM Settings
const int buzzerChannel = 0;
const int ledRChannel = 1;
const int ledGChannel = 2;
const int ledBChannel = 3;
const int pwmFreq = 1000;
const int pwmResolution = 10; // 0-1023

// ═══════════════════════════════════════════
// Global Objects
// ═══════════════════════════════════════════

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Global Variables
float temperature = 0;
float humidity = 0;
int gasValue = 0;
String currentStatus = "normal";
bool alertActive = false;
bool fanManual = false;
unsigned long lastReadTime = 0;
unsigned long lastAlertTime = 0;

// ═══════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════

void setRGB(int r, int g, int b)
{
  ledcWrite(ledRChannel, r);
  ledcWrite(ledGChannel, g);
  ledcWrite(ledBChannel, b);
}

void setStatusLEDs(String status)
{
  digitalWrite(LED_NORMAL, status == "normal" ? HIGH : LOW);
  digitalWrite(LED_WARNING, status == "warning" ? HIGH : LOW);
  digitalWrite(LED_CRITICAL, status == "critical" ? HIGH : LOW);
}

void beep(int duration)
{
  ledcWrite(buzzerChannel, 512); // 50% duty
  delay(duration);
  ledcWrite(buzzerChannel, 0);
}

void updateOLED()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setCursor(0, 0);
  display.println("Server Guardian");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Sensor Data
  display.setCursor(0, 15);
  display.print("Temp: ");
  display.print(temperature, 1);
  display.println(" C");

  display.setCursor(0, 25);
  display.print("Humi: ");
  display.print(humidity, 1);
  display.println(" %");

  display.setCursor(0, 35);
  display.print("Gas:  ");
  display.println(gasValue);

  // Status
  display.setCursor(0, 50);
  if (currentStatus == "critical")
  {
    display.println("CRITICAL!");
  }
  else if (currentStatus == "warning")
  {
    display.println("Warning");
  }
  else
  {
    display.println("Normal");
  }

  // Fan Status
  if (fanManual)
  {
    display.setCursor(70, 50);
    display.println("[FAN ON]");
  }

  display.display();
}

void readSensors()
{
  // Read DHT22
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Check if reading failed
  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("❌ DHT22 read error!");
    return;
  }

  // Read Gas (Potentiometer)
  gasValue = analogRead(GAS_PIN);

  // Print to Serial
  Serial.println("\n--- Sensor Reading ---");
  Serial.printf("Temperature: %.1f°C\n", temperature);
  Serial.printf("Humidity: %.1f%%\n", humidity);
  Serial.printf("Gas Level: %d (0-4095)\n", gasValue);
}

void checkAlerts()
{
  String newStatus = "normal";
  String alertMsg = "";

  // Critical conditions
  if (temperature > TEMP_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "🔥 CRITICAL TEMP: " + String(temperature, 1) + "°C\n";
  }

  if (gasValue > GAS_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "☠️ CRITICAL GAS: " + String(gasValue) + "\n";
  }

  // Warning conditions (if not critical)
  if (newStatus != "critical")
  {
    if (temperature > TEMP_WARNING)
    {
      newStatus = "warning";
      alertMsg += "⚠️ High Temp: " + String(temperature, 1) + "°C\n";
    }

    if (humidity < HUMIDITY_LOW)
    {
      newStatus = "warning";
      alertMsg += "⚠️ Low Humidity: " + String(humidity, 1) + "%\n";
    }
    else if (humidity > HUMIDITY_HIGH)
    {
      newStatus = "warning";
      alertMsg += "⚠️ High Humidity: " + String(humidity, 1) + "%\n";
    }

    if (gasValue > GAS_WARNING)
    {
      newStatus = "warning";
      alertMsg += "⚠️ Gas Detected: " + String(gasValue) + "\n";
    }
  }

  // Handle status change
  if (newStatus != currentStatus)
  {
    currentStatus = newStatus;

    Serial.println("\n==================================================");
    Serial.println("STATUS CHANGE: " + String(currentStatus));
    Serial.println("==================================================");
    if (alertMsg.length() > 0)
    {
      Serial.print(alertMsg);
    }

    // Visual/Audio alerts
    if (currentStatus == "critical")
    {
      setRGB(1023, 0, 0); // Red
      setStatusLEDs("critical");
      beep(500);
      alertActive = true;
    }
    else if (currentStatus == "warning")
    {
      setRGB(512, 512, 0); // Yellow
      setStatusLEDs("warning");
      beep(200);
      alertActive = true;
    }
    else
    {
      setRGB(0, 512, 0); // Green
      setStatusLEDs("normal");
      alertActive = false;
    }
  }

  // Continuous beeping for critical
  if (currentStatus == "critical" && alertActive)
  {
    if (millis() - lastAlertTime > 2000)
    {
      beep(100);
      lastAlertTime = millis();
    }
  }
}

void handleButtons()
{
  // Reset Alert Button
  if (digitalRead(BTN_RESET) == LOW)
  {
    Serial.println("\n[BUTTON] Alert Reset");
    alertActive = false;
    ledcWrite(buzzerChannel, 0);
    beep(100);
    delay(300); // Debounce
  }

  // Manual Fan Button
  if (digitalRead(BTN_FAN) == LOW)
  {
    fanManual = !fanManual;
    Serial.printf("\n[BUTTON] Manual Fan: %s\n", fanManual ? "ON" : "OFF");
    beep(100);
    delay(300); // Debounce
  }
}

// ═══════════════════════════════════════════
// Setup
// ═══════════════════════════════════════════

void setup()
{
  Serial.begin(115200);
<<<<<<< HEAD
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(GAS_PIN, INPUT);
  
  // ตั้งค่า Buzzer
  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  // เริ่มต้น Sensor
  dht.begin();
  
  Serial.println("--- Smart Server Room Guardian Pro ---");
  Serial.println("Initializing...");
=======
  Serial.println("\n\n==================================================");
  Serial.println("  Smart Server Room Guardian Pro");
  Serial.println("  C++ Version for Wokwi");
  Serial.println("==================================================\n");

  // Setup Pins
  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  pinMode(LED_CRITICAL, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_FAN, INPUT_PULLUP);
  pinMode(GAS_PIN, INPUT);

  // Setup PWM for RGB LED
  ledcSetup(ledRChannel, pwmFreq, pwmResolution);
  ledcSetup(ledGChannel, pwmFreq, pwmResolution);
  ledcSetup(ledBChannel, pwmFreq, pwmResolution);
  ledcAttachPin(LED_R, ledRChannel);
  ledcAttachPin(LED_G, ledGChannel);
  ledcAttachPin(LED_B, ledBChannel);

  // Setup PWM for Buzzer
  ledcSetup(buzzerChannel, pwmFreq, pwmResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
>>>>>>> b1cee04255997d8098520eff9262de55c6928bea

  // Initialize DHT22
  dht.begin();
  Serial.println("✓ DHT22 initialized");

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("✗ OLED initialization failed!");
  }
  else
  {
    Serial.println("✓ OLED initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("Smart Server");
    display.setCursor(10, 35);
    display.println("Room Guardian");
    display.display();
    delay(2000);
  }

  // Connect WiFi
  WiFi.begin(ssid, password);
<<<<<<< HEAD
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
}

void loop() {
  // 1. อ่านค่าแก๊ส (0-4095)
  int gasValue = analogRead(GAS_PIN);
  
  // 2. อ่านค่าจาก DHT22 จริง
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // เช็คว่าอ่านค่า Sensor ได้ไหม (กันค่า Nan)
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("❌ Failed to read from DHT sensor!");
    delay(2000); // ✨ เพิ่มบรรทัดนี้ เพื่อรอให้เซนเซอร์พร้อมก่อนลองใหม่
    return;
  }

  // 3. แสดงผลใน Serial
  Serial.printf("Temp: %.1f C | Humid: %.1f %% | Gas: %d\n", temperature, humidity, gasValue);

  // 4. เงื่อนไขความปลอดภัย (เพิ่มเงื่อนไข Temp สูงเกินด้วยก็ดีสำหรับ Server Room)
  if (gasValue > 2000 || temperature > 40.0) {
    // อันตราย: ไฟติด + เสียงดัง
    digitalWrite(LED_PIN, HIGH);
    ledcWrite(buzzerChannel, 128); 
    Serial.println("🚨 DANGER! Critical Condition Detected");
  } else {
    // ปกติ
    digitalWrite(LED_PIN, LOW);
    ledcWrite(buzzerChannel, 0);
  }

  delay(2000); // DHT22 อ่านค่าได้ช้า ควร Delay อย่างน้อย 2 วินาที
=======
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initial RGB - Green
  setRGB(0, 512, 0);
  setStatusLEDs("normal");

  Serial.println("\n==================================================");
  Serial.println("  System Ready!");
  Serial.println("==================================================\n");
  Serial.printf("Thresholds:\n");
  Serial.printf("  Temp Warning: %.1f°C, Critical: %.1f°C\n", TEMP_WARNING, TEMP_CRITICAL);
  Serial.printf("  Humidity: %.1f%% - %.1f%%\n", HUMIDITY_LOW, HUMIDITY_HIGH);
  Serial.printf("  Gas Warning: %d, Critical: %d\n", GAS_WARNING, GAS_CRITICAL);
  Serial.println("--------------------------------------------------\n");
}

// ═══════════════════════════════════════════
// Main Loop
// ═══════════════════════════════════════════

void loop()
{
  unsigned long currentTime = millis();

  // Read sensors every 2 seconds
  if (currentTime - lastReadTime >= 2000)
  {
    readSensors();
    checkAlerts();
    updateOLED();
    lastReadTime = currentTime;
  }

  // Check buttons
  handleButtons();

  delay(100);
>>>>>>> b1cee04255997d8098520eff9262de55c6928bea
}