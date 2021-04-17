
    ///////////////////////////////////////////////////////////////////////////////////////////
   ///                                                                                     ///
  ///              DIY DATALOGGER FOR GROUND TEMPERATURE MONITORING                       ///
 ///                                                                                     ///
/////////////////////////////////////////////////////////////////////////////////////////// 

/*  TL:DR
  Basic run down of how this works: There's a bootloded ATmega238p which controls the 
  system. One cycle = temperature is recorded from the DS18b20 temp probe (+- 0.5 degree
  between -10 -> +85 degrees celcius). This value is combined with a timestamp from the 
  RTC (Real time clock) and written to an SD card. The system then sets an alarm and goes
  to sleep to conserve power (current draw in sleep ~0.24mA) and wakes up to repeat the
  above cycle on the alarm. 

  Coded by Matthew Allison, PhD researcher at the University of Birmingham, UK.
  Please feel free to get in contact: mxa415@student.bham.ac.uk
*/

#include <SparkFun_RV1805.h>    // RTC library from Sparkfun
#include <Wire.h>               // Allows for I2C comms
#include <avr/sleep.h>          // Sleep modes
#include <SPI.h>                // Allows for SD card comms
#include <SD.h>                 // SD card library 
#include <OneWire.h>            // Temp probe data comms
#include <DallasTemperature.h>  // Temp probe library

//RTC module shortcut
RV1805 rtc;
// Data wire is plugged into digital pin 5 on the Arduino
#define ONE_WIRE_BUS 5
// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  
// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

//Setting up important pins
      // CS-Pin for the SD card
      const int chipSelect = 10;
      // The pin that is connected to RTC INT
      const int alarmPin = 3;
      // 5V rail SHDW pin
      const int regulator = 6;

///////////////////////////////////////////////////////////////////////////////////////////////////
//----------------------------------    User Input     ------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////////////

  /******************************** 
    Mode must be between 0 and 7 to tell when the alarm should be triggered.
    Alarm is triggered when listed characteristics match
    0: Disabled
    1: seconds, minutes, hours, date and month match (once per year)
    2: seconds, minutes, hours and date match (once per month)
    3: seconds, minutes, hours and weekday match (once per week)
    4: seconds, minutes and hours match (once per day)
    5: seconds and minutes match (once per hour)
    6: seconds match (once per minute)
    7: once per second
  ********************************/
  // Choose your alarm mode from the list above.
  const int AlarmMode = 6;

  // Choose your alarm timestamps. These are the reference integers used by the selected alarm mode
  // Alarm timestamps
      byte secondsAlarm = 30;
      byte minuteAlarm = 0;
      byte hourAlarm = 0;
      byte dateAlarm = 0;
      byte monthAlarm = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------    Setup    ---------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  // Begin comms
  Wire.begin();
  // Standard Arduino guff
  Serial.begin(9600);
  Serial.println("DIY Datalogger, stting up...");

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // Initialize the RTC
  if (rtc.begin() == false) {
    Serial.println("Something went wrong, check wiring");}

// Below code is greyed out but it lets you set the RTC time to the compile time. It is useful,
// but should the RTC ever lose power it'll reset to compile rather than persist with time keeping
// This code requires you to set the time outside of this sketch and then all this sketch does is
// read the time off the RTC.
/*
  // Use the time from the Arduino compiler (build time) to set the RTC
  // Keep in mind that Arduino does not get the new compiler time every time it compiles. 
  // To ensure the proper time is loaded, open up a fresh version of the IDE and load the sketch.
  if (rtc.setToCompilerTime() == false) {
    Serial.println("Something went wrong setting the time");
  }
*/ 

  // Disable ADC, power saving measure (surpisingly effective)
  ADCSRA = 0;  

  // An important part of this design is the ability to switch power on/off on the 5V rail and
  // connected devices to save power in sleep (SD card and temp probe). Regulator has a built in
  // SHDW pin to allow this. 

  // A useful problem solving tip. Connect an LED up to the 5V rail with a resistor and you'll
  // be able to use this to see when the system is awake and active or sleeping
  Serial.print("Setting up 5V rail control...");  
  pinMode(regulator,INPUT);
  Serial.print("5V rail on...");
  digitalWrite(regulator, HIGH);
  delay(2000);

  // Initialize the SD card
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
        // 5V rail won't turn off if there is a problem. 5V indicator LED will persist.
        digitalWrite(regulator, HIGH);        
    while (1);
  }
  Serial.println("card initialized."); 


  //sets the alarm pin as pullup 
  pinMode(alarmPin, INPUT_PULLUP);
  
  //Sets the alarm with the values initialized at the start of code
  rtc.setAlarm(secondsAlarm, minuteAlarm, hourAlarm, dateAlarm, monthAlarm); 
  rtc.setAlarmMode(AlarmMode); // Sets the alarm mode from user input
  rtc.enableInterrupt(INTERRUPT_AIE); //Enable the Alarm Interrupt
  
}      

///////////////////////////////////////////////////////////////////////////////////////////////////
//----------------------------------------   Loop   ---------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  
  //Update RTC time
  if (rtc.updateTime() == false) //Updates the time variables from RTC
  {
    Serial.print("RTC failed to update");
  }

  //Turning on 5V rail
  Serial.print("5V rail -> on...");
  digitalWrite(regulator, HIGH);
  // Very small delay to ensure all 5V components are active.
  delay(500);

  Serial.print("Re-initializing SD card...");

  // see if the card is present and can be initialized:
      if (!SD.begin(chipSelect)) {
      Serial.println("Card failed, or not present");
      // don't do anything more:
      digitalWrite(regulator,HIGH);
      while (1);
  }
  Serial.println("card re-initialized.");
  
  // Make a string for assembling the data to log:
  String datastring = "";
  
  //String currentDate = rtc.stringDateUSA(); //Get the current date in mm/dd/yyyy format (we're weird)
  String currentDate = rtc.stringDate(); //Get the current date in dd/mm/yyyy format
  String currentTime = rtc.stringTime(); //Get the time

  // Collect time information
  datastring += currentDate;
  datastring += String("  ");  
  datastring += currentTime;
  datastring += String("  ");

  // Retrieves the temperature from DS18B20
  sensors.requestTemperatures(); 
  float Current_Temp = (sensors.getTempCByIndex(0));
  datastring += String(Current_Temp);
  datastring += String("  ");

  Serial.println(datastring);

  //Makes sure to clear any outstanding flags.
  byte rtcStatus = rtc.status(); //Get the latest status from RTC. Reading the status clears the flags
  if(rtcStatus != 0)
  {
    Serial.println("An interrupt has occured: ");
  }

  // Writing to SD card
      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      File dataFile = SD.open("datalog.txt", FILE_WRITE);

      // if the file is available, write to it:
      if (dataFile) {
        dataFile.println(datastring);
        dataFile.close();
      }
      // if the file isn't open, pop up an error:
      else {
        Serial.println("error opening datalog.txt");
      }

      //Turning off 5V rail
      Serial.print("5V rail -> off...");
      digitalWrite(regulator, LOW);
      delay(100);

      //sssh he's sleeping now....
      enterSleep();
}

void enterSleep(){
  sleep_enable();                       // Enabling sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Setting the sleep mode, in this case full sleep
  
  noInterrupts();                       // Disable interrupts
  attachInterrupt(digitalPinToInterrupt(alarmPin), alarm_ISR, LOW);
  
  Serial.println("Going to sleep!");    // Print message to serial monitor      //digitalWrite(LED, HIGH); 
  Serial.flush();                       // Ensure all characters are sent to the serial monitor
  
  interrupts();                         // Allow interrupts again
  sleep_cpu();                          // Enter sleep mode

  /* The program will continue from here when it wakes */
  
  // Disable and clear alarm
  //rtc.disableAlarm(1);
  //rtc.clearAlarm(1);
  
  Serial.println("I'm back!");          // Print message to show we're back
 }    


void alarm_ISR() {
  // This runs when SQW pin is low. It will wake up the µController
  
  sleep_disable(); // Disable sleep mode
  detachInterrupt(digitalPinToInterrupt(alarmPin)); // Detach the interrupt to stop it firing
}
