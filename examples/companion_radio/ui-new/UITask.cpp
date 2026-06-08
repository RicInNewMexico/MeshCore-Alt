#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"

#ifndef CORE2_ENV_DASHBOARD
  #define CORE2_ENV_DASHBOARD 0
#endif

#if defined(ARDUINO_M5STACK_CORE2)
#include <M5Core2.h>
#include <SD.h>
#include "ProtoNerdFont.h"
#include "ProtoNerdValueFont.h"
#if CORE2_ENV_DASHBOARD
  #include <RTClib.h>
#endif
#ifndef CORE2_TOUCH_DEBUG_LOGGING
  #define CORE2_TOUCH_DEBUG_LOGGING 1
#endif
#if CORE2_TOUCH_DEBUG_LOGGING
  #define CORE2_TOUCH_TRACE(fmt, ...) MESH_DEBUG_PRINTLN("core2-touch: " fmt, ##__VA_ARGS__)
#else
  #define CORE2_TOUCH_TRACE(...) do {} while (0)
#endif
#ifdef RED
#undef RED
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef BLUE
#undef BLUE
#endif
#ifdef YELLOW
#undef YELLOW
#endif
#ifdef ORANGE
#undef ORANGE
#endif
#endif
#ifndef CORE2_TOUCH_TRACE
  #define CORE2_TOUCH_TRACE(...) do {} while (0)
#endif
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200
#define CORE2_MODE_SWITCH_LONG_PRESS_MILLIS 1000
#define CORE2_SWIPE_MIN_DX 50
#define CORE2_SWIPE_MAX_DY 80

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

#if defined(ARDUINO_M5STACK_CORE2)
#if CORE2_ENV_DASHBOARD
static void formatDashboardValue(char* out, size_t out_len, bool has_value, float value, const char* unit) {
  if (!has_value) {
    snprintf(out, out_len, "-- %s", unit);
    return;
  }
  if (value > 999.0f || value < -999.0f) {
    snprintf(out, out_len, "%.0f %s", value, unit);
  } else {
    snprintf(out, out_len, "%.1f %s", value, unit);
  }
  for (char* p = out; *p; ++p) {
    if (*p == ',') {
      *p = '.';
    }
  }
}

static void drawDashboardCardValue(M5Display& lcd, int x, int y, int w, int h, uint16_t card, const char* value) {
  const char* unit = strchr(value, ' ');
  constexpr size_t kMaxValueChars = 6;
  constexpr int kDashboardValueFontHeight = 29;
  const int value_top = y + 18;
  const int value_height = h - 28;
  const int unit_top = y + h - 20;
  const int value_center_x = x + (w / 2);
  const int value_right = x + w - 8;
  const int number_y = y + ((h - kDashboardValueFontHeight) / 2);

  lcd.fillRect(x + 2, value_top, w - 4, value_height, card);

  if (unit) {
    char number[20];
    size_t number_len = static_cast<size_t>(unit - value);
    if (number_len >= sizeof(number)) {
      number_len = sizeof(number) - 1;
    }
    memcpy(number, value, number_len);
    number[number_len] = 0;
    if (number_len > kMaxValueChars) {
      number[kMaxValueChars] = 0;
    }
    while (*unit == ' ') {
      ++unit;
    }

    const int number_w = proto_nerd_value_font::textWidth(number, 1);
    const int number_x = value_center_x - (number_w / 2);
    proto_nerd_value_font::drawTextTransparent(lcd, number_x, number_y, number, TFT_BLACK, 1);

    const int unit_w = proto_nerd_font::textWidth(unit, 1);
    const int unit_x = value_right - unit_w;
    proto_nerd_font::drawTextTransparent(lcd, unit_x, unit_top, unit, TFT_BLACK, 1);
  } else {
    char number[20];
    strncpy(number, value, sizeof(number) - 1);
    number[sizeof(number) - 1] = 0;
    if (strlen(number) > kMaxValueChars) {
      number[kMaxValueChars] = 0;
    }
    const int number_w = proto_nerd_value_font::textWidth(number, 1);
    const int number_x = value_center_x - (number_w / 2);
    proto_nerd_value_font::drawTextTransparent(lcd, number_x, number_y, number, TFT_BLACK, 1);
  }
}
#endif

static void formatStorageBytes(char* out, size_t out_len, uint64_t bytes) {
  if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
    snprintf(out, out_len, "%.2f GB", bytes / 1073741824.0);
  } else {
    snprintf(out, out_len, "%.0f MB", bytes / 1048576.0);
  }
}

#if CORE2_ENV_DASHBOARD
static bool readCsvLine(File& file, char* out, size_t out_len) {
  if (!file) {
    return false;
  }

  size_t pos = 0;
  bool saw_any = false;
  while (file.available()) {
    int c = file.read();
    if (c < 0) {
      break;
    }
    saw_any = true;
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      break;
    }
    if (pos + 1 < out_len) {
      out[pos++] = static_cast<char>(c);
    }
  }

  if (!saw_any && pos == 0) {
    return false;
  }

  out[pos] = 0;
  return true;
}

