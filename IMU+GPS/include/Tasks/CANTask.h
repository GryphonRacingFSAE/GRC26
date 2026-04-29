#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

/* CANTaskParameters
 * @brief: Struct to hold parameters for the CANTask, currently just a pointer to the queue handle for receiving IMU data.
*/
typedef struct {
    QueueHandle_t* dataQueue;
} CANTaskParameters;

/* void CANTask(void* pvParameters)
 * @brief: FreeRTOS task function that continuously reads IMU data from a queue and transmits it over the CAN bus.
 * @param: pvParameters - Pointer to a CANTaskParameters struct containing the queue handle for receiving IMU data. 
*/
void CANTask(void* pvParameters);

#endif // CAN_TASK_H
