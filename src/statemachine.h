/**
 * @file statemachine.h
 * @brief Message composing state machine and button handling.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <pico/stdlib.h>

/**
 * @brief GPIO interrupt handler for both buttons (SW1, SW2).
 *
 * Runs in interrupt context: no printf, no blocking calls, only *FromISR
 * FreeRTOS functions. Registered in main.c.
 *
 * @param gpio      pin that triggered the interrupt
 * @param eventMask edge type (rising = pressed, falling = released)
 */
void button_irq_handler(uint gpio, uint32_t eventMask);

/**
 * @brief FreeRTOS task running the state machine.
 * @param arg unused
 */
void state_machine_task(void *arg);

#endif /* STATE_MACHINE_H */