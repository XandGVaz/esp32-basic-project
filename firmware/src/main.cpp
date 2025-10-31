/*===============================================================================*/
// Bibliotecas
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "servo.hpp"
#include "dht.hpp"
#include "display.hpp"

/*===============================================================================*/
// Pinos

// Servo e butões de controle
#define SERVO_PIN 13
#define SERVO_PWM_CHANNEL 0
#define ADD_SERVO_ANGLE_BUTTON 4
#define SUB_SERVO_ANGLE_BUTTON 5

// DHT
#define DHT_PIN 15

// Display
#define DISPLAY_SDA_PIN 21
#define DISPLAY_SCL_PIN 22

/*===============================================================================*/
// Prototypes e handles das tasks 

/*
Task                       Core  Prio     Descrição
--------------------------------------------------------------------------------------------------
vTaskAdjustServo            1     2     Ajusta servo quando interrupção externa de botões
vTaskUpdateDisplay          1     1     Atualiza display com o ângulo atual do servo
vTaskGetTemperature         0     1     Obtém temperatura através do DHT
vTaskGetHumidity            0     1     Obtém humidade através do DHT
vTaslUpdateSerialMonitor    0     2     Atualiza monitor serial com humidade e temperatura obtidas
*/

// Prototypes
void vTaskAdjustServo(void *pvParameters);
void vTaskUpdateDisplay(void *pvParameters);
void vTaskGetTemperature(void *pvParameters);
void vTaskGetHumidity(void *pvParameters);
void vTaskUpdateSerialMonitor(void *pvParameters);

// Handles
xTaskHandle xTaskAdjustServoHandle = NULL;
xTaskHandle xTaskUpdateDisplayHandle = NULL;
xTaskHandle xTaskGetTemperatureHandle = NULL;
xTaskHandle xTaskGetHumidityHandle =  NULL;
xTaskHandle xTaskUpdateSerialMonitorHandle = NULL;

/*===============================================================================*/
// Variáveis do FreeRTOS 

// Filas
xQueueHandle xQueueAngleVariation = NULL; 
xQueueHandle xQueueAngleToDisplay = NULL; 
xQueueHandle xQueueTemperatureToSerial = NULL;
xQueueHandle xQueueHumidityToSerial = NULL;

// Timers digitais e prototypes de callbacks
xTimerHandle xTimerGetTemperature = NULL;
xTimerHandle xTimerGetHumidity = NULL;

/*===============================================================================*/
// Prototypes de callbacks e interrupções externas

// Calbacks
void vCallBackTimerGetTemperature(xTimerHandle xTimer);
void vCallBackTimerGetHumidity(xTimerHandle xTimer);

// Prototypes
void vISRAddAngle();
void vISRSubAngle();

