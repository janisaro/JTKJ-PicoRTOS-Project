 /**
  * @file imu.c
  * @brief IMU reading and device position recognition.
  *
    * The task does NOT create symbols by itself. It only keeps track of which
    * position the device is in. The state machine reads that position when the
    * user presses SW1 (the confirm button):
    *   POSITION_FLAT   -> dot
    *   POSITION_TILTED -> dash
    *
    * Gravity gives about 1 g on the axis that points down, so the position is
    * found by checking which axis has the gravity:
    *   flat:   az is about +-1 g, ax is about 0
    *   tilted: ax is about +-1 g (device standing on its edge)
    *
    * The axes and the thresholds below should be checked with your own data.
    * Build with IMU_DATA_LOGGING=1 to record it.
    */

#include <stdio.h>
#include <math.h>

#include <pico/stdlib.h>

#include "tkjhat/sdk.h"

#include "project.h"
#include "imu.h"

/* Set to 1 to build a data-collection program: the task prints CSV rows
 * (time_ms,ax,ay,az,gx,gy,gz) instead of recognising positions. Use it with a
 * normal serial monitor (NOT the Serial Client) to record your own data. */
#ifndef IMU_DATA_LOGGING
#define IMU_DATA_LOGGING 0
#endif

#define SAMPLE_PERIOD_MS   20     /* 50 Hz sampling                         */
#define FILTER_WINDOW      5      /* moving average over 5 samples          */

#define FLAT_Z_MIN_G       0.8f   /* |az| above this: lying flat            */
#define FLAT_X_MAX_G       0.3f   /* ... and |ax| below this                */
#define TILTED_X_MIN_G     0.7f   /* |ax| above this: standing on its edge  */

/**
 * @brief Decide the device position from the filtered acceleration.
 * @param ax_g filtered x acceleration in g
 * @param az_g filtered z acceleration in g
 * @return the position
 */
static device_position_t classify_position(float ax_g, float az_g)
{
    if (fabsf(az_g) > FLAT_Z_MIN_G && fabsf(ax_g) < FLAT_X_MAX_G) {
        return POSITION_FLAT;
    }
    if (fabsf(ax_g) > TILTED_X_MIN_G) {
        return POSITION_TILTED;
    }
    return POSITION_UNKNOWN; /* in between: ignore */
}

/**
 * @brief Moving average filter (reduces sensor noise).
 * @param history buffer of the last FILTER_WINDOW values
 * @return average of the buffer
 */
static float average(const float *history)
{
    float sum = 0.0f;
    for (int i = 0; i < FILTER_WINDOW; i++) {
        sum += history[i];
    }
    return sum / (float)FILTER_WINDOW;
}

void imu_task(void *arg)
{
    (void)arg;

    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0, temperature = 0;

    /* Start the IMU. Nothing else uses the I2C bus in the Tier 1 version. */
    int initResult = init_ICM42670();
    if (initResult == 0) {
        initResult = ICM42670_start_with_default_values();
    }

    if (initResult != 0) {
        printf("__IMU init failed (%d)__\n", initResult);
        vTaskDelete(NULL);
    }

#if IMU_DATA_LOGGING
    printf("time_ms,ax,ay,az,gx,gy,gz\n");
#else
    float axHistory[FILTER_WINDOW] = {0};
    float azHistory[FILTER_WINDOW] = {0};
    int historyIndex = 0;
#endif

    for (;;) {
        int readResult = ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temperature);

        if (readResult == 0) {
#if IMU_DATA_LOGGING
            /* Data collection mode: timestamp + 6 values, one row per sample. */
            printf("%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                   (unsigned long)pdTICKS_TO_MS(xTaskGetTickCount()),
                   ax, ay, az, gx, gy, gz);
#else
            /* 1. Filter */
            axHistory[historyIndex] = ax;
            azHistory[historyIndex] = az;
            historyIndex = (historyIndex + 1) % FILTER_WINDOW;

            /* 2. Classify */
            device_position_t position = classify_position(average(axHistory),
                                                           average(azHistory));

            /* 3. Publish. The queue has length 1, so Overwrite always
             *    replaces the old position with the newest one. */
            xQueueOverwrite(positionQueue, &position);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}