static bool trimSensorLogToLimit(const char* log_path, const char* temp_path) {
  File source = SD.open(log_path, FILE_READ);
  if (!source) {
    return false;
  }

  SD.remove(temp_path);
  File temp = SD.open(temp_path, FILE_WRITE);
  if (!temp) {
    source.close();
    return false;
  }

  char line[256];
  if (readCsvLine(source, line, sizeof(line))) {
    temp.println(line);
    bool skipped_oldest_entry = false;
    while (readCsvLine(source, line, sizeof(line))) {
      if (!skipped_oldest_entry) {
        skipped_oldest_entry = true;
        continue;
      }
      temp.println(line);
    }
  }

  temp.flush();
  temp.close();
  source.close();

  SD.remove(log_path);
  return SD.rename(temp_path, log_path);
}

static bool ensureSensorLogHeader(const char* log_path, const char* temp_path, const char* header) {
  File source = SD.open(log_path, FILE_READ);
  if (!source) {
    return false;
  }

  char first_line[256] = "";
  const bool has_first_line = readCsvLine(source, first_line, sizeof(first_line));
  if (has_first_line && strcmp(first_line, header) == 0) {
    source.close();
    return true;
  }

  SD.remove(temp_path);
  File temp = SD.open(temp_path, FILE_WRITE);
  if (!temp) {
    source.close();
    return false;
  }

  temp.println(header);
  if (has_first_line && first_line[0] != 0) {
    temp.println(first_line);
  }

  char line[256];
  while (readCsvLine(source, line, sizeof(line))) {
    temp.println(line);
  }

  temp.flush();
  temp.close();
  source.close();

  SD.remove(log_path);
  return SD.rename(temp_path, log_path);
}

static void formatSensorLogTimestamp(char* out, size_t out_len, uint32_t epoch) {
  DateTime dt(epoch);
  snprintf(out,
           out_len,
           "%02u-%02u-%04u %02u:%02u:%02u",
           static_cast<unsigned>(dt.month()),
           static_cast<unsigned>(dt.day()),
           static_cast<unsigned>(dt.year()),
           static_cast<unsigned>(dt.hour()),
           static_cast<unsigned>(dt.minute()),
           static_cast<unsigned>(dt.second()));
}

static bool appendSensorLogRow(SensorManager* sensors) {
  constexpr uint64_t kMaxSensorLogBytes = (4ULL * 1024ULL * 1024ULL * 1024ULL) - 1ULL;
  constexpr uint32_t kSensorLogIntervalMs = 15UL * 60UL * 1000UL;
  constexpr char kLogPath[] = "/meshcore/sensorlog.csv";
  constexpr char kTempPath[] = "/meshcore/sensorlog.tmp";
  constexpr char kCsvHeader[] = "timestamp,temp_c,humidity_rh,pressure_hpa,eco2_ppm,tvoc_ppb";

  static unsigned long next_log_ms = 0;
  static bool header_verified = false;
  const unsigned long now_ms = millis();
  if (next_log_ms != 0 && (int32_t)(now_ms - next_log_ms) < 0) {
    return false;
  }

  if (sensors == NULL || SD.cardType() == CARD_NONE) {
    next_log_ms = now_ms + kSensorLogIntervalMs;
    return false;
  }

  CayenneLPP telemetry(96);
  telemetry.reset();
  sensors->querySensors(TELEM_PERM_ENVIRONMENT, telemetry);

  EnvTelemetrySnapshot snap{};
  sensors->getTelemetrySnapshot(snap);
  if (!snap.has_temperature && !snap.has_humidity && !snap.has_pressure && !snap.has_eco2 && !snap.has_tvoc) {
    next_log_ms = now_ms + kSensorLogIntervalMs;
    return false;
  }

  File log_file = SD.open(kLogPath, FILE_APPEND);
  if (!log_file) {
    log_file = SD.open(kLogPath, FILE_WRITE);
  }
  if (!log_file) {
    next_log_ms = now_ms + kSensorLogIntervalMs;
    return false;
  }

  if ((uint64_t)log_file.size() >= kMaxSensorLogBytes) {
    log_file.close();
    if (!trimSensorLogToLimit(kLogPath, kTempPath)) {
      next_log_ms = now_ms + kSensorLogIntervalMs;
      return false;
    }
    log_file = SD.open(kLogPath, FILE_APPEND);
    if (!log_file) {
      next_log_ms = now_ms + kSensorLogIntervalMs;
      return false;
    }
  }

  if (!header_verified && log_file.size() > 0) {
    log_file.close();
    if (!ensureSensorLogHeader(kLogPath, kTempPath, kCsvHeader)) {
      next_log_ms = now_ms + kSensorLogIntervalMs;
      return false;
    }
    log_file = SD.open(kLogPath, FILE_APPEND);
    if (!log_file) {
      next_log_ms = now_ms + kSensorLogIntervalMs;
      return false;
    }
    header_verified = true;
  }

  if (log_file.size() == 0) {
    log_file.println(kCsvHeader);
    header_verified = true;
  }

  char buf[32];
  char timestamp[24];
  const uint32_t epoch = rtc_clock.getCurrentTime();
  formatSensorLogTimestamp(timestamp, sizeof(timestamp), epoch);
  log_file.print(timestamp);
  log_file.print(',');
  if (snap.has_temperature) {
    snprintf(buf, sizeof(buf), "%.2f", snap.temperature_c);
    log_file.print(buf);
  }
  log_file.print(',');
  if (snap.has_humidity) {
    snprintf(buf, sizeof(buf), "%.2f", snap.humidity_rh);
    log_file.print(buf);
  }
  log_file.print(',');
  if (snap.has_pressure) {
    snprintf(buf, sizeof(buf), "%.2f", snap.pressure_hpa);
    log_file.print(buf);
  }
  log_file.print(',');
  if (snap.has_eco2) {
    snprintf(buf, sizeof(buf), "%.0f", snap.eco2_ppm);
    log_file.print(buf);
  }
  log_file.print(',');
  if (snap.has_tvoc) {
    snprintf(buf, sizeof(buf), "%.0f", snap.tvoc_ppb);
    log_file.print(buf);
  }
  log_file.println();
  log_file.flush();
  log_file.close();

  next_log_ms = now_ms + kSensorLogIntervalMs;
  CORE2_TOUCH_TRACE("sensorlog appended epoch=%lu", static_cast<unsigned long>(epoch));
  return true;
}
#endif

