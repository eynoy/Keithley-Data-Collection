// Pins connected to the MAX3232 which connects to the RS232 connection
#define PIN_TXD 8
#define PIN_RXD 9

// Pins connected to the measurement switch
#define SWITCH_POWER A3
#define SWITCH_INPUT A4
#define SWITCH_GROUND A5

#define KEITHLEY_INTERNAL_NAN_VALUE 9.91e37

// The follow macros are for allowing SCPI commands to be used and viewed when a computer connects to the arduino
// #define COMPUTER_CONNECTED
// #define COMPUTER_CONNECTED_INFO_DEBUG

/*
  COMMAND STARTUP SEQUENCE (Reference from https://download.tek.com/manual/2400S-900-01_K-Sep2011_User.pdf):

  *RST                        1. Resets settings and commands
  *CLS                        2. Clear all messages and errors

  :SOUR:FUNC VOLT             3. Select Voltage generator function
  :SOUR:VOLT:MODE FIX         4. Select Voltage fixed mode
  :SOUR:VOLT:RANG 1000        5. Select 1 kiloVolt range
  :SOUR:VOLT:LEV 1000         6. Set Voltage source at 1kV

  :SENS:FUNC "CURR"           7. Select Current sensing function
  :SENS:CURR:PROT 1.05e-6     8. Set Current compliance limit at 1.05 microAmps 
  :SENS:CURR:RANG 1.00e-6     9. Select 1.0 microAmp range

  :ARM:SOUR IMM               10. Arm Source: Immediate
  :ARM:COUN 1                 11. Take 1 Measurement       

  :TRIG:SOUR IMM              12. Trigger Source: Immediate
  :TRIG:COUN 1                13. Take 1 Measurement
  :TRIG:DEL MIN               14. Trigger delay set to minimum

  ******************************

  :OUTP ON                    
  :READ?

  ******************************

  ALL IN ONE:

  *RST;*CLS;:SOUR:FUNC VOLT;:SOUR:VOLT:MODE FIX;:SOUR:VOLT:RANG 1000;:SOUR:VOLT:LEV 1000;:SENS:FUNC "CURR";:SENS:CURR:PROT 1.05e-6;:SENS:CURR:RANG 1.00e-6;:ARM:SOUR IMM;:ARM:COUN 1;:TRIG:SOUR IMM;:TRIG:COUN 1;:TRIG:DEL MIN

*/

#include <SoftwareSerial.h>

SoftwareSerial outSerial(PIN_RXD, PIN_TXD, false);
const char startup_command[250] = "*RST;*CLS;:SOUR:FUNC VOLT;:SOUR:VOLT:MODE FIX;:SOUR:VOLT:RANG 1000;:SOUR:VOLT:LEV 1000;:SENS:FUNC \"CURR\";:SENS:CURR:PROT 1.05e-6;:SENS:CURR:RANG 1.00e-6;:ARM:SOUR IMM;:ARM:COUN 1;:TRIG:SOUR IMM;:TRIG:COUN 1;:TRIG:DEL MIN";
const char read_command[7] = ":READ?";
const char beep_command[19] = ":SYST:BEEP 500,0.5"; // A beep at 500 Hz for 0.5 seconds56

void setup() {
  pinMode(SWITCH_GROUND, OUTPUT); digitalWrite(SWITCH_GROUND, LOW);
  pinMode(SWITCH_POWER, OUTPUT); digitalWrite(SWITCH_POWER, HIGH);
  pinMode(SWITCH_INPUT, INPUT_PULLUP);

#ifdef COMPUTER_CONNECTED
  // Open serial communications and wait for port to open:
  Serial.begin(57600);
  while (!Serial) {
    ;
  }
#endif

  // Set the data rate for the SoftwareSerial port -- must match baud setting on the Keithley
  outSerial.begin(57600);

  outSerial.println(startup_command);
}

