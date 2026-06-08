#pragma once

#include <Arduino.h>

void improvWifiBegin();
void improvWifiLoop();

/** True während Improv-Wartezeit nach Erstflash (kein AP, kein Serial-Log). */
bool improvWifiShouldDeferNetwork();

/** Serial-Ausgaben unterdrücken, damit Improv-Pakete nicht zerstört werden. */
bool improvWifiSerialQuiet();
