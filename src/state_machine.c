// Morseviestin muodostus ja lähetys SW1/SW2-painikkeista ja laitteen asennosta
// lähettää viestin USB-sarjaliikenteen kautta työasemalle

// Syötteet:
// - SW1 (aktiivinen ylhäällä): piste (vaakataso) tai viiva (kallistettu)
// - SW2 lyhyt painallus: lisää välilyönnin
// - SW2 pitkä painallus: lähettää viestin (kun ei tyhjä
// - Asento: globaali currentPosition (päivittää imu_task)

// Tilat ja siirtymät:
// - WAITING: SW1 lisää merkin ja pysyy tilassa WAITING;
//   SW2 painettu (viesti ei tyhjä) -> SW2_HELD
// - SW2_HELD: SW2 vapautettu -> WAITING (lyhyt: välilyönti, pitkä: lähetys)
// - Muut tilaan sopimattomat tapahtumat ohitetaan.

// Toteutus: keskeytys -> buttonQueue -> state_machine_task.

#include <stdio.h>
#include <pico/stdlib.h>
#include "tkjhat/sdk.h"
#include "project.h"

#define DEBOUNCE_MS    50      // hylkää tätä lähempänä olevat reunat
#define LONG_PRESS_MS  1000    // lähettää kun pohjassa näin kauan
#define TASK_PERIOD_MS 10      // kuinka usein tehtävä tarkistaa jonon

// tilakoneen tilat
enum state { STATE_WAITING = 1, STATE_SW2_HELD };

// globaali tilamuuttuja
enum state programState = STATE_WAITING;

// nämä muuttujat ovat static koska käytetään vain tässä tiedostossa
static char messageBuffer[MORSE_BUFFER_SIZE];
static int messageLength = 0;
static TickType_t sw2PressedTick = 0;

// tämän funktion rungon tekemiseen käytin Claudea apuna
// käsittelee keskeytykset
// muuttaa reunan painiketapahtumaksi ja laittaa buttonqueueen
void button_irq_handler(uint gpio, uint32_t eventMask)
{
    // viimeisimmän hyväksytyn reunan ajanhetki debouncea varten
    static TickType_t lastButton1EdgeTick = 0;
    static TickType_t lastButton2EdgeTick = 0;

    button_event_t event;
    TickType_t now = xTaskGetTickCountFromISR();

    if (gpio == BUTTON1) {
        if (now - lastButton1EdgeTick < pdMS_TO_TICKS(DEBOUNCE_MS)) {
            return;     // bounce
        }
        lastButton1EdgeTick = now;
        if (!(eventMask & GPIO_IRQ_EDGE_RISE)) {
            return;     // vain painalluksella väliä SW1:lle
        }
        event = BUTTON_EVENT_SW1_PRESSED;
    } else if (gpio == BUTTON2) {
        if (now - lastButton2EdgeTick < pdMS_TO_TICKS(DEBOUNCE_MS)) {
            return;     // bounce
        }
        lastButton2EdgeTick = now;

        if (eventMask & GPIO_IRQ_EDGE_RISE) {
            event = BUTTON_EVENT_SW2_PRESSED;
        } else {
            event = BUTTON_EVENT_SW2_RELEASED;
        }
    } else {
        return;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(buttonQueue, &event, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

//SW1 painallus, piste tai viiva riippuen asennosta
//ei mitään jos tuntematon tai puskuri täynnä
void add_symbol(void)
{
    // kopioidaan asento kerran, jotta se ei muutu kesken funktion
    device_position_t position = currentPosition;

    if (messageLength >= MORSE_BUFFER_SIZE - 1) {
        return;     // bufferi on täynnä
    }
    if (position == POSITION_FLAT) {
        messageBuffer[messageLength] = MORSE_CHAR_DOT;
        messageLength++;
    } else if (position == POSITION_TILTED) {
        messageBuffer[messageLength] = MORSE_CHAR_DASH;
        messageLength++;
    }
}

// sw2 lyhyt painallus -> lisää välilyönnin
//voidaan painaa vain jos viesti ei ole tyhjä -> viesti ei voi alkaa välilyönnillä
void add_space(void)
{
    if (messageLength < MORSE_BUFFER_SIZE - 1) {
        messageBuffer[messageLength] = MORSE_CHAR_SPACE;
        messageLength++;
    }

    programState = STATE_WAITING;
}

//sw 2 pitkä painallus -> lähettää viestin USB portin kautta
//poistetaan lopun välilyönnit ja päätetään kahteen välilyöntiin ja rivinvaihtoon
void send_message(void)
{
    while (messageLength > 0 && messageBuffer[messageLength - 1] == MORSE_CHAR_SPACE) {
        messageLength--;
    }
    if (messageLength > 0) {
        messageBuffer[messageLength] = '\0';     // c-string
        printf("%s  \n", messageBuffer);         // viesti, kaksi välilyöntiä ja rivinvaihto
        fflush(stdout);                          // lähetä heti
    }
    messageLength = 0;

    programState = STATE_WAITING;
}

// päättää mitä painiketapahtuma tekee nykytilassa
void handle_button_event(button_event_t event)
{   //tapahtumat jotka eivät sovi nykyiseen tilaan ignorataan, joten vain sallitut siirtymät voivat tapahtua
    switch (programState) {
    case STATE_WAITING:
        if (event == BUTTON_EVENT_SW1_PRESSED) {
            // tilan toiminnallisuus
            add_symbol();
        } else if (event == BUTTON_EVENT_SW2_PRESSED && messageLength > 0) {
            sw2PressedTick = xTaskGetTickCount();    // remember when SW2 went down

            // tilamuutos WAITING -> SW2_HELD
            programState = STATE_SW2_HELD;
        }
        break;

    case STATE_SW2_HELD:
        if (event == BUTTON_EVENT_SW2_RELEASED) {
            // painon kesto ratkaisee mitä tehdään
            TickType_t heldTicks = xTaskGetTickCount() - sw2PressedTick;
            if (heldTicks >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                send_message();     //tilamuutos SW2_HELD -> WAITING
            } else {
                add_space();        // sama
            }
        }
        // sw1 ignoree kun sw2 on pohjassa
        break;
    }
}

// tilakoneen tehtävä, odottaa painiketapahtumia ja käsittelee ne
void state_machine_task(void *arg)
{
    (void)arg;

    button_event_t event;

    for (;;) {
        // timeout 0: ei odoteta jos jono on tyhjä
        while (xQueueReceive(buttonQueue, &event, 0) == pdTRUE) {
            handle_button_event(event);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}