static void drawCore2NerdText(int x, int y, const char* text, uint16_t color) {
  proto_nerd_font::drawTextTransparent(M5.Lcd, x, y, text, color, 1);
}

static void drawCore2NerdCentered(int mid_x, int y, const char* text, uint16_t color) {
  const int width = proto_nerd_font::textWidth(text, 1);
  drawCore2NerdText(mid_x - (width / 2), y, text, color);
}

static void drawCore2NerdRight(int right_x, int y, const char* text, uint16_t color) {
  const int width = proto_nerd_font::textWidth(text, 1);
  drawCore2NerdText(right_x - width, y, text, color);
}

static void drawCore2ValueCentered(int mid_x, int y, const char* text, uint16_t color) {
  const int width = proto_nerd_value_font::textWidth(text, 1);
  proto_nerd_value_font::drawTextTransparent(M5.Lcd, mid_x - (width / 2), y, text, color, 1);
}

static void drawCore2NerdEllipsized(int x, int y, int max_width, const char* text, uint16_t color) {
  char temp[128];
  StrHelper::strncpy(temp, text, sizeof(temp));
  if (proto_nerd_font::textWidth(temp, 1) <= max_width) {
    drawCore2NerdText(x, y, temp, color);
    return;
  }

  while (temp[0] != 0) {
    const size_t len = strlen(temp);
    temp[len - 1] = 0;
    char candidate[128];
    snprintf(candidate, sizeof(candidate), "%s...", temp);
    if (proto_nerd_font::textWidth(candidate, 1) <= max_width) {
      drawCore2NerdText(x, y, candidate, color);
      return;
    }
  }

  drawCore2NerdText(x, y, "...", color);
}

static int drawCore2NerdWrapped(int x, int y, int max_width, const char* text, uint16_t color, int line_height) {
  char remaining[192];
  StrHelper::strncpy(remaining, text, sizeof(remaining));
  int line_y = y;

  while (remaining[0] != 0) {
    char line[96] = "";
    char* cursor = remaining;
    char* last_space = nullptr;

    while (*cursor != 0) {
      const size_t candidate_len = static_cast<size_t>(cursor - remaining + 1);
      char candidate[96];
      if (candidate_len >= sizeof(candidate)) {
        break;
      }
      memcpy(candidate, remaining, candidate_len);
      candidate[candidate_len] = 0;
      if (proto_nerd_font::textWidth(candidate, 1) > max_width) {
        break;
      }
      if (*cursor == ' ') {
        last_space = cursor;
      }
      ++cursor;
    }

    if (*cursor == 0 && proto_nerd_font::textWidth(remaining, 1) <= max_width) {
      drawCore2NerdText(x, line_y, remaining, color);
      line_y += line_height;
      break;
    }

    if (last_space != nullptr) {
      const size_t line_len = static_cast<size_t>(last_space - remaining);
      memcpy(line, remaining, line_len);
      line[line_len] = 0;
      while (*last_space == ' ') {
        ++last_space;
      }
      memmove(remaining, last_space, strlen(last_space) + 1);
    } else {
      size_t take = 1;
      while (remaining[take] != 0) {
        char probe[96];
        memcpy(probe, remaining, take + 1);
        probe[take + 1] = 0;
        if (proto_nerd_font::textWidth(probe, 1) > max_width) {
          break;
        }
        ++take;
      }
      memcpy(line, remaining, take);
      line[take] = 0;
      memmove(remaining, remaining + take, strlen(remaining + take) + 1);
    }

    drawCore2NerdText(x, line_y, line, color);
    line_y += line_height;
  }

  return line_y;
}
#endif

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    uint16_t websiteWidth = display.getTextWidth(website);
    display.setCursor((display.width() - websiteWidth) / 2, 22);
    display.print(website);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 35, _version_info);

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 48, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    RADIO,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
#if defined(ARDUINO_M5STACK_CORE2)
  STORAGE,
#endif
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);

    // show muted icon if buzzer is muted
#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(DisplayDriver::RED);
      display.drawXbm(iconX - 9, iconY + 1, muted_icon, 8, 8);
    }
#endif
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _shutdown_init(false), sensors_lpp(200) {  }

  bool isFirstPanel() const { return _page == HomePage::FIRST; }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
