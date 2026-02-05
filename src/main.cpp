#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

// --- 1. ตั้งค่า Wi-Fi (สำหรับ Wokwi) ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- 2. ตั้งค่า Pin อุปกรณ์ ---
#define LED_PIN 2
#define GAS_PIN 34
#define BUZZER_PIN 19
#define I2C_SDA 21
#define I2C_SCL 22

// ตั้งค่า PWM สำหรับ Buzzer (ESP32 ต้องใช้ LEDC)
const int buzzerChannel = 0;
const int buzzerFreq = 1000;
const int buzzerResolution = 8;

void setup() {
  Serial.begin(115200);
  
  // ตั้งค่า Pin
  pinMode(LED_PIN, OUTPUT);
  pinMode(GAS_PIN, INPUT);
  
  // ตั้งค่า Buzzer PWM
  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  // เริ่มต้น I2C (จำลอง BME680)
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("--- Smart Server Room Guardian Pro (C++) ---");

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // --- 5.1 อ่านค่าแก๊ส ---
  // ESP32 Analog อ่านได้ 0-4095
  int gasValue = analogRead(GAS_PIN);
  
  // --- 5.2 อ่านค่าอุณหภูมิ (จำลองค่าเหมือนในโค้ด Python) ---
  float temperature = 30.5; // ถ้ามี Library BME680 ของจริงค่อยใส่เพิ่ม

  // --- 5.3 ตรวจสอบความปลอดภัย ---
  if (gasValue > 2000) {
    // แจ้งเตือน: เปิดไฟ + เปิดเสียง Buzzer (Duty cycle 128 = 50%)
    digitalWrite(LED_PIN, HIGH);
    ledcWrite(buzzerChannel, 128); 
    Serial.printf("🚨 DANGER! Gas Level: %d (Critical)\n", gasValue);
  } else {
    // ปกติ: ปิดไฟ + ปิดเสียง
    digitalWrite(LED_PIN, LOW);
    ledcWrite(buzzerChannel, 0);
    Serial.printf("💚 Normal. Gas Level: %d\n", gasValue);
  }

  delay(1000); // รอ 1 วินาที
}