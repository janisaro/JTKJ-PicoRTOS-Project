/**
 * @file ui.c
 * @brief Display, buzzer and LED output.
 *
 * Tier 2 requirements covered here:
 *   - received symbols are shown on the LCD AND played with the buzzer + LED
 *   - feedback when a message was sent successfully
 * Extra: received Morse is also translated to letters (morse.c).
 *
 * Display rows (the pixel positions are guesses, adjust to your screen/font):
 *   status text
 *   TX <last symbols of the message being composed>
 *   RX <received message as letters>
 *      <symbols of the letter being received>
 *   Next: <symbol that SW1 would add now>
 */

#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include "tkjhat/sdk.h"

#include "project.h"
#include "morse.h"
#include "ui.h"

#define TEXT_WIDTH         12     /* characters shown per row after the 3-char label (TODO check font) */
#define LETTER_BUFFER_SIZE 8      /* longest Morse letter is 5 symbols */
#define DOT_BEEP_MS        100
#define DASH_BEEP_MS       300
#define BEEP_FREQUENCY_HZ  1000
#define SENT_BEEP_HZ       2000
#define SENT_BEEP_MS       200

/* Text kept for each display row. */
static char txLine[TEXT_WIDTH + 1] = "";
static char rxText[TEXT_WIDTH + 1] = "";
static char rxLetter[LETTER_BUFFER_SIZE] = "";
static size_t rxLetterLength = 0;
static bool rxLastWasSpace = false;
static bool rxMessageEnded = false;     /* next received symbol starts a new message */
static char statusText[TEXT_WIDTH + 4] = "Ready";
static char hintSymbol = '?';

/** Append a character to a row, dropping the oldest one when it is full. */
static void append_to_line(char *line, char character)
{
    size_t length = strlen(line);
    if (length >= TEXT_WIDTH) {
        memmove(line, line + 1, length);   /* shift left, drop oldest */
        length--;
    }
    line[length] = character;
    line[length + 1] = '\0';
}

/** Set the status row text. */
static void set_status(const char *text)
{
    snprintf(statusText, sizeof(statusText), "%s", text);
}

/** Redraw the whole display. The display is on the shared I2C bus. */
static void redraw_display(void)
{
    char row[24];

    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    clear_display();
    write_text_xy(0, 0, statusText);
    snprintf(row, sizeof(row), "TX %s", txLine);
    write_text_xy(0, 14, row);
    snprintf(row, sizeof(row), "RX %s", rxText);
    write_text_xy(0, 28, row);
    snprintf(row, sizeof(row), "   %s", rxLetter);
    write_text_xy(0, 40, row);
    snprintf(row, sizeof(row), "Next: %c", hintSymbol);
    write_text_xy(0, 54, row);
    xSemaphoreGive(i2cMutex);
}

/** Which symbol would SW1 add in this position? */
static char hint_for(device_position_t position)
{
    if (position == POSITION_FLAT) {
        return MORSE_CHAR_DOT;
    }
    if (position == POSITION_TILTED) {
        return MORSE_CHAR_DASH;
    }
    return '?';
}

/** Translate the letter being received and add it to the received text. */
static void flush_received_letter(void)
{
    if (rxLetterLength > 0) {
        rxLetter[rxLetterLength] = '\0';
        append_to_line(rxText, morse_to_char(rxLetter));
        rxLetterLength = 0;
        rxLetter[0] = '\0';
    }
}

/**
 * Handle one character received from the workstation.
 * @return true if it was a dot or dash that should be played on the buzzer
 */
static bool handle_received(char character)
{
    if (character == MORSE_CHAR_END) {
        flush_received_letter();
        size_t length = strlen(rxText);
        if (length > 0 && rxText[length - 1] == ' ') {
            rxText[length - 1] = '\0';               /* drop the closing word gap */
        }
        rxMessageEnded = true;
        rxLastWasSpace = false;
        set_status("Msg received");
        return false;
    }

    if (character == MORSE_CHAR_SPACE) {
        if (rxLetterLength > 0) {
            flush_received_letter();                 /* one space: letter gap */
        } else if (rxLastWasSpace && rxText[0] != '\0') {
            if (rxText[strlen(rxText) - 1] != ' ') { /* two spaces: word gap */
                append_to_line(rxText, ' ');
            }
        }
        rxLastWasSpace = true;
        return false;
    }

    if (character == MORSE_CHAR_DOT || character == MORSE_CHAR_DASH) {
        if (rxMessageEnded) {          /* first symbol of a new message */
            rxText[0] = '\0';
            rxMessageEnded = false;
        }
        if (rxLetterLength < LETTER_BUFFER_SIZE - 1) {
            rxLetter[rxLetterLength++] = character;
            rxLetter[rxLetterLength] = '\0';
        }
        rxLastWasSpace = false;
        set_status("Receiving");
        return true;
    }

    return false;   /* ignore anything else */
}

/** Play a received symbol on the buzzer and the LED. */
static void play_symbol(char symbol)
{
    set_led_status(true);
    buzzer_play_tone(BEEP_FREQUENCY_HZ,
                     (symbol == MORSE_CHAR_DOT) ? DOT_BEEP_MS : DASH_BEEP_MS);
    set_led_status(false);
}

void ui_task(void *arg)
{
    (void)arg;

    /* Initialise the actuators here, after the scheduler has started. */
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    init_display();
    xSemaphoreGive(i2cMutex);
    init_buzzer();
    init_led();

    redraw_display();

    for (;;) {
        bool needRedraw = false;
        bool playSent = false;
        char symbolToPlay = 0;

        /* The "Next:" hint follows the device position. */
        device_position_t position;
        char newHint = '?';
        if (xQueuePeek(positionQueue, &position, 0) == pdTRUE) {
            newHint = hint_for(position);
        }
        if (newHint != hintSymbol) {
            hintSymbol = newHint;
            needRedraw = true;
        }

        /* Events from the state machine and the USB task. */
        ui_message_t message;
        if (xQueueReceive(uiQueue, &message, 0) == pdTRUE) {
            switch (message.event) {
            case UI_EVENT_TX_SYMBOL:
                append_to_line(txLine, message.value);
                set_status("Composing");
                break;
            case UI_EVENT_TX_SENDING:
                txLine[0] = '\0';
                set_status("Sending...");
                break;
            case UI_EVENT_MESSAGE_SENT:
                set_status("Message sent!");
                playSent = true;
                break;
            case UI_EVENT_BUFFER_FULL:
                set_status("Buffer full");
                break;
            }
            needRedraw = true;
        }

        /* Received from the workstation: show it and play it. */
        char received;
        if (xQueueReceive(rxQueue, &received, 0) == pdTRUE) {
            if (handle_received(received)) {
                symbolToPlay = received;
            }
            needRedraw = true;
        }

        if (needRedraw) {
            redraw_display();
        }
        if (symbolToPlay != 0) {
            play_symbol(symbolToPlay);
        }
        if (playSent) {
            buzzer_play_tone(SENT_BEEP_HZ, SENT_BEEP_MS);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}