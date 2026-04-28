#include <DataAcqTask.h>
#include <Arduino.h>
#include <SPI.h>
#include <ICM_20948.h>
#include <PinDefs.h> 

#define DATA_TASK_PERIOD_MS 20
#define IMU_SPI_FREQUENCY 100000

static ICM_20948_SPI imu;

static void initIMU() {
    pinMode(IMU_CS, OUTPUT);
    digitalWrite(IMU_CS, HIGH);
    delay(10);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setDataMode(SPI_MODE3);
 
    Serial.println("[Data] Initializing ICM-20948...");
    while (true) {
        imu.begin(IMU_CS, SPI, IMU_SPI_FREQUENCY);
        Serial.printf("[Data] WHO_AM_I = 0x%02X (expected 0xEA)\n", imu.getWhoAmI());
        if (imu.status == ICM_20948_Stat_Ok) {
            Serial.println("[Data] ICM-20948 OK");
            break;
        }
        Serial.printf("[Data] IMU init failed (%s), retrying in 500 ms...\n", imu.statusString());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
    
void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);

    Serial.println("[Data] Task Started");

    initIMU();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
 
        IMUData_t imuData = {};

        if (imu.dataReady()) {
            imu.getAGMT();
            imuData.updated = (imu.status == ICM_20948_Stat_Ok);
        }
 
        if (imuData.updated) {
            imuData.accel_x = imu.accX();    
            imuData.accel_y = imu.accY();    
            imuData.accel_z = imu.accZ();    
            imuData.gyro_x  = imu.gyrX();   
            imuData.gyro_y  = imu.gyrY();   
            imuData.gyro_z  = imu.gyrZ();  
            Serial.println("[Data] IMU accel X = " + String(imuData.accel_x) + "`, Y = `" + String(imuData.accel_y) + "`, Z = `" + String(imuData.accel_z) + "`");
            Serial.println("[Data] IMU gyro X = " + String(imuData.gyro_x) + "`, Y = `" + String(imuData.gyro_y) + "`, Z = `" + String(imuData.gyro_z) + "`");
        } else {
            Serial.println("[Data] WARNING: IMU not ready or read failed");
        }
 
        if (xQueueSend(data_queue, &imuData, 0) != pdTRUE) {
            Serial.println("[Data] WARNING: queue full, dropping IMU packet");
        }
    }
}
