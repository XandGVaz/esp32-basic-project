#ifndef DHT_HPP
#define DHT_HPP

/*===============================================================================*/
// Uso do sensor DHT
#include <Arduino.h>
#include <Adafruit_Sensor.h> 
#include <DHT.h>
#include <DHT_U.h>

/*===============================================================================*/
// Definições do DHT
#define DHTTYPE DHT22   // tipo de sensor DHT

/*===============================================================================*/
// Classe do sensor DHT

class Dht{
  DHT_Unified *dht = NULL;
  sensor_t* sensor = NULL;
 public:
  Dht(uint8_t dhtPin);
  bool setup();
  float getTemperature();
  float getHumidity();
};

#endif