// Kaikkien tiedostojen yhteiset määrittelyt: vakiot, asento- ja painiketapahtumatyyppi,
// globaali muuttuja currentPosition, jono buttonQueue sekä funktioesittelyt.

// datan liikkuminen:
// imu_task -> currentPosition -> state_machine_task
// SW1/2 -> button_irq_handler -> buttonQueue -> state_machine_task
// state_machine_task -> printf USB portille -> serial client


#ifndef PROJECT_H
#define PROJECT_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>


#define DEFAULT_STACK_SIZE 2048 //pinon koko
#define MORSE_BUFFER_SIZE 100 // merkkien enimmäismäärä

// laitteen asento imu_task -> currentPosition -> state_machine_task
typedef enum {
    POSITION_UNKNOWN = 0,   // välissä -> ei mitään
    POSITION_FLAT,          // piste
    POSITION_TILTED         // viiva
    
} device_position_t;
// painiketapahtumat button_irq_handler -> buttonQueue -> state_machine_task
typedef enum {
    SW1_PRESSED = 1,   // pisteen tai viivan lisäys
    SW2_PRESSED,       
    SW2_RELEASED       
} button_event_t;

// merkit
#define MORSE_CHAR_DOT        '.'
#define MORSE_CHAR_DASH       '-'
#define MORSE_CHAR_SPACE      ' '


// volatile koska kääntäjän on luettava aina todellinen arvo (imu_task muuttaa ja state_machine_task lukee)
extern volatile device_position_t currentPosition; // laitteen nykyinen asento
extern QueueHandle_t buttonQueue;    // painiketapahtumien arvot jonossa, max 8 arvoa

// esittelyt main.c:ssä määritellyille tehtäville ja keskeytyskäsittelijälle
void imu_task(void *arg);
void state_machine_task(void *arg);
void button_irq_handler(uint gpio, uint32_t eventMask);
#endif
