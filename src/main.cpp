#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

const char *ssid = "Wokwi-GUEST";
const char *password = "";

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
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// เกณฑ์การแจ้งเตือน
#define TEMP_NORMAL_TARGET 22.0
#define TEMP_WARNING 26.0
#define TEMP_CRITICAL 30.0
#define HUMIDITY_LOW 35.0
#define HUMIDITY_HIGH 65.0
#define HUMIDITY_NORMAL_TARGET 50.0
#define GAS_WARNING 2500
#define GAS_CRITICAL 4000

const int buzzerChannel = 0;

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ค่าเริ่มต้นอยู่ในระดับ NORMAL ทุกค่า
float temperature = 22.0; // อุณหภูมิปกติ
float humidity = 50.0;    // ความชื้นปกติ
float gasValue = 500.0;   // ก๊าซในระดับปลอดภัย

String currentStatus = "normal";
bool alertActive = false;
bool fanActive = false;
bool dehumidifierActive = false;
unsigned long lastReadTime = 0;
unsigned long lastAlertTime = 0;

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

  display.setCursor(0, 0);
  display.println("Server Guardian");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.print("Temp: ");
  display.print(temperature, 1);
  display.print("C");
  if (fanActive)
    display.print(" [FAN]");
  display.println();

  display.setCursor(0, 27);
  display.print("Humi: ");
  display.print(humidity, 1);
  display.print("%");
  if (dehumidifierActive)
    display.print(" [DRY]");
  display.println();

  display.setCursor(0, 39);
  display.print("Gas: ");
  display.print((int)gasValue);
  display.print(" ppm");
  display.println();

  display.setCursor(0, 54);
  if (currentStatus == "critical")
    display.println("CRITICAL!");
  else if (currentStatus == "warning")
    display.println("Warning");
  else
    display.println("Normal");

  display.display();
}

void readSensors()
{
  // ========================================
  // 1. อ่านค่า Gas แบบเสถียร (ไม่สุ่ม)
  // ========================================
  int rawGas = analogRead(GAS_PIN);

  // แปลง ADC เป็น ppm โดยตรง (ไม่มีการสุ่ม)
  // ใช้ค่าเฉลี่ยจาก 10 ครั้งเพื่อความเสถียร
  static float gasSmooth = 500.0; // เริ่มต้นที่ค่าปกติ
  float newGas = (rawGas / 4095.0) * 5000.0;
  gasSmooth = (gasSmooth * 0.95) + (newGas * 0.05); // Smoothing มากขึ้น
  gasValue = gasSmooth;

  // ========================================
  // 2. อ่านค่า DHT22 (อุณหภูมิ/ความชื้น)
  // ========================================
  float dhtTemp = dht.readTemperature();
  float dhtHumi = dht.readHumidity();

  // ถ้าอ่านค่า DHT22 ได้ และไม่มีพัดลม/Dry ทำงาน ให้ใช้ค่าจาก sensor
  if (!isnan(dhtTemp) && !fanActive && !dehumidifierActive)
  {
    temperature = dhtTemp;
    humidity = dhtHumi;
  }

  // ========================================
  // 3. ระบบพัดลมอัตโนมัติ (ลดอุณหภูมิจนถึง target แล้วหยุดเอง)
  // ========================================
  if (fanActive)
  {
    // ลดอุณหภูมิลงเรื่อยๆ
    if (temperature > TEMP_NORMAL_TARGET)
    {
      temperature -= 0.3; // ลดทีละ 0.3 องศา
      Serial.println("🌀 Fan cooling...");

      // ถ้าถึง target แล้ว ปิดพัดลมอัตโนมัติ
      if (temperature <= TEMP_NORMAL_TARGET)
      {
        temperature = TEMP_NORMAL_TARGET;
        fanActive = false;
        Serial.println("✅ Fan auto-stopped (reached target)");
        beep(100);
        delay(50);
        beep(100);
      }
    }
    else
    {
      // ถ้าอุณหภูมิต่ำกว่า target อยู่แล้ว ปิดเลย
      fanActive = false;
      Serial.println("✅ Fan stopped (already cool)");
    }
  }

  // ========================================
  // 4. ระบบ Dehumidifier อัตโนมัติ (ปรับความชื้นจนถึง target แล้วหยุดเอง)
  // ========================================
  if (dehumidifierActive)
  {
    // ถ้าชื้นเกิน → ลดความชื้น
    if (humidity > HUMIDITY_NORMAL_TARGET + 1.0)
    {
      humidity -= 1.5; // ลดทีละ 1.5%
      Serial.println("💨 Dehumidifier drying...");
    }
    // ถ้าแห้งเกิน → เพิ่มความชื้น
    else if (humidity < HUMIDITY_NORMAL_TARGET - 1.0)
    {
      humidity += 1.5; // เพิ่มทีละ 1.5%
      Serial.println("💨 Dehumidifier humidifying...");
    }
    // ถ้าอยู่ใกล้ target แล้ว (±1%) → ปิดอัตโนมัติ
    else
    {
      humidity = HUMIDITY_NORMAL_TARGET;
      dehumidifierActive = false;
      Serial.println("✅ Dehumidifier auto-stopped (reached target)");
      beep(100);
      delay(50);
      beep(100);
    }
  }

  // ========================================
  // 5. แสดงผล Serial
  // ========================================
  Serial.println("\n--- Sensor Reading ---");
  Serial.printf("Temperature: %.1f°C", temperature);
  if (fanActive)
    Serial.print(" [FAN ACTIVE]");
  Serial.println();

  Serial.printf("Humidity: %.1f%%", humidity);
  if (dehumidifierActive)
    Serial.print(" [DRY ACTIVE]");
  Serial.println();

  Serial.printf("Gas: %.0f ppm (Raw ADC: %d)\n", gasValue, rawGas);
}

