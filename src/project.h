/**
 * @file project.h
 * @brief Shared types, constants and queue handles for the Morse messenger.
 *
 * ALL team members include this file. Agree on changes here together,
 * because every module depends on these definitions.
 *
 * How the user makes a message:
 *   - Put the device in a position (e.g. flat = dot, tilted = dash).
 *   - SW1 confirms: the symbol of the current position is added to the message.
 *   - SW2 short press adds a space, SW2 long press sends the message.
 *
 * Data flow (arrows are FreeRTOS queues):
 *
 *   IMU --> [imu_task] --positionQueue (mailbox, newest value)--> [state_machine_task]
 *   SW1/SW2 --> [button_irq_handler] --buttonQueue--> [state_machine_task]
 *   [state_machine_task] --txQueue--> [usb_task] --> PC (Serial Client)
 *   PC --> [usb_task] --rxQueue--> [ui_task] --> display + buzzer + LED
 *   [state_machine_task], [usb_task] --uiQueue--> [ui_task]
 *   [ui_task] also reads positionQueue to show the "Next:" symbol hint.
 */

#ifndef PROJECT_H
#define PROJECT_H

#include <stdint.h>
#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
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
    POSITION_FLAT,          /**< e.g. lying flat on the table -> dot      */
    POSITION_TILTED         /**< e.g. standing on its edge    -> dash     */
} device_position_t;

/** Event produced by the button interrupt handler. */
typedef enum {
    BUTTON_EVENT_SW1_PRESSED = 1,   /**< confirm the symbol of the current position */
    BUTTON_EVENT_SW2_PRESSED,       /**< start of a SW2 press (short = space, long = send) */
    BUTTON_EVENT_SW2_RELEASED       /**< end of a SW2 press */
} button_event_t;

/** What the UI task should do. */
typedef enum {
    UI_EVENT_TX_SYMBOL = 1,   /**< a symbol or space was added to the message (value = char) */
    UI_EVENT_TX_SENDING,      /**< the message was handed over to the USB task */
    UI_EVENT_MESSAGE_SENT,    /**< the USB task wrote the message successfully */
    UI_EVENT_BUFFER_FULL      /**< message buffer is full, symbol was ignored */
} ui_event_t;

/** Item of the uiQueue. */
typedef struct {
    ui_event_t event;
    char value;               /**< used by UI_EVENT_TX_SYMBOL, otherwise 0 */
} ui_message_t;

/* ------------------------------------------------------------------ */
/* Characters of the Morse protocol (see assignment "Message protocol") */
/* ------------------------------------------------------------------ */
#define MORSE_CHAR_DOT        '.'
#define MORSE_CHAR_DASH       '-'
#define MORSE_CHAR_SPACE      ' '
#define MORSE_CHAR_END        '\n'   /* put into txQueue to mean "end of message".   */
                                     /* usb_task then sends "  \n" on the wire.      */

/* ------------------------------------------------------------------ */
/* Shared FreeRTOS objects. Created in main.c, used everywhere.        */
/* ------------------------------------------------------------------ */
extern QueueHandle_t positionQueue;  /**< length 1 "mailbox": newest device_position_t */
extern QueueHandle_t buttonQueue;    /**< button_event_t, button ISR -> state machine */
extern QueueHandle_t txQueue;        /**< char, state machine -> usb_task */
extern QueueHandle_t rxQueue;        /**< char, usb_task -> ui_task */
extern QueueHandle_t uiQueue;        /**< ui_message_t, state machine / usb_task -> ui_task */

/**
 * The IMU and the display are on the SAME I2C bus. Two tasks must never use it
 * at the same moment, so every call that touches I2C (IMU read, display write)
 * must be wrapped in xSemaphoreTake(i2cMutex, ...) / xSemaphoreGive(i2cMutex).
 */
extern SemaphoreHandle_t i2cMutex;

#endif /* PROJECT_H */