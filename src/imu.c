// imu_task lukee kiihtyvyysanturia 50 kertaa sekunnissa, laskee keskiarvon
// muutamasta viime lukemasta ja päättelee laitteen asennon
// tulos tallennetaan globaaliin muuttujaancurrentPosition

#include <stdio.h>
#include <math.h>
#include <pico/stdlib.h>
#include "tkjhat/sdk.h"
#include "project.h"

#define SAMPLE_PERIOD_MS 20     // -> 50hz lukunopeus
#define FILTER_SIZE      5      // lukujen määrä keskiarvoon

// raja-arvot määritetty kokeellisesti sarjaporttitulostusten avulla:
// -> maan vetovoima vaakatasossa(n. 1.0g) kohidstuu z-akseliin ja x-akseli on lähellä nollaa (az = 1.0 ja ax = 0.0)
// 0.8f ja 0.3f sallii pienen tärinän ja epätasaisuuden (n. 15-20 astetta)
// kyljellään painovoima siirtyy x-akselille (ax = 1.0) -> 0.7f tunnistaa selkeän kallistuksen (yli 45 astetta),
// mutta jättää 0.3f-0.7f välille varmuusalueen (POSITION_UNKNOWN), jotta heilautukset ei luo merkkejä


// raja-arvot (1g = 9.81 m/s^2, 1g = 1.0f)
#define FLAT_Z_MIN       0.8f   // az tämän yli -> vaakatasossa
#define FLAT_X_MAX       0.3f   // ax tämän alle -> vaakatasossa
#define TILTED_X_MIN     0.7f   // ax tämän yli -> pystysuorassa

// sijainti on globaali, koska imu_task ja state_machine_task käyttävät sitä
volatile device_position_t currentPosition = POSITION_UNKNOWN;

void imu_task(void *arg)
{
    (void)arg;

    float ax, ay, az, gx, gy, gz, temperature;
    float axReadings[FILTER_SIZE] = {0};
    float azReadings[FILTER_SIZE] = {0};
    int readingIndex = 0;
    device_position_t position;

    // IMU käynnistys + virheviestin konfigurointi
    int initResult = init_ICM42670();
    if (initResult == 0) {
        initResult = ICM42670_start_with_default_values();
    }
    if (initResult != 0) {
        printf("IMU init failed (%d)\n", initResult);
        vTaskDelete(NULL);
    }

    for (;;) {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temperature) == 0) {

            // tallenna lukemat vanhimman päälle
            axReadings[readingIndex] = ax;
            azReadings[readingIndex] = az;
            readingIndex = (readingIndex + 1) % FILTER_SIZE;

            // liikkuva keskiarvo: summataan tallennetut lukemat ja jaetaan niiden määrällä. 
            // tämä tasoittaa yksittäisiä kohinalukemia
            float axSum = 0;
            float azSum = 0;
            for (int i = 0; i < FILTER_SIZE; i++) {
                axSum += axReadings[i];
                azSum += azReadings[i];
            }
            float axAverage = axSum / FILTER_SIZE;
            float azAverage = azSum / FILTER_SIZE;

            // asennon päätteleminen
            if (fabsf(azAverage) > FLAT_Z_MIN && fabsf(axAverage) < FLAT_X_MAX) {
                position = POSITION_FLAT;
            } else if (fabsf(axAverage) > TILTED_X_MIN) {
                position = POSITION_TILTED;
            } else {
                position = POSITION_UNKNOWN;
            }

            //uusin asento
            currentPosition = position;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}