#if defined(ARDUINO_M5STACK_CORE2)
    M5.Lcd.fillScreen(TFT_BLACK);

    char tmp_core[80];
    char filtered_name_core[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name_core, _node_prefs->node_name, sizeof(filtered_name_core));
    drawCore2NerdText(10, 8, filtered_name_core, TFT_GREEN);
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    int indicator_x = (display.width() / 2) - (9 * (HomePage::Count - 1));
    for (uint8_t i = 0; i < HomePage::Count; i++, indicator_x += 18) {
      M5.Lcd.fillRoundRect(indicator_x, 30, (i == _page) ? 12 : 6, 6, 3, (i == _page) ? TFT_CYAN : TFT_DARKGREY);
    }

    if (_page == HomePage::FIRST) {
      drawCore2NerdCentered(display.width() / 2, 56, "MESSAGES", TFT_YELLOW);
      snprintf(tmp_core, sizeof(tmp_core), "%d", _task->getMsgCount());
      drawCore2ValueCentered(display.width() / 2, 84, tmp_core, TFT_WHITE);

      if (_task->hasConnection()) {
        drawCore2NerdCentered(display.width() / 2, 142, "CONNECTED", TFT_GREEN);
      } else if (the_mesh.getBLEPin() != 0) {
        snprintf(tmp_core, sizeof(tmp_core), "PIN %d", the_mesh.getBLEPin());
        drawCore2NerdCentered(display.width() / 2, 142, tmp_core, TFT_RED);
      }

#ifdef WIFI_SSID
      IPAddress ip = WiFi.localIP();
      snprintf(tmp_core, sizeof(tmp_core), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      drawCore2NerdCentered(display.width() / 2, 172, tmp_core, TFT_WHITE);
#endif
      drawCore2NerdCentered(display.width() / 2, 202, "SWIPE FOR PANELS", TFT_DARKGREY);
      return 5000;
    }

    if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      drawCore2NerdCentered(display.width() / 2, 48, "RECENT NODES", TFT_YELLOW);
      int row_y = 78;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, row_y += 34) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) snprintf(tmp_core, sizeof(tmp_core), "%ds", secs);
        else if (secs < 3600) snprintf(tmp_core, sizeof(tmp_core), "%dm", secs / 60);
        else snprintf(tmp_core, sizeof(tmp_core), "%dh", secs / 3600);

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        drawCore2NerdEllipsized(12, row_y, display.width() - proto_nerd_font::textWidth(tmp_core, 1) - 28, filtered_recent_name, TFT_GREEN);
        drawCore2NerdRight(display.width() - 12, row_y, tmp_core, TFT_WHITE);
      }
      return 5000;
    }

    if (_page == HomePage::RADIO) {
      drawCore2NerdCentered(display.width() / 2, 48, "RADIO", TFT_YELLOW);
      snprintf(tmp_core, sizeof(tmp_core), "FREQ %.3f", _node_prefs->freq);
      drawCore2NerdText(16, 82, tmp_core, TFT_WHITE);
      snprintf(tmp_core, sizeof(tmp_core), "SF %d   BW %.2f", _node_prefs->sf, _node_prefs->bw);
      drawCore2NerdText(16, 114, tmp_core, TFT_WHITE);
      snprintf(tmp_core, sizeof(tmp_core), "CR %d   TX %ddBm", _node_prefs->cr, _node_prefs->tx_power_dbm);
      drawCore2NerdText(16, 146, tmp_core, TFT_WHITE);
      snprintf(tmp_core, sizeof(tmp_core), "NOISE FLOOR %d", radio_driver.getNoiseFloor());
      drawCore2NerdText(16, 178, tmp_core, TFT_WHITE);
      return 5000;
    }

    if (_page == HomePage::BLUETOOTH) {
      M5.Lcd.drawXBitmap((display.width() - 32) / 2, 70,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off,
          32, 32, TFT_GREEN);
      drawCore2NerdCentered(display.width() / 2, 48, "BLUETOOTH", TFT_YELLOW);
      drawCore2NerdCentered(display.width() / 2, 148, _task->isSerialEnabled() ? "ENABLED" : "DISABLED", TFT_WHITE);
      drawCore2NerdCentered(display.width() / 2, 198, "LONG PRESS TO TOGGLE", TFT_DARKGREY);
      return 5000;
    }

    if (_page == HomePage::ADVERT) {
      M5.Lcd.drawXBitmap((display.width() - 32) / 2, 70, advert_icon, 32, 32, TFT_GREEN);
      drawCore2NerdCentered(display.width() / 2, 48, "ADVERT", TFT_YELLOW);
      drawCore2NerdCentered(display.width() / 2, 148, "SEND NODE ADVERT", TFT_WHITE);
      drawCore2NerdCentered(display.width() / 2, 198, "LONG PRESS TO SEND", TFT_DARKGREY);
      return 5000;
    }
