#ifndef DATA_ACQ_TASK_H
#define DATA_ACQ_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

/* DataAcqTaskParameters
 * @brief: Struct to hold parameters for the DataAcqTask, currently just a pointer to the queue handle for sending IMU data.
*/
typedef struct {
    QueueHandle_t* dataQueue;
} DataAcqTaskParameters;

/* IMUData_t
 * @brief: Struct to hold the latest IMU data read from the ICM-20948. Contains accelerometer and gyroscope readings along with an "updated" flag.
 * Note: Accel units: milli g's, Gyro (for testing) units: degrees per second, Latitude/Longitude units: degrees, Speed units: km/h, Course units: degrees
*/
typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float latitude;
    float longitude;
    float speed;
    float course;
    bool valid;
    bool updated;
} IMUGPSData_t;

/* GPSData_t
 * @brief: Struct to hold the latest GPS data read from the TinyGPS++ library.
*/
typedef struct {

    bool updated;
} GPSData_t;

/* void DataAcqTask(void* pvParameters)
 * @brief: FreeRTOS task function that continuously reads data from the ICM-20948 IMU and sends it to a queue for other tasks to consume.
 * @param: pvParameters - Pointer to a DataAcqTaskParameters struct containing the queue handle for sending IMU data.
*/
void DataAcqTask(void* pvParameters);

#endif // DATA_ACQ_TASK_H
