#pragma once

#include <stdint.h>

#define DHT_DEFAULT_TEMP10 -400
#define DHT_DEFAULT_HUM10 0
#define DHT_CHECKSUM_ERROR_HUM10 1

void dhtSimulatorInit();
void dhtSimulatorPublish(int16_t temp10, uint16_t hum10);
bool dhtSimulatorClaimReadWindow(uint32_t nowMs);
const volatile bool &dhtSimulatorCollision();
bool dhtSimulatorTakeCollision();
bool dhtSimulatorBusy();
