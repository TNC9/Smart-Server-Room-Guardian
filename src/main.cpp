#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

// ═══════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════

const char *ssid = "Wokwi-GUEST";
const char *password = "";

// Pin Definitions
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

// I2C for OLED
#define I2C_SDA 21
#define I2C_SCL 22

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Realistic Thresholds for Server Room
#define TEMP_NORMAL_TARGET 22.0     // ค่าที่ต้องการ
#define TEMP_WARNING 28.0           // เริ่มร้อน
#define TEMP_CRITICAL 32.0          // อันตราย
#define HUMIDITY_LOW 40.0           // แห้งเกิน
#define HUMIDITY_HIGH 60.0          // ชื้นเกิน
#define HUMIDITY_NORMAL_TARGET 50.0 // ค่าที่ต้องการ
#define GAS_WARNING 400             // เริ่มตรวจพบ (ปกติ ~100-300)
#define GAS_CRITICAL 800            // เข้มข้น

// PWM Settings
const int buzzerChannel = 0;
const int pwmFreq = 1000;
const int pwmResolution = 10;

// ═══════════════════════════════════════════
// Global Objects
// ═══════════════════════════════════════════

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Global Variables
float baseTemperature = 22.0;    // อุณหภูมิจริงจาก sensor
float displayTemperature = 22.0; // อุณหภูมิที่แสดง (หลัง cooling)
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
const float coolingRate = 0.3;              // ลดอุณหภูมิทีละ 0.3°C
const float dehumidifyingRate = 1.0;        // ลดความชื้นทีละ 1%
const unsigned long controlInterval = 2000; // ทุก 2 วินาที

// ═══════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════

void setStatusLEDs(String status)
{
  digitalWrite(LED_NORMAL, status == "normal" ? HIGH : LOW);
  digitalWrite(LED_WARNING, status == "warning" ? HIGH : LOW);
  digitalWrite(LED_CRITICAL, status == "critical" ? HIGH : LOW);
}

void beep(int duration)
{
  ledcWrite(buzzerChannel, 512);
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
  display.print(displayTemperature, 1);
  display.print("C");
  if (fanManual)
    display.print(" [COOL]");
  display.println();

  display.setCursor(0, 27);
  display.print("Humi: ");
  display.print(displayHumidity, 1);
  display.print("%");
  if (dehumidifierManual)
    display.print(" [DRY]");
  display.println();

  display.setCursor(0, 39);
  display.print("Gas:  ");
  display.print(gasValue);
  display.println();

  // Status
  display.setCursor(0, 54);
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

  display.display();
}

void readSensors()
{
  // Read DHT22 (base values)
  float rawHumidity = dht.readHumidity();
  float rawTemperature = dht.readTemperature();

  if (isnan(rawHumidity) || isnan(rawTemperature))
  {
    Serial.println("❌ DHT22 read error!");
    return;
  }

  baseTemperature = rawTemperature;
  baseHumidity = rawHumidity;

  // Read Gas Sensor (MQ-2)
  gasValue = analogRead(GAS_PIN);
  // Scale to realistic range (0-1000 instead of 0-4095)
  gasValue = map(gasValue, 0, 4095, 0, 1000);

  // Apply cooling effect (gradual)
  unsigned long currentTime = millis();
  if (currentTime - lastCoolingTime >= controlInterval)
  {
    // Temperature control
    if (fanManual)
    {
      if (displayTemperature > TEMP_NORMAL_TARGET)
      {
        displayTemperature -= coolingRate;
        if (displayTemperature < TEMP_NORMAL_TARGET)
        {
          displayTemperature = TEMP_NORMAL_TARGET;
        }
        Serial.println("🌀 Fan cooling...");
      }
      else
      {
        displayTemperature = TEMP_NORMAL_TARGET;
      }
    }
    else
    {
      // ถ้าปิด Fan ให้อุณหภูมิค่อยๆ กลับไปตามค่าจริง
      if (displayTemperature < baseTemperature)
      {
        displayTemperature += coolingRate;
        if (displayTemperature > baseTemperature)
        {
          displayTemperature = baseTemperature;
        }
      }
      else if (displayTemperature > baseTemperature)
      {
        displayTemperature -= coolingRate;
        if (displayTemperature < baseTemperature)
        {
          displayTemperature = baseTemperature;
        }
      }
    }

    // Humidity control
    if (dehumidifierManual)
    {
      if (displayHumidity > HUMIDITY_NORMAL_TARGET)
      {
        displayHumidity -= dehumidifyingRate;
        if (displayHumidity < HUMIDITY_NORMAL_TARGET)
        {
          displayHumidity = HUMIDITY_NORMAL_TARGET;
        }
        Serial.println("💨 Dehumidifier running...");
      }
      else if (displayHumidity < HUMIDITY_NORMAL_TARGET)
      {
        displayHumidity += dehumidifyingRate;
        if (displayHumidity > HUMIDITY_NORMAL_TARGET)
        {
          displayHumidity = HUMIDITY_NORMAL_TARGET;
        }
      }
    }
    else
    {
      // ถ้าปิด Dehumidifier ให้ความชื้นค่อยๆ กลับไปตามค่าจริง
      if (displayHumidity < baseHumidity)
      {
        displayHumidity += dehumidifyingRate;
        if (displayHumidity > baseHumidity)
        {
          displayHumidity = baseHumidity;
        }
      }
      else if (displayHumidity > baseHumidity)
      {
        displayHumidity -= dehumidifyingRate;
        if (displayHumidity < baseHumidity)
        {
          displayHumidity = baseHumidity;
        }
      }
    }

    lastCoolingTime = currentTime;
  }

  // Print to Serial
  Serial.println("\n--- Sensor Reading ---");
  Serial.printf("Temperature: %.1f°C (Base: %.1f°C)", displayTemperature, baseTemperature);
  if (fanManual)
    Serial.print(" [COOLING]");
  Serial.println();

  Serial.printf("Humidity: %.1f%% (Base: %.1f%%)", displayHumidity, baseHumidity);
  if (dehumidifierManual)
    Serial.print(" [DRYING]");
  Serial.println();

  Serial.printf("Gas Level: %d (0-1000)\n", gasValue);
}

