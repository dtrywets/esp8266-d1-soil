#include "eeprom_store.h"

#include <EEPROM.h>

static constexpr uint16_t kEepromSize = 512;
static bool eepromReady = false;

void eepromStoreBegin() {
  if (eepromReady) {
    return;
  }
  EEPROM.begin(kEepromSize);
  eepromReady = true;
}
