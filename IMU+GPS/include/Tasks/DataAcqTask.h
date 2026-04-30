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
*/
typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    bool updated;
} IMUData_t;

/* GPSData_t
 * @brief: Struct to hold the latest GPS data read from the TinyGPS++ library.
*/
typedef struct {
    float latitude;
    float longitude;
    float speed;
    float course;
    bool valid;
    bool updated;
} GPSData_t;

/* SensorType_t
 * @brief: Enum to represent the type of sensor data to be enqueued.
*/
typedef enum {
    SENSOR_TYPE_IMU,
    SENSOR_TYPE_GPS
} SensorType_t;

/* SensorPacket_t
 * @brief: Tagged union passed through the single sensor queue. Inspect "type" first, then read the matching union member.
*/
typedef struct {
    SensorType_t type;
    union {
        IMUData_t imu;
        GPSData_t gps;
    };
} SensorPacket_t;

/* void DataAcqTask(void* pvParameters)
 * @brief: FreeRTOS task function that continuously reads data from the ICM-20948 IMU and sends it to a queue for other tasks to consume.
 * @param: pvParameters - Pointer to a DataAcqTaskParameters struct containing the queue handle for sending IMU data.
*/
void DataAcqTask(void* pvParameters);

#endif // DATA_ACQ_TASK_H
