/**
 * @file imu.h
 * @brief IMU reading and device position recognition.
 */

#ifndef IMU_H
#define IMU_H

/**
 * @brief FreeRTOS task: reads the IMU, works out which position the device
 *        is in and keeps the newest position in positionQueue.
 * @param arg unused
 */
void imu_task(void *arg);

#endif /* IMU_H */