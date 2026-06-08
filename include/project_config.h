#pragma once

#if defined(CONFIG_BOARD_ESP32C3)
#if __has_include("config.esp32c3.h")
#include "config.esp32c3.h"
#else
#include "config.esp32c3.defaults.h"
#endif
#ifndef AP_SSID_PREFIX
#define AP_SSID_PREFIX "C3Soil-"
#endif
#elif __has_include("config.h")
#include "config.h"
#ifndef AP_SSID_PREFIX
#define AP_SSID_PREFIX "D1Soil-"
#endif
#else
#include "config.example.h"
#ifndef AP_SSID_PREFIX
#define AP_SSID_PREFIX "D1Soil-"
#endif
#endif

#include "adc_config.h"
