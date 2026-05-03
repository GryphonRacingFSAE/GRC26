#ifndef TASK_FACTORY_H  
#define TASK_FACTORY_H

/* void createTasks()
 * @brief: Factory class to create and manage FreeRTOS tasks for the IMU+GPS project. Currently responsible for creating the DataAcqTask and CANTask, and managing shared resources like queues.
*/
void createTasks();

#endif // TASK_FACTORY_H