void checkAlerts()
{
  String newStatus = "normal";
  String alertMsg = "";

  // Critical conditions
  if (displayTemperature > TEMP_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "🔥 CRITICAL TEMP: " + String(displayTemperature, 1) + "°C\n";
  }

  if (gasValue > GAS_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "☠️ CRITICAL GAS: " + String(gasValue) + "\n";
  }

  // Warning conditions
  if (newStatus != "critical")
  {
    if (displayTemperature > TEMP_WARNING)
    {
      newStatus = "warning";
      alertMsg += "⚠️ High Temp: " + String(displayTemperature, 1) + "°C\n";
    }

    if (displayHumidity < HUMIDITY_LOW)
    {
      newStatus = "warning";
      alertMsg += "⚠️ Low Humidity: " + String(displayHumidity, 1) + "%\n";
    }
    else if (displayHumidity > HUMIDITY_HIGH)
    {
      newStatus = "warning";
      alertMsg += "⚠️ High Humidity: " + String(displayHumidity, 1) + "%\n";
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

    if (currentStatus == "critical")
    {
      setStatusLEDs("critical");
      beep(500);
      alertActive = true;
    }
    else if (currentStatus == "warning")
    {
      setStatusLEDs("warning");
      beep(200);
      alertActive = true;
    }
    else
    {
      setStatusLEDs("normal");
      alertActive = false;
    }
  }

  // Continuous beeping for critical
  if (currentStatus == "critical" && alertActive)
  {
    if (millis() - lastAlertTime > 3000)
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
    delay(300);
  }

  // Fan Button
  if (digitalRead(BTN_FAN) == LOW)
  {
    fanManual = !fanManual;
    Serial.printf("\n[BUTTON] Cooling Fan: %s\n", fanManual ? "ON" : "OFF");
    beep(100);
    delay(300);
  }

  // Dehumidifier Button
  if (digitalRead(BTN_DEHUMIDIFIER) == LOW)
  {
    dehumidifierManual = !dehumidifierManual;
    Serial.printf("\n[BUTTON] Dehumidifier: %s\n", dehumidifierManual ? "ON" : "OFF");
    beep(100);
    delay(300);
  }
}

// ═══════════════════════════════════════════
// Setup
// ═══════════════════════════════════════════

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n==================================================");
  Serial.println("  Smart Server Room Guardian Pro");
  Serial.println("  Final Version with MQ-2 Sensor");
  Serial.println("==================================================\n");

  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  pinMode(LED_CRITICAL, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_FAN, INPUT_PULLUP);
  pinMode(BTN_DEHUMIDIFIER, INPUT_PULLUP);
  pinMode(GAS_PIN, INPUT);

  ledcSetup(buzzerChannel, pwmFreq, pwmResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  Serial.println("✓ DHT22 initialized");

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
    display.setCursor(5, 20);
    display.println("Smart Server");
    display.setCursor(5, 35);
    display.println("Room Guardian");
    display.display();
    delay(2000);
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  setStatusLEDs("normal");

  Serial.println("\n==================================================");
  Serial.println("  System Ready!");
  Serial.println("==================================================");
  Serial.println("\nRealistic Server Room Thresholds:");
  Serial.printf("  Temperature: %.1f°C (Normal) | %.1f°C (Warning) | %.1f°C (Critical)\n",
                TEMP_NORMAL_TARGET, TEMP_WARNING, TEMP_CRITICAL);
  Serial.printf("  Humidity: %.1f%%-%.1f%% (Normal) | <%.1f%% or >%.1f%% (Warning)\n",
                HUMIDITY_LOW, HUMIDITY_HIGH, HUMIDITY_LOW, HUMIDITY_HIGH);
  Serial.printf("  Gas: <%.0f (Normal) | %.0f-%.0f (Warning) | >%.0f (Critical)\n",
                (float)GAS_WARNING, (float)GAS_WARNING, (float)GAS_CRITICAL, (float)GAS_CRITICAL);
  Serial.println("\nControls:");
  Serial.println("  🔴 Red Button   = Reset Alert");
  Serial.println("  🔵 Blue Button  = Cooling Fan (Target: 22°C)");
  Serial.println("  🟢 Green Button = Dehumidifier (Target: 50%)");
  Serial.println("--------------------------------------------------\n");
}

// ═══════════════════════════════════════════
// Main Loop
// ═══════════════════════════════════════════

void loop()
{
  unsigned long currentTime = millis();

  if (currentTime - lastReadTime >= 2000)
  {
    readSensors();
    checkAlerts();
    updateOLED();
    lastReadTime = currentTime;
  }

  handleButtons();
  delay(100);
}