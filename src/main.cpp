#include <Arduino.h>
#include <WiFi.h>
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

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(GAS_PIN, INPUT);
  
  // ตั้งค่า Buzzer
  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  // เริ่มต้น Sensor
  dht.begin();
  
  Serial.println("--- Smart Server Room Guardian Pro ---");
  Serial.println("Initializing...");

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);
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
}