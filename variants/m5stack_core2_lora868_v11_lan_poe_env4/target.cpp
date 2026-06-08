#include <Arduino.h>
#include "target.h"

#ifdef DISPLAY_CLASS
static void showRadioInitStage(const char* msg) {
#if defined(SENSOR_TILE_DASHBOARD) && SENSOR_TILE_DASHBOARD
  (void)msg;
  return;
#else
  display.startFrame();
  display.setCursor(0, 0);
  display.print(msg);
  display.endFrame();
#endif
}
#endif

Core2Board board;

#if defined(P_LORA_SCLK)
  // Core2 routes LCD on VSPI pins (SCK/MOSI). Keep LoRa on VSPI too when sharing these pins.
  static SPIClass spi(VSPI);
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_0, P_LORA_RESET, P_LORA_DIO_1, spi);
#else
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_0, P_LORA_RESET, P_LORA_DIO_1);
#endif

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
EnvironmentSensorManager sensors;
#ifdef DISPLAY_CLASS
DISPLAY_CLASS display;
#endif

bool radio_init() {
#ifdef DISPLAY_CLASS
  showRadioInitStage("Init radio...\nRTC fallback");
#endif
  Serial.println("radio_init: fallback_clock.begin");
  fallback_clock.begin();

#ifdef DISPLAY_CLASS
  showRadioInitStage("Init radio...\nRTC core2");
#endif
  Serial.println("radio_init: rtc_clock.begin");
  rtc_clock.begin(Wire);

#if defined(P_LORA_RESET) && (P_LORA_RESET >= 0)
#ifdef DISPLAY_CLASS
  showRadioInitStage("Init radio...\nLoRa reset");
#endif
  Serial.println("radio_init: lora reset pulse");
  pinMode(P_LORA_RESET, OUTPUT);
  digitalWrite(P_LORA_RESET, LOW);
  delay(50);
  digitalWrite(P_LORA_RESET, HIGH);
  delay(250);
#endif

#if defined(P_LORA_SCLK)
#ifdef DISPLAY_CLASS
  showRadioInitStage("Init radio...\nLoRa SPI");
#endif
  Serial.println("radio_init: spi.begin");
  spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
#endif

#if defined(P_LORA_NSS) && (P_LORA_NSS >= 0)
  Serial.println("radio_init: nss high");
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  delay(10);
#endif

#ifdef DISPLAY_CLASS
  showRadioInitStage("Init radio...\nLoRa begin");
#endif

#ifdef LORA_CR
  constexpr uint8_t coding_rate = LORA_CR;
#else
  constexpr uint8_t coding_rate = 5;
#endif

  Serial.println("radio_init: radio.begin");
  int status = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, coding_rate, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER, 16);
  if (status != RADIOLIB_ERR_NONE) {
    Serial.print("ERROR: radio init failed: ");
    Serial.println(status);
    return false;
  }

#ifdef SX127X_CURRENT_LIMIT
  radio.setCurrentLimit(SX127X_CURRENT_LIMIT);
#endif

#if defined(SX176X_RXEN) || defined(SX176X_TXEN)
  #ifndef SX176X_RXEN
    #define SX176X_RXEN RADIOLIB_NC
  #endif
  #ifndef SX176X_TXEN
    #define SX176X_TXEN RADIOLIB_NC
  #endif
  radio.setRfSwitchPins(SX176X_RXEN, SX176X_TXEN);
#endif

  radio.setCRC(1);
  return true;
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
