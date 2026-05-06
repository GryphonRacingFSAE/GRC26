#include <DataAcqTask.h>
#include <Arduino.h>
#include <SPI.h>
#include <ICM_20948.h>
#include <TinyGPS++.h>
#include <PinDefs.h> 

#define DATA_TASK_PERIOD_MS 20
#define IMU_SPI_FREQUENCY 4000000

static ICM_20948_SPI imu;
static TinyGPSPlus gps;

/* static void initIMU()
 * @brief: Initializes the onboard ICM 20498 IMU. Retries until successful.
*/
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
            break;
        }
        Serial.printf("[Data] IMU init failed (%s), retrying in 500 ms...\n", imu.statusString());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* static void readIMU(IMUGPSData_t& imuData)
 * @brief: Reads data from the ICM-20948 IMU and populates an IMUGPSData_t struct. Sets the "updated" flag if new data was read successfully.
 * @param: imuData - Reference to an IMUGPSData_t struct to populate with the latest IMU readings.
*/
static void readIMU(IMUGPSData_t& imuData) {
    imuData = { .accel_x = 0, .accel_y = 0, .accel_z = 0, .gyro_x = 0, .gyro_y = 0, .gyro_z = 0, .imu_updated = false};
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
        Serial.println("[Data] IMU accel X = " + String(imuData.accel_x) + ", Y = " + String(imuData.accel_y) + ", Z = " + String(imuData.accel_z) + "");
        Serial.println("[Data] IMU gyro X = " + String(imuData.gyro_x) + ", Y = " + String(imuData.gyro_y) + ", Z = " + String(imuData.gyro_z) + "");
    } else {
        Serial.println("[Data] WARNING: IMU not ready or read failed");
    }
}

/* static void initGPS()
 * @brief: Initializes the GPS module by starting the serial communication on the appropriate pins.
*/    
static void initGPS() {
    Serial0.begin(9600, SERIAL_8N1, UART0_RX, UART0_TX);
    Serial.println("[Data] Initializing GPS...");
}

/* static void drainGPSSerial()
 * @brief: Feeds all pending UART bytes into TinyGPS++ without blocking. Called every task iteration so no bytes are left in the hardware FIFO.
*/
static void drainGPSSerial() {
    while(Serial0.available() > 0) {
        gps.encode(Serial0.read());
    }
}

static bool buildGPSPacket(IMUGPSData_t& gpsData) {
    gpsData = { .latitude = 0, .longitude = 0, .speed = 0, .course = 0, .valid = false, .gps_updated = false };

    drainGPSSerial(); 

    gpsData.valid = gps.location.isValid() && (gps.location.age() < 2000); // Consider location valid if it's been updated in the last 2 seconds

     if (gpsData.valid) {
        gpsData.latitude = (float)gps.location.lat();
        gpsData.longitude = (float)gps.location.lng();
        gpsData.speed = gps.speed.isValid() ? (float)gps.speed.kmph() : 0.0f;
        gpsData.course = gps.course.isValid() ? (float)gps.course.deg() : 0.0f;
        Serial.println("[Data] GPS lat = " + String(gpsData.latitude, 6) + ", lng = " + String(gpsData.longitude, 6) + "speed = " + String(gpsData.speed) + " km/h, course = " + String(gpsData.course) + " deg, valid = " + String(gpsData.valid));
    } else {
        Serial.println("[Data] WARNING: GPS data invalid");
    }

    gpsData.gps_updated = gps.location.isUpdated(); 
    return gpsData.gps_updated;
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
        drainGPSSerial(); // Ensure we read all pending GPS bytes every iteration

        if (buildGPSPacket(packet)) {
            if (xQueueSend(data_queue, &packet, 0) != pdTRUE) {
                Serial.println("[Data] WARNING: queue full, dropping IMU+GPS packet");
            }

            Serial.println("[IMU] accel X = " + String(packet.accel_x) + ", Y = " + String(packet.accel_y) + ", Z = " + String(packet.accel_z) + "");
            Serial.println("[IMU] gyro X = " + String(packet.gyro_x) + ", Y = " + String(packet.gyro_y) + ", Z = " + String(packet.gyro_z) + "");
            Serial.println("[GPS] lat = " + String(packet.latitude, 6) + ", lng = " + String(packet.longitude, 6) + "speed = " + String(packet.speed) + " km/h, course = " + String(packet.course) + " deg, valid = " + String(packet.valid));
        }  
    }
}
