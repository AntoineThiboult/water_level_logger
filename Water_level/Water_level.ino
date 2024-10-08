#include <Wire.h>     // For DS3231 real-time clock communication with the I2C protocol
#include <RTClib.h>   // For using the DS3231 RTC
#include <SD.h>       // Read from and write to SD cards
#include <NewPing.h>  // Eeasy interfacing of ultrasonic sensors
#include <avr/sleep.h>
#include <avr/power.h>

// Wiring
//  HC-SR04
//    Vcc: 5V board
//    Trig: D3 nano
//    Echo: D4 nano
//    Gnd: Gnd board
//  DS3231
//    SQW: D2 nano
//    SCL: A5 nano
//    SDA: A4 nano
//    Vcc: 5V board
//    Gnd: Gnd board
//  SD module
//    Vcc: 5V board
//    Gnd: Gnd board
//    MISO: D12
//    MOSI: D11
//    SCK: D13
//    CS: D10uuu

// Pin definitions
#define TRIG_PIN 3
#define ECHO_PIN 4
#define CS_PIN 10 // Chip select pin for the SD card
#define interruptPin  2 // Connect DS3231 SQW/INT to pin 2 (use any external interrupt pin)
#define LED_PIN 6 // LED pin to show error

// Constants for ultrasonic sensor
#define MAX_DISTANCE 500 // Maximum distance to measure (in cm)
#define SAMPLES 3        // Number of measurements to average

// Create objects
RTC_DS3231 rtc;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
File dataFile;
volatile bool alarmFlag = false;


// Function to take and average ultrasonic readings
float getAverageDistance() {
  float totalDistance = 0;
  for (int i = 0; i < SAMPLES; i++) {
    totalDistance += sonar.ping_cm();
    delay(500);
  }
  return totalDistance / SAMPLES;
}


// Function to write to SD card
void logData(float distance) {
  DateTime now = rtc.now();
  // Open the file for appending
  dataFile = SD.open("logs.txt", FILE_WRITE);
  if (dataFile) {
    // Write the time and distance to the file
    dataFile.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    dataFile.print(", ");
    dataFile.println(distance);
    dataFile.close(); // Close the file
  } else {
    Serial.println("Error opening logs.txt");
  }
}


// Interrupt Service Routine (ISR) for the RTC alarm
void alarmISR() {
  alarmFlag = true;  // Set the flag when the alarm triggers
}


// Function to put Arduino to sleep
void enterSleepMode() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();  // Go to sleep... Zzzzz
  // Sleeps until the interrupt is performed by RTC
  sleep_disable();  // Disable sleep mode after waking up
}


// Function to blink LED to indicate issue
void blink_error() {
  while (1){
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(interruptPin, INPUT_PULLUP); // Set interrupt pin as input with internal pull-up
  pinMode(LED_PIN, OUTPUT);
  

  //////////////////////////////
  /// SD card initialization ///
  //////////////////////////////

  if (SD.begin(CS_PIN)) {
    Serial.println("SD card successfully initialized");    
  }
  else {
    Serial.println("SD card initialization failed");
    blink_error();
  }

  // Write header
  dataFile = SD.open("logs.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println("timestamp, distance");
    dataFile.println("YYYY-MM-DDThh:mm:ss, cm");
    dataFile.close();
  } else {
    Serial.println("Error opening logs.txt");
  }


  ////////////////////////////////////
  // Real time clock initialization //
  ////////////////////////////////////

  // Initialize the RTC
  if (!rtc.begin()) {    
    Serial.println("Couldn't find RTC");
    blink_error();
  }

  DateTime now = rtc.now();
  Serial.print("Real time clock current time: ");
  Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));

  if (rtc.lostPower()) {
    // Synchronize RTC with the laptop time
    Serial.println("RTC lost power, setting the time!");  
    rtc.adjust(DateTime(__DATE__, __TIME__));
    DateTime now = rtc.now();
    Serial.print("Real time clock set at: ");
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
  }


  /////////////////////////////////////////
  // Alarm setting and interrupt setting //
  /////////////////////////////////////////

  // Disable any existing alarms
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  // Wait until beginning of the minute (second =0)
  // Set Alarm 1 to trigger every minute when seconds = 00
  while (1) {
    DateTime now = rtc.now();
    if (now.second() == 59) {
      rtc.setAlarm1(now + TimeSpan(0, 0, 1, 0), DS3231_A1_Second); // Set alarm to the start of the next minute
      break;
    }
  }

  // Clear any existing alarm flags
  rtc.clearAlarm(1);
  rtc.writeSqwPinMode(DS3231_OFF);  // Turn off square wave output (we want interrupts instead)
  
  // Interrupt setting
  attachInterrupt(digitalPinToInterrupt(interruptPin), alarmISR, FALLING);  // Attach the interrupt
}



void loop() {

  // Check if alarm has triggered
  if (alarmFlag) {
    alarmFlag = false;  // Reset the flag
    
    // Your code to execute on every minute
    DateTime now = rtc.now();
    Serial.print("Woke up at: ");
    Serial.println(now.timestamp());
    
    // Reset and re-enable the alarm for the next minute
    rtc.clearAlarm(1);
    rtc.setAlarm1(now + TimeSpan(0, 0, 1, 0), DS3231_A1_Second);  // Set alarm to the next minute

    // Measure distance
    float distance = getAverageDistance();
  
    // Log the data to the SD card
    logData(distance);
  }

  // Go to sleep until the next alarm triggers
  Serial.println("Entering sleep mode");
  enterSleepMode();
}

