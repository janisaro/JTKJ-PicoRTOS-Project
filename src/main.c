// Käynnistää järjestelmän: alustaa hatin, luo jonon buttonQueue ja kaksi tehtävää
// imu_task ja state_machine_task), ottaa painikekeskeytykset käyttöön ja käynnistää ajastimen.

#include <stdio.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include "tkjhat/sdk.h"
#include "project.h"

// jonot määritelty project.h:ssä
QueueHandle_t buttonQueue;

#define BUTTON_QUEUE_LENGTH   8    // tilaa useammalle painiketapahtumalle

int main(void)
{   // alustetaan printf USB portille ja JTKJ Hat
    stdio_init_all();       
    init_hat_sdk();         
    sleep_ms(300);          

    // jonot pitää olla olemassa ennen keskeytysten sallimista
    // koska keskeytyskäsittelijä käyttää buttonqueue
    buttonQueue = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(button_event_t));
    if (buttonQueue == NULL) {
        printf("Jonon luonti epäonnistui\n");
        return 0;
    }

    // painikkeet aktiivisiä ylhäällä: painallus = nouseva reuna, vapautus = laskeva reuna
    // sw1 tarvii yhden, sw2 kaksi painallusta
    init_button1();
    init_button2();
    // ääni
    init_buzzer();
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, button_irq_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);


    BaseType_t imuTaskResult = xTaskCreate(imu_task, "imu", DEFAULT_STACK_SIZE, NULL, 2, NULL);
    BaseType_t stateTaskResult = xTaskCreate(state_machine_task, "state", DEFAULT_STACK_SIZE, NULL, 2, NULL);

    if (imuTaskResult != pdPASS || stateTaskResult != pdPASS) {
        printf("Tehtävien luonti epäonnistui\n");
        return 0;
    }

    vTaskStartScheduler();  // ei palaa tästä

    return 0;
}