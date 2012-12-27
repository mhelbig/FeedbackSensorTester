// Constants:
const unsigned long EventInterval = 60;          // Send event string every 60 seconds
const unsigned int SensorPPR = 60;               // DUT sensor pulses per revolution

// Global Variables:
char SerialCommand;                              // Character buffer from serial port
bool TestRunning = 0;                            // Flag for whether the test is running or not
unsigned long EventLogTimer=0;                   // Event data Timer (in seconds) for serial output interval
unsigned long SensorCounts = 0;                  // Accumulator for sensor counts
long ShaftRotations = 0;                         // Accumulator for shaft rotations
long SensorRotations = 0;                        // Accumulator for sensor rotations

// I/O Ports:
const int ShaftSensor = 2;
const int FB60Sensor = 3;
const int Motor = 4;
const int LED = 13;

void setup()
{                
  // I/O pin setup:
  pinMode(ShaftSensor, INPUT);
  pinMode(FB60Sensor, INPUT);     
  pinMode(Motor, OUTPUT);     
  pinMode(LED, OUTPUT);     

  // Interrupt Setup:
  attachInterrupt(0,AccumulateShaftRotations,RISING);
  attachInterrupt(1,AccumulateSensorRotations,FALLING);

  // Serial port setup:
  Serial.begin(9600);

  PrintEventHeader();
}

void loop()
{
  if (Serial.available())
  {
    SerialCommand = Serial.read();
    
    switch (SerialCommand)
    {
      case '1':                                      // 1 = Start the test
        EventLogTimer = Seconds();                   // Synchronize the event timer to the current seconds time
        TestRunning = 1;
        SetMotor(HIGH);
        Serial.println("Test Started by user");
        break;
        
      case '0':                                      // 0 = Stop the test
        TestRunning = 0;
        SetMotor(LOW);
        Serial.println("Test Paused by user");
        break;
        
      case 'r':                                      // r or R = reset counts
      case 'R':
        Serial.println("Counts Reset by user");
        ResetSensorCounts();
        break;
    }  
  }

  if (TestRunning)
  {
    PrintEventData();
    
    if (abs(ShaftRotations - SensorRotations) >=2)
    {
      Serial.println("Test stopped by Sensor Count Error");
      Serial.print("Shaft Rotations: ");
      Serial.println(ShaftRotations);
      Serial.print("Sensor Rotations: ");
      Serial.println(SensorRotations);
      
      TestRunning = 0;
      SetMotor(LOW);
    }
  }
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
}

void PrintEventData(void)
{
  long CurrentShaftCount;
  
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
  }
}

void AccumulateShaftRotations(void)
{
  ShaftRotations++;
}

void AccumulateSensorRotations(void)
{
  SensorCounts++;
  if (SensorCounts >= 60)
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
