#include <Arduino.h>
#include <bitset>
#include <U8g2lib.h>
#include <math.h>
#include <STM32FreeRTOS.h>

//Constants
const uint32_t interval = 100; //Display update interval
const uint32_t freq_a = 440; //frequency of the "a" note
const uint32_t sample_rate = 22000;
const float note_spacing = pow(2, (1/12));
const int index_a = 10;
volatile uint32_t currentStepSize;
volatile int note_string_index;
std::string note_string;


struct {
std::bitset<32> inputs;  
} sysState;
  
//rows of the matrix
const int num_cols = 32;
  
constexpr uint32_t step_size(uint32_t f) {
  uint32_t scalar = (pow(2, 32)) / sample_rate;
  return  scalar * f;
}

constexpr uint32_t construct_step_sizes(int index) {
  uint32_t frequency = (int32_t)(freq_a * pow(2, (index - index_a) / 12.0)); //shift by 12 (divide by 2, 12 times)
  return step_size(frequency);
}


  //Frequency Ranges 
constexpr uint32_t stepSizes [] = { 
  construct_step_sizes(0),
  construct_step_sizes(1),
  construct_step_sizes(2),
  construct_step_sizes(3),
  construct_step_sizes(4),
  construct_step_sizes(5),
  construct_step_sizes(6),
  construct_step_sizes(7),
  construct_step_sizes(8),
  construct_step_sizes(9),
  construct_step_sizes(10),
  construct_step_sizes(11)
};

const std::string names[] {
  "C",
  "C#",
  "D",
  "D#",
  "E",
  "F",
  "F#",
  "G",
  "G#",
  "A",
  "A#",
  "B"

};

//Pin definitions
//Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;

//Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;

//Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;

//Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;

//Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

//Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

//Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value) {
      digitalWrite(REN_PIN,LOW);
      digitalWrite(RA0_PIN, bitIdx & 0x01);
      digitalWrite(RA1_PIN, bitIdx & 0x02);
      digitalWrite(RA2_PIN, bitIdx & 0x04);
      digitalWrite(OUT_PIN,value);
      digitalWrite(REN_PIN,HIGH);
      delayMicroseconds(2);
      digitalWrite(REN_PIN,LOW);
}

//isr = interrupt service routine
void sampleISR() {
  static uint32_t phaseAcc = 0;
  //Serial.println(currentStepSize);
  phaseAcc += currentStepSize;
  int32_t Vout = (phaseAcc >> 24) - 128;
  analogWrite(OUTR_PIN, Vout + 128);
}

void setRow(uint8_t rowIdx){
  
  digitalWrite(REN_PIN, LOW);
  delayMicroseconds(3);
  switch(rowIdx) {

    case 0: {   digitalWrite(RA2_PIN, LOW);    digitalWrite(RA1_PIN, LOW);    digitalWrite(RA0_PIN, LOW);  break;  } //0
    case 1: {   digitalWrite(RA2_PIN, LOW);    digitalWrite(RA1_PIN, LOW);    digitalWrite(RA0_PIN, HIGH); break;  } //1
    case 2: {   digitalWrite(RA2_PIN, LOW);    digitalWrite(RA1_PIN, HIGH);   digitalWrite(RA0_PIN, LOW);  break;  } //2
    case 3: {   digitalWrite(RA2_PIN, LOW);    digitalWrite(RA1_PIN, HIGH);   digitalWrite(RA0_PIN, HIGH); break;  } //3
    case 4: {   digitalWrite(RA2_PIN, HIGH);   digitalWrite(RA1_PIN, LOW);    digitalWrite(RA0_PIN, LOW);  break;  } //4
    case 5: {   digitalWrite(RA2_PIN, HIGH);   digitalWrite(RA1_PIN, LOW);    digitalWrite(RA0_PIN, HIGH); break;  } //5
    case 6: {   digitalWrite(RA2_PIN, HIGH);   digitalWrite(RA1_PIN, HIGH);   digitalWrite(RA0_PIN, LOW);  break;  } //6
    case 7: {   digitalWrite(RA2_PIN, HIGH);   digitalWrite(RA1_PIN, HIGH);   digitalWrite(RA0_PIN, HIGH); break;  } //7

    default:{   digitalWrite(RA2_PIN, LOW);    digitalWrite(RA1_PIN, LOW);    digitalWrite(RA0_PIN, HIGH);    } //default
  }

  digitalWrite(REN_PIN, HIGH);


}
  
std::bitset<num_cols> readCols(){
  //Write a function that will read the inputs from the four columns of the 
  //switch matrix (C0, C1, C2, C3) and return the four bits as a 
  //C++ bitset, which is a fixed-sized vector of Booleans.
  std::bitset<num_cols> result; 
  note_string = "";

  uint32_t localCurrentStepSize = 0;
  __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);
  for (int i = 0; i < 3; i++) {
    
    setRow((uint8_t)i);
    result[(4 * i) + 0] = digitalRead(C0_PIN);
    if (!digitalRead(C0_PIN)) {note_string = note_string + names[(4 * i) + 0] + " "; localCurrentStepSize = stepSizes[(4 * i) + 0];}
    result[(4 * i) + 1] = digitalRead(C1_PIN);
    if (!digitalRead(C1_PIN)) {note_string = note_string + names[(4 * i) + 1] + " "; localCurrentStepSize = stepSizes[(4 * i) + 1];}
    result[(4 * i) + 2] = digitalRead(C2_PIN);
    if (!digitalRead(C2_PIN)) {note_string = note_string + names[(4 * i) + 2] + " "; localCurrentStepSize = stepSizes[(4 * i) + 2];}
    result[(4 * i) + 3] = digitalRead(C3_PIN);
    if (!digitalRead(C3_PIN)) {note_string = note_string + names[(4 * i) + 3] + " "; localCurrentStepSize = stepSizes[(4 * i) + 3];}

  }
  // for (int i = 0; i < sizeof(stepSizes) / sizeof(stepSizes[0]); ++i) {
  //   Serial.println(result[i]);
  // }
  //Serial.println(localCurrentStepSize);
  __atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
  return result;

} 


void scanKeysTask(void * pvParameters) {
  const TickType_t xFrequency = 50/portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
    sysState.inputs = readCols();
  }
}

void setup() {
  // put your setup code here, to run once:
  note_string_index = 0;
  //Set pin directions
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  //Initialise display
  setOutMuxBit(DRST_BIT, LOW);  //Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);  //Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);  //Enable display power supply

  //Initialise UART
  Serial.begin(9600);
  Serial.println("Hello World");

  TIM_TypeDef *Instance = TIM1;
  HardwareTimer *sampleTimer = new HardwareTimer(Instance);
  sampleTimer->setOverflow(22000, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(sampleISR);
  sampleTimer->resume();
  /*
  for (int i = 0; i < sizeof(stepSizes) / sizeof(stepSizes[0]); ++i) {
    Serial.println(stepSizes[i]);
  }



  for (int i = 0; i < sizeof(stepSizes) / sizeof(stepSizes[0]); ++i) {
    Serial.println((float)((i - index_a)/ 12.0));
    Serial.println((int32_t)(freq_a * pow(2, (i - index_a) / 12.0))); //shift by 12 (divide by 2, 12 times));
  }
  */

  TaskHandle_t scanKeysHandle = NULL;
  xTaskCreate(
    scanKeysTask,		/* Function that implements the task */
    "scanKeys",		/* Text name for the task */
    64,      		/* Stack size in words, not bytes */
    NULL,			/* Parameter passed into the task */
    1,			/* Task priority */
    &scanKeysHandle 
  );	/* Pointer to store the task handle */
  vTaskStartScheduler();
}




void loop() {
  // put your main code here, to run repeatedly:
  static uint32_t next = millis();
  static uint32_t count = 0;
  while (millis() < next);  //Wait for next interval
  next += interval;
  //scanKeysTask(NULL);
  //sysState.inputs = readCols();




  //Update display
  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawStr(2,10,"Helllo World!");  // write something to the internal memory
  //u8g2.print(count++);
  u8g2.setCursor(2,20);
  u8g2.print(sysState.inputs.to_ulong(),BIN);
  //u8g2.print(inputs.to_string()); 

  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawStr(3,30,note_string.c_str());  // write something to the internal memory

  u8g2.sendBuffer();          // transfer internal memory to the display

  //Toggle LED
  digitalToggle(LED_BUILTIN);
  
}