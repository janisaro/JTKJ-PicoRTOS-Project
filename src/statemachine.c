/**
 * @file statemachine.c
 * @brief Message composing state machine and button handling.
 *
 * Buttons (both active-high, so pressed = rising edge, released = falling edge):
 *   SW1 press        add the symbol of the current device position
 *                    (flat = dot, tilted = dash)
 *   SW2 short press  add a space (one = between letters, two = between words)
 *   SW2 long press   send the message
 *
 * States:
 *
 *   IDLE        message buffer is empty
 *   COMPOSING   buffer has symbols; waiting for the next button event
 *   SW2_HELD    SW2 is down; on release the press length decides space or send
 *
 *   IDLE --SW1--> COMPOSING --SW1--> COMPOSING
 *   COMPOSING --SW2 pressed--> SW2_HELD
 *   SW2_HELD --released, short--> COMPOSING   (a space was added)
 *   SW2_HELD --released, long---> IDLE        (message was sent)
 *
 * The task sleeps until a button event arrives (blocking xQueueReceive), so it
 * uses no CPU while the user does nothing.
 */

#include <string.h>

#include <pico/stdlib.h>

#include "tkjhat/sdk.h"

#include "project.h"
#include "statemachine.h"

#define BUTTON_DEBOUNCE_MS  30     /* ignore edges closer than this on the same pin */
#define LONG_PRESS_MS       1000   /* SW2 held at least this long = send            */

typedef enum {
    STATE_IDLE = 1,
    STATE_COMPOSING,
    STATE_SW2_HELD
} program_state_t;

/* These variables are only used by the state machine task, so they are static
 * (not global). */
static program_state_t programState = STATE_IDLE;
static char messageBuffer[MORSE_BUFFER_SIZE];
static size_t messageLength = 0;
static TickType_t sw2PressedTick = 0;

/* --------------------------------------------------------------------- */
/* Interrupt handler                                                      */
/* --------------------------------------------------------------------- */

void button_irq_handler(uint gpio, uint32_t eventMask)
{
    /* Last accepted edge time of each button, for debouncing. */
    static TickType_t lastEdgeTick[2] = {0, 0};

    int index;
    if (gpio == BUTTON1) {
        index = 0;
    } else if (gpio == BUTTON2) {
        index = 1;
    } else {
        return;
    }

    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - lastEdgeTick[index]) < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        return;               /* contact bounce */
    }
    lastEdgeTick[index] = now;

    button_event_t event;
    if (gpio == BUTTON1) {
        if (!(eventMask & GPIO_IRQ_EDGE_RISE)) {
            return;           /* SW1: only the press matters */
        }
        event = BUTTON_EVENT_SW1_PRESSED;
    } else if (eventMask & GPIO_IRQ_EDGE_RISE) {
        event = BUTTON_EVENT_SW2_PRESSED;
    } else {
        event = BUTTON_EVENT_SW2_RELEASED;
    }

    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &event, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);
}

/* --------------------------------------------------------------------- */
/* Helpers                                                                */
/* --------------------------------------------------------------------- */

/** Tell the UI task something happened. */
static void notify_ui(ui_event_t event, char value)
{
    ui_message_t message = { event, value };
    xQueueSend(uiQueue, &message, 0);
}

/** Add a character to the message buffer. Returns false if it is full. */
static bool add_to_buffer(char character)
{
    if (messageLength >= MORSE_BUFFER_SIZE - 1) {
        return false;
    }
    messageBuffer[messageLength++] = character;
    return true;
}

/** Number of spaces at the end of the message. */
static size_t trailing_spaces(void)
{
    size_t count = 0;
    while (count < messageLength &&
           messageBuffer[messageLength - 1 - count] == MORSE_CHAR_SPACE) {
        count++;
    }
    return count;
}

/** SW1 was pressed: add the symbol of the current position. */
static void handle_confirm(void)
{
    device_position_t position = POSITION_UNKNOWN;
    if (xQueuePeek(positionQueue, &position, 0) != pdTRUE) {
        return;               /* IMU has not produced a position yet */
    }

    char symbol;
    if (position == POSITION_FLAT) {
        symbol = MORSE_CHAR_DOT;
    } else if (position == POSITION_TILTED) {
        symbol = MORSE_CHAR_DASH;
    } else {
        return;               /* between positions: ignore the press */
    }

    if (add_to_buffer(symbol)) {
        notify_ui(UI_EVENT_TX_SYMBOL, symbol);
        programState = STATE_COMPOSING;
    } else {
        notify_ui(UI_EVENT_BUFFER_FULL, 0);
    }
}

/** SW2 short press: add a space (never first, never more than two in a row). */
static void handle_space(void)
{
    if (messageLength > 0 && trailing_spaces() < 2) {
        if (add_to_buffer(MORSE_CHAR_SPACE)) {
            notify_ui(UI_EVENT_TX_SYMBOL, MORSE_CHAR_SPACE);
        }
    }
    programState = STATE_COMPOSING;
}

/**
 * SW2 long press: hand the message to the USB task.
 * Trailing spaces are removed, because usb_task adds the two closing spaces.
 */
static void handle_send(void)
{
    while (messageLength > 0 &&
           messageBuffer[messageLength - 1] == MORSE_CHAR_SPACE) {
        messageLength--;
    }

    if (messageLength == 0) {
        programState = STATE_IDLE;   /* nothing to send */
        return;
    }

    for (size_t i = 0; i < messageLength; i++) {
        xQueueSend(txQueue, &messageBuffer[i], pdMS_TO_TICKS(50));
    }
    char endMarker = MORSE_CHAR_END;
    xQueueSend(txQueue, &endMarker, pdMS_TO_TICKS(50));

    messageLength = 0;
    notify_ui(UI_EVENT_TX_SENDING, 0);
    /* "Message sent" feedback comes from usb_task AFTER the write is done. */
    programState = STATE_IDLE;
}

/** The state machine: what each state does with each button event. */
static void handle_button(button_event_t event)
{
    switch (programState) {
    case STATE_IDLE:
    case STATE_COMPOSING:
        if (event == BUTTON_EVENT_SW1_PRESSED) {
            handle_confirm();
        } else if (event == BUTTON_EVENT_SW2_PRESSED && programState == STATE_COMPOSING) {
            sw2PressedTick = xTaskGetTickCount();
            programState = STATE_SW2_HELD;
        }
        break;

    case STATE_SW2_HELD:
        if (event == BUTTON_EVENT_SW2_RELEASED) {
            TickType_t held = xTaskGetTickCount() - sw2PressedTick;
            if (held >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                handle_send();
            } else {
                handle_space();
            }
        }
        /* SW1 while SW2 is held: ignored */
        break;
    }
}

/* --------------------------------------------------------------------- */
/* Task                                                                   */
/* --------------------------------------------------------------------- */

void state_machine_task(void *arg)
{
    (void)arg;

    button_event_t event;

    for (;;) {
        /* Sleep until a button event arrives. */
        if (xQueueReceive(buttonQueue, &event, portMAX_DELAY) == pdTRUE) {
            handle_button(event);
        }
    }
}