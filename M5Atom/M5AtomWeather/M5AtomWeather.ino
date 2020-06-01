#include <M5Atom.h>
#include "AtomClient.h"

volatile int windCounter = 0;
volatile unsigned long windCounterLastUpdate = 0;

volatile int rainCounter = 0;
volatile unsigned long rainCounterLastUpdate = 0;
unsigned long rainLastReported = 0;

char* ssid = "your_ssid";
char* password = "your_password";
char* server = "your_mqtt_server";

AtomClient ac;

portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR windCount(){
  portENTER_CRITICAL_ISR(&mutex);
  unsigned long now = millis();
  if(now - windCounterLastUpdate > 30){
    windCounter++;
    windCounterLastUpdate = now;
  }
  portEXIT_CRITICAL_ISR(&mutex);
}

void IRAM_ATTR rainCount(){
  portENTER_CRITICAL_ISR(&mutex);
  unsigned long now = millis();
  if(now -rainCounterLastUpdate >200){
    rainCounter++;
    rainCounterLastUpdate = now;
  }
  portEXIT_CRITICAL_ISR(&mutex);
}

void setup() {
  M5.begin(true, false, true);

  // Client ID
  ac.setup("Weather2", ssid, password, server);
  Serial.println();
  Serial.print("MQTT Client ID: ");
  Serial.println(ac.getClientId());

  // Wind
  pinMode(23, INPUT_PULLUP);
  attachInterrupt(23, windCount,RISING);

  // Rain
  pinMode(19, INPUT_PULLUP);
  attachInterrupt(19, rainCount,RISING);
}


int getWindDirection(int analogValue){
  const int table[] = { 308,741,1283,2104,2602,3187,3792,5000 };
  const int dir[] = {0,1,2,7,3,6,5,4};

  int c = 0;
  for(int i=0; i<8;i++){
    if(analogValue < table[i]){
      break;
    }
    c++;
  }
  return dir[c];
}

void loop(){
  const int rainDurationSec = 3600;
  unsigned long now = millis();

  // Wind direction
  int a = analogRead(33);
  int windDirection = getWindDirection(a);

  // Wind speed (m/s)
  portENTER_CRITICAL_ISR(&mutex);
  float windSpeed = windCounter * 2400.0 / 3600.0; // 2.4km/h per count
  windCounter = 0;
  portEXIT_CRITICAL_ISR(&mutex);
  
  // Accumulated Rainfall (mm)
  portENTER_CRITICAL_ISR(&mutex);
  float rain = rainCounter * 0.2794;
  if(now - rainLastReported > rainDurationSec * 1000){
    rainCounter = 0;
    rainLastReported = now;
  }
  portEXIT_CRITICAL_ISR(&mutex);

  ac.reconnect();

  String id = ac.getName();
  String topicW = "Direction";
  String dataW = "{\"Id\":\"" + id + "\",\"Value\":" + String(windDirection) + "}";
  ac.publish(topicW,dataW);

  String topicC = "WindSpeed";
  String dataC = "{\"Id\":\"" + id + "\",\"Value\":" + String(windSpeed,2) + "}";
  ac.publish(topicC,dataC);   

  String topicR = "AccumulatedRainfall";
  String dataR = "{\"Id\":\"" + id + "\",\"Value\":" + String(rain,2) + "}";
  ac.publish(topicR,dataR);

  delay(1000);
  M5.update();
}