void checkAlerts()
{
  String newStatus = "normal";
  String alertMsg = "";

  // ตรวจสอบ Critical ก่อน
  if (temperature > TEMP_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "🔥 CRITICAL TEMP: " + String(temperature, 1) + "°C\n";
  }

  if (gasValue > GAS_CRITICAL)
  {
    newStatus = "critical";
    alertMsg += "☠️ CRITICAL GAS: " + String((int)gasValue) + " ppm\n";
  }

  // ถ้าไม่ Critical ตรวจสอบ Warning
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
      alertMsg += "⚠️ Gas: " + String((int)gasValue) + " ppm\n";
    }
  }

  // ถ้าสถานะเปลี่ยน
  if (newStatus != currentStatus)
  {
    currentStatus = newStatus;
    Serial.println("\n==================================================");
    Serial.print("STATUS CHANGED: ");
    Serial.println(currentStatus); // แก้ไข: ไม่ใช้ toUpperCase()
    Serial.println("==================================================");
    if (alertMsg.length() > 0)
      Serial.print(alertMsg);

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

  // Beep ซ้ำถ้า Critical
  if (currentStatus == "critical" && alertActive && millis() - lastAlertTime > 3000)
  {
    beep(100);
    lastAlertTime = millis();
  }
}

void handleButtons()
{
  // ปุ่ม Reset Alert
  if (digitalRead(BTN_RESET) == LOW)
  {
    Serial.println("\n[BUTTON] Alert Reset");
    alertActive = false;
    ledcWrite(buzzerChannel, 0);
    beep(100);
    delay(300);
  }

  // ปุ่ม Fan - Toggle on/off
  if (digitalRead(BTN_FAN) == LOW)
  {
    fanActive = !fanActive;
    Serial.printf("\n[BUTTON] Fan: %s\n", fanActive ? "ON (will auto-stop at target)" : "OFF");
    beep(100);
    delay(300);
  }

  // ปุ่ม Dehumidifier - Toggle on/off
  if (digitalRead(BTN_DEHUMIDIFIER) == LOW)
  {
    dehumidifierActive = !dehumidifierActive;
    Serial.printf("\n[BUTTON] Dehumidifier: %s\n", dehumidifierActive ? "ON (will auto-stop at target)" : "OFF");
    beep(100);
    delay(300);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n==============================================");
  Serial.println("   Smart Server Room Guardian Pro");
  Serial.println("   Fixed Version - Stable & Auto-Control");
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

  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, 15);
    display.println("Smart Server");
    display.setCursor(5, 30);
    display.println("Room Guardian");
    display.setCursor(15, 45);
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
  Serial.println("\n✅ WiFi Connected");
  Serial.printf("IP Address: %s\n\n", WiFi.localIP().toString().c_str());

  setStatusLEDs("normal");

  Serial.println("==== System Thresholds ====");
  Serial.printf("Temperature:\n");
  Serial.printf("  • Normal: < %.1f°C\n", TEMP_WARNING);
  Serial.printf("  • Warning: %.1f - %.1f°C\n", TEMP_WARNING, TEMP_CRITICAL);
  Serial.printf("  • Critical: > %.1f°C\n", TEMP_CRITICAL);
  Serial.printf("  • Target: %.1f°C\n\n", TEMP_NORMAL_TARGET);

  Serial.printf("Humidity:\n");
  Serial.printf("  • Normal: %.1f - %.1f%%\n", HUMIDITY_LOW, HUMIDITY_HIGH);
  Serial.printf("  • Target: %.1f%%\n\n", HUMIDITY_NORMAL_TARGET);

  Serial.printf("Gas:\n");
  Serial.printf("  • Normal: < %d ppm\n", GAS_WARNING);
  Serial.printf("  • Warning: %d - %d ppm\n", GAS_WARNING, GAS_CRITICAL);
  Serial.printf("  • Critical: > %d ppm\n\n", GAS_CRITICAL);

  Serial.println("==== Features ====");
  Serial.println("✓ Stable gas sensor (no random values)");
  Serial.println("✓ Auto-stop fan when temp reaches target");
  Serial.println("✓ Auto-stop dehumidifier when humidity reaches target");
  Serial.println("✓ System starts at NORMAL state\n");

  Serial.println("System Ready! 🚀\n");
}

void loop()
{
  if (millis() - lastReadTime >= 2000)
  {
    readSensors();
    checkAlerts();
    updateOLED();
    lastReadTime = millis();
  }

  handleButtons();
  delay(100);
}