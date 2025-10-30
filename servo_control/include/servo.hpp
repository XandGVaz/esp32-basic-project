#ifndef SERVO_HPP
#define SERVO_HPP

/*===============================================================================*/
// Bibliotecas de implementação das funções da ArduinoIDE
#include <Arduino.h> // funções arduino

/*===============================================================================*/
// Defines
#define SERVO_PWM_FREQ 50           // hertz
#define SERVO_PWM_PERIOD 20000      // microssegunds
#define SERVO_PWM_BITS 13           // 0 - 8191
#define SERVO_PWM_TIMER_CHANNEL 0   // canal 0 de timer   

/*===============================================================================*/
// Classe do servo
class Servo{

  uint8_t servoPin;
 
 public:

  Servo(uint8_t servoPin);
  void begin();
  void setAngle(uint8_t angle);
};

#endif