#if defined(ARDUINO_M5STACK_CORE2)
    if (_page == HomePage::STORAGE) {
      const bool mounted = (SD.cardType() != CARD_NONE) && (SD.totalBytes() > 0);
      const uint64_t total_bytes = mounted ? SD.totalBytes() : 0;
      const uint64_t used_bytes = mounted ? SD.usedBytes() : 0;
      char total_buf[24];
      char used_buf[24];
      formatStorageBytes(total_buf, sizeof(total_buf), total_bytes);
      formatStorageBytes(used_buf, sizeof(used_buf), used_bytes);

      M5.Lcd.drawXBitmap((display.width() - 32) / 2, 62, sdcard_icon, 32, 32, TFT_GREEN);
      drawCore2NerdCentered(display.width() / 2, 48, "STORAGE", TFT_YELLOW);
      drawCore2NerdText(16, 118, "STATE", TFT_GREEN);
      drawCore2NerdRight(display.width() - 16, 118, mounted ? "MOUNTED" : "NOT MOUNTED", mounted ? TFT_WHITE : TFT_RED);
      drawCore2NerdText(16, 150, "USED", TFT_GREEN);
      drawCore2NerdRight(display.width() - 16, 150, used_buf, TFT_WHITE);
      drawCore2NerdText(16, 182, "TOTAL", TFT_GREEN);
      drawCore2NerdRight(display.width() - 16, 182, total_buf, TFT_WHITE);
      return 5000;
    }
#endif
#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int row_y = 50;
      bool gps_state = _task->getGPSState();
      drawCore2NerdCentered(display.width() / 2, 20, "GPS", TFT_YELLOW);
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) strcpy(buf, gps_state ? "OFF (HW)" : "OFF (SW)");
      else strcpy(buf, gps_state ? "ON" : "OFF");
#else
      strcpy(buf, gps_state ? "ON" : "OFF");
#endif
      drawCore2NerdText(16, row_y, "STATE", TFT_GREEN);
      drawCore2NerdRight(display.width() - 16, row_y, buf, TFT_WHITE);
      row_y += 32;
      if (nmea == NULL) {
        drawCore2NerdCentered(display.width() / 2, row_y, "CAN'T ACCESS GPS", TFT_RED);
      } else {
        drawCore2NerdText(16, row_y, "FIX", TFT_GREEN);
        drawCore2NerdRight(display.width() - 16, row_y, nmea->isValid() ? "YES" : "NO", TFT_WHITE);
        row_y += 32;
        drawCore2NerdText(16, row_y, "SAT", TFT_GREEN);
        snprintf(buf, sizeof(buf), "%d", nmea->satellitesCount());
        drawCore2NerdRight(display.width() - 16, row_y, buf, TFT_WHITE);
        row_y += 32;
        drawCore2NerdText(16, row_y, "POS", TFT_GREEN);
        snprintf(buf, sizeof(buf), "%.4f %.4f", nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        drawCore2NerdRight(display.width() - 16, row_y, buf, TFT_WHITE);
        row_y += 32;
        drawCore2NerdText(16, row_y, "ALT", TFT_GREEN);
        snprintf(buf, sizeof(buf), "%.2f", nmea->getAltitude()/1000.);
        drawCore2NerdRight(display.width() - 16, row_y, buf, TFT_WHITE);
      }
      return 5000;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (_page == HomePage::SENSORS) {
      int y = 50;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());
      drawCore2NerdCentered(display.width() / 2, 20, "SENSORS", TFT_YELLOW);

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll ? UI_RECENT_LIST_SIZE : sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) {
          r.reset();
          r.readHeader(channel, type);
        }

        float v;
        switch (type) {
          case LPP_GPS: { float lat, lon, alt; r.readGPS(lat, lon, alt); strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon); break; }
          case LPP_VOLTAGE: r.readVoltage(v); strcpy(name, "voltage"); sprintf(buf, "%6.2f", v); break;
          case LPP_CURRENT: r.readCurrent(v); strcpy(name, "current"); sprintf(buf, "%.3f", v); break;
          case LPP_TEMPERATURE: r.readTemperature(v); strcpy(name, "temperature"); sprintf(buf, "%.2f", v); break;
          case LPP_RELATIVE_HUMIDITY: r.readRelativeHumidity(v); strcpy(name, "humidity"); sprintf(buf, "%.2f", v); break;
          case LPP_BAROMETRIC_PRESSURE: r.readPressure(v); strcpy(name, "pressure"); sprintf(buf, "%.2f", v); break;
          case LPP_ALTITUDE: r.readAltitude(v); strcpy(name, "altitude"); sprintf(buf, "%.0f", v); break;
          case LPP_POWER: r.readPower(v); strcpy(name, "power"); sprintf(buf, "%6.2f", v); break;
          default: r.skipData(type); strcpy(name, "unk"); sprintf(buf, "");
        }
        drawCore2NerdText(12, y, name, TFT_GREEN);
        drawCore2NerdRight(display.width() - 12, y, buf, TFT_WHITE);
        y += 28;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset + 1) % sensors_nb;
      else sensors_scroll_offset = 0;
      return 5000;
    }
#endif
    drawCore2NerdCentered(display.width() / 2, 48, "POWER", TFT_YELLOW);
    if (_shutdown_init) {
      drawCore2NerdCentered(display.width() / 2, 148, "HIBERNATING...", TFT_WHITE);
    } else {
      M5.Lcd.drawXBitmap((display.width() - 32) / 2, 70, power_icon, 32, 32, TFT_GREEN);
      drawCore2NerdCentered(display.width() / 2, 148, "ENTER HIBERNATE", TFT_WHITE);
      drawCore2NerdCentered(display.width() / 2, 198, "LONG PRESS TO CONFIRM", TFT_DARKGREY);
    }
    return 5000;
