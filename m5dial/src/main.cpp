#include <Arduino.h>
#include "M5Dial.h"
#include <WiFi.h>

#include <vector>

#include "c_icon/icon_30x30/faucet.hpp"
#include "c_icon/icon_30x30/lamp.hpp"
#include "c_icon/icon_30x30/padlock.hpp"

#include "config.hpp"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

#define JST 3600 * 9

#define RELAY_1_PIN 1
#define RELAY_2_PIN 2

#define ADC_SOIL_MOISTURE_PIN 15 // A2CH4

struct tm timeinfo;
bool button_a, button_b, button_c = false;
int last_ntp_update_day = -1;
bool time_initialized = false;


enum Mode {
  MODE_TEMP,
  MODE_HUMI,
  MODE_SOIL_MOISTURE,
  MODE_CO2,
};

static long prev_pos = 0;
static bool viz_data_updated = true;
enum Mode current_mode = MODE_TEMP;

// Soil moisture sensor variables
volatile int soil_moisture_adc = 0;
volatile float soil_moisture_voltage = 0.0;
volatile float soil_moisture_percent = 0.0;

// Sensor reading task
void sensorTask(void* parameter) {
  while (true) {
    int adc_value = analogRead(ADC_SOIL_MOISTURE_PIN);
    soil_moisture_adc = adc_value;
    soil_moisture_voltage = (adc_value / 4095.0) * 3.3;
    soil_moisture_percent = (adc_value / 4095.0) * 100.0;

    // Serial output
    Serial.print("Soil Moisture - ADC: ");
    Serial.print(soil_moisture_adc);
    Serial.print(" | Voltage: ");
    Serial.print(soil_moisture_voltage, 3);
    Serial.print("V | Percentage: ");
    Serial.print(soil_moisture_percent, 1);
    Serial.println("%");

    vTaskDelay(pdMS_TO_TICKS(1000)); // Update every 1 second
  }
}

void ntp_update_with_timeout() {
  WiFi.mode(WIFI_STA);
  configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2);
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo) && millis() - start < 10000) {
    delay(100);
  }
}

void setup() {
  auto cfg = M5.config();
  // M5.begin(cfg);
  M5Dial.begin(cfg, true, true);

  delay(1000);

  Serial.println("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t time_cnt = millis();
  prev_pos = M5Dial.Encoder.read();

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (millis() - time_cnt > 1000) {
      break;
    }
    M5.Lcd.print(".");
  }
  Serial.println("");

  // delay
  delay(1000);

  if (WiFi.status() == WL_CONNECTED) {
    ntp_update_with_timeout();
    if (getLocalTime(&timeinfo)) {
      time_initialized = true;
      Serial.println("Time initialized successfully");
    }
  }
  Serial.println("WiFi connected.");

  // SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, LOW);
  digitalWrite(RELAY_2_PIN, LOW);

  pinMode(ADC_SOIL_MOISTURE_PIN, INPUT);
  analogReadResolution(12); // 12-bit ADC (0-4095)

  // Create sensor reading task on Core 0
  xTaskCreatePinnedToCore(
    sensorTask,       // Task function
    "SensorTask",     // Task name
    4096,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    NULL,             // Task handle
    0                 // Core 0
  );

  M5.Lcd.fillScreen(TFT_BLACK);

}


void loop() {

  M5.update();
  bool relay_1_on_past = false, relay_2_on_past = false;
  M5.Lcd.setTextColor(TFT_WHITE);

  // Update time
  getLocalTime(&timeinfo);

  // Auto control RELAY_1_PIN based on time (8:00-18:00)
  bool relay_1_auto_on = false;
  if (time_initialized) {
    int hour = timeinfo.tm_hour;
    if (hour >= 8 && hour < 18) {
      relay_1_auto_on = true;
    }
  }

  static bool prev_button_a = false;
  if (prev_button_a && !button_a) {
    Serial.println("Switched to AUTO mode - RELAY_1 follows schedule");
  } else if (!prev_button_a && button_a) {
    Serial.println("Switched to MANUAL mode");
  }
  prev_button_a = button_a;

  if (viz_data_updated)
  {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(7);
    M5.Lcd.setCursor(70, 80);
    M5.Lcd.fillRect(70, 80, 110, 50, TFT_BLACK);
    viz_data_updated = false;

// TODO: Replace with real sensor data
    switch (current_mode) {
      case MODE_TEMP:
        M5.Lcd.drawCircle(120, 120, 119, TFT_CYAN);
        M5.Lcd.printf("23.5");
        break;
      case MODE_HUMI:
        M5.Lcd.drawCircle(120, 120, 119, TFT_BLUE);
        M5.Lcd.printf("68.1");
        break;
      case MODE_SOIL_MOISTURE:
        M5.Lcd.drawCircle(120, 120, 119, TFT_GREEN);
        M5.Lcd.printf("%.1f", soil_moisture_percent);
        break;
      case MODE_CO2:
        M5.Lcd.drawCircle(120, 120, 119, TFT_ORANGE);
        M5.Lcd.printf("566");
        break;
    }
  }

  // Update soil moisture display continuously when in MODE_SOIL_MOISTURE
  if (current_mode == MODE_SOIL_MOISTURE) {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(7);
    M5.Lcd.setCursor(70, 80);
    M5.Lcd.fillRect(70, 80, 110, 50, TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.1f", soil_moisture_percent);
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextFont(0);
  M5.Lcd.setCursor(90, 30);
  M5.Lcd.fillRect(90, 30, 80, 50, TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  auto t = M5.Touch.getDetail();
  auto x = t.x;
  auto y = t.y;

  if (t.isPressed()) {
    if (x >= 25 && x <= 75 && y >= 155 && y <= 205) {
      button_a = !button_a;
    } else if (x >= 95 && x <= 145 && y >= 180 && y <= 230) {
      button_b = !button_b;
    } else if (x >= 165 && x <= 215 && y >= 155 && y <= 205) {
      button_c = !button_c;
    }
  } else {
    if (button_a) {
      // Manual mode
      digitalWrite(RELAY_1_PIN, button_b ? HIGH : LOW);
      digitalWrite(RELAY_2_PIN, button_c ? HIGH : LOW);
    } else {
      // Auto mode for RELAY_1, manual for RELAY_2
      digitalWrite(RELAY_1_PIN, relay_1_auto_on ? HIGH : LOW);
      digitalWrite(RELAY_2_PIN, relay_2_on_past ? HIGH : LOW);

      button_b = digitalRead(RELAY_1_PIN) == HIGH;
      button_c = digitalRead(RELAY_2_PIN) == HIGH;
    }
  }

  M5.Lcd.drawCircle(50, 180, 25, TFT_WHITE);   // a
  M5.Lcd.drawCircle(120, 205, 25, TFT_WHITE);  // b
  M5.Lcd.drawCircle(190, 180, 25, TFT_WHITE);  // c

  if (button_a) {
  M5.Lcd.fillCircle(50, 180, 24, M5.Lcd.color565(255, 215, 0));
  } else {
  M5.Lcd.fillCircle(50, 180, 24, TFT_BLACK);
  }

  if (digitalRead(RELAY_1_PIN) == HIGH) {
  M5.Lcd.fillCircle(120, 205, 24, M5.Lcd.color565(255, 165, 0));
  } else {
  M5.Lcd.fillCircle(120, 205, 24, TFT_BLACK);
  }
  if (digitalRead(RELAY_2_PIN) == HIGH) {
  M5.Lcd.fillCircle(190, 180, 24, M5.Lcd.color565(173, 216, 230));
  } else {
  M5.Lcd.fillCircle(190, 180, 24, TFT_BLACK);
  }

  long curr = M5Dial.Encoder.read();
  if (curr - prev_pos >= 4) {
    current_mode = static_cast<Mode>((current_mode + 1) % 4);
    prev_pos = curr;
    viz_data_updated = true;
  } else if (curr - prev_pos <= -4) {
    current_mode = static_cast<Mode>((current_mode + 3) % 4);
    prev_pos = curr;
    viz_data_updated = true;
  }

  
  // Padlock icon
  const uint16_t padlock_x = 35, padlock_y = 165;
  const uint16_t padlock_color = button_a ? TFT_BLACK : TFT_WHITE;
  const bool* padlock_icon = button_a ? padlock_unlocked_30x30 : padlock_locked_30x30;

  // Lamp icon
  const uint16_t lamp_x = 105, lamp_y = 190;
  const uint16_t lamp_color = (digitalRead(RELAY_1_PIN) == HIGH) ? TFT_BLACK : TFT_WHITE;

  // Faucet icon
  const uint16_t faucet_x = 175, faucet_y = 165;
  const uint16_t faucet_color = (digitalRead(RELAY_2_PIN) == HIGH) ? TFT_BLACK : TFT_WHITE;

  for (int i = 0; i < 900; ++i) {
    if (padlock_icon[i]) {
      M5.Lcd.drawPixel(padlock_x + (i % 30), padlock_y + (i / 30), padlock_color);
    }

    if (lamp_30x30[i]) {
      M5.Lcd.drawPixel(lamp_x + (i % 30), lamp_y + (i / 30), lamp_color);
    }

    if (faucet_30x30[i]) {
      M5.Lcd.drawPixel(faucet_x + (i % 30), faucet_y + (i / 30), faucet_color);
    }
  }

  delay(10);
}