/*===============================================================================*/
// Instanciação de módulos
Servo ServoMotor(SERVO_PIN, SERVO_PWM_CHANNEL);
Display16x2 Display(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
Dht DHT(DHT_PIN);

/*===============================================================================*/
// Setup do microcontrolador e módulos conectados
void setup() {

    // Configuração do monitor serial
    Serial.begin(115200);

    /*
        Configuração de pinos associados aos butões.
        Ambos os pinos associados a resistores internos de pull-up
    */
    pinMode(ADD_SERVO_ANGLE_BUTTON, INPUT_PULLUP);
    pinMode(SUB_SERVO_ANGLE_BUTTON, INPUT_PULLUP);
    
    // Criação de interrupções externas associadas aos botões de controle do motor
    attachInterrupt(ADD_SERVO_ANGLE_BUTTON, vISRAddAngle, FALLING);
    attachInterrupt(SUB_SERVO_ANGLE_BUTTON, vISRSubAngle, FALLING);

    // Criação de filas de dados
    xQueueAngleVariation = xQueueCreate(4, sizeof(int16_t));
    xQueueAngleToDisplay = xQueueCreate(10, sizeof(int16_t));
    xQueueTemperatureToSerial = xQueueCreate(10, sizeof(float));
    xQueueHumidityToSerial = xQueueCreate(10, sizeof(float));

    // Setup de módulos
    if(!ServoMotor.setup()){
        Serial.println("Erro na configuracao do servo");
        while(1);
    }
    Display.setup();
    if(!Display.setup()){
        Serial.println("Erro na configuracao do display");
        while(1);
    }
    DHT.setup();
    if(!DHT.setup()){
        Serial.println("Erro na configuracao do DHT");
        while(1);
    }

    /* 
        Criação de software timers com autoreload e associação com interrupções internas
        de notificação para obtenção de dados de humidade e temperatura    
    */
    xTimerGetTemperature = xTimerCreate("TIMER_TEMP", pdMS_TO_TICKS(1000), pdTRUE, 0, vCallBackTimerGetTemperature);
    if(xTimerGetTemperature == NULL){
        Serial.println("Erro na criacao do timer de temperatura");
        while(1);
    }
    xTimerGetHumidity = xTimerCreate("TIMER_HUM", pdMS_TO_TICKS(1000), pdTRUE, 0, vCallBackTimerGetHumidity);
    if(xTimerGetTemperature == NULL){
        Serial.println("Erro na criacao do timer de humidade");
        while(1);
    }

    // Criação de Tasks do sistema
    xTaskCreatePinnedToCore(vTaskAdjustServo, "TASK_SERVO_ANGLE", configMINIMAL_STACK_SIZE + 1024, NULL, 2, &xTaskAdjustServoHandle, APP_CPU_NUM);
    if(xTaskAdjustServoHandle == NULL){
        Serial.println("Erro na criação da TASK_SERVO_ANGLE");
        while(1);
    }
    xTaskCreatePinnedToCore(vTaskUpdateDisplay, "TASK_UPDATE_DISPLAY", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTaskUpdateDisplayHandle, APP_CPU_NUM);
    if(xTaskUpdateDisplayHandle == NULL){
        Serial.println("Erro na criação da TASK_UPDATE_DISPLAY");
        while(1);
    }
    xTaskCreatePinnedToCore(vTaskGetTemperature, "TASK_GET_TEMPERATURE", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTaskGetTemperatureHandle, PRO_CPU_NUM);
    if(xTaskGetTemperatureHandle == NULL){
        Serial.println("Erro na criação da TASK_GET_TEMPERATURE");
        while(1);
    }
    xTaskCreatePinnedToCore(vTaskGetHumidity, "TASK_GET_HUMIDITY", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTaskGetHumidityHandle, PRO_CPU_NUM);
    if(xTaskGetHumidityHandle == NULL){
        Serial.println("Erro na criação da TASK_GET_HUMIDITY");
        while(1);
    }
    xTaskCreatePinnedToCore(vTaskUpdateSerialMonitor, "TASK_UPDATE_SERIAL", configMINIMAL_STACK_SIZE + 1024, NULL, 2, &xTaskUpdateSerialMonitorHandle, PRO_CPU_NUM);
    if(xTaskUpdateSerialMonitorHandle == NULL){
        Serial.println("Erro na criação da TASK_UPDATE_SERIAL");
        while(1);
    }

}

void loop() {
    // Deleta task do loop (loop é uma task por padrão na arduinoIDE)
    vTaskDelete(NULL);
}

/*===============================================================================*/
// Definição das tasks

void vTaskAdjustServo(void *pvParameters){
    // Variáveis locais
    int16_t servoAngle = 90;
    int16_t angleVariation = 0;
    
    // Definição do ângulo inicial do servo
    ServoMotor.setAngle(servoAngle);

    while(1){
        // Espera por variação de ângulo através da fila
        xQueueReceive(xQueueAngleVariation, &angleVariation, portMAX_DELAY);
        
        // Ajuste do ângulo do servo
        servoAngle += angleVariation;

        // Limitação do ângulo do servo entre 0 e 180 graus
        if(servoAngle < 0){
            servoAngle = 0;
            xQueueReset(xQueueAngleVariation);
        }
        if(servoAngle > 180){
            servoAngle = 180;
            xQueueReset(xQueueAngleVariation);
        }
        
        // Definição do novo ângulo do servo
        ServoMotor.setAngle(servoAngle);
        
        // Envio do novo ângulo para atualização do display
        xQueueSend(xQueueAngleToDisplay, &servoAngle, portMAX_DELAY);
    }
}

void vTaskUpdateDisplay(void *pvParameters){
    // Variável local
    int16_t servoAngle = 0;

    while(1){
        // Espera por novo ângulo do servo através da fila
        xQueueReceive(xQueueAngleToDisplay, &servoAngle, portMAX_DELAY);

        // Atualização da mensagem do display com o ângulo atual do servo
        Display.updateMessage("Angulo: " + String(servoAngle));
    }
}

void vTaskGetTemperature(void *pvParameters){
    // Inicia o timer de obtenção de temperatura
    xTimerStart(xTimerGetTemperature, 0);

    // Variável local
    float temperature = 0;

    while(1){
        // Espera notificação do timer para obter a temperatura
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Obtém a temperatura do DHT
        temperature = DHT.getTemperature();

        // Envia a temperatura para o monitor serial através da fila
        xQueueSend(xQueueTemperatureToSerial, &temperature, portMAX_DELAY);
    }
}

void vTaskGetHumidity(void *pvParameters){
    // Inicia o timer de obtenção de humidade
    xTimerStart(xTimerGetHumidity, 0);

    // Variável local
    float humidity = 0;

    while(1){
        // Espera notificação do timer para obter a humidade
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Obtém a humidade do DHT
        humidity = DHT.getHumidity();

        // Envia a humidade para o monitor serial através da fila
        xQueueSend(xQueueHumidityToSerial, &humidity, portMAX_DELAY);
    }
}

void vTaskUpdateSerialMonitor(void *pvParameters){
    // Variáveis locais
    float humidity = 0, temperature = 0;

    while(1){
        // Espera por novos dados de humidade e temperatura através das filas
        xQueueReceive(xQueueTemperatureToSerial, &temperature, portMAX_DELAY);
        xQueueReceive(xQueueHumidityToSerial, &humidity, portMAX_DELAY);
        
        // Atualiza o monitor serial com os novos dados
        Serial.println("Temperatura: " + String(temperature) + "ºC || Humidade: " + String(humidity) + "%");
    }
}

void vCallBackTimerGetTemperature(xTimerHandle xTimer){
    // Notifica a task de obtenção de temperatura
    vTaskNotifyGiveFromISR(xTaskGetTemperatureHandle, NULL);
}

void vCallBackTimerGetHumidity(xTimerHandle xTimer){
    // Notifica a task de obtenção de humidade
    vTaskNotifyGiveFromISR(xTaskGetHumidityHandle, NULL);
}


void vISRAddAngle(){
    // Envio de variação positiva de ângulo para a fila ao pressionar o botão direito
    static int16_t angle = 5;
    xQueueSendToBackFromISR(xQueueAngleVariation, &angle, NULL);
}

void vISRSubAngle(){
    // Envio de variação negativa de ângulo para a fila ao pressionar o botão esquerdo
    static int16_t angle = -5;
    xQueueSendToBackFromISR(xQueueAngleVariation, &angle, NULL);
}