#endif

    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 20, tmp);

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 54, tmp);
      #endif
      if (_task->hasConnection()) {
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Connected >");

      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(DisplayDriver::RED);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off,
          32, 32);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, "toggle: " PRESS_LABEL);
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
#if defined(ARDUINO_M5STACK_CORE2)
    } else if (_page == HomePage::STORAGE) {
      const bool mounted = (SD.cardType() != CARD_NONE) && (SD.totalBytes() > 0);
      const uint64_t total_bytes = mounted ? SD.totalBytes() : 0;
      const uint64_t used_bytes = mounted ? SD.usedBytes() : 0;
      char total_buf[24];
      char used_buf[24];
      formatStorageBytes(total_buf, sizeof(total_buf), total_bytes);
      formatStorageBytes(used_buf, sizeof(used_buf), used_bytes);

      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18, sdcard_icon, 32, 32);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, mounted ? "mounted" : "not mounted");
      display.drawTextLeftAlign(0, 92, "used");
      display.drawTextRightAlign(display.width() - 1, 92, used_buf);
      display.drawTextLeftAlign(0, 104, "total");
      display.drawTextRightAlign(display.width() - 1, 104, total_buf);
#endif
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isSerialEnabled()) {  // toggle Bluetooth on/off
        _task->disableSerial();
      } else {
        _task->enableSerial();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[78];
  };
  #ifndef MAX_UNREAD_MSGS
    #define MAX_UNREAD_MSGS 32
  #endif
  int num_unread;
  int head = MAX_UNREAD_MSGS - 1; // index of latest unread message
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    head = (head + 1) % MAX_UNREAD_MSGS;
    if (num_unread < MAX_UNREAD_MSGS) num_unread++;

    auto p = &unread[head];
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
#if defined(ARDUINO_M5STACK_CORE2)
    M5.Lcd.fillScreen(TFT_BLACK);
    char tmp_core[16];
    drawCore2NerdText(10, 8, "UNREAD", TFT_GREEN);
    snprintf(tmp_core, sizeof(tmp_core), "%d", num_unread);
    drawCore2NerdRight(display.width() - 10, 8, tmp_core, TFT_GREEN);

    auto preview = &unread[head];
    int preview_secs = _rtc->getCurrentTime() - preview->timestamp;
    if (preview_secs < 60) {
      snprintf(tmp_core, sizeof(tmp_core), "%ds", preview_secs);
    } else if (preview_secs < 60*60) {
      snprintf(tmp_core, sizeof(tmp_core), "%dm", preview_secs / 60);
    } else {
      snprintf(tmp_core, sizeof(tmp_core), "%dh", preview_secs / (60*60));
    }
    drawCore2NerdRight(display.width() - 10, 30, tmp_core, TFT_DARKGREY);
    M5.Lcd.drawFastHLine(10, 54, display.width() - 20, TFT_DARKGREY);

    char filtered_origin_core[sizeof(preview->origin)];
    display.translateUTF8ToBlocks(filtered_origin_core, preview->origin, sizeof(filtered_origin_core));
    drawCore2NerdEllipsized(10, 70, display.width() - 20, filtered_origin_core, TFT_YELLOW);

    char filtered_msg_core[sizeof(preview->msg)];
    display.translateUTF8ToBlocks(filtered_msg_core, preview->msg, sizeof(filtered_msg_core));
    drawCore2NerdWrapped(10, 102, display.width() - 20, filtered_msg_core, TFT_WHITE, 22);

#if AUTO_OFF_MILLIS==0
    return 10000;
#else
    return 1000;
#endif
#endif

    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = &unread[head];

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    display.setCursor(0, 25);
    display.setColor(DisplayDriver::LIGHT);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
      num_unread--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      }
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // clear unread queue
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  setCurrScreen(splash);

#if defined(ARDUINO_M5STACK_CORE2) && CORE2_ENV_DASHBOARD
  resetCore2DashboardState();
#endif
}

#if defined(ARDUINO_M5STACK_CORE2) && CORE2_ENV_DASHBOARD
void UITask::resetCore2DashboardState() {
  _core2_dashboard_layout_drawn = false;
  _dash_prev_temp[0] = 0;
  _dash_prev_humi[0] = 0;
  _dash_prev_press[0] = 0;
  _dash_prev_eco2[0] = 0;
  _dash_prev_tvoc[0] = 0;
}

char UITask::handleCore2TouchToggle() {
  M5.update();
  char action = 0;
  const bool pressed = (M5.Touch.points > 0);
  const unsigned long now = millis();

  if (pressed) {
    _core2_touch_last_x = M5.Touch.point[0].x;
    _core2_touch_last_y = M5.Touch.point[0].y;
  }

  if (pressed && !_core2_touch_was_pressed) {
    if (_display != NULL && !_display->isOn()) {
      _display->turnOn();
      _auto_off = now + AUTO_OFF_MILLIS;
      _next_refresh = 0;
      CORE2_TOUCH_TRACE("wake-on-touch mode=%d", _core2_dashboard_mode ? 1 : 0);
    }
    _core2_touch_press_started_at = now;
    _core2_touch_longpress_fired = false;
    _core2_touch_start_x = _core2_touch_last_x;
    _core2_touch_start_y = _core2_touch_last_y;
    CORE2_TOUCH_TRACE("down x=%d y=%d points=%d display_on=%d mode=%d", _core2_touch_start_x, _core2_touch_start_y, M5.Touch.points, (_display != NULL && _display->isOn()) ? 1 : 0, _core2_dashboard_mode ? 1 : 0);
  }

  if (pressed && !_core2_touch_longpress_fired && now >= _core2_touch_debounce_until) {
    if ((uint32_t)(now - _core2_touch_press_started_at) >= CORE2_MODE_SWITCH_LONG_PRESS_MILLIS) {
      _core2_touch_longpress_fired = true;
      _core2_touch_debounce_until = now + 250;
      const bool can_switch_to_dashboard = (!_core2_dashboard_mode && curr == home && ((HomeScreen*)home)->isFirstPanel());

      if (_core2_dashboard_mode || can_switch_to_dashboard) {
        _core2_dashboard_mode = !_core2_dashboard_mode;
        if (_core2_dashboard_mode) {
          resetCore2DashboardState();
        }
        CORE2_TOUCH_TRACE("long-press toggle -> mode=%d", _core2_dashboard_mode ? 1 : 0);
      } else {
        // Preserve existing panel long-press behavior in MeshCore UI panels.
        action = KEY_ENTER;
        CORE2_TOUCH_TRACE("long-press passthrough panel-action");
      }

      _next_refresh = 0;
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
  }

  if (!pressed && _core2_touch_was_pressed) {
    if (!_core2_touch_longpress_fired && !_core2_dashboard_mode && curr == home) {
      const int dx = _core2_touch_last_x - _core2_touch_start_x;
      const int dy = _core2_touch_last_y - _core2_touch_start_y;
      if (abs(dx) >= CORE2_SWIPE_MIN_DX && abs(dy) <= CORE2_SWIPE_MAX_DY) {
        action = (dx < 0) ? KEY_RIGHT : KEY_LEFT;
        CORE2_TOUCH_TRACE("swipe dx=%d dy=%d action=%s", dx, dy, dx < 0 ? "right" : "left");
      }
      else {
        CORE2_TOUCH_TRACE("release dx=%d dy=%d no-swipe", dx, dy);
      }
    } else {
      CORE2_TOUCH_TRACE("release longpress=%d mode=%d", _core2_touch_longpress_fired ? 1 : 0, _core2_dashboard_mode ? 1 : 0);
    }
  }

  _core2_touch_was_pressed = pressed;
  return action;
}

bool UITask::renderCore2Dashboard() {
  if (_display == NULL || !_display->isOn() || _sensors == NULL) {
    return false;
  }

  // Keep snapshot-backed dashboard values fresh, but rate-limit the actual
  // I2C sensor reads to once every 5 seconds. Calling querySensors every
  // 750ms (render cycle) tears down and reinits Wire1 continuously which
  // degrades I2C reliability for the FT6336 touch controller over time.
  static CayenneLPP dashboard_probe_lpp(96);
  static unsigned long dashboard_probe_next_ms = 0;
  const unsigned long now_ms = millis();
  if ((uint32_t)(now_ms - dashboard_probe_next_ms) >= 5000UL || dashboard_probe_next_ms == 0) {
    CORE2_TOUCH_TRACE("dashboard sensor probe");
    dashboard_probe_lpp.reset();
    _sensors->querySensors(TELEM_PERM_ENVIRONMENT, dashboard_probe_lpp);
    dashboard_probe_next_ms = now_ms + 5000;
  }

  EnvTelemetrySnapshot snap{};
  _sensors->getTelemetrySnapshot(snap);

  char v_temp[20], v_humi[20], v_press[20], v_eco2[20], v_tvoc[20];
  const float temp_f = snap.has_temperature ? (snap.temperature_c * 9.0f / 5.0f + 32.0f) : 0.0f;
  formatDashboardValue(v_temp, sizeof(v_temp), snap.has_temperature, temp_f, "F");
  formatDashboardValue(v_humi, sizeof(v_humi), snap.has_humidity, snap.humidity_rh, "%");
  formatDashboardValue(v_press, sizeof(v_press), snap.has_pressure, snap.pressure_hpa, "mbar");
  formatDashboardValue(v_eco2, sizeof(v_eco2), snap.has_eco2, snap.eco2_ppm, "ppm");
  formatDashboardValue(v_tvoc, sizeof(v_tvoc), snap.has_tvoc, snap.tvoc_ppb, "ppb");

  auto drawCardFrame = [](int x, int y, int w, int h, uint16_t card, uint16_t border, const char* label) {
    M5.Lcd.fillRoundRect(x, y, w, h, 8, card);
    M5.Lcd.drawRoundRect(x, y, w, h, 8, border);
    // Clear only the inner label strip so the top border remains intact.
    M5.Lcd.fillRect(x + 6, y + 2, w - 12, 16, card);
    proto_nerd_font::drawTextTransparent(M5.Lcd, x + 8, y, label, TFT_BLACK, 1);
  };

  constexpr uint16_t kBg = 0xC618;
  constexpr uint16_t kHeader = 0x8410;
  constexpr uint16_t kCard = TFT_WHITE;
  constexpr uint16_t kBorder = 0x4208;

  const int margin = 8;
  const int gap = 8;
  const int cols = 2;
  const int rows = 3;
  const int header_h = 26;
  const int tile_w = (M5.Lcd.width() - (margin * 2) - gap) / cols;
  const int tile_h = (M5.Lcd.height() - header_h - (margin * 2) - (gap * (rows - 1))) / rows;
  const int bottom_tile_x = (M5.Lcd.width() - tile_w) / 2;

  const int row0 = header_h + gap;
  const int row1 = row0 + tile_h + gap;
  const int row2 = row1 + tile_h + gap;
  const int col0 = margin;
  const int col1 = margin + tile_w + gap;

  M5.Lcd.startWrite();
  if (!_core2_dashboard_layout_drawn) {
    M5.Lcd.fillScreen(kBg);
    M5.Lcd.fillRect(0, 0, M5.Lcd.width(), header_h, kHeader);
    M5.Lcd.drawFastHLine(0, header_h, M5.Lcd.width(), kBorder);
    const char* title = "MESHCORE SENSOR DASH";
    const int title_w = proto_nerd_font::textWidth(title, 1);
    proto_nerd_font::drawText(M5.Lcd, (M5.Lcd.width() - title_w) / 2, 4, title, TFT_WHITE, kHeader, 1);

    drawCardFrame(col0, row0, tile_w, tile_h, kCard, kBorder, "TEMP");
    drawCardFrame(col1, row0, tile_w, tile_h, kCard, kBorder, "HUM");
    drawCardFrame(col0, row1, tile_w, tile_h, kCard, kBorder, "PRES");
    drawCardFrame(col1, row1, tile_w, tile_h, kCard, kBorder, "eCO2");
    drawCardFrame(bottom_tile_x, row2, tile_w, tile_h, kCard, kBorder, "TVOC");
    _core2_dashboard_layout_drawn = true;
  }

  if (strcmp(_dash_prev_temp, v_temp) != 0) {
    drawDashboardCardValue(M5.Lcd, col0, row0, tile_w, tile_h, kCard, v_temp);
    strncpy(_dash_prev_temp, v_temp, sizeof(_dash_prev_temp) - 1);
    _dash_prev_temp[sizeof(_dash_prev_temp) - 1] = 0;
  }
  if (strcmp(_dash_prev_humi, v_humi) != 0) {
    drawDashboardCardValue(M5.Lcd, col1, row0, tile_w, tile_h, kCard, v_humi);
    strncpy(_dash_prev_humi, v_humi, sizeof(_dash_prev_humi) - 1);
    _dash_prev_humi[sizeof(_dash_prev_humi) - 1] = 0;
  }
  if (strcmp(_dash_prev_press, v_press) != 0) {
    drawDashboardCardValue(M5.Lcd, col0, row1, tile_w, tile_h, kCard, v_press);
    strncpy(_dash_prev_press, v_press, sizeof(_dash_prev_press) - 1);
    _dash_prev_press[sizeof(_dash_prev_press) - 1] = 0;
  }
  if (strcmp(_dash_prev_eco2, v_eco2) != 0) {
    drawDashboardCardValue(M5.Lcd, col1, row1, tile_w, tile_h, kCard, v_eco2);
    strncpy(_dash_prev_eco2, v_eco2, sizeof(_dash_prev_eco2) - 1);
    _dash_prev_eco2[sizeof(_dash_prev_eco2) - 1] = 0;
  }
  if (strcmp(_dash_prev_tvoc, v_tvoc) != 0) {
    drawDashboardCardValue(M5.Lcd, bottom_tile_x, row2, tile_w, tile_h, kCard, v_tvoc);
    strncpy(_dash_prev_tvoc, v_tvoc, sizeof(_dash_prev_tvoc) - 1);
    _dash_prev_tvoc[sizeof(_dash_prev_tvoc) - 1] = 0;
  }
  M5.Lcd.endWrite();
  return true;
}
#endif

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#if defined(ARDUINO_M5STACK_CORE2) && CORE2_ENV_DASHBOARD
  c = handleCore2TouchToggle();
#endif
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

#if defined(ARDUINO_M5STACK_CORE2) && CORE2_ENV_DASHBOARD
  appendSensorLogRow(_sensors);
#endif

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh) {
#if defined(ARDUINO_M5STACK_CORE2) && CORE2_ENV_DASHBOARD
      if (_core2_dashboard_mode) {
        renderCore2Dashboard();
        _next_refresh = millis() + 750;
      } else
#endif
      if (curr) {
        _display->startFrame();
        int delay_millis = curr->render(*_display);
        if (millis() < _alert_expiry) {  // render alert popup
          _display->setTextSize(1);
          int y = _display->height() / 3;
          int p = _display->height() / 32;
          _display->setColor(DisplayDriver::DARK);
          _display->fillRect(p, y, _display->width() - p*2, y);
          _display->setColor(DisplayDriver::LIGHT);  // draw box border
          _display->drawRect(p, y, _display->width() - p*2, y);
          _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
          _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
        } else {
          _next_refresh = millis() + delay_millis;
        }
        _display->endFrame();
      }
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if (millis() > _auto_off) {
      CORE2_TOUCH_TRACE("display auto-off");
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(DisplayDriver::RED);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