char inputBuffer[500];
int inputBuffercurrentIndex = 0;
void addToInputBuffer(char recievedChar) {
  // Collect characters recieved from Keithley untill a new line
  inputBuffer[inputBuffercurrentIndex] = recievedChar;
  inputBuffercurrentIndex++;

  if (recievedChar == '\n' || inputBuffercurrentIndex >= 500) {
#ifdef COMPUTER_CONNECTED_INFO_DEBUG
    Serial.print("Buffer Contains: \n\t"); Serial.println(inputBuffer);
#endif

    // Check that a reading is recieved (always begins with a positive voltage with a 'plus' char)
    if (inputBuffer[0] = '+') {
      // Split string into component number strings (separated by commas)
      char voltage_str[30], current_str[30], resistance_str[30], timestamp_str[30], status_word_str[30];
      
      int inputStrCharIndex = 0;
      int outputStrCharIndex = 0;
      char currentStrChar = '0';
      while (currentStrChar != ',') { currentStrChar = inputBuffer[inputStrCharIndex++]; voltage_str[outputStrCharIndex++] = currentStrChar; }
      voltage_str[outputStrCharIndex] = 0; outputStrCharIndex = 0; currentStrChar = '0'; while (currentStrChar != ',') { currentStrChar = inputBuffer[inputStrCharIndex++]; current_str[outputStrCharIndex++] = currentStrChar; }
      current_str[outputStrCharIndex] = 0; outputStrCharIndex = 0; currentStrChar = '0'; while (currentStrChar != ',') { currentStrChar = inputBuffer[inputStrCharIndex++]; resistance_str[outputStrCharIndex++] = currentStrChar; }
      resistance_str[outputStrCharIndex] = 0; outputStrCharIndex = 0; currentStrChar = '0'; while (currentStrChar != ',') { currentStrChar = inputBuffer[inputStrCharIndex++]; timestamp_str[outputStrCharIndex++] = currentStrChar; }
      timestamp_str[outputStrCharIndex] = 0; outputStrCharIndex = 0; currentStrChar = '0'; while (currentStrChar != '\n') { currentStrChar = inputBuffer[inputStrCharIndex++]; status_word_str[outputStrCharIndex++] = currentStrChar; }
      status_word_str[outputStrCharIndex] = 0;

      // Turn component number strings into floats
      double voltage = atof(voltage_str);
      double current = atof(current_str) * 1e6; // Convert from Amps to microAmps
      double resistance = atof(resistance_str);
      double timestamp = atof(timestamp_str);
      double status_word = atof(status_word_str);
    
#ifdef COMPUTER_CONNECTED_INFO_DEBUG
      Serial.print("\nThe following data was read:\n\tVoltage: "); Serial.print(voltage);
      Serial.print(" V\n\tCurrent: "); Serial.print(current);
      Serial.print(" microA\n\tResistance: "); Serial.print(resistance);
      Serial.print(" Ohms\n\tTaken "); Serial.print(timestamp);
      Serial.print(" s after powered on\n\n");
#endif

      // Check if Ampage exceeds our tolerance
      if (current >= 1.049) {
        outSerial.println(beep_command);
        Serial.print("\n\tCurrent: "); Serial.println(current);
        delay(1000);
      }
    }
    
    inputBuffercurrentIndex = 0;
    memset(inputBuffer, 0, sizeof(inputBuffer));
  }
}


bool dataTakingOn = false;
unsigned long loopCounter = millis();
unsigned long lastMeasurment = millis();
bool dataTakingOnSwitched = false;
void loop() {
  // Check state of switch and if it has changed
  int switchState = digitalRead(SWITCH_INPUT);
  if (switchState != dataTakingOn) {
    dataTakingOn = !dataTakingOn;
    dataTakingOnSwitched = true;
  }

  // Read data every 100 ms
  loopCounter = millis();
  if (!dataTakingOnSwitched && dataTakingOn && inputBuffercurrentIndex == 0 && (loopCounter - lastMeasurment) >= 100) {
    outSerial.println(read_command);
    lastMeasurment = loopCounter;
  }

  // Recieve data from Keithley
  if (outSerial.available()) {
    byte recieved_byte = outSerial.read();
    addToInputBuffer(recieved_byte);
  }
#ifdef COMPUTER_CONNECTED
  // If computer is connected, you can send any SCPI command over the RS232 connection just by sending it in Arduino's Serial Monitor
  else if (Serial.available()) {
    byte sent_byte = Serial.read();
    
    // The arduino will intercept a '7' byte and treat it as a switch flipping (only use if switch wires are disconnected, otherwise the switch will overwrite this)
    if (sent_byte == '7') {
      dataTakingOn = !dataTakingOn;
      dataTakingOnSwitched = true;
    }
    else outSerial.write(sent_byte);
    
  }
#endif
  // Only start data taking with the :READ? commands if no data is being currently read by the arduino from the Keithley or from a connected computer
  else if (dataTakingOnSwitched) {
    dataTakingOnSwitched = false;
    if (dataTakingOn) {
#ifdef COMPUTER_CONNECTED
      Serial.println("ARDUINO: Data Taking Switched On!");
#endif
      outSerial.println(":OUTP ON");
    }
    else {
#ifdef COMPUTER_CONNECTED
      Serial.println("ARDUINO: Data Taking Switched Off!");
#endif
      outSerial.println(":OUTP OFF");
    }
  }
}
