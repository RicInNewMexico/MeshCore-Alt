#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/ESP32Board.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1276Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#ifdef DISPLAY_CLASS
#include <helpers/ui/DisplayDriver.h>
#include "Core2M5Display.h"
#endif
#include <M5Core2.h>

class Core2Board : public ESP32Board {
public:
  uint16_t getBattMilliVolts() override {
    float v = M5.Axp.GetBatVoltage();
    if (v < 0.1f) return 0;
    return (uint16_t)(v * 1000.0f);
  }

  bool isExternalPowered() override {
    return M5.Axp.isCharging() || M5.Axp.GetVBusVoltage() > 4.0f;
  }
};

extern Core2Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;
#ifdef DISPLAY_CLASS
extern DISPLAY_CLASS display;
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
