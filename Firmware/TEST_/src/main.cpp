#include <Arduino.h>
#include "Adafruit_CCS811.h"

Adafruit_CCS811 ccs;

void setup() {
  Serial.begin(9600);
  pinMode(19, OUTPUT);
  digitalWrite(19, HIGH);

  Serial.println("CCS811 test");

  while(!ccs.begin()){
    Serial.println("Failed to start sensor! Please check your wiring.");
    // while(1);
    delay(500);
  }

  // Wait for the sensor to be ready
  while(!ccs.available());
}

void loop() {
  if(ccs.available()){
    if(!ccs.readData()){
      Serial.print("CO2: ");
      Serial.print(ccs.geteCO2());
      Serial.print("ppm, TVOC: ");
      Serial.println(ccs.getTVOC());
    }
    else{
      Serial.println("ERROR!");
      while(1);
    }
  }
  delay(500);
}
