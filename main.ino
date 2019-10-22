
#include <Wire.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"

#define AL_ADDR 0x48
#define GAIN_SETTING .125 /* Possible values: .125; .25; 1 (unsafe); 2 (unsafe) */
#define INTEGRATION_MS_SETTING 800 /* Possible values: 25; 50; 100; 200; 400; 800; */

#define POLL_BACKOFF_MS     1000 /* how much to increase the poll duration when backing off */
#define POLL_DEFAULT_MS     1000 /* the default polling rate with zero backoff */
#define POLL_MAX_MS        10000 /* the maximum polling rate when fully backed-off */
#define POLL_FUZZ_LUX         30 /* the minimum cumulative change in lux before polling backoff resets */

SparkFun_Ambient_Light light(AL_ADDR);

struct PollInfo {
  bool didLightChange;
  long pollDelay;
};

void setup(){
  Wire.begin();
  Serial.begin(115200);
  configureLightSensor();
}

void configureLightSensor() {
  if(light.begin())
    Serial.println("Light sensor successfully enabled"); 
  else
    Serial.println("ERROR: Light sensor not detected");

  Serial.println("Reading settings..."); 
  Serial.printf("Gain: %.3-5f\nIntegration Time: %-5d\n", light.readGain(), light.readIntegTime());
}

void loop(){
  long luxVal = light.readLight(); 
  struct PollInfo pollInfo = getPollDelay(luxVal);

  Serial.printf(
    "Ambient Light Reading:\n\t%-15s%d lux (%.2f foot-candles)\n\t%-15s%s\n\t%-15s%dms\n", 
    "Luminance: ", luxVal, luxToFootCandles(luxVal), 
    "Light changed: ", pollInfo.didLightChange ? "yes" : "no", 
    "Next poll: ", pollInfo.pollDelay);
    
  delay(pollInfo.pollDelay);
}

float luxToFootCandles(long lux) {
  return lux / 10.7639 /* lux per footcandle */;
}

long lastLuxVal = -2 * POLL_FUZZ_LUX;
long lastPollDelay = POLL_DEFAULT_MS;
struct PollInfo getPollDelay(long luxValue) {
  long luxDelta = abs(lastLuxVal - luxValue);
  
  if (luxDelta > POLL_FUZZ_LUX) {
    lastLuxVal = luxValue;
    return ((struct PollInfo) {true, lastPollDelay = POLL_DEFAULT_MS});
  }

  // don't set lastLuxVal, since that was the last measurement we really counted
  return ((struct PollInfo) {false, lastPollDelay = min<long>(lastPollDelay + POLL_BACKOFF_MS, POLL_MAX_MS) });
}
