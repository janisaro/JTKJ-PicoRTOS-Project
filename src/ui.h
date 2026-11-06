/**
 * @file ui.h
 * @brief Display, buzzer and LED output.
 */

#ifndef UI_H
#define UI_H

/**
 * @brief FreeRTOS task: shows the message being composed, the received message
 *        (as symbols and as letters) and the "Next:" symbol hint on the LCD,
 *        plays received symbols on the buzzer and LED, and gives the
 *        "message sent" feedback.
 * @param arg unused
 */
void ui_task(void *arg);

#endif /* UI_H */