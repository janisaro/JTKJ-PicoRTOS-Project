/**
 * @file main.c
 * @brief Morse code messenger on Raspberry Pi Pico W + JTKJ HAT (FreeRTOS).
 *
 * Program structure (see project.h for the data flow diagram):
 *   imu_task            reads the IMU, works out the device position
 *   state_machine_task  builds the message from button presses + position
 *   usb_task            sends/receives Morse text over USB serial
 *   ui_task             display, buzzer and LED output
 *   button_irq_handler  GPIO interrupt for SW1 and SW2
 */

#include <stdio.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "tkjhat/sdk.h"

#include "project.h"
#include "imu.h"
#include "statemachine.h"
#include "ui.h"
#include "usbcomm.h"

/* Shared objects declared extern in project.h */
QueueHandle_t positionQueue;
QueueHandle_t buttonQueue;
QueueHandle_t txQueue;
QueueHandle_t rxQueue;
QueueHandle_t uiQueue;
SemaphoreHandle_t i2cMutex;

#define POSITION_QUEUE_LENGTH 1                    /* mailbox: only the newest value */
#define BUTTON_QUEUE_LENGTH   8
#define TX_QUEUE_LENGTH       (MORSE_BUFFER_SIZE + 8)  /* a whole message fits */
#define RX_QUEUE_LENGTH       64
#define UI_QUEUE_LENGTH       16

/** Create all queues and the mutex. Returns true if everything was created. */
static bool create_shared_objects(void)
{
    positionQueue = xQueueCreate(POSITION_QUEUE_LENGTH, sizeof(device_position_t));
    buttonQueue   = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(button_event_t));
    txQueue       = xQueueCreate(TX_QUEUE_LENGTH, sizeof(char));
    rxQueue       = xQueueCreate(RX_QUEUE_LENGTH, sizeof(char));
    uiQueue       = xQueueCreate(UI_QUEUE_LENGTH, sizeof(ui_message_t));
    i2cMutex      = xSemaphoreCreateMutex();

    return positionQueue && buttonQueue && txQueue && rxQueue && uiQueue && i2cMutex;
}

int main(void)
{
    stdio_init_all();            /* printf / getchar over USB serial */
    init_hat_sdk();              /* I2C and other HAT basics */
    sleep_ms(300);               /* wait for USB and HAT initialisation */

    /* Create the queues BEFORE enabling the interrupts, because the
     * interrupt handler uses buttonQueue. */
    if (!create_shared_objects()) {
        printf("__Creating queues or mutex failed__\n");
        return 0;
    }

    /* Buttons are active-high: a press is a RISING edge, a release is FALLING.
     * SW1 only needs the press. SW2 needs both, to measure how long it is held. */
    init_button1();
    init_button2();
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_irq_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    BaseType_t result = pdPASS;
    result &= xTaskCreate(imu_task,           "imu",   DEFAULT_STACK_SIZE, NULL, 2, NULL);
    result &= xTaskCreate(state_machine_task, "state", DEFAULT_STACK_SIZE, NULL, 2, NULL);
    result &= xTaskCreate(usb_task,           "usb",   DEFAULT_STACK_SIZE, NULL, 2, NULL);
    result &= xTaskCreate(ui_task,            "ui",    DEFAULT_STACK_SIZE, NULL, 1, NULL);

    if (result != pdPASS) {
        printf("__Task creation failed__\n");
        return 0;
    }

    vTaskStartScheduler();       /* never returns */

    return 0;
}