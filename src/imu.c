// imu_task lukee kiihtyvyysanturia 50 kertaa sekunnissa ja päättelee laitteen asennon
// tulos tallennetaan globaaliin muuttujaan currentPosition

#include <stdio.h>
#include <math.h>
#include <pico/stdlib.h>
#include "tkjhat/sdk.h"
#include "project.h"

#define SAMPLE_PERIOD_MS 20     // -> 50hz lukunopeus

// raja-arvot määritetty fyysisesti kääntelemällä laitetta
// -> maan vetovoima vaakatasossa(n. 1.0g) kohidstuu z-akseliin ja x-akseli on lähellä nollaa (az = 1.0 ja ax = 0.0)
// 0.8f ja 0.3f sallii pienen tärinän ja epätasaisuuden (n. 15-20 astetta)
// kyljellään painovoima siirtyy x-akselille (ax = 1.0) -> 0.8f tunnistaa selkeän kallistuksen (yli 45 astetta),
// x-akselin välillä 0.3-0.8 on POSITION_UNKNOWN, jotta nopeat heilatukset eivät luo merkkejä

// raja-arvot (1g = 9.81 m/s^2, 1g = 1.0f)
#define FLAT_Z_MIN       0.8f   // az tämän yli -> vaakatasossa
#define FLAT_X_MAX       0.3f   // ax tämän alle -> vaakatasossa
#define TILTED_X_MIN     0.8f   // ax tämän yli -> pystysuorassa

// sijainti on globaali, koska imu_task ja state_machine_task käyttävät sitä
volatile device_position_t currentPosition = POSITION_UNKNOWN;

void imu_task(void *arg)
{
    (void)arg;

    float ax, ay, az, gx, gy, gz, temperature;
    device_position_t position;

    // IMU käynnistys + virheviestin konfigurointi
    int initResult = init_ICM42670();
    if (initResult == 0) {
        initResult = ICM42670_start_with_default_values();
    }
    if (initResult != 0) {
        printf("IMU init epäonnistui (%d)\n", initResult);
        vTaskDelete(NULL);
    }

    for (;;) {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temperature) == 0) {

            // asennon päätteleminen
            if (fabsf(az) > FLAT_Z_MIN && fabsf(ax) < FLAT_X_MAX) {
                position = POSITION_FLAT;
            } else if (fabsf(ax) > TILTED_X_MIN) {
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