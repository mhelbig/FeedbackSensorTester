// Libraries:
#include <LiquidCrystal.h>
#include <avr/eeprom.h>

// Constants:
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);                   // LCD I/O assignments
const unsigned long EventInterval = 60;                  // Send event string every 60 seconds
const unsigned int SensorPPR = 60;                       // DUT sensor pulses per revolution
const unsigned int SensorRotationError = 2;              // If there are more than this much difference in counts, the test stops
const unsigned int MinimumTesterRPM = 300;               // If RPM drops below this value, the test stops
const unsigned long NumberOfRotationsToTest = 100000;    // Sets the number of rotations to run before stopping the test

// Global Variables:
char SerialCommand;                              // Character buffer from serial port
bool TestRunning = 0;                            // Flag for whether the test is running or not
unsigned long EventLogTimer=0;                   // Event data Timer (in seconds) for serial output interval
unsigned long SensorCounts = 0;                  // Accumulator for sensor counts
long ShaftRotations = 0;                         // Accumulator for shaft rotations
long SensorRotations = 0;                        // Accumulator for sensor rotations
uint32_t EEMEM NonVolatileShaftCounts;           // EEPROM storage for shaft counts
int ButtonState = 0;                             // On/Off button state
int LastButtonState = 0;                         // Previous On/Off button state
int Mode = 0;                                    // On/Off state of the motor

// I/O Ports:
const int ShaftSensor = 2;
const int FB60Sensor = 3;
const int Motor = 4;
const int LED = 13;
const int PushButton = 12;


void setup()
{                
  // I/O pin setup:
  pinMode(ShaftSensor, INPUT);
  pinMode(FB60Sensor, INPUT);     
  pinMode(Motor, OUTPUT);     
  pinMode(LED, OUTPUT);     
  pinMode(PushButton, INPUT);
  digitalWrite(PushButton, HIGH);               // Enable the built-in pullup resistor on the pushbutton
  
  // LCD setup
  lcd.begin(16, 2);                             // set up the LCD's number of columns and rows

  // Interrupt Setup:
  attachInterrupt(0,AccumulateShaftRotations,RISING);
  attachInterrupt(1,AccumulateSensorRotations,FALLING);

  // Serial port setup:
  Serial.begin(9600);

  // read in the total number of shaft counts from the EEPROM as a starting point
  ShaftRotations = eeprom_read_dword(&NonVolatileShaftCounts);
  SensorRotations = ShaftRotations;  // These need to start out the same, or we'll get a mismatch error on startup

  PrintEventHeader();
  PrintEventData();
}

void loop()
{
  if (Serial.available())
  {
    SerialCommand = Serial.read();
    
    switch (SerialCommand)
    {
      case '1':                                      // 1 = Start the test
        StartMotor();
        break;
        
      case '0':                                      // 0 = Stop the test
        StopMotor();
        break;
        
      case 'r':                                      // r or R = reset counts
      case 'R':
        Serial.println("Counts Reset by user");
        ResetSensorCounts();
        break;
    }  
  }
  
  // Check for someone pressing the On/Off button:
  ButtonState = digitalRead(PushButton);
  
  if(ButtonState ==0 && LastButtonState ==1)
  {
    if (Mode ==0)
    {
      StartMotor();
      Mode = 1;
    }
    else
    {
      StopMotor();
      Mode = 0;
    }
  }

  LastButtonState = ButtonState;
  
  if (TestRunning)
  {
    PrintEventData();
    
 // Check for any discrepancy between the shaft and sensor rotations and stop the test if so
    if (abs(ShaftRotations - SensorRotations) >=SensorRotationError)
    {
      Serial.println("Test stopped by Sensor Count Error");
      Serial.print("Shaft Rotations: ");
      Serial.println(ShaftRotations);
      Serial.print("Sensor Rotations: ");
      Serial.println(SensorRotations);
      
      TestRunning = 0;
      SetMotor(LOW);
    }
    
// Check number of revolutions to see if we should stop the test
    if (ShaftRotations >= NumberOfRotationsToTest)
    {
      Serial.println("Test stopped at predetermined number of revolutions");
      Serial.print("Shaft Rotations: ");
      Serial.println(ShaftRotations);
      Serial.print("Sensor Rotations: ");
      Serial.println(SensorRotations);
      
      ShaftRotations = 0;        // Reset these for the next test
      SensorRotations =  0;
      
      TestRunning = 0;
      SetMotor(LOW);
    }

// Check for adequate RPM and stop the test if speed is too low
  //not sure how to do this, since we have to account for stopping and starting...

  }
}

