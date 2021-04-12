#include "ticker.h"

volatile uint32_t TickCount;

void SysTick_Handler() { TickCount += TICK_COUNT_UPDATE_MS; }
