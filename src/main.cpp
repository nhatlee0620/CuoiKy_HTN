#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
//#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <queue.h>

/*
 *Chân VSS, RW, K - GND
 *Chân VDD, A - 5v
 *V0 - chân tín hiệu biến trở
 *RS - 12 
 *E - 11
 *D4 - 10
 *D5 - 4
 *D6 - 3
 *D7 - 2
*/

//LiquidCrystal_I2C lcd(0x27,16,2); //Khai báo địa chỉ I2C (0x27 or 0x3F) và LCD 16x02
LiquidCrystal lcd(12, 11, 10, 4, 3, 2); //Các chân điều khiển LCD

int buzzer = 6;
int lamp = 7;
int fan = 8;
int IR_sensorIn = 5;
int IR_sensorOut = 9;
unsigned int people = 0;
struct pinRead
{
  int pin;
  float value;
};

QueueHandle_t structQueue;
QueueHandle_t intQueue;

void readGasSensor(void *pvParameters)
{
  (void) pvParameters; 
  for (;;)
  {   
    struct pinRead currentPinRead; 
    currentPinRead.pin = 0; 
    currentPinRead.value = analogRead(A0);        
    // Read adc value from A0 channel and store it in value element of struct                                              
    xQueueSend(structQueue, &currentPinRead, portMAX_DELAY);
    //xQueueSend(structQueue, &currentPinRead, 0);  
    Serial.println("Read GAS value from sensor (A0)"); 
    taskYIELD();
    vTaskDelay(350/portTICK_PERIOD_MS);
  }
}

void readTemperatureSensor(void *pvParameters)
{
  (void) pvParameters;
  
  for (;;)
  {   
    struct pinRead currentPinRead;
    currentPinRead.pin = 1;
    currentPinRead.value = analogRead(A1)*500.0/1024.0; 
    // Read adc value from A1 channel and store it in value element of struct
    // 1mV = 1'C
    xQueueSend(structQueue, &currentPinRead, portMAX_DELAY);
    Serial.println("");
    Serial.println("Read temperature from LM35 sensor(A1)");
    taskYIELD(); 
    vTaskDelay(450/portTICK_PERIOD_MS);
  }
}

void peopleCouter(void *pvParameters)
{
  for(;;)
  {
    int sensorInState = digitalRead(IR_sensorIn);
    int sensorOutState = digitalRead(IR_sensorOut);
    if(sensorInState == LOW)
    {
      people++;
      delay(100);
    }
    if(sensorOutState == LOW && people > 0)
    {
      people--;
      delay(100);
    }
    xQueueSend(intQueue, &people, portMAX_DELAY == pdPASS);
    vTaskDelay(600/portTICK_PERIOD_MS);
  }
}

void Turn_on_Light(void *pvParameters)
{
  int receive_Value = 0;
  for(;;)
  {
    if(xQueueReceive(intQueue, &receive_Value, portMAX_DELAY) == pdPASS)
    {
      Serial.print("People:");
      Serial.println(receive_Value);
      lcd.setCursor(0,0);
      lcd.print("People: ");
      lcd.setCursor(7,0);
      lcd.print(receive_Value);
      if(receive_Value>0)
      {
        Serial.println("LED 2 ON");
        digitalWrite(lamp,HIGH);
      }
      else
      {
        Serial.println("LED 2 OFF");
        digitalWrite(lamp,LOW);
      }
    }
    taskYIELD();
    vTaskDelay(250/portTICK_PERIOD_MS);
  }
}

//Receive task
void alertTask(void * pvParameters)
{
  (void) pvParameters; 
  for (;;) 
  {
   float GAS = 0; 
   float temp = 0;
   struct pinRead currentPinRead; 
   if (xQueueReceive(structQueue, &currentPinRead, portMAX_DELAY) == pdPASS)
   //if (xQueueReceive(structQueue, &currentPinRead, 0) == pdPASS)
    {
      Serial.print("Pin: ");   
      Serial.print(currentPinRead.pin);
      Serial.print(" Value: ");
      Serial.println(currentPinRead.value);
      if(currentPinRead.pin == 0)
      {        
        GAS = currentPinRead.value; 
        lcd.setCursor(10,0);
        lcd.print("G:");
        if(GAS>300) {
          lcd.setCursor(13, 0);
          lcd.print("WAN");
          digitalWrite(buzzer,HIGH);
        }
        else{
          lcd.setCursor(13, 0);
          lcd.print("NOR");
          digitalWrite(buzzer,LOW);
        }
      }
      if(currentPinRead.pin == 1)
      {
        temp = currentPinRead.value;
        Serial.print("TEMP: ");
        Serial.println(temp);
        lcd.setCursor(0,1);
        lcd.print("TEMP:");
        if(temp>30)
        {
          lcd.setCursor(6,1);
          lcd.print(temp);
          digitalWrite(fan,HIGH);
        }
        else
        {
          lcd.setCursor(6,1);
          lcd.print(temp);
          digitalWrite(fan,LOW);
        }
      }
    }
    taskYIELD(); // terminate the task and inform schulder about it
    vTaskDelay(150/portTICK_PERIOD_MS);
  }
}
  
void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2); //Khai báo LCD 16x02
  lcd.backlight();
  // lcd.init();
  pinMode(buzzer,OUTPUT);
  pinMode(lamp,OUTPUT);
  pinMode(fan,OUTPUT);
  pinMode(9,INPUT);
  pinMode(5,INPUT);
  pinMode(A0,INPUT);
  pinMode(A1,INPUT);
  intQueue = xQueueCreate(15, sizeof(int));
  if(intQueue!=NULL)
  {
    xTaskCreate(peopleCouter, "Displaydata1", 256, NULL, 1, NULL);
    xTaskCreate(Turn_on_Light, "Turn_on_the_light", 256, NULL, 2, NULL);
  }
  structQueue = xQueueCreate(15, // Queue length
                sizeof(struct pinRead)); // Queue item size                     
  if(structQueue!=NULL)
  {
    // Create task that consumes the queue if it was created.
    xTaskCreate(alertTask,       // Task function
                "Displaydata2", // Task name
                256,  // Stack size
                NULL, //Task parameters
                2, // Priority
                NULL); //task handle
    // Create task that publish data in the queue if it was created.
    xTaskCreate(readGasSensor, // Task function
                "readGasSensor", // Task name
                256, // Stack size
                NULL, //Task parameters
                1, // Priority
                NULL); //task handle
    xTaskCreate(readTemperatureSensor, // Task function
                "ReadTempSensor", // Task name
                256, // Stack size
                NULL, //Task parameters
                1, // Priority
                NULL); //task handle
  }
  vTaskStartScheduler();
}

void loop() {
  // put your main code here, to run repeatedly:
}

