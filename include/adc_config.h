#pragma once

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define ADC_MAX_VALUE 4095
#ifndef DEFAULT_DRY_ADC
#define DEFAULT_DRY_ADC 3200
#endif
#ifndef DEFAULT_WET_ADC
#define DEFAULT_WET_ADC 1200
#endif
#else
#define ADC_MAX_VALUE 1023
#ifndef DEFAULT_DRY_ADC
#define DEFAULT_DRY_ADC 800
#endif
#ifndef DEFAULT_WET_ADC
#define DEFAULT_WET_ADC 300
#endif
#endif
