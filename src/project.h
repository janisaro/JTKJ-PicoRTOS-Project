/**
 * @file project.h
 * @brief Header file for the Morse code project.
 *
 * This file contains the definitions and declarations used in the Morse code project.
 * It defines the message types, characters of the Morse protocol, and shared FreeRTOS objects.
 *
* How the user makes a message:
 *   - Put the device in a position (flat = dot, tilted = dash).
 *   - SW1 confirms: the symbol of the current position is added to the message.
 *   - SW2 short press adds a space, SW2 long press sends the message.
 *
 * Data flow (arrows are FreeRTOS queues):
 *
 *   IMU --> [imu_task] --positionQueue (mailbox, newest value)--> [state_machine_task]
 *   SW1/SW2 --> [button_irq_handler] --buttonQueue--> [state_machine_task]
 *   [state_machine_task] --printf over USB serial--> PC (Serial Client)
 */

#ifndef PROJECT_H
#define PROJECT_H

#include <stdint.h>
#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

/* Stack size (in words) used for all tasks. Can be lowered to 1024 if a task is small. */
#define DEFAULT_STACK_SIZE 2048

/* Size of the buffer where the message symbols are kept before sending. */
#define MORSE_BUFFER_SIZE 100

/* ------------------------------------------------------------------ */
/* Message types passed between tasks                                  */
/* ------------------------------------------------------------------ */

/** Position of the device, produced by the IMU task. */
typedef enum {
    POSITION_UNKNOWN = 0,   /**< in between positions: SW1 does nothing   */
    POSITION_FLAT,          /**< lying flat on the table      -> dot      */
    POSITION_TILTED         /**< standing on its edge         -> dash     */
} device_position_t;

/** Event produced by the button interrupt handler. */
typedef enum {
    BUTTON_EVENT_SW1_PRESSED = 1,   /**< confirm the symbol of the current position */
    BUTTON_EVENT_SW2_PRESSED,       /**< start of a SW2 press (short = space, long = send) */
    BUTTON_EVENT_SW2_RELEASED       /**< end of a SW2 press */
} button_event_t;

/* ------------------------------------------------------------------ */
/* Characters of the Morse protocol (see assignment "Message protocol") */
/* ------------------------------------------------------------------ */
#define MORSE_CHAR_DOT        '.'
#define MORSE_CHAR_DASH       '-'
#define MORSE_CHAR_SPACE      ' '

/* ------------------------------------------------------------------ */
/* Shared FreeRTOS objects. Created in main.c, used in other files.    */
/* ------------------------------------------------------------------ */
extern QueueHandle_t positionQueue;  /**< length 1 "mailbox": newest device_position_t */
extern QueueHandle_t buttonQueue;    /**< button_event_t, button ISR -> state machine */

#endif /* PROJECT_H */