void StartMotor(void)
{
  EventLogTimer = Seconds();                   // Synchronize the event timer to the current seconds time
  TestRunning = 1;
  SetMotor(HIGH);
  Serial.println("Test Started by user");
}

void StopMotor(void)
{
  TestRunning = 0;
  SetMotor(LOW);
  eeprom_write_dword(&NonVolatileShaftCounts,ShaftRotations);  // Store the shaft rotations to EEPROM
  Serial.println("Test Paused by user");
}

void ResetSensorCounts(void)
{
  ShaftRotations = 0;
  
  if (TestRunning)                // Do a dynamic sync of the shaft and sensor counts
    {
      while(ShaftRotations == 0)       // Wait for the next shaft pulse so we can sync the sensor pulses with it
      {
        delay(1);                      // I needed to put something here otherwise the while() never processed
      }
    }
  
  SensorCounts = SensorPPR/2;      // Preload the sensor counts half a turn ahead of the shaft counts to prevent jitter
  SensorRotations = 0;             // Reset the sensor counts
  ShaftRotations = 0;              // Reset the shaft counts
}

void PrintEventHeader(void)
{
  Serial.println("FB60 Endurance Tester.  Command set:");
  Serial.println("  '1' = Start Test");
  Serial.println("  '0' = Pause Test");
  Serial.println("  'r' = Reset Counts");
  Serial.println("Minutes,ShaftRotations,SensorRotations,RotationError");

  lcd.setCursor(0,0);       //Set cursor to first position on first row
  lcd.print(" Shaft:");     // Print a header message on the LCD.
  lcd.setCursor(0,1);       //Set cursor to first position on second row
  lcd.print("Sensor:");     // Print a header message on the LCD.
}

void PrintEventData(void)
{
  long CurrentShaftCount;
  
  lcd.setCursor(8,0);              //Set cursor to first position on second row
  lcd.print(ShaftRotations);
  lcd.print("  ");
  lcd.setCursor(8,1);              //Set cursor to rotations position on second row
  lcd.print(SensorRotations);
  lcd.print("  ");
  
  if (EventLogTimer < Seconds())
  {
    CurrentShaftCount = ShaftRotations;
    
    while(CurrentShaftCount == ShaftRotations)
    {
      delay(1);
    }
    
    EventLogTimer += EventInterval;
    Serial.print(Seconds()/60);
    Serial.print(",");
    Serial.print(ShaftRotations);
    Serial.print(",");
    Serial.print(SensorRotations);
    Serial.print(",");
    Serial.print(ShaftRotations - SensorRotations);
    Serial.println();

    eeprom_write_dword(&NonVolatileShaftCounts,ShaftRotations);  // Store the shaft rotations to EEPROM each minute
    // This should last 1.9 years @ 24/7 operation for 1,000,000 EEPROM write cycles
  }
}

void AccumulateShaftRotations(void)
{
  ShaftRotations++;
}

void AccumulateSensorRotations(void)
{
  SensorCounts++;
  if (SensorCounts >= SensorPPR)
  {
    SensorCounts = 0;
    SensorRotations++;
  }
}

unsigned long Seconds(void)
{
  return (millis()/1000);
}

void SetLED(bool State)
{
  if(State == HIGH) digitalWrite(LED, HIGH);
  else digitalWrite(LED,LOW);
}

void SetMotor(bool State)
{
  if(State == HIGH) digitalWrite(Motor, HIGH);
  else digitalWrite(Motor, LOW);
}
