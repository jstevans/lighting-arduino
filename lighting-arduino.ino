
#include <Wire.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define LED_PIN 13
#define AL_ADDR 0x48
#define GAIN_SETTING .125 /* Possible values: .125; .25; 1 (unsafe); 2 (unsafe) */
#define INTEGRATION_MS_SETTING 800 /* Possible values: 25; 50; 100; 200; 400; 800; */

#define POLL_BACKOFF_MS     1000 /* how much to increase the poll duration when backing off */
#define POLL_DEFAULT_MS     1000 /* the default polling rate with zero backoff */
#define POLL_MAX_MS        10000 /* the maximum polling rate when fully backed-off */
#define POLL_FUZZ_LUX         10 /* the minimum cumulative change in lux before polling backoff resets */

#define MIN_LUX        80
#define MAX_LUX       120
#define LED_BEHAVIOR   -1   /* 0: disabled; 1: flashes when unacceptable; -1: flashes when acceptable; */

#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914a"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

SparkFun_Ambient_Light light(AL_ADDR);

struct PollInfo {
  bool didLightChange;
  long pollDelay;
};

long lastLuxVal = -2 * POLL_FUZZ_LUX;
long lastPollDelay = POLL_DEFAULT_MS;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

enum DatapointProperty {
  DP_BEGIN_VERSION = 0x0,
  DP_LuxValue = 0x1,
  DP_MinLuxValue = 0x2,
  DP_MaxLuxValue = 0x3,
  DP_PollDuration = 0x4,
  DP_END_LENGTH = 0xFF
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup(){
  pinMode(LED_PIN, OUTPUT);
  Wire.begin();
  Serial.begin(115200);
  configureLightSensor();
  configureBluetoothServer();
}

void configureLightSensor() {
  if(light.begin())
    Serial.println("Light sensor successfully enabled"); 
  else
    Serial.println("ERROR: Light sensor not detected");

  Serial.println("Reading settings..."); 
  Serial.printf("Gain: %.3-5f\nIntegration Time: %-5d\n", light.readGain(), light.readIntegTime());
}

void configureBluetoothServer() {
  BLEDevice::init("Light Monitor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      BLE_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
}

void loop(){
  long luxVal = light.readLight(); 
  long savedLastPollDelay = lastPollDelay;
  struct PollInfo pollInfo = getPollDelay(luxVal);
  int luxAcceptability = isLuxValueAcceptable(luxVal);

  Serial.printf(
    "Ambient Light Reading:\n\t%-15s%s\n\t%-15s%d lux (%.2f foot-candles)\n\t%-15s%s\n\t%-15s%dms\n", 
    "Status: ", luxAcceptability < 0 ? "TOO LOW" : luxAcceptability > 0 ? "TOO HIGH" : "OK",
    "Illuminance: ", luxVal, luxToFootCandles(luxVal), 
    "Light changed: ", pollInfo.didLightChange ? "yes" : "no", 
    "Next poll: ", pollInfo.pollDelay);
  
  // notify changed value
  if (deviceConnected) {
      serializeDatapoint(luxVal, MIN_LUX, MAX_LUX, savedLastPollDelay);
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      pServer->startAdvertising(); // restart advertising
      Serial.println("start advertising");
      oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
      // do stuff here on connecting
      oldDeviceConnected = deviceConnected;
  }
    
  delay(pollInfo.pollDelay);
}

float luxToFootCandles(long lux) {
  return lux / 10.7639 /* lux per footcandle */;
}

struct PollInfo getPollDelay(long luxValue) {
  long luxDelta = abs(lastLuxVal - luxValue);
  
  if (luxDelta > POLL_FUZZ_LUX) {
    lastLuxVal = luxValue;
    return ((struct PollInfo) {true, lastPollDelay = POLL_DEFAULT_MS});
  }

  // don't set lastLuxVal, since that was the last measurement we really counted
  return ((struct PollInfo) {false, lastPollDelay = min<long>(lastPollDelay + POLL_BACKOFF_MS, POLL_MAX_MS) });
}

int isLuxValueAcceptable(long luxValue) {
  int cmp = luxValue < MIN_LUX ? -1 : luxValue > MAX_LUX ? 1 : 0;
  int ledValue = cmp == 0 ? LED_BEHAVIOR == -1 ? HIGH : LOW : HIGH;
  digitalWrite(LED_PIN, ledValue);
  return cmp;
}

void serializeDatapoint(long luxValue, long minLuxValue, long maxLuxValue, long pollDuration) {
  long datapoints[][2] = {
    {(long)DP_BEGIN_VERSION, 0},
    {(long)DP_LuxValue, luxValue},
    {(long)DP_MinLuxValue, minLuxValue},
    {(long)DP_MaxLuxValue, maxLuxValue},
    {(long)DP_PollDuration, pollDuration},
    {(long)DP_END_LENGTH, 4}
  };
  
  for (int i = 0; i == 0 || datapoints[i-1][0] != DP_END; i++) {
    pCharacteristic->setValue((uint8_t*)(datapoints[i]), 2*sizeof(long)/(sizeof(uint8_t)));
    pCharacteristic->notify();
    delay(3);
  }
}
