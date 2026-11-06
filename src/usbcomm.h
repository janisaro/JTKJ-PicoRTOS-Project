/**
 * @file usbcomm.h
 * @brief USB serial communication with the workstation.
 */

#ifndef USB_COMM_H
#define USB_COMM_H

/**
 * @brief FreeRTOS task: sends characters from txQueue to the workstation and
 *        pushes characters received from the workstation into rxQueue.
 *
 * Wire format (assignment "Message protocol"):
 *   '.' and '-' symbols, ONE space between letters, TWO spaces between words,
 *   message ends with two spaces and a line feed ("  \n").
 *
 * @param arg unused
 */
void usb_task(void *arg);

#endif /* USB_COMM_H */