#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include <vector>

#include "c_icon/icon_30x30/faucet.hpp"
#include "c_icon/icon_30x30/lamp.hpp"
#include "c_icon/icon_30x30/padlock.hpp"

#include "config.hpp"

#define SD_SPI_SCK_PIN 36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN 4

#define JST 3600 * 9

#define RELAY_1_PIN 1
#define RELAY_2_PIN 2

struct tm timeinfo;
bool button_a, button_b, button_c = false;
int last_ntp_update_day = -1;

void ntp_update_with_timeout() {
  WiFi.mode(WIFI_STA);
  configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo) && millis() - start < 10000) {
    delay(100);
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  WiFi.begin(ssid, password);
  Serial.begin(115200);
  uint32_t time_cnt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (millis() - time_cnt > 5000) {
      break;
    }
    M5.Lcd.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  }

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, LOW);
  digitalWrite(RELAY_2_PIN, LOW);
  M5.Lcd.fillScreen(TFT_BLACK);
}


void loop() {
  bool relay_1_on_past = false, relay_2_on_past = false;

  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(80, 30);
  M5.Lcd.fillRect(100, 10, 330, 50, TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min,
                timeinfo.tm_sec);

  M5.update();
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
      digitalWrite(RELAY_1_PIN, button_b ? HIGH : LOW);
      digitalWrite(RELAY_2_PIN, button_c ? HIGH : LOW);
    } else {
      digitalWrite(RELAY_1_PIN, relay_1_on_past ? HIGH : LOW);
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