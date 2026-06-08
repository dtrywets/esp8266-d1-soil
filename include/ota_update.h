#pragma once

#include <stdint.h>

void otaUpdateOnWifiConnected();
void otaUpdateLoop();

bool otaUpdateIsActive();
uint16_t otaUpdatePort();
