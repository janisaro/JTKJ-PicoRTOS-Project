/**
 * @file main.c
 * @brief Main program for the Morse code project.
 *
 * This file contains the main function that initializes the system, creates FreeRTOS tasks,
 * and starts the scheduler. It also sets up the GPIO interrupts for button presses.
 *

* Program structure (see project.h for the data flow diagram):
 *   imu_task            reads the IMU, works out the device position
 *   state_machine_task  builds the message from button presses + position,
 *                       and sends it over USB serial
 *   button_irq_handler  GPIO interrupt for SW1 and SW2
 */

#include <stdio.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#include "project.h"
#include "imu.h"
#include "state_machine.h"

/* Shared objects declared extern in project.h */
QueueHandle_t positionQueue;
QueueHandle_t buttonQueue;

#define POSITION_QUEUE_LENGTH 1    /* mailbox: only the newest value */
#define BUTTON_QUEUE_LENGTH   8

/** Create the two queues. Returns true if both were created. */
static bool create_queues(void)
{
    positionQueue = xQueueCreate(POSITION_QUEUE_LENGTH, sizeof(device_position_t));
    buttonQueue   = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(button_event_t));

    return positionQueue && buttonQueue;
}

int main(void)
{
    stdio_init_all();            /* printf over USB serial */
    init_hat_sdk();              /* I2C and other HAT basics */
    sleep_ms(300);               /* wait for USB and HAT initialisation */

    /* Create the queues BEFORE enabling the interrupts, because the
     * interrupt handler uses buttonQueue. */
    if (!create_queues()) {
        printf("__Creating queues failed__\n");
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

    if (result != pdPASS) {
        printf("__Task creation failed__\n");
        return 0;
    }

    vTaskStartScheduler();       /* never returns */

    return 0;
}