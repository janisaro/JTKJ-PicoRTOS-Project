/**
 * @file usbcomm.c
 * @brief USB serial communication with the workstation.
 *
 * Uses the stdio-over-USB serial port (printf / getchar), the same way as the
 * example examples/hello_serial_bidirectional_client. The workstation program
 * "Serial Client" decodes what we print.
 *
 * Debug text must be wrapped in "__" ... "__" so the Serial Client does not
 * try to decode it as Morse, e.g. printf("__debug text__\n").
 */

#include <stdio.h>

#include <pico/stdlib.h>

#include "project.h"
#include "usbcomm.h"

/** Write one character to the workstation. */
static void send_character(char character)
{
    printf("%c", character);
    fflush(stdout);           /* push it out now instead of waiting for a full buffer */
}

/** Finish a message: two spaces and a line feed, then report success to the UI. */
static void send_end_of_message(void)
{
    printf("  \n");
    fflush(stdout);

    /* TODO: if you can detect a failed write, do NOT report success. */
    ui_message_t message = { UI_EVENT_MESSAGE_SENT, 0 };
    xQueueSend(uiQueue, &message, 0);
}

/**
 * Read every character that is waiting on the USB serial port and push it
 * into rxQueue. getchar_timeout_us(0) returns immediately.
 */
static void receive_available_characters(void)
{
    int received;
    while ((received = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (received == '\r') {
            continue; /* ignore carriage return, wait for line feed */
        }
        char character = (char)received;
        xQueueSend(rxQueue, &character, 0);
    }
}

void usb_task(void *arg)
{
    (void)arg;

    char toSend;

    for (;;) {
        /* Wait up to 10 ms for something to send. */
        if (xQueueReceive(txQueue, &toSend, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (toSend == MORSE_CHAR_END) {
                send_end_of_message();
            } else {
                send_character(toSend);
            }
        }

        receive_available_characters();
    }
}