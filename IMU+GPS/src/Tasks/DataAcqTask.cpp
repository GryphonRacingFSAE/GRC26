#include <DataAcqTask.h>
#include <Arduino.h>
#include <SPI.h>
#include <ICM_20948.h>
#include <TinyGPS++.h>
#include <PinDefs.h> 

#include "driver/uart.h"

#define DATA_TASK_PERIOD_MS 20
#define IMU_SPI_FREQUENCY 4000000

// By default +/- 2g accel range, +/- 250 dps gyro range.
static ICM_20948_SPI imu;
static TinyGPSPlus gps;

/* static void initIMU()
 * @brief: Initializes the onboard ICM 20498 IMU. Retries until successful.
*/
static void initIMU() {
    pinMode(IMU_CS, OUTPUT);
    digitalWrite(IMU_CS, HIGH);
    delay(10);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, IMU_CS);
    SPI.setDataMode(SPI_MODE3);
 
    Serial.println("[IMU] Initializing ICM-20948...");
    while (true) {
        imu.begin(IMU_CS, SPI, IMU_SPI_FREQUENCY);
        Serial.printf("[IMU] WHO_AM_I = 0x%02X (expected 0xEA)\n", imu.getWhoAmI());
        if (imu.status == ICM_20948_Stat_Ok) {
            ICM_20948_fss_t myFSS; 
            myFSS.a = gpm4; // +/- 4g 
            myFSS.g = dps250; // +/- 250 dps
            imu.setFullScale(ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr, myFSS);
            break;
        }
        Serial.printf("[IMU] IMU init failed (%s), retrying in 500 ms...\n", imu.statusString());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* static void readIMU(IMUGPSData_t& imuData)
 * @brief: Reads data from the ICM-20948 IMU and populates an IMUGPSData_t struct. Sets the "updated" flag if new data was read successfully.
 * @param: imuData - Reference to an IMUGPSData_t struct to populate with the latest IMU readings.
*/
static void readIMU(IMUGPSData_t& imuData) {
    imuData.imu_updated = false;

    if (imu.dataReady()) {
        imu.getAGMT();
        imuData.imu_updated = (imu.status == ICM_20948_Stat_Ok);
    }
 
    if (imuData.imu_updated) {
        imuData.accel_x = imu.accX();    
        imuData.accel_y = imu.accY();    
        imuData.accel_z = imu.accZ();    
        imuData.gyro_x  = imu.gyrX();   
        imuData.gyro_y  = imu.gyrY();   
        imuData.gyro_z  = imu.gyrZ();
        Serial.println("[IMU] IMU accel X = " + String(imuData.accel_x) + ", Y = " + String(imuData.accel_y) + ", Z = " + String(imuData.accel_z) + "");
        Serial.println("[IMU] IMU gyro X = " + String(imuData.gyro_x) + ", Y = " + String(imuData.gyro_y) + ", Z = " + String(imuData.gyro_z) + "");
    } else {
        Serial.println("[IMU] WARNING: IMU not ready or read failed");
    }
}

/* static void initGPS()
 * @brief: Initializes the GPS module by starting the serial communication on the appropriate pins.
*/    
static void initGPS() {
    Serial0.begin(9600, SERIAL_8N1, UART0_RX, UART0_TX);
    Serial.println("[GPS] Initializing GPS...");
}

/* static void buildGPSPacket(IMUGPSData_t& gpsData)
 * @brief: Reads data from the GPS module and populates an IMUGPSData_t struct. Sets the "updated" flag if new GPS data was read successfully.
 * @param: gpsData - Reference to an IMUGPSData_t struct to populate with the latest GPS readings. The "gps_updated" flag will be set to true if new GPS data was successfully read and parsed.
*/

static void buildGPSPacket(IMUGPSData_t& gpsData) {
    gpsData.gps_updated = false;

    while (Serial0.available() > 0) {
        char c = Serial0.read();

        // // For debugging: print raw GPS data to serial monitor
        // Serial.print(c); 
        
        if (gps.encode(c)) {
            if (gps.location.isUpdated()) {
                gpsData.gps_updated = true;
            }
        }
    }

    gpsData.valid = gps.location.isValid();

    if (gpsData.valid) {
        gpsData.latitude = (float)gps.location.lat();
        gpsData.longitude = (float)gps.location.lng();
        gpsData.speed = gps.speed.isValid() ? (float)gps.speed.kmph() : 0.0f;
        gpsData.course = gps.course.isValid() ? (float)gps.course.deg() : 0.0f;
    }

    if (gpsData.gps_updated && gpsData.valid) {
        Serial.println("[GPS] GPS Latitude = " + String(gpsData.latitude, 6) + ", Longitude = " + String(gpsData.longitude, 6) + ", Speed = " + String(gpsData.speed) + " km/h, Course = " + String(gpsData.course) + " degrees");
    }
}

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);

    Serial.println("[Data] Task Started");

    initIMU();
    initGPS();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
 
        IMUGPSData_t packet = {};
        readIMU(packet);
        buildGPSPacket(packet);

        if (xQueueSend(data_queue, &packet, 0) != pdTRUE) {
            Serial.println("[Data] WARNING: queue full, dropping IMU+GPS packet");
        }
    }
}