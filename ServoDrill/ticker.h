#pragma once
#include "mbed.h"

extern volatile uint32_t TickCount;

static constexpr unsigned TICK_COUNT_UPDATE_MS = 10;

inline void stopTickCounter() {
  SysTick->CTRL =
      (1 << SysTick_CTRL_CLKSOURCE_Pos) | (0 << SysTick_CTRL_ENABLE_Pos) |
      (0 << SysTick_CTRL_TICKINT_Pos); /* Disable SysTick IRQ and SysTick Timer */
}

inline void resetTickCounter() {
  SysTick->CTRL =
      (1 << SysTick_CTRL_CLKSOURCE_Pos) | (0 << SysTick_CTRL_ENABLE_Pos) |
      (0 << SysTick_CTRL_TICKINT_Pos); /* Disable SysTick IRQ and SysTick Timer */
  TickCount = 0;
  SysTick->CTRL =
      (1 << SysTick_CTRL_CLKSOURCE_Pos) | (1 << SysTick_CTRL_ENABLE_Pos) |
      (1 << SysTick_CTRL_TICKINT_Pos); /* Enable SysTick IRQ and SysTick Timer */
}

inline void initializeTickCounter() {
  stopTickCounter();
  SysTick->LOAD = SystemCoreClock * TICK_COUNT_UPDATE_MS / 1000;
  resetTickCounter();
}

inline uint32_t milliseconds() { return TickCount; }

extern "C" void SysTick_